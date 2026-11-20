#include "Core/ChickenGame.h"
#if !UE_BUILD_SHIPPING
#include "ShaderCompiler.h"
#endif
#include "Core/ChickenCommand.h"
#include "MediaPlayer.h"
#include "ImageUtils.h"
#include "Engine/Texture2D.h"
#include "MediaTexture.h"
#include "MediaSoundComponent.h"
#include "Misc/Paths.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Core/ChickenGameState.h"
#include "Core/ChickenSave.h"
#include "Core/CivRules.h"
#include "Core/ArmyRules.h"
#include "Core/CivAI.h"
#include "Map/MapGenerator.h"
#include "Player/StrategyPlayerController.h"
#include "Core/LlmStrategy.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/ConfigCacheIni.h"
#include "Model/TechTree.h"
#include "Map/HexMapActor.h"
#include "Map/WorldViewActor.h"
#include "Player/StrategyPawn.h"
#include "Player/StrategyPlayerController.h"
#include "Player/ChickenHUD.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"

AChickenGame::AChickenGame()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false; // kazda maszyna ma wlasna instancje; synchronizacja przez polecenia
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}

AChickenGame* AChickenGame::Get(const UWorld* World)
{
	if (!World) return nullptr;
	for (TActorIterator<AChickenGame> It(const_cast<UWorld*>(World)); It; ++It) return *It;
	return nullptr;
}

AChickenGame* AChickenGame::Ensure(UWorld* World, const FString& Options)
{
	if (!World) return nullptr;
	if (AChickenGame* G = Get(World)) return G;
	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AChickenGame* G = World->SpawnActorDeferred<AChickenGame>(AChickenGame::StaticClass(), FTransform::Identity, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!G) return nullptr;
	G->ApplyUrlOptions(Options);
	G->FinishSpawning(FTransform::Identity);
	return G;
}

void AChickenGame::ApplyUrlOptions(const FString& Options)
{
	bUrlOptions = UGameplayStatics::HasOption(Options, TEXT("Size")) || UGameplayStatics::HasOption(Options, TEXT("Players"))
		|| UGameplayStatics::HasOption(Options, TEXT("Seed")) || UGameplayStatics::HasOption(Options, TEXT("Difficulty"))
		|| UGameplayStatics::HasOption(Options, TEXT("NoMenu"));
	bLobbyRequested = UGameplayStatics::HasOption(Options, TEXT("Lobby"));
	if (UGameplayStatics::HasOption(Options, TEXT("Size")))
	{
		const FString V = UGameplayStatics::ParseOption(Options, TEXT("Size"));
		if (V.Equals(TEXT("Small"), ESearchCase::IgnoreCase)) DefaultSettings.Size = EMapSize::Small;
		else if (V.Equals(TEXT("Large"), ESearchCase::IgnoreCase)) DefaultSettings.Size = EMapSize::Large;
		else DefaultSettings.Size = EMapSize::Medium;
	}
	if (UGameplayStatics::HasOption(Options, TEXT("Players"))) DefaultSettings.NumPlayers = UGameplayStatics::GetIntOption(Options, TEXT("Players"), 2);
	if (UGameplayStatics::HasOption(Options, TEXT("Seed"))) DefaultSettings.Seed = UGameplayStatics::GetIntOption(Options, TEXT("Seed"), 0);
	if (UGameplayStatics::HasOption(Options, TEXT("Difficulty")))
	{
		const FString V = UGameplayStatics::ParseOption(Options, TEXT("Difficulty"));
		if (V.Equals(TEXT("Easy"), ESearchCase::IgnoreCase)) DefaultSettings.Difficulty = EDifficulty::Easy;
		else if (V.Equals(TEXT("Hard"), ESearchCase::IgnoreCase)) DefaultSettings.Difficulty = EDifficulty::Hard;
		else DefaultSettings.Difficulty = EDifficulty::Normal;
	}
}

void AChickenGame::BeginPlay()
{
	Super::BeginPlay();

	bClientProxy = (GetNetMode() == NM_Client);
	if (bClientProxy)
	{
		// Klient LAN: czeka w menu, az gospodarz rozpocznie gre (polecenie NewGame).
		Menu.Settings = DefaultSettings;
		Menu.bOpen = true;
		Menu.bGameStarted = false;
		Menu.Row = (int32)EMenuRow::Quit;
	}
	else
	{
		// Menu startowe, chyba ze gra poszla z opcjami w URL albo narzedziem zrzutow (-AutoShot / -NoMenu).
		const bool bForceMenu = bLobbyRequested || FParse::Param(FCommandLine::Get(), TEXT("ShowMenu"));
		const bool bSkipMenu = !bForceMenu && (bUrlOptions || FParse::Param(FCommandLine::Get(), TEXT("NoMenu"))
			|| FString(FCommandLine::Get()).Contains(TEXT("-AutoShot=")));
		Menu.Settings = DefaultSettings;
		if (!bSkipMenu) LoadMenuPrefs();
		Menu.Settings.Seed = 0; // kazda nowa gra ma losowa mape, chyba ze gracz wpisze ziarno
		StartNewGame(bSkipMenu ? DefaultSettings : Menu.Settings);
		Menu.bGameStarted = bSkipMenu;
		Menu.bOpen = !bSkipMenu;
		if (!IsMenuRowVisible((EMenuRow)Menu.Row)) SetMenuPage(Menu.Page);
		bHandoff = false;
		// Logowanie: pamietane konto wchodzi od razu, inaczej ekran logowania (lub -ShowLogin do zrzutow).
		if (!bSkipMenu && !bLobbyRequested) { if (!Auth::LoadRemembered()) Login.bOpen = true; }
		if (FParse::Param(FCommandLine::Get(), TEXT("ShowLogin"))) Login.bOpen = true;
		int32 DbgPage = -1; // narzedzie do zrzutow: -MenuPage=0..4 pokazuje wybrana strone menu
		if (FParse::Value(FCommandLine::Get(), TEXT("-MenuPage="), DbgPage) && DbgPage >= 0 && DbgPage <= 6) { Login.bOpen = false; SetMenuPage((EMenuPage)DbgPage); }
		// Intro: film przed pierwszym menu (nie w LAN-lobby, nie przy opcjach URL; -NoIntro wylacza, -ShowIntro wymusza).
		if (FParse::Param(FCommandLine::Get(), TEXT("ShowIntro")) || (!bSkipMenu && !bLobbyRequested && !FParse::Param(FCommandLine::Get(), TEXT("NoIntro")))) StartIntro();
	}

#if !UE_BUILD_SHIPPING
	// Narzedzie testowe: -DebugBattle ustawia bitwe armii gracza 1 z armia gracza 2 (do zrzutu planszy walki).
	if (FParse::Param(FCommandLine::Get(), TEXT("DebugBattle")) && !bClientProxy)
	{
		if (AChickenGameState* DGS = GetChickenState())
		{
			const FUnit* First = DGS->Civ.Units.FindByPredicate([](const FUnit& U) { return U.Owner == 0; });
			if (First)
			{
				const FHexCoord A = First->Pos.Neighbor(0).Neighbor(0);
				const FHexCoord Bt = A.Neighbor(0);
				const int32 Cmd = CivRules::SpawnUnit(DGS->Civ, 0, EUnitType::Commander, A).Id;
				ArmyRules::CreateArmy(DGS->Civ, Cmd);
				const int32 ArmyId = CivRules::FindUnit(DGS->Civ, Cmd)->ArmyId;
				const EUnitType Mine[] = { EUnitType::Warrior, EUnitType::Warrior, EUnitType::Archer, EUnitType::Archer, EUnitType::Warrior, EUnitType::Medic, EUnitType::BlackHoleThrower, EUnitType::Resurrector };
				for (EUnitType T : Mine) ArmyRules::JoinArmy(DGS->Civ, ArmyId, CivRules::SpawnUnit(DGS->Civ, 0, T, A).Id);
				const int32 ECmd = CivRules::SpawnUnit(DGS->Civ, 1, EUnitType::Commander, Bt).Id;
				ArmyRules::CreateArmy(DGS->Civ, ECmd);
				const int32 EArmy = CivRules::FindUnit(DGS->Civ, ECmd)->ArmyId;
				const EUnitType Theirs[] = { EUnitType::Warrior, EUnitType::Warrior, EUnitType::Warrior, EUnitType::Archer, EUnitType::Deputy, EUnitType::Medic };
				for (EUnitType T : Theirs) ArmyRules::JoinArmy(DGS->Civ, EArmy, CivRules::SpawnUnit(DGS->Civ, 1, T, Bt).Id);
				CivAI::DeclareWar(DGS->Civ, 0, 1);
				Impl_StartAttack(Cmd, Bt, nullptr);
			}
		}
	}

	// Narzedzie testowe: -DebugUnits rozstawia po jednej jednostce kazdego typu kurczaka (do strojenia modeli i broni).
	if (FParse::Param(FCommandLine::Get(), TEXT("DebugUnits")) && !bClientProxy)
	{
		if (AChickenGameState* DGS = GetChickenState())
		{
			const FUnit* First = DGS->Civ.Units.FindByPredicate([](const FUnit& U) { return U.Owner == 0; });
			if (First)
			{
				const EUnitType All[] = { EUnitType::Settler, EUnitType::Worker, EUnitType::Warrior, EUnitType::Archer, EUnitType::Scout, EUnitType::Commander,
					EUnitType::Medic, EUnitType::Resurrector, EUnitType::Deputy, EUnitType::LaserGunslinger, EUnitType::BlackHoleThrower, EUnitType::GreenGuardian, EUnitType::Ship };
				for (int32 i = 0; i < 13; ++i)
				{
					FHexCoord P = First->Pos;
					for (int32 c = 0; c < (i % 6) + 1; ++c) P = P.Neighbor(0);
					for (int32 r = 0; r < (i / 6) * 2 + 1; ++r) P = P.Neighbor(r % 2 ? 2 : 1);
					CivRules::SpawnUnit(DGS->Civ, 0, All[i], P);
				}
				// teren testowy: pas lasu, wzgorz, gor i farm przed kurczakami
				const ETerrainType Ter[] = { ETerrainType::Forest, ETerrainType::Hills, ETerrainType::Mountain, ETerrainType::Forest, ETerrainType::Hills, ETerrainType::Mountain };
				for (int32 i = 0; i < 6; ++i)
				{
					FHexCoord P = First->Pos;
					for (int32 s = 0; s < 3; ++s) P = P.Neighbor(0);
					for (int32 s = 0; s < i; ++s) P = P.Neighbor(1);
					const int32 Idx = DGS->Grid.IndexOf(P);
					if (Idx != INDEX_NONE) DGS->Grid.Tiles[Idx].Terrain = Ter[i];
					FHexCoord F2 = P;
					for (int32 s = 0; s < 2; ++s) F2 = F2.Neighbor(0);
					FImprovement Imp; Imp.Pos = F2; Imp.Type = EImprovement::Farm;
					if (i % 2 == 0) DGS->Civ.Improvements.Add(Imp);
				}
				const int32 Pops[] = { 1, 4, 9 };
				for (int32 k = 0; k < 3; ++k)
				{
					FCity C;
					C.Id = 900 + k; C.Owner = 0; C.Name = TEXT("Test"); C.Population = Pops[k];
					FHexCoord CP = First->Pos;
					for (int32 s = 0; s < 2 * (k + 1); ++s) CP = CP.Neighbor(3);
					C.Pos = CP;
					DGS->Civ.Cities.Add(C);
				}
				CivRules::ComputeVisibility(DGS->Grid, DGS->Civ, 0);
				RefreshViews();
			}
		}
	}

	if (FParse::Param(FCommandLine::Get(), TEXT("AutoLan")))
	{
		int32 TargetTurns = 12;
		FParse::Value(FCommandLine::Get(), TEXT("-AutoLanTurns="), TargetTurns);
		FTimerHandle LanTimer;
		GetWorldTimerManager().SetTimer(LanTimer, FTimerDelegate::CreateWeakLambda(this, [this, TargetTurns]() { AutoLanStep(TargetTurns); }), 0.7f, true);
	}
#endif

#if !UE_BUILD_SHIPPING
	// Narzedzie testowe: -AutoShot="plik.png" robi zrzut po -AutoShotDelay sekundach i zamyka gre.
	FString ShotPath;
	if (FParse::Value(FCommandLine::Get(), TEXT("-AutoShot="), ShotPath) && !ShotPath.IsEmpty())
	{
		float Delay = 6.f;
		FParse::Value(FCommandLine::Get(), TEXT("-AutoShotDelay="), Delay);
		// Zoom kamery ustawiamy z wyprzedzeniem, zeby klatka zrzutu juz go uwzgledniala.
		float ZoomSteps = 0.f;
		if (FParse::Value(FCommandLine::Get(), TEXT("-CamZoomSteps="), ZoomSteps))
		{
			FTimerHandle Z;
			GetWorldTimerManager().SetTimer(Z, FTimerDelegate::CreateLambda([this, ZoomSteps]()
			{
				if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) if (AStrategyPawn* Pawn = Cast<AStrategyPawn>(PC->GetPawn())) Pawn->ZoomBy(ZoomSteps);
			}), FMath::Max(1.f, Delay - 3.f), false);
		}
		// -CamX / -CamY przesuwaja kamere (strojenie widoku modeli).
		float CamX = 0.f, CamY = 0.f;
		FParse::Value(FCommandLine::Get(), TEXT("-CamX="), CamX);
		FParse::Value(FCommandLine::Get(), TEXT("-CamY="), CamY);
		if (CamX != 0.f || CamY != 0.f)
		{
			FTimerHandle PanT;
			GetWorldTimerManager().SetTimer(PanT, FTimerDelegate::CreateLambda([this, CamX, CamY]()
			{
				if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) if (APawn* Pawn = PC->GetPawn()) Pawn->AddActorWorldOffset(FVector(CamX, CamY, 0.f));
			}), FMath::Max(1.f, Delay - 4.f), false);
		}
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, FTimerDelegate::CreateLambda([this, ShotPath]()
		{
			if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation(); // zrzut dopiero po skompilowaniu shaderow
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
			{
				PC->ConsoleCommand(FString::Printf(TEXT("HighResShot 1280x720 filename=\"%s\""), *ShotPath));
			}
			FTimerHandle Q;
			GetWorldTimerManager().SetTimer(Q, FTimerDelegate::CreateLambda([this]() { if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->ConsoleCommand(TEXT("quit")); }), 3.f, false);
		}), Delay, false);
	}
#endif
}

void AChickenGame::StartNewGame(const FMapSettings& Settings)
{
	AChickenGameState* GS = GetChickenState();
	if (!GS) return;

	if (!Map)
	{
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Map = GetWorld()->SpawnActor<AHexMapActor>(AHexMapActor::StaticClass(), FTransform::Identity, P);
		View = GetWorld()->SpawnActor<AWorldViewActor>(AWorldViewActor::StaticClass(), FTransform::Identity, P);

		// Swiatlo, bez ktorego jednostki (materialy oswietlane) sa czarne.
		if (ADirectionalLight* Sun = GetWorld()->SpawnActor<ADirectionalLight>(FVector(0, 0, 1000), FRotator(-50.f, 30.f, 0.f), P))
		{
			Sun->GetLightComponent()->SetIntensity(4.f);
			Sun->SetMobility(EComponentMobility::Movable);
		}
		if (ADirectionalLight* Fill = GetWorld()->SpawnActor<ADirectionalLight>(FVector(0, 0, 1000), FRotator(-35.f, 210.f, 0.f), P))
		{
			Fill->GetLightComponent()->SetIntensity(2.5f);
			Fill->SetMobility(EComponentMobility::Movable);
		}
		if (ASkyLight* Sky = GetWorld()->SpawnActor<ASkyLight>(FVector(0, 0, 1000), FRotator::ZeroRotator, P))
		{
			Sky->GetLightComponent()->SetMobility(EComponentMobility::Movable);
			Sky->GetLightComponent()->SetIntensity(1.5f);
			Sky->GetLightComponent()->SetLowerHemisphereColor(FLinearColor(0.3f, 0.3f, 0.3f));
			Sky->GetLightComponent()->bLowerHemisphereIsBlack = false;
			Sky->GetLightComponent()->RecaptureSky();
		}
	}

	if (!GS->NewGame(Settings))
	{
		UE_LOG(LogTemp, Warning, TEXT("Nie udalo sie rozstawic wszystkich graczy na tej mapie"));
	}
	const FCivPlayerState* PS = GS->GetPlayerState(GetViewer());
	Map->ShowMap(&GS->Grid, PS ? &PS->Visibility : nullptr);
	View->Sync(*GS, *Map, GetViewer());
	FocusOnViewerStart();
}

AChickenGameState* AChickenGame::GetChickenState() const
{
	return GetWorld() ? GetWorld()->GetGameState<AChickenGameState>() : nullptr;
}

const FMapSettings& AChickenGame::GetActiveSettings() const
{
	static const FMapSettings Fallback;
	const AChickenGameState* GS = GetChickenState();
	return GS ? GS->Settings : Fallback;
}

int32 AChickenGame::GetViewer() const
{
	if (IsNetworked()) return LocalPlayerIndex();
	const AChickenGameState* GS = GetChickenState();
	return GS ? GS->CurrentPlayer : 0;
}

void AChickenGame::RefreshViews()
{
	AChickenGameState* GS = GetChickenState();
	if (!GS || !Map || !View) return;
	const FCivPlayerState* PS = GS->GetPlayerState(GetViewer());
	Map->UpdateVisibility(PS ? &PS->Visibility : nullptr);
	View->Sync(*GS, *Map, GetViewer());
}

void AChickenGame::Impl_EndTurn()
{
	AChickenGameState* GS = GetChickenState();
	if (!GS) return;
	GS->EndTurn();
	RefreshViews();
	UE_LOG(LogTemp, Display, TEXT("LANHASH turn=%d player=%d hash=%u"), GS->Turn, GS->CurrentPlayer, ComputeStateHash());
	// Hot seat: nastepny gracz ludzki dostaje ekran przekazania, zeby nie podgladac planszy poprzedniego.
	if (!IsNetworked() && GS->Settings.NumHumans > 1 && GS->Players.IsValidIndex(GS->CurrentPlayer) && !GS->Players[GS->CurrentPlayer].bIsAI)
	{
		bHandoff = true;
		ClearControllerSelection();
	}
}

void AChickenGame::DismissHandoff()
{
	if (!bHandoff) return;
	bHandoff = false;
	RefreshViews();
	FocusOnViewerStart();
}

bool AChickenGame::Impl_OrderMove(int32 UnitId, const FHexCoord& To)
{
	AChickenGameState* GS = GetChickenState();
	if (!GS) return false;
	const FUnit* U = CivRules::FindUnit(GS->Civ, UnitId);
	if (!U || U->Owner != GS->CurrentPlayer) return false;
	if (!CivRules::OrderMove(GS->Grid, GS->Civ, UnitId, To)) return false;
	CivRules::ComputeVisibility(GS->Grid, GS->Civ, GS->CurrentPlayer);
	RefreshViews();
	return true;
}

bool AChickenGame::Impl_FoundCity(int32 UnitId, FString* Why)
{
	AChickenGameState* GS = GetChickenState();
	if (!GS) return false;
	const FUnit* U = CivRules::FindUnit(GS->Civ, UnitId);
	if (!U || U->Owner != GS->CurrentPlayer) return false;
	const int32 CityId = CivRules::FoundCity(GS->Grid, GS->Civ, UnitId, Why);
	if (CityId == INDEX_NONE) return false;
	if (const FCity* C = CivRules::FindCity(GS->Civ, CityId)) GS->AddMessage(FString::Printf(TEXT("Założono miasto %s"), *C->Name));
	CivRules::ComputeVisibility(GS->Grid, GS->Civ, GS->CurrentPlayer);
	RefreshViews();
	return true;
}

bool AChickenGame::Impl_SkipUnit(int32 UnitId)
{
	AChickenGameState* GS = GetChickenState();
	if (!GS) return false;
	FUnit* U = CivRules::FindUnit(GS->Civ, UnitId);
	if (!U || U->Owner != GS->CurrentPlayer) return false;
	U->MovesLeft = 0;
	U->Path.Reset();
	return true;
}

bool AChickenGame::Impl_SetProduction(int32 CityId, const FProduction& P)
{
	AChickenGameState* GS = GetChickenState();
	if (!GS) return false;
	FCity* C = CivRules::FindCity(GS->Civ, CityId);
	if (!C || C->Owner != GS->CurrentPlayer) return false;
	return CivRules::SetProduction(GS->Grid, GS->Civ, *C, P);
}

bool AChickenGame::Impl_CreateArmy(int32 UnitId, FString* Why)
{
	AChickenGameState* GS = GetChickenState();
	const FUnit* U = GS ? CivRules::FindUnit(GS->Civ, UnitId) : nullptr;
	if (!U || U->Owner != GS->CurrentPlayer) return false;
	if (!ArmyRules::CreateArmy(GS->Civ, UnitId, Why)) return false;
	RefreshViews();
	return true;
}

bool AChickenGame::Impl_JoinArmy(int32 UnitId, FString* Why)
{
	AChickenGameState* GS = GetChickenState();
	const FUnit* U = GS ? CivRules::FindUnit(GS->Civ, UnitId) : nullptr;
	if (!U || U->Owner != GS->CurrentPlayer) return false;
	const FArmy* A = ArmyRules::ArmyAt(GS->Civ, U->Pos, U->Owner);
	if (!A) { if (Why) *Why = TEXT("Na tym polu nie ma armii"); return false; }
	if (!ArmyRules::JoinArmy(GS->Civ, A->Id, UnitId, Why)) return false;
	RefreshViews();
	return true;
}

bool AChickenGame::Impl_LeaveArmy(int32 UnitId, FString* Why)
{
	AChickenGameState* GS = GetChickenState();
	const FUnit* U = GS ? CivRules::FindUnit(GS->Civ, UnitId) : nullptr;
	if (!U || U->Owner != GS->CurrentPlayer) return false;
	// Wybrany dowodca wypuszcza ostatnio dodana jednostke (czlonkow nie da sie zaznaczyc).
	if (U->bCommander)
	{
		const FArmy* A = ArmyRules::FindArmy(GS->Civ, U->ArmyId);
		if (!A || A->Members.Num() == 0) { if (Why) *Why = TEXT("Armia jest pusta"); return false; }
		UnitId = A->Members.Last();
	}
	if (!ArmyRules::LeaveArmy(GS->Grid, GS->Civ, UnitId, Why)) return false;
	RefreshViews();
	return true;
}

bool AChickenGame::Impl_SetResearch(ETech Tech)
{
	AChickenGameState* GS = GetChickenState();
	if (!GS || !GS->Civ.PlayerStates.IsValidIndex(GS->CurrentPlayer)) return false;
	return CivTech::SetResearch(GS->Civ.PlayerStates[GS->CurrentPlayer], Tech);
}

// ===== Menu =====
static const TCHAR* MenuSection = TEXT("ChickenCiv.Menu");

void AChickenGame::LoadMenuPrefs()
{
	if (!GConfig) return;
	int32 V = 0;
	if (GConfig->GetInt(MenuSection, TEXT("Size"), V, GGameUserSettingsIni)) Menu.Settings.Size = (EMapSize)FMath::Clamp(V, 0, 2);
	if (GConfig->GetInt(MenuSection, TEXT("Players"), V, GGameUserSettingsIni)) Menu.Settings.NumPlayers = FMath::Clamp(V, 2, 8);
	if (GConfig->GetInt(MenuSection, TEXT("Humans"), V, GGameUserSettingsIni)) Menu.Settings.NumHumans = FMath::Clamp(V, 1, Menu.Settings.NumPlayers);
	if (GConfig->GetInt(MenuSection, TEXT("Difficulty"), V, GGameUserSettingsIni)) Menu.Settings.Difficulty = (EDifficulty)FMath::Clamp(V, 0, 2);
	if (GConfig->GetInt(MenuSection, TEXT("Race"), V, GGameUserSettingsIni)) Menu.Settings.PlayerRace = (EChickenRace)FMath::Clamp(V, 0, 3);
	if (GConfig->GetInt(MenuSection, TEXT("Color"), V, GGameUserSettingsIni)) Menu.Settings.PlayerColorIndex = FMath::Clamp(V, -1, CivColors::NumPalette - 1);
	FString Addr;
	if (GConfig->GetString(MenuSection, TEXT("JoinAddress"), Addr, GGameUserSettingsIni) && !Addr.IsEmpty()) Menu.JoinAddress = Addr;
}

void AChickenGame::SaveMenuPrefs() const
{
	if (!GConfig) return;
	const FMapSettings& S = Menu.Settings;
	GConfig->SetInt(MenuSection, TEXT("Size"), (int32)S.Size, GGameUserSettingsIni);
	GConfig->SetInt(MenuSection, TEXT("Players"), S.NumPlayers, GGameUserSettingsIni);
	GConfig->SetInt(MenuSection, TEXT("Humans"), S.NumHumans, GGameUserSettingsIni);
	GConfig->SetInt(MenuSection, TEXT("Difficulty"), (int32)S.Difficulty, GGameUserSettingsIni);
	GConfig->SetInt(MenuSection, TEXT("Race"), (int32)S.PlayerRace, GGameUserSettingsIni);
	GConfig->SetInt(MenuSection, TEXT("Color"), S.PlayerColorIndex, GGameUserSettingsIni);
	GConfig->SetString(MenuSection, TEXT("JoinAddress"), *Menu.JoinAddress, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void AChickenGame::ClearControllerSelection()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AStrategyPlayerController* PC = Cast<AStrategyPlayerController>(*It)) PC->ClearSelection();
	}
}

EMenuPage AChickenGame::GetEffectiveMenuPage() const
{
	return IsNetServer() ? EMenuPage::Setup : Menu.Page;
}

bool AChickenGame::IsMenuRowVisible(EMenuRow Row) const
{
	if (bClientProxy) return Row == EMenuRow::Quit; // klient LAN czeka na gospodarza
	const bool bNet = IsNetworked();
	const bool bLobby = IsNetServer();
	const EMenuPage P = GetEffectiveMenuPage();
	const bool bSetup = (P == EMenuPage::Setup || P == EMenuPage::HotSeat);
	switch (Row)
	{
	case EMenuRow::Resume: return Menu.bGameStarted && (P == EMenuPage::Main || bNet);
	case EMenuRow::NewGame:
	case EMenuRow::MultiGame:
	case EMenuRow::Load:
	case EMenuRow::OpenSettings:
	case EMenuRow::OpenCredits: return !bNet && P == EMenuPage::Main;
	case EMenuRow::Account: return !bNet && P == EMenuPage::Settings;
	case EMenuRow::Save: return Menu.bGameStarted && !bNet && P == EMenuPage::Main;
	case EMenuRow::ApiKey: return P == EMenuPage::Settings;
	case EMenuRow::Quit: return P == EMenuPage::Main || bNet;
	case EMenuRow::Size:
	case EMenuRow::Civs:
	case EMenuRow::Difficulty:
	case EMenuRow::Race:
	case EMenuRow::Color:
	case EMenuRow::Seed:
	case EMenuRow::Start: return bSetup;
	case EMenuRow::Humans: return P == EMenuPage::HotSeat;
	case EMenuRow::HotSeat:
	case EMenuRow::LanMenu: return P == EMenuPage::Multi;
	case EMenuRow::HostLan:
	case EMenuRow::JoinAddress:
	case EMenuRow::JoinLan: return P == EMenuPage::Lan;
	case EMenuRow::Back: return !bLobby && P != EMenuPage::Main;
	default: return false;
	}
}

void AChickenGame::SetMenuPage(EMenuPage Page)
{
	Menu.Page = Page;
	Menu.Message.Reset();
	FMapSettings& S = Menu.Settings;
	if (Page == EMenuPage::HotSeat) S.NumHumans = FMath::Clamp(FMath::Max(2, S.NumHumans), 2, FMath::Max(2, S.NumPlayers));
	if (Page == EMenuPage::Setup) S.NumHumans = 1;
	for (int32 i = 0; i < (int32)EMenuRow::Count; ++i)
	{
		if (IsMenuRowVisible((EMenuRow)i)) { Menu.Row = i; break; }
	}
}

void AChickenGame::MenuBack()
{
	if (IsNetworked()) return;
	switch (Menu.Page)
	{
	case EMenuPage::HotSeat:
	case EMenuPage::Lan: SetMenuPage(EMenuPage::Multi); break;
	case EMenuPage::Main: break;
	default: SetMenuPage(EMenuPage::Main); break;
	}
}

void AChickenGame::ToggleMenu()
{
	if (bIntro) { SkipIntro(); return; }
	if (Login.bOpen) return;
	if (IsInBattle() || bHandoff) return;
	if (Menu.bOpen)
	{
		if (!IsNetworked() && Menu.Page != EMenuPage::Main) MenuBack();
		else if (Menu.bGameStarted) Menu.bOpen = false; // Esc w menu startowym nic nie robi
	}
	else
	{
		Menu.bOpen = true;
		// Ustawienia edytowane zaczynaja od tych z biezacej gry.
		if (const AChickenGameState* GS = GetChickenState()) { const int32 Seed = Menu.Settings.Seed; Menu.Settings = GS->Settings; Menu.Settings.Seed = Seed; }
		SetMenuPage(EMenuPage::Main);
	}
}

void AChickenGame::MenuMove(int32 Delta)
{
	if (!Menu.bOpen || Login.bOpen) return;
	const int32 N = (int32)EMenuRow::Count;
	if (!IsMenuRowVisible((EMenuRow)Menu.Row)) { SetMenuPage(Menu.Page); return; }
	for (int32 Step = 0; Step < N; ++Step)
	{
		Menu.Row = ((Menu.Row + Delta) % N + N) % N;
		if (IsMenuRowVisible((EMenuRow)Menu.Row)) break;
	}
	Menu.Message.Reset();
}

void AChickenGame::MenuChange(int32 Delta)
{
	if (!Menu.bOpen || Login.bOpen) return;
	FMapSettings& S = Menu.Settings;
	auto Cycle = [Delta](int32 V, int32 Min, int32 Max) { const int32 R = Max - Min + 1; return Min + ((V - Min + Delta) % R + R) % R; };
	switch ((EMenuRow)Menu.Row)
	{
	case EMenuRow::Size: S.Size = (EMapSize)Cycle((int32)S.Size, 0, 2); break;
	case EMenuRow::Civs: S.NumPlayers = Cycle(S.NumPlayers, 2, 8); S.NumHumans = FMath::Min(S.NumHumans, S.NumPlayers); break;
	case EMenuRow::Humans: S.NumHumans = Cycle(S.NumHumans, 2, FMath::Max(2, S.NumPlayers)); break;
	case EMenuRow::Difficulty: S.Difficulty = (EDifficulty)Cycle((int32)S.Difficulty, 0, 2); break;
	case EMenuRow::Race: S.PlayerRace = (EChickenRace)Cycle((int32)S.PlayerRace, 0, 3); break;
	case EMenuRow::Color: S.PlayerColorIndex = Cycle(S.PlayerColorIndex, -1, CivColors::NumPalette - 1); break;
	default: break;
	}
}

void AChickenGame::MenuActivate()
{
	if (!Menu.bOpen || Login.bOpen) return;
	Menu.Message.Reset();
	switch ((EMenuRow)Menu.Row)
	{
	case EMenuRow::Resume:
		Menu.bOpen = false;
		break;
	case EMenuRow::NewGame: SetMenuPage(EMenuPage::Setup); break;
	case EMenuRow::OpenSettings: SetMenuPage(EMenuPage::Settings); break;
	case EMenuRow::OpenCredits: SetMenuPage(EMenuPage::Credits); break;
	case EMenuRow::MultiGame: SetMenuPage(EMenuPage::Multi); break;
	case EMenuRow::HotSeat: SetMenuPage(EMenuPage::HotSeat); break;
	case EMenuRow::LanMenu: SetMenuPage(EMenuPage::Lan); break;
	case EMenuRow::Back: MenuBack(); break;
	case EMenuRow::Start:
		SaveMenuPrefs();
		if (IsNetServer())
		{
			// LAN: wszyscy polaczeni to gracze ludzcy; gospodarz losuje ziarno i rozsyla start gry.
			FMapSettings S = Menu.Settings;
			S.NumHumans = FMath::Max(1, GetNumLobbyPlayers());
			S.NumPlayers = FMath::Max(S.NumPlayers, S.NumHumans);
			if (S.Seed == 0) S.Seed = FMapGenerator::RandomSeed();
			FChickenCommand C = FChickenCommand::Make(ECmd::NewGame);
			C.Settings = S;
			NewNetworkGame(S);
				Menu.Settings.Seed = 0;
			NetBroadcast(C);
			break;
		}
		if (Menu.Page == EMenuPage::Setup) Menu.Settings.NumHumans = 1;
		StartNewGame(Menu.Settings);
		ClearControllerSelection();
		Menu.bGameStarted = true;
		Menu.bOpen = false;
		Menu.Page = EMenuPage::Main;
		Menu.Settings.Seed = 0; // nastepna gra znowu losowa
		break;
	case EMenuRow::HostLan:
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			SaveMenuPrefs();
			PC->ConsoleCommand(FString::Printf(TEXT("open %s?listen?Lobby"), *GetWorld()->GetOutermost()->GetName()));
		}
		break;
	case EMenuRow::JoinLan:
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
		{
			SaveMenuPrefs();
			PC->ClientTravel(Menu.JoinAddress, ETravelType::TRAVEL_Absolute);
		}
		break;
	case EMenuRow::Load:
		if (LoadGame()) { ClearControllerSelection(); Menu.bGameStarted = true; Menu.bOpen = false; }
		else Menu.Message = TEXT("Brak zapisu do wczytania");
		break;
	case EMenuRow::Save:
		Menu.Message = SaveGame() ? TEXT("Zapisano grę") : TEXT("Zapis nie powiódł się");
		break;
	case EMenuRow::Quit:
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->ConsoleCommand(TEXT("quit"));
		break;
	case EMenuRow::ApiKey:
		MenuPasteApiKey();
		break;
	case EMenuRow::Account:
		OpenLogin();
		break;
	default:
		MenuChange(1);
		break;
	}
}

void AChickenGame::MenuPeriod()
{
	if (Menu.bOpen && (EMenuRow)Menu.Row == EMenuRow::JoinAddress && Menu.JoinAddress.Len() < 40) Menu.JoinAddress += TEXT(".");
}

int32 AChickenGame::GetNumLobbyPlayers() const
{
	return GetWorld() ? GetWorld()->GetNumPlayerControllers() : 1;
}

void AChickenGame::MenuDigit(int32 Digit)
{
	if (Menu.bOpen && (EMenuRow)Menu.Row == EMenuRow::JoinAddress)
	{
		if (Menu.JoinAddress.Len() < 40) Menu.JoinAddress += FString::FromInt(FMath::Clamp(Digit, 0, 9));
		return;
	}
	if (!Menu.bOpen || (EMenuRow)Menu.Row != EMenuRow::Seed) return;
	int32& Seed = Menu.Settings.Seed;
	if (Seed < 100000000) Seed = Seed * 10 + FMath::Clamp(Digit, 0, 9);
}

void AChickenGame::MenuBackspace()
{
	if (Menu.bOpen && (EMenuRow)Menu.Row == EMenuRow::JoinAddress) { Menu.JoinAddress.LeftChopInline(1, EAllowShrinking::No); return; }
	if (!Menu.bOpen || (EMenuRow)Menu.Row != EMenuRow::Seed) return;
	Menu.Settings.Seed /= 10;
}

void AChickenGame::MenuPasteApiKey()
{
	FString Text;
	FPlatformApplicationMisc::ClipboardPaste(Text);
	Text.TrimStartAndEndInline();
	if (Text.IsEmpty() || Text.Len() > 300 || Text.Contains(TEXT(" ")) || Text.Contains(TEXT("\n")))
	{
		Menu.Message = TEXT("Schowek nie zawiera klucza API");
		return;
	}
	LlmStrategy::SetApiKey(Text);
	Menu.Message = TEXT("Klucz API zapisany lokalnie (tylko na tym komputerze)");
}

void AChickenGame::MenuClearApiKey()
{
	LlmStrategy::SetApiKey(FString());
	Menu.Message = TEXT("Klucz API usunięty");
}

// ===== Bitwa taktyczna =====
bool AChickenGame::Impl_StartAttack(int32 UnitId, const FHexCoord& Target, FString* Why)
{
	auto Fail = [&](const TCHAR* Msg) { if (Why) *Why = Msg; return false; };
	AChickenGameState* GS = GetChickenState();
	if (!GS || IsInBattle()) return false;
	FUnit* U = CivRules::FindUnit(GS->Civ, UnitId);
	if (!U || U->Owner != GS->CurrentPlayer) return false;
	if (U->IsCivilian()) return Fail(TEXT("Cywile nie atakują"));
	if (U->IsArmyMember()) return Fail(TEXT("Rozkazy wydaje dowódca armii"));
	if (U->MovesLeft <= 0) return Fail(TEXT("Jednostka nie ma już ruchu"));
	if (U->Pos.Distance(Target) != 1) return Fail(TEXT("Cel musi sąsiadować z jednostką"));

	// Atak na kogos oznacza wojne z jego wlascicielem.
	{
		int32 Enemy = -1;
		for (const FUnit& D : GS->Civ.Units) if (D.Pos == Target && D.Owner != U->Owner) { Enemy = D.Owner; break; }
		if (Enemy < 0) if (const FCity* C = CivRules::CityAt(GS->Civ, Target)) if (C->Owner != U->Owner) Enemy = C->Owner;
		if (Enemy >= 0 && CivAI::DeclareWar(GS->Civ, U->Owner, Enemy))
		{
			GS->AddMessage(FString::Printf(TEXT("Wypowiedziano wojnę cywilizacji %d"), Enemy + 1));
		}
	}

	// Brak obroncow: puste obce miasto jest po prostu zajmowane.
	const bool bDefenders = GS->Civ.Units.ContainsByPredicate([&](const FUnit& D) { return D.Pos == Target && D.Owner != U->Owner; });
	if (!bDefenders)
	{
		FCity* City = GS->Civ.Cities.FindByPredicate([&](const FCity& C) { return C.Pos == Target && C.Owner != U->Owner; });
		if (!City) return Fail(TEXT("Nie ma tam wroga"));
		City->Owner = U->Owner;
		City->Current = FProduction();
		U->Pos = Target;
		U->MovesLeft = 0;
		if (U->bCommander)
		{
			for (FUnit& Mem : GS->Civ.Units) if (Mem.ArmyId == U->ArmyId && Mem.IsArmyMember()) Mem.Pos = Target;
		}
		GS->AddMessage(FString::Printf(TEXT("Zdobyto miasto %s"), *City->Name));
		CivRules::ComputeVisibility(GS->Grid, GS->Civ, GS->CurrentPlayer);
		RefreshViews();
		return true;
	}

	TSharedPtr<FBattle> B = MakeShared<FBattle>();
	if (!BattleRules::Setup(GS->Civ, UnitId, Target, *B, Why)) return false;
	BattleRules::Begin(*B);
	ActiveBattle = B;
	bDeathStarMode = false;
	GS->AddMessage(B->bGreenWipe ? TEXT("Zielony strażnik zniszczył atakującą armię!") : TEXT("Rozpoczęła się bitwa"));
	BattleAdvance();
	return true;
}

void AChickenGame::BattleAdvance()
{
	if (!ActiveBattle.IsValid()) return;
	FBattle& B = *ActiveBattle;
	for (int32 Guard = 0; Guard < 1000 && B.Winner < 0; ++Guard)
	{
		const FBattleUnit* Cur = B.Current();
		if (!Cur || Cur->Side == 0) break;
		BattleRules::AIPlayUnit(B);
	}
}

bool AChickenGame::Impl_BattleClick(const FHexCoord& Hex)
{
	if (!ActiveBattle.IsValid()) return false;
	FBattle& B = *ActiveBattle;
	FBattleUnit* Cur = B.Current();
	if (B.Winner >= 0 || !Cur || Cur->Side != 0 || !BattleRules::InBoard(Hex)) return false;
	const int32 Id = Cur->Id;
	const FBattleUnit* T = BattleRules::UnitAt(B, Hex);

	bool bDone = false;
	if (bDeathStarMode)
	{
		bDeathStarMode = false;
		if (T && T->Side == 1) bDone = BattleRules::DeathStarLaser(B, 0, T->Id);
	}
	else if (Cur->Type == EUnitType::BlackHoleThrower && Cur->AimTurns == 0 && (!T || T->Side == 1))
	{
		bDone = BattleRules::BlackHole(B, Id, Hex);
	}
	else if (Cur->Type == EUnitType::Medic && T && T->Side == 0)
	{
		bDone = BattleRules::Heal(B, Id, T->Id);
	}
	else if (Cur->Type == EUnitType::Resurrector && BattleRules::CorpseAt(B, Hex, 0))
	{
		bDone = BattleRules::Resurrect(B, Id, BattleRules::CorpseAt(B, Hex, 0)->Id);
	}
	else if (T && T->Side == 1)
	{
		const int32 TargetId = T->Id;
		if (BattleRules::Distance(*Cur, *T) > Cur->Range)
		{
			// Podejdz na pole, z ktorego cel jest w zasiegu (najkrotszy dojazd).
			TArray<FHexCoord> Reach;
			BattleRules::GetReachable(B, *Cur, Reach);
			FHexCoord Best;
			int32 BestCost = MAX_int32;
			for (const FHexCoord& H : Reach)
			{
				if (H.Distance(T->Pos) > Cur->Range) continue;
				const int32 Cost = Cur->Pos.Distance(H);
				if (Cost < BestCost) { BestCost = Cost; Best = H; }
			}
			if (BestCost == MAX_int32) return false;
			BattleRules::Move(B, Id, Best);
		}
		bDone = BattleRules::Attack(B, Id, TargetId);
	}
	else if (!T)
	{
		bDone = BattleRules::Move(B, Id, Hex);
	}
	BattleAdvance();
	return bDone;
}

void AChickenGame::Impl_BattleEndUnitTurn()
{
	if (!ActiveBattle.IsValid() || ActiveBattle->Winner >= 0) return;
	const FBattleUnit* Cur = ActiveBattle->Current();
	if (!Cur || Cur->Side != 0) return;
	bDeathStarMode = false;
	BattleRules::EndUnitTurn(*ActiveBattle);
	BattleAdvance();
}

void AChickenGame::Impl_BattleAutoResolve()
{
	if (!ActiveBattle.IsValid()) return;
	bDeathStarMode = false;
	BattleRules::AutoResolve(*ActiveBattle);
}

bool AChickenGame::BattleToggleDeathStar()
{
	if (!ActiveBattle.IsValid() || ActiveBattle->Winner >= 0) return false;
	const FBattle& B = *ActiveBattle;
	if (!B.bDeathStar[0] || B.bDeathStarUsed[0]) return false;
	bDeathStarMode = !bDeathStarMode;
	return true;
}

void AChickenGame::Impl_CloseBattle()
{
	AChickenGameState* GS = GetChickenState();
	if (!GS || !ActiveBattle.IsValid() || ActiveBattle->Winner < 0) return;
	TArray<FString> Log;
	const int32 Winner = ActiveBattle->Winner;
	BattleRules::Apply(GS->Grid, GS->Civ, *ActiveBattle, &Log);
	GS->AddMessage(Winner == 0 ? TEXT("Zwycięstwo w bitwie!") : TEXT("Przegrana bitwa"));
	if (ActiveBattle->Nuggets[0] > 0) GS->AddMessage(FString::Printf(TEXT("Nuggetsy z bitwy: +%d"), ActiveBattle->Nuggets[0]));
	for (const FString& L : Log) GS->AddMessage(L);
	ActiveBattle.Reset();
	bDeathStarMode = false;
	CivRules::ComputeVisibility(GS->Grid, GS->Civ, GS->CurrentPlayer);
	RefreshViews();
}

int32 AChickenGame::NextIdleUnit(int32 CurrentId) const
{
	const AChickenGameState* GS = GetChickenState();
	if (!GS) return 0;
	const TArray<FUnit>& Units = GS->Civ.Units;
	const int32 N = Units.Num();
	int32 Start = 0;
	for (int32 i = 0; i < N; ++i) if (Units[i].Id == CurrentId) { Start = i + 1; break; }
	for (int32 k = 0; k < N; ++k)
	{
		const FUnit& U = Units[(Start + k) % N];
		if (U.Owner == GetViewer() && !U.IsArmyMember() && U.MovesLeft > 0 && U.Path.Num() == 0) return U.Id;
	}
	return 0;
}

bool AChickenGame::SaveGame(const FString& Slot)
{
	const AChickenGameState* GS = GetChickenState();
	if (!GS) return false;
	FChickenSaveData D;
	D.Settings = GS->Settings;
	D.Turn = GS->Turn;
	D.CurrentPlayer = GS->CurrentPlayer;
	D.Players = GS->Players;
	D.Civ = GS->Civ;
	return FChickenSave::Save(D, Slot);
}

bool AChickenGame::LoadGame(const FString& Slot)
{
	FChickenSaveData D;
	if (!FChickenSave::Load(D, Slot)) return false;
	AChickenGameState* GS = GetChickenState();
	if (!GS) return false;

	StartNewGame(D.Settings);
	GS->ApplySave(D);
	RefreshViews();
	return true;
}

void AChickenGame::FocusCamerasOnMap()
{
	AChickenGameState* GS = GetChickenState();
	if (!GS) return;
	const FBox2D B = GS->Grid.Bounds;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AStrategyPawn* Pawn = Cast<AStrategyPawn>((*It)->GetPawn()))
		{
			Pawn->SetMapBounds(B);
			const FVector2D C = B.GetCenter();
			Pawn->SetActorLocation(FVector(C.X, C.Y, 0.f));
		}
	}
}

void AChickenGame::FocusCamerasOn(const FHexCoord& Hex)
{
	AChickenGameState* GS = GetChickenState();
	if (!GS) return;
	const FVector2D P = Hex.ToWorld(FHexMap::HexSize);
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (AStrategyPawn* Pawn = Cast<AStrategyPawn>((*It)->GetPawn()))
		{
			Pawn->SetMapBounds(GS->Grid.Bounds);
			// Kamera patrzy pod katem, wiec srodek ekranu lezy dalej niz pawn: przesuwamy go o kawalek do tylu.
			Pawn->SetActorLocation(FVector(P.X - 250.f, P.Y, 0.f));
		}
	}
}

void AChickenGame::FocusOnViewerStart()
{
	const AChickenGameState* GS = GetChickenState();
	if (GS)
	{
		const FUnit* First = GS->Civ.Units.FindByPredicate([&](const FUnit& U) { return U.Owner == GetViewer(); });
		if (First) { FocusCamerasOn(First->Pos); return; }
		if (GS->Civ.Cities.Num() > 0) { FocusCamerasOn(GS->Civ.Cities[0].Pos); return; }
	}
	FocusCamerasOnMap();
}


// ===== Siec LAN: deterministyczna replikacja polecen =====
bool AChickenGame::IsNetworked() const { return GetNetMode() != NM_Standalone; }
bool AChickenGame::IsNetServer() const { return GetNetMode() == NM_ListenServer || GetNetMode() == NM_DedicatedServer; }
bool AChickenGame::IsNetClient() const { return GetNetMode() == NM_Client; }

AStrategyPlayerController* AChickenGame::LocalPC() const
{
	return GetWorld() ? Cast<AStrategyPlayerController>(GetWorld()->GetFirstPlayerController()) : nullptr;
}

int32 AChickenGame::LocalPlayerIndex() const
{
	if (!IsNetworked() || !IsNetClient()) return 0; // gospodarz (i gra pojedyncza) = gracz 1
	const AStrategyPlayerController* PC = LocalPC();
	return PC ? PC->LanPlayerIndex : 0;
}

bool AChickenGame::IsMyTurn() const
{
	const AChickenGameState* GS = GetChickenState();
	return !IsNetworked() || (GS && GS->CurrentPlayer == LocalPlayerIndex());
}

bool AChickenGame::NetRoute(const FChickenCommand& Cmd)
{
	if (bApplyingRemote || !IsNetClient()) return false;
	if (AStrategyPlayerController* PC = LocalPC()) PC->ServerSubmitCommand(Cmd);
	return true;
}

uint32 AChickenGame::ComputeStateHash() const
{
	const AChickenGameState* GS = GetChickenState();
	if (!GS) return 0;
	uint32 H = (uint32)GS->Turn * 2654435761u + (uint32)GS->CurrentPlayer;
	auto Mix = [&H](int32 V) { H = (H ^ (uint32)V) * 16777619u; };
	for (const FUnit& U : GS->Civ.Units) { Mix(U.Id); Mix(U.Owner); Mix((int32)U.Type); Mix(U.Pos.Q); Mix(U.Pos.R); Mix(U.Hp); Mix(U.ArmyId); }
	for (const FCity& C : GS->Civ.Cities) { Mix(C.Id); Mix(C.Owner); Mix(C.Pos.Q); Mix(C.Pos.R); Mix(C.Population); Mix(C.ProdStock); Mix(C.FoodStock); }
	for (const FCivPlayerState& P : GS->Civ.PlayerStates) { Mix(P.Gold); Mix(P.Science); Mix(P.Nuggets); Mix(P.Known.Num()); Mix((int32)P.Researching); Mix(P.ResearchProgress); }
	Mix(GS->Civ.NextId);
	return H;
}

void AChickenGame::NetBroadcast(const FChickenCommand& Cmd)
{
	if (bApplyingRemote || !IsNetServer() || !GetWorld()) return;
	const AChickenGameState* GS = GetChickenState();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		AStrategyPlayerController* PC = Cast<AStrategyPlayerController>(*It);
		if (!PC || PC->IsLocalController()) continue;
		PC->ClientExecuteCommand(Cmd);
		if (GS && Cmd.Type == ECmd::EndTurn) PC->ClientCheckHash(GS->Turn, (int32)ComputeStateHash());
	}
}

void AChickenGame::ExecuteCommand(const FChickenCommand& Cmd)
{
	switch (Cmd.Type)
	{
	case ECmd::EndTurn: EndTurn(); break;
	case ECmd::OrderMove: OrderMove(Cmd.A, FHexCoord(Cmd.B, Cmd.C)); break;
	case ECmd::FoundCity: FoundCity(Cmd.A); break;
	case ECmd::SkipUnit: SkipUnit(Cmd.A); break;
	case ECmd::SetProduction: { FProduction P; P.Kind = (EProductionKind)Cmd.B; P.Item = Cmd.C; SetProduction(Cmd.A, P); break; }
	case ECmd::SetResearch: SetResearch((ETech)Cmd.A); break;
	case ECmd::CreateArmy: CreateArmy(Cmd.A); break;
	case ECmd::JoinArmy: JoinArmy(Cmd.A); break;
	case ECmd::LeaveArmy: LeaveArmy(Cmd.A); break;
	case ECmd::BuildImprovement: BuildImprovement(Cmd.A); break;
	case ECmd::ProposePeace: ProposePeace(); break;
	case ECmd::StartAttack: StartAttack(Cmd.A, FHexCoord(Cmd.B, Cmd.C)); break;
	case ECmd::BattleClick: bDeathStarMode = (Cmd.C != 0); BattleClick(FHexCoord(Cmd.A, Cmd.B)); break;
	case ECmd::BattleEndUnitTurn: BattleEndUnitTurn(); break;
	case ECmd::BattleAutoResolve: BattleAutoResolve(); break;
	case ECmd::CloseBattle: CloseBattle(); break;
	case ECmd::NewGame: NewNetworkGame(Cmd.Settings); break;
	}
}

void AChickenGame::ReceiveCommandFromClient(AStrategyPlayerController* Sender, const FChickenCommand& Cmd)
{
	const AChickenGameState* GS = GetChickenState();
	if (!Sender || !GS || !IsNetServer()) return;
	// Tylko gracz, ktorego jest tura, moze wydawac polecenia (w bitwie tez); start gry tylko gospodarz.
	if (Cmd.Type == ECmd::NewGame) return;
	if (Sender->LanPlayerIndex != GS->CurrentPlayer || IsMenuOpen()) return;
	ExecuteCommand(Cmd); // wrappery wykonaja i rozgloszą do klientow
}

void AChickenGame::ApplyRemoteCommand(const FChickenCommand& Cmd)
{
	TGuardValue<bool> Guard(bApplyingRemote, true);
	ExecuteCommand(Cmd);
}

void AChickenGame::CheckRemoteHash(int32 Turn, int32 ServerHash)
{
	const AChickenGameState* GS = GetChickenState();
	if (!GS || GS->Turn != Turn) return; // inna tura: pomijamy (komunikaty mogly sie rozminac)
	if ((int32)ComputeStateHash() != ServerHash)
	{
		bDesync = true;
		UE_LOG(LogTemp, Error, TEXT("LAN: ROZJAZD stanu gry w turze %d"), Turn);
	}
}

void AChickenGame::NewNetworkGame(const FMapSettings& Settings)
{
	Menu.Settings = Settings;
	StartNewGame(Settings);
	ClearControllerSelection();
	bDesync = false;
	Menu.bGameStarted = true;
	Menu.bOpen = false;
	bHandoff = false;
}

// ----- Wrappery: klient wysyla polecenie na serwer; serwer wykonuje i rozglasza -----
void AChickenGame::EndTurn()
{
	const FChickenCommand C = FChickenCommand::Make(ECmd::EndTurn);
	if (NetRoute(C)) return;
	Impl_EndTurn();
	NetBroadcast(C);
}

bool AChickenGame::OrderMove(int32 UnitId, const FHexCoord& To)
{
	const FChickenCommand C = FChickenCommand::Make(ECmd::OrderMove, UnitId, To.Q, To.R);
	if (NetRoute(C)) return true;
	const bool bOk = Impl_OrderMove(UnitId, To);
	if (bOk) NetBroadcast(C);
	return bOk;
}

bool AChickenGame::FoundCity(int32 UnitId, FString* Why)
{
	const FChickenCommand C = FChickenCommand::Make(ECmd::FoundCity, UnitId);
	if (NetRoute(C)) return true;
	const bool bOk = Impl_FoundCity(UnitId, Why);
	if (bOk) NetBroadcast(C);
	return bOk;
}

bool AChickenGame::SkipUnit(int32 UnitId)
{
	const FChickenCommand C = FChickenCommand::Make(ECmd::SkipUnit, UnitId);
	if (NetRoute(C)) return true;
	const bool bOk = Impl_SkipUnit(UnitId);
	if (bOk) NetBroadcast(C);
	return bOk;
}

bool AChickenGame::SetProduction(int32 CityId, const FProduction& P)
{
	const FChickenCommand C = FChickenCommand::Make(ECmd::SetProduction, CityId, (int32)P.Kind, P.Item);
	if (NetRoute(C)) return true;
	const bool bOk = Impl_SetProduction(CityId, P);
	if (bOk) NetBroadcast(C);
	return bOk;
}

bool AChickenGame::SetResearch(ETech Tech)
{
	const FChickenCommand C = FChickenCommand::Make(ECmd::SetResearch, (int32)Tech);
	if (NetRoute(C)) return true;
	const bool bOk = Impl_SetResearch(Tech);
	if (bOk) NetBroadcast(C);
	return bOk;
}

bool AChickenGame::CreateArmy(int32 UnitId, FString* Why)
{
	const FChickenCommand C = FChickenCommand::Make(ECmd::CreateArmy, UnitId);
	if (NetRoute(C)) return true;
	const bool bOk = Impl_CreateArmy(UnitId, Why);
	if (bOk) NetBroadcast(C);
	return bOk;
}

bool AChickenGame::JoinArmy(int32 UnitId, FString* Why)
{
	const FChickenCommand C = FChickenCommand::Make(ECmd::JoinArmy, UnitId);
	if (NetRoute(C)) return true;
	const bool bOk = Impl_JoinArmy(UnitId, Why);
	if (bOk) NetBroadcast(C);
	return bOk;
}

bool AChickenGame::LeaveArmy(int32 UnitId, FString* Why)
{
	const FChickenCommand C = FChickenCommand::Make(ECmd::LeaveArmy, UnitId);
	if (NetRoute(C)) return true;
	const bool bOk = Impl_LeaveArmy(UnitId, Why);
	if (bOk) NetBroadcast(C);
	return bOk;
}

bool AChickenGame::StartAttack(int32 UnitId, const FHexCoord& Target, FString* Why)
{
	const FChickenCommand C = FChickenCommand::Make(ECmd::StartAttack, UnitId, Target.Q, Target.R);
	if (NetRoute(C)) return true;
	const bool bOk = Impl_StartAttack(UnitId, Target, Why);
	if (bOk) NetBroadcast(C);
	return bOk;
}

bool AChickenGame::BattleClick(const FHexCoord& Hex)
{
	const FChickenCommand C = FChickenCommand::Make(ECmd::BattleClick, Hex.Q, Hex.R, bDeathStarMode ? 1 : 0);
	if (NetRoute(C)) return true;
	const bool bOk = Impl_BattleClick(Hex);
	if (bOk) NetBroadcast(C);
	return bOk;
}

void AChickenGame::BattleEndUnitTurn()
{
	const FChickenCommand C = FChickenCommand::Make(ECmd::BattleEndUnitTurn);
	if (NetRoute(C)) return;
	Impl_BattleEndUnitTurn();
	NetBroadcast(C);
}

void AChickenGame::BattleAutoResolve()
{
	const FChickenCommand C = FChickenCommand::Make(ECmd::BattleAutoResolve);
	if (NetRoute(C)) return;
	Impl_BattleAutoResolve();
	NetBroadcast(C);
}

void AChickenGame::CloseBattle()
{
	const FChickenCommand C = FChickenCommand::Make(ECmd::CloseBattle);
	if (NetRoute(C)) return;
	Impl_CloseBattle();
	NetBroadcast(C);
}


void AChickenGame::AutoLanStep(int32 TargetTurns)
{
	AChickenGameState* GS = GetChickenState();
	if (!GS) return;

	if (!Menu.bGameStarted)
	{
		// Gospodarz rozpoczyna gre, gdy dolaczy drugi gracz (i dodaje jednego AI, zeby testowac tez deterministyczne AI).
		if (IsNetServer() && GetNumLobbyPlayers() >= 2)
		{
			Menu.Settings.Size = EMapSize::Small;
			Menu.Settings.NumPlayers = 3;
			Menu.Settings.Seed = 7;
			Menu.Row = (int32)EMenuRow::Start;
			MenuActivate();
		}
		return;
	}

	if (GS->Turn != AutoLanLastTurn)
	{
		AutoLanLastTurn = GS->Turn;
		UE_LOG(LogTemp, Display, TEXT("LANTEST turn=%d current=%d viewer=%d desync=%d"), GS->Turn, GS->CurrentPlayer, GetViewer(), bDesync ? 1 : 0);
	}
	if (GS->Turn > TargetTurns)
	{
		UE_LOG(LogTemp, Display, TEXT("LANTEST done turn=%d hash=%u desync=%d cities=%d units=%d"), GS->Turn, ComputeStateHash(), bDesync ? 1 : 0, GS->Civ.Cities.Num(), GS->Civ.Units.Num());
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->ConsoleCommand(TEXT("quit"));
		return;
	}
	if (IsInBattle())
	{
		if (IsMyTurn()) { if (GetBattle()->Winner >= 0) CloseBattle(); else BattleAutoResolve(); }
		return;
	}
	if (!IsMyTurn()) return;

	// Ruchy: osadnik zaklada miasto (albo idzie dalej), reszta jednostek spaceruje.
	const int32 Me = GetViewer();
	TArray<int32> Ids;
	for (const FUnit& U : GS->Civ.Units) if (U.Owner == Me && !U.IsArmyMember() && U.MovesLeft > 0) Ids.Add(U.Id);
	for (int32 Id : Ids)
	{
		const FUnit* U = CivRules::FindUnit(GS->Civ, Id);
		if (!U) continue;
		if (U->Type == EUnitType::Settler && FoundCity(Id)) continue;
		FHexCoord To = U->Pos;
		for (int32 k = 0; k < 3; ++k) To = To.Neighbor(AutoLanRand.RandRange(0, 5));
		OrderMove(Id, To);
	}
	// Produkcja w nowych miastach.
	for (const FCity& City : GS->Civ.Cities)
	{
		if (City.Owner != Me || City.Current.Kind != EProductionKind::None) continue;
		TArray<FProductionOption> Opt;
		CivRules::GetProductionOptions(GS->Grid, GS->Civ, City, Opt);
		if (Opt.Num() > 0) SetProduction(City.Id, Opt[0].Item);
	}
	if (const FCivPlayerState* PS = GS->GetPlayerState(Me))
	{
		if (PS->Researching == ETech::None) SetResearch(CivTech::AutoPick(*PS));
	}
	EndTurn();
}

bool AChickenGame::Impl_BuildImprovement(int32 UnitId, FString* Why)
{
	AChickenGameState* GS = GetChickenState();
	const FUnit* U = GS ? CivRules::FindUnit(GS->Civ, UnitId) : nullptr;
	if (!U || U->Owner != GS->CurrentPlayer) return false;
	if (!CivRules::StartImprovement(GS->Grid, GS->Civ, UnitId, Why)) return false;
	RefreshViews();
	return true;
}

bool AChickenGame::BuildImprovement(int32 UnitId, FString* Why)
{
	const FChickenCommand C = FChickenCommand::Make(ECmd::BuildImprovement, UnitId);
	if (NetRoute(C)) return true;
	const bool bOk = Impl_BuildImprovement(UnitId, Why);
	if (bOk) NetBroadcast(C);
	return bOk;
}

bool AChickenGame::Impl_ProposePeace()
{
	AChickenGameState* GS = GetChickenState();
	if (!GS || !GS->Civ.PlayerStates.IsValidIndex(GS->CurrentPlayer)) return false;
	const int32 Me = GS->CurrentPlayer;
	TArray<FString> Log;
	bool bAny = false;
	for (int32 Enemy : TArray<int32>(GS->Civ.PlayerStates[Me].AtWarWith)) bAny |= CivAI::ProposePeace(GS->Civ, Me, Enemy, GS->Turn, &Log);
	for (const FString& L : Log) GS->AddMessage(L);
	return bAny;
}

bool AChickenGame::ProposePeace()
{
	const FChickenCommand C = FChickenCommand::Make(ECmd::ProposePeace);
	if (NetRoute(C)) return true;
	const bool bOk = Impl_ProposePeace();
	if (bOk) NetBroadcast(C);
	return bOk;
}


// ===== Intro =====
UTexture* AChickenGame::GetIntroTexture() const { return IntroTexture; }

UTexture* AChickenGame::GetMenuBgTexture()
{
	if (!bMenuBgTried)
	{
		bMenuBgTried = true;
		const FString Path = FPaths::ProjectContentDir() / TEXT("UI/MenuBg.jpg");
		if (FPaths::FileExists(Path)) MenuBgTexture = FImageUtils::ImportFileAsTexture2D(Path);
	}
	return MenuBgTexture;
}

void AChickenGame::StartIntro()
{
	const FString Path = FPaths::ProjectContentDir() / TEXT("Movies/Intro.mp4");
	if (!FPaths::FileExists(Path) || !GetWorld()) return;

	IntroPlayer = NewObject<UMediaPlayer>(this);
	IntroPlayer->SetLooping(false);
	IntroPlayer->PlayOnOpen = true;
	IntroPlayer->OnEndReached.AddDynamic(this, &AChickenGame::OnIntroEnded);
	IntroPlayer->OnMediaOpenFailed.AddDynamic(this, &AChickenGame::OnIntroFailed);

	IntroTexture = NewObject<UMediaTexture>(this);
	IntroTexture->AutoClear = true;
	IntroTexture->SetMediaPlayer(IntroPlayer);
	IntroTexture->UpdateResource();

	IntroSound = NewObject<UMediaSoundComponent>(this);
	IntroSound->SetMediaPlayer(IntroPlayer);
	IntroSound->RegisterComponent();

	if (!IntroPlayer->OpenFile(Path)) { SkipIntro(); return; }
	bIntro = true;
	IntroStartTime = GetWorld()->GetTimeSeconds();
	// Zabezpieczenie: jesli film nie ruszy w 8 s (brak kodeka), przechodzimy do menu.
	GetWorldTimerManager().SetTimer(IntroWatch, FTimerDelegate::CreateWeakLambda(this, [this]()
	{
		if (bIntro && GetWorld()->GetTimeSeconds() - IntroStartTime > 8.0 && (!IntroPlayer || !IntroPlayer->IsPlaying())) SkipIntro();
	}), 1.f, true);
}

void AChickenGame::SkipIntro()
{
	if (!bIntro && !IntroPlayer) return;
	bIntro = false;
	GetWorldTimerManager().ClearTimer(IntroWatch);
	if (IntroPlayer)
	{
		IntroPlayer->OnEndReached.RemoveAll(this);
		IntroPlayer->OnMediaOpenFailed.RemoveAll(this);
		IntroPlayer->Close();
	}
	if (IntroSound)
	{
		IntroSound->SetMediaPlayer(nullptr);
		IntroSound->DestroyComponent();
		IntroSound = nullptr;
	}
}

void AChickenGame::OnIntroEnded() { SkipIntro(); }
void AChickenGame::OnIntroFailed(FString FailedUrl)
{
	UE_LOG(LogTemp, Warning, TEXT("Intro: nie udalo sie otworzyc filmu (%s)"), *FailedUrl);
	SkipIntro();
}


// ===== Logowanie =====
void AChickenGame::OpenLogin()
{
	LoginCancelWait();
	Login = FLoginState();
	Login.bOpen = true;
	if (!Auth::Current().Provider.IsEmpty()) Login.Message = FString::Printf(TEXT("Zalogowano jako %s – wybierz inne konto lub Esc"), *Auth::DisplayName());
}

void AChickenGame::LoginFinish(const FAccount& Account)
{
	LoginCancelWait();
	Auth::SetCurrent(Account);
	Auth::SaveRemembered();
	Login = FLoginState();
	Menu.Message = FString::Printf(TEXT("Zalogowano: %s"), *Account.DisplayName);
}

void AChickenGame::LoginCancelWait()
{
	Login.bWaiting = false;
	GetWorldTimerManager().ClearTimer(LoginTimer);
}

void AChickenGame::LoginTick()
{
	if (!Login.bWaiting || Login.bPollInFlight || !GetWorld()) return;
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - Login.WaitStart > Login.Device.ExpiresIn) { LoginCancelWait(); Login.Message = TEXT("Kod wygasł – spróbuj ponownie"); return; }
	if (Now < Login.NextPoll) return;
	Login.bPollInFlight = true;
	TWeakObjectPtr<AChickenGame> Weak(this);
	Auth::PollToken((Auth::EProvider)Login.Provider, Login.Device, [Weak](int32 State, const FAccount& Account, const FString& Error)
	{
		AChickenGame* G = Weak.Get();
		if (!G) return;
		G->Login.bPollInFlight = false;
		G->Login.NextPoll = G->GetWorld()->GetTimeSeconds() + G->Login.Device.Interval;
		if (State == 1) G->LoginFinish(Account);
		else if (State == 2) { G->LoginCancelWait(); G->Login.Message = Error; }
	});
}

void AChickenGame::LoginActivate()
{
	if (Login.FormMode != 0)
	{
		FString Err;
		const bool bOk = Login.FormMode == 2 ? Auth::Register(Login.Email, Login.Password, Err) : Auth::Login(Login.Email, Login.Password, Err);
		if (!bOk) { Login.Message = Err; return; }
		LoginFinish(Auth::Current());
		return;
	}
	switch ((ELoginRow)Login.Row)
	{
	case ELoginRow::Google:
	case ELoginRow::Microsoft:
	{
		const Auth::EProvider P = (ELoginRow)Login.Row == ELoginRow::Google ? Auth::EProvider::Google : Auth::EProvider::Microsoft;
		Login.Message = TEXT("Łączenie…");
		Login.Provider = (int32)P;
		TWeakObjectPtr<AChickenGame> Weak(this);
		Auth::RequestDeviceCode(P, [Weak](bool bOk, const Auth::FDeviceCode& Code, const FString& Error)
		{
			AChickenGame* G = Weak.Get();
			if (!G || !G->Login.bOpen) return;
			if (!bOk) { G->Login.Message = Error; return; }
			G->Login.Device = Code;
			G->Login.bWaiting = true;
			G->Login.WaitStart = G->GetWorld()->GetTimeSeconds();
			G->Login.NextPoll = G->Login.WaitStart + Code.Interval;
			G->Login.Message.Reset();
			FPlatformProcess::LaunchURL(*Code.VerificationUrl, nullptr, nullptr); // otwiera strone logowania dostawcy
			G->GetWorldTimerManager().SetTimer(G->LoginTimer, FTimerDelegate::CreateWeakLambda(G, [G]() { G->LoginTick(); }), 1.f, true);
		});
		break;
	}
	case ELoginRow::EmailLogin: Login.FormMode = 1; Login.Field = 0; Login.Message.Reset(); break;
	case ELoginRow::EmailRegister: Login.FormMode = 2; Login.Field = 0; Login.Message.Reset(); break;
	case ELoginRow::Guest:
	{
		FAccount A;
		A.Provider = TEXT("guest");
		A.DisplayName = TEXT("Gość");
		LoginFinish(A);
		break;
	}
	default: break;
	}
}

bool AChickenGame::LoginKey(const FKey& Key, bool bShift, bool bCtrl)
{
	if (!Login.bOpen) return false;
	const FString Name = Key.GetFName().ToString();

	if (Key == EKeys::Escape)
	{
		if (Login.bWaiting) { LoginCancelWait(); Login.Message = TEXT("Anulowano"); }
		else if (Login.FormMode != 0) { Login.FormMode = 0; Login.Message.Reset(); }
		else if (Auth::Current().IsLoggedIn()) Login = FLoginState(); // zamkniecie bez zmiany konta
		return true;
	}
	if (Login.bWaiting) return true; // czekamy na przegladarke: tylko Esc

	if (Login.FormMode == 0)
	{
		const int32 N = (int32)ELoginRow::Count;
		if (Key == EKeys::Up) Login.Row = (Login.Row + N - 1) % N;
		else if (Key == EKeys::Down) Login.Row = (Login.Row + 1) % N;
		else if (Key == EKeys::Enter || Key == EKeys::SpaceBar) LoginActivate();
		return true;
	}

	// Formularz e-mail: pole 0 = e-mail, 1 = haslo.
	FString& Target = Login.Field == 0 ? Login.Email : Login.Password;
	if (Key == EKeys::Tab || Key == EKeys::Up || Key == EKeys::Down) { Login.Field = 1 - Login.Field; return true; }
	if (Key == EKeys::Enter)
	{
		if (Login.Field == 0) Login.Field = 1; else LoginActivate();
		return true;
	}
	if (Key == EKeys::BackSpace) { if (Target.Len() > 0) Target.LeftChopInline(1, EAllowShrinking::No); return true; }
	if (bCtrl && Key == EKeys::V)
	{
		FString Clip;
		FPlatformApplicationMisc::ClipboardPaste(Clip);
		Clip.TrimStartAndEndInline();
		if (!Clip.Contains(TEXT("\n")) && Clip.Len() <= 80) Target = Clip;
		return true;
	}
	if (Target.Len() >= 80) return true;
	TCHAR Ch = 0;
	if (Name.Len() == 1 && FChar::IsAlpha(Name[0])) Ch = bShift ? FChar::ToUpper(Name[0]) : FChar::ToLower(Name[0]);
	else if (Name == TEXT("Zero")) Ch = TEXT('0');
	else if (Name == TEXT("One")) Ch = TEXT('1');
	else if (Name == TEXT("Two")) Ch = bShift ? TEXT('@') : TEXT('2');
	else if (Name == TEXT("Three")) Ch = TEXT('3');
	else if (Name == TEXT("Four")) Ch = TEXT('4');
	else if (Name == TEXT("Five")) Ch = TEXT('5');
	else if (Name == TEXT("Six")) Ch = TEXT('6');
	else if (Name == TEXT("Seven")) Ch = TEXT('7');
	else if (Name == TEXT("Eight")) Ch = TEXT('8');
	else if (Name == TEXT("Nine")) Ch = TEXT('9');
	else if (Name == TEXT("Period")) Ch = TEXT('.');
	else if (Name == TEXT("Hyphen")) Ch = bShift ? TEXT('_') : TEXT('-');
	else if (Name == TEXT("Underscore")) Ch = TEXT('_');
	else if (Name == TEXT("Comma") && Login.Field == 1) Ch = TEXT(',');
	if (Ch != 0) Target.AppendChar(Ch);
	return true;
}
