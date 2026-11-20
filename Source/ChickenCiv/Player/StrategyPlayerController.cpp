#include "Player/StrategyPlayerController.h"
#include "Player/StrategyPawn.h"
#include "Core/ChickenGame.h"
#include "Core/ChickenGameState.h"
#include "Core/CivRules.h"
#include "Model/TechTree.h"
#include "Player/BattleLayout.h"
#include "Net/UnrealNetwork.h"
#include "Map/HexMapActor.h"
#include "Components/InputComponent.h"
#include "InputCoreTypes.h"
#include "Engine/World.h"
#include "Math/Plane.h"

AStrategyPlayerController::AStrategyPlayerController()
{
	bShowMouseCursor = true;
	DefaultMouseCursor = EMouseCursor::Default;
}

void AStrategyPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::LeftMouseButton, IE_Pressed, this, &AStrategyPlayerController::OnClick);
	InputComponent->BindKey(EKeys::RightMouseButton, IE_Pressed, this, &AStrategyPlayerController::OnRightClick);
	InputComponent->BindKey(EKeys::B, IE_Pressed, this, &AStrategyPlayerController::OnFoundCity);
	InputComponent->BindKey(EKeys::T, IE_Pressed, this, &AStrategyPlayerController::OnToggleTech);
	InputComponent->BindKey(EKeys::I, IE_Pressed, this, &AStrategyPlayerController::OnImprove);
	InputComponent->BindKey(EKeys::P, IE_Pressed, this, &AStrategyPlayerController::OnPeace);
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this, &AStrategyPlayerController::OnEscape);
	InputComponent->BindKey(EKeys::Period, IE_Pressed, this, &AStrategyPlayerController::OnPeriod);
	InputComponent->BindKey(EKeys::Up, IE_Pressed, this, &AStrategyPlayerController::OnMenuUp);
	InputComponent->BindKey(EKeys::Down, IE_Pressed, this, &AStrategyPlayerController::OnMenuDown);
	InputComponent->BindKey(EKeys::Left, IE_Pressed, this, &AStrategyPlayerController::OnMenuLeft);
	InputComponent->BindKey(EKeys::Right, IE_Pressed, this, &AStrategyPlayerController::OnMenuRight);
	InputComponent->BindKey(EKeys::BackSpace, IE_Pressed, this, &AStrategyPlayerController::OnBackspace);
	InputComponent->BindKey(EKeys::Delete, IE_Pressed, this, &AStrategyPlayerController::OnDelete);
	InputComponent->BindKey(EKeys::V, IE_Pressed, this, &AStrategyPlayerController::OnPasteKey);
	InputComponent->BindKey(EKeys::Zero, IE_Pressed, this, &AStrategyPlayerController::OnDigit0);
	InputComponent->BindKey(EKeys::R, IE_Pressed, this, &AStrategyPlayerController::OnBattleAuto);
	InputComponent->BindKey(EKeys::D, IE_Pressed, this, &AStrategyPlayerController::OnDeathStar);
	InputComponent->BindKey(EKeys::A, IE_Pressed, this, &AStrategyPlayerController::OnCreateArmy);
	InputComponent->BindKey(EKeys::J, IE_Pressed, this, &AStrategyPlayerController::OnJoinArmy);
	InputComponent->BindKey(EKeys::L, IE_Pressed, this, &AStrategyPlayerController::OnLeaveArmy);
	InputComponent->BindKey(EKeys::PageUp, IE_Pressed, this, &AStrategyPlayerController::OnPageUp);
	InputComponent->BindKey(EKeys::PageDown, IE_Pressed, this, &AStrategyPlayerController::OnPageDown);
	InputComponent->BindKey(EKeys::K, IE_Pressed, this, &AStrategyPlayerController::OnSkipUnit);
	InputComponent->BindKey(EKeys::Tab, IE_Pressed, this, &AStrategyPlayerController::OnNextUnit);
	InputComponent->BindKey(EKeys::Enter, IE_Pressed, this, &AStrategyPlayerController::OnEndTurn);
	InputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &AStrategyPlayerController::OnEndTurn);
	InputComponent->BindKey(EKeys::F5, IE_Pressed, this, &AStrategyPlayerController::OnSave);
	InputComponent->BindKey(EKeys::F9, IE_Pressed, this, &AStrategyPlayerController::OnLoad);
	InputComponent->BindKey(EKeys::N, IE_Pressed, this, &AStrategyPlayerController::OnNewMap);
	InputComponent->BindKey(EKeys::MouseScrollUp, IE_Pressed, this, &AStrategyPlayerController::OnZoomIn);
	InputComponent->BindKey(EKeys::MouseScrollDown, IE_Pressed, this, &AStrategyPlayerController::OnZoomOut);
	InputComponent->BindKey(EKeys::One, IE_Pressed, this, &AStrategyPlayerController::OnDigit1);
	InputComponent->BindKey(EKeys::Two, IE_Pressed, this, &AStrategyPlayerController::OnDigit2);
	InputComponent->BindKey(EKeys::Three, IE_Pressed, this, &AStrategyPlayerController::OnDigit3);
	InputComponent->BindKey(EKeys::Four, IE_Pressed, this, &AStrategyPlayerController::OnDigit4);
	InputComponent->BindKey(EKeys::Five, IE_Pressed, this, &AStrategyPlayerController::OnDigit5);
	InputComponent->BindKey(EKeys::Six, IE_Pressed, this, &AStrategyPlayerController::OnDigit6);
	InputComponent->BindKey(EKeys::Seven, IE_Pressed, this, &AStrategyPlayerController::OnDigit7);
	InputComponent->BindKey(EKeys::Eight, IE_Pressed, this, &AStrategyPlayerController::OnDigit8);
	InputComponent->BindKey(EKeys::Nine, IE_Pressed, this, &AStrategyPlayerController::OnDigit9);
}

bool AStrategyPlayerController::GetHexUnderCursor(FHexCoord& Out) const
{
	FVector Origin, Dir;
	if (!DeprojectMousePositionToWorld(Origin, Dir)) return false;

	const FPlane Ground(FVector(0.f, 0.f, 20.f), FVector::UpVector);
	const double T = FMath::RayPlaneIntersectionParam(Origin, Dir, Ground);
	if (T <= 0.0) return false;

	const FVector P = Origin + Dir * T;
	Out = FHexCoord::FromWorld(FVector2D(P.X, P.Y), AHexMapActor::HexSize);
	return true;
}

void AStrategyPlayerController::ShowNotice(const FString& Text)
{
	Notice = Text;
	NoticeUntil = FPlatformTime::Seconds() + 4.0;
}

void AStrategyPlayerController::SelectHex(const FHexCoord& C)
{
	AChickenGame* GM = AChickenGame::Get(GetWorld());
	const AChickenGameState* GS = GM ? GM->GetChickenState() : nullptr;
	if (!GM || !GM->GetMap() || !GS || !GS->Grid.Contains(C)) { ClearSelection(); return; }
	bHasSelection = true;
	SelectedHex = C;
	GM->GetMap()->SetSelection(&C);
}

void AStrategyPlayerController::SelectUnit(int32 UnitId)
{
	AChickenGame* GM = AChickenGame::Get(GetWorld());
	const AChickenGameState* GS = GM ? GM->GetChickenState() : nullptr;
	const FUnit* U = GS ? CivRules::FindUnit(GS->Civ, UnitId) : nullptr;
	if (!U) return;
	SelectedUnitId = UnitId;
	SelectedCityId = 0;
	SelectHex(U->Pos);
}

void AStrategyPlayerController::SelectCity(int32 CityId)
{
	AChickenGame* GM = AChickenGame::Get(GetWorld());
	const AChickenGameState* GS = GM ? GM->GetChickenState() : nullptr;
	const FCity* C = GS ? CivRules::FindCity(GS->Civ, CityId) : nullptr;
	if (!C) return;
	SelectedCityId = CityId;
	SelectedUnitId = 0;
	SelectHex(C->Pos);
}

void AStrategyPlayerController::ClearSelection()
{
	bHasSelection = false;
	SelectedUnitId = 0;
	SelectedCityId = 0;
	if (AChickenGame* GM = AChickenGame::Get(GetWorld()))
	{
		if (GM->GetMap()) GM->GetMap()->SetSelection(nullptr);
	}
}

void AStrategyPlayerController::BeginPlay()
{
	Super::BeginPlay();
	// Klient LAN nie ma AGameModeBase: logike gry tworzymy lokalnie.
	if (IsLocalController()) AChickenGame::Ensure(GetWorld());
}

void AStrategyPlayerController::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AStrategyPlayerController, LanPlayerIndex);
}

void AStrategyPlayerController::ServerSubmitCommand_Implementation(const FChickenCommand& Cmd)
{
	if (AChickenGame* G = AChickenGame::Get(GetWorld())) G->ReceiveCommandFromClient(this, Cmd);
}

void AStrategyPlayerController::ClientExecuteCommand_Implementation(const FChickenCommand& Cmd)
{
	if (AChickenGame* G = AChickenGame::Get(GetWorld())) G->ApplyRemoteCommand(Cmd);
}

void AStrategyPlayerController::ClientCheckHash_Implementation(int32 Turn, int32 ServerHash)
{
	if (AChickenGame* G = AChickenGame::Get(GetWorld())) G->CheckRemoteHash(Turn, ServerHash);
}

void AStrategyPlayerController::OnPeriod()
{
	if (AChickenGame* GM = AChickenGame::Get(GetWorld())) GM->MenuPeriod();
}

bool AStrategyPlayerController::NotMyTurn()
{
	AChickenGame* GM = AChickenGame::Get(GetWorld());
	if (GM && !GM->IsMyTurn()) { ShowNotice(TEXT("Czekaj na swoją turę")); return true; }
	return false;
}

bool AStrategyPlayerController::InMenu() const
{
	const AChickenGame* GM = GetWorld() ? AChickenGame::Get(GetWorld()) : nullptr;
	return GM && (GM->IsMenuOpen() || GM->IsHandoff());
}

void AStrategyPlayerController::OnEscape()
{
	if (AChickenGame* GM = AChickenGame::Get(GetWorld())) GM->ToggleMenu();
}

void AStrategyPlayerController::OnMenuUp() { if (AChickenGame* GM = AChickenGame::Get(GetWorld())) GM->MenuMove(-1); }
void AStrategyPlayerController::OnMenuDown() { if (AChickenGame* GM = AChickenGame::Get(GetWorld())) GM->MenuMove(1); }
void AStrategyPlayerController::OnMenuLeft() { if (AChickenGame* GM = AChickenGame::Get(GetWorld())) GM->MenuChange(-1); }
void AStrategyPlayerController::OnMenuRight() { if (AChickenGame* GM = AChickenGame::Get(GetWorld())) GM->MenuChange(1); }
void AStrategyPlayerController::OnBackspace() { if (AChickenGame* GM = AChickenGame::Get(GetWorld())) GM->MenuBackspace(); }
void AStrategyPlayerController::OnDelete() { if (AChickenGame* GM = AChickenGame::Get(GetWorld())) if (GM->IsMenuOpen() && (EMenuRow)GM->GetMenu().Row == EMenuRow::ApiKey) GM->MenuClearApiKey(); }
void AStrategyPlayerController::OnPasteKey()
{
	AChickenGame* GM = AChickenGame::Get(GetWorld());
	if (GM && GM->IsMenuOpen() && (IsInputKeyDown(EKeys::LeftControl) || IsInputKeyDown(EKeys::RightControl))) GM->MenuPasteApiKey();
}

bool AStrategyPlayerController::InBattle() const
{
	const AChickenGame* GM = GetWorld() ? AChickenGame::Get(GetWorld()) : nullptr;
	return GM && GM->IsInBattle();
}

void AStrategyPlayerController::OnBattleAuto()
{
	if (AChickenGame* GM = AChickenGame::Get(GetWorld())) if (GM->IsInBattle() && !NotMyTurn()) GM->BattleAutoResolve();
}

void AStrategyPlayerController::OnDeathStar()
{
	AChickenGame* GM = AChickenGame::Get(GetWorld());
	if (!GM || !GM->IsInBattle() || NotMyTurn()) return;
	if (!GM->BattleToggleDeathStar()) ShowNotice(TEXT("Laser Gwiazdy Śmierci niedostępny (brak cudu albo już użyty w tej rundzie)"));
}

void AStrategyPlayerController::OnClick()
{
	AChickenGame* GM = AChickenGame::Get(GetWorld());
	if (GM && GM->IsIntro()) { GM->SkipIntro(); return; }
	if (GM && (GM->IsMenuOpen() || GM->IsHandoff())) return;
	if (GM && GM->IsInBattle())
	{
		if (NotMyTurn()) return;
		float MX = 0.f, MY = 0.f;
		int32 VX = 0, VY = 0;
		GetViewportSize(VX, VY);
		FHexCoord H;
		if (GetMousePosition(MX, MY) && BattleLayout::HexAt(FVector2D(MX, MY), (float)VX, H)) GM->BattleClick(H);
		return;
	}
	const AChickenGameState* GS = GM ? GM->GetChickenState() : nullptr;
	FHexCoord C;
	if (!GS || !GetHexUnderCursor(C)) { ClearSelection(); return; }

	// Wlasne jednostki i miasto na polu; kolejne klikniecie przechodzi do nastepnego z listy.
	TArray<int32> Cycle; // dodatnie = jednostka, ujemne = miasto
	for (const FUnit& U : GS->Civ.Units) if (U.Pos == C && U.Owner == GM->GetViewer() && !U.IsArmyMember()) Cycle.Add(U.Id);
	if (const FCity* City = CivRules::CityAt(GS->Civ, C)) if (City->Owner == GM->GetViewer()) Cycle.Add(-City->Id);

	if (Cycle.Num() == 0) { ClearSelection(); SelectHex(C); return; }

	const int32 Current = SelectedUnitId != 0 ? SelectedUnitId : (SelectedCityId != 0 ? -SelectedCityId : 0);
	int32 Next = Cycle[0];
	const int32 Idx = Cycle.Find(Current);
	if (Idx != INDEX_NONE) Next = Cycle[(Idx + 1) % Cycle.Num()];

	if (Next > 0) SelectUnit(Next); else SelectCity(-Next);
}

void AStrategyPlayerController::OnRightClick()
{
	if (InBattle() || InMenu() || NotMyTurn()) return;
	AChickenGame* GM = AChickenGame::Get(GetWorld());
	if (!GM || SelectedUnitId == 0) return;
	FHexCoord C;
	if (!GetHexUnderCursor(C)) return;
	{
		const AChickenGameState* AGS = GM->GetChickenState();
		const bool bEnemy = AGS && (AGS->Civ.Units.ContainsByPredicate([&](const FUnit& U) { return U.Pos == C && U.Owner != GM->GetViewer(); })
			|| AGS->Civ.Cities.ContainsByPredicate([&](const FCity& Ci) { return Ci.Pos == C && Ci.Owner != GM->GetViewer(); }));
		if (bEnemy)
		{
			FString Why;
			if (!GM->StartAttack(SelectedUnitId, C, &Why)) ShowNotice(Why.IsEmpty() ? FString(TEXT("Nie można zaatakować")) : Why);
			return;
		}
	}
	if (!GM->OrderMove(SelectedUnitId, C))
	{
		ShowNotice(TEXT("Nie można tam dojść"));
		return;
	}
	// Zaznaczenie podaza za jednostka (lub znika, gdy jednostki juz nie ma).
	const AChickenGameState* GS = GM->GetChickenState();
	if (GS && CivRules::FindUnit(GS->Civ, SelectedUnitId)) SelectUnit(SelectedUnitId);
}

void AStrategyPlayerController::OnFoundCity()
{
	if (InBattle() || InMenu() || NotMyTurn()) return;
	AChickenGame* GM = AChickenGame::Get(GetWorld());
	if (!GM || SelectedUnitId == 0) return;
	FString Why;
	const AChickenGameState* GS = GM->GetChickenState();
	const FUnit* U = GS ? CivRules::FindUnit(GS->Civ, SelectedUnitId) : nullptr;
	const FHexCoord Pos = U ? U->Pos : FHexCoord();
	if (GM->FoundCity(SelectedUnitId, &Why))
	{
		if (const FCity* C = GS ? CivRules::CityAt(GS->Civ, Pos) : nullptr) SelectCity(C->Id); else ClearSelection();
	}
	else
	{
		ShowNotice(Why.IsEmpty() ? FString(TEXT("Nie można założyć miasta")) : Why);
	}
}

void AStrategyPlayerController::OnSkipUnit()
{
	if (InBattle() || InMenu() || NotMyTurn()) return;
	AChickenGame* GM = AChickenGame::Get(GetWorld());
	if (!GM || SelectedUnitId == 0) return;
	GM->SkipUnit(SelectedUnitId);
	OnNextUnit();
}

void AStrategyPlayerController::OnNextUnit()
{
	if (InBattle() || InMenu()) return;
	AChickenGame* GM = AChickenGame::Get(GetWorld());
	if (!GM) return;
	const int32 Next = GM->NextIdleUnit(SelectedUnitId);
	if (Next != 0)
	{
		SelectUnit(Next);
		if (const FUnit* U = CivRules::FindUnit(GM->GetChickenState()->Civ, Next)) GM->FocusCamerasOn(U->Pos);
	}
	else
	{
		ShowNotice(TEXT("Brak jednostek do ruchu"));
	}
}

void AStrategyPlayerController::OnEndTurn()
{
	AChickenGame* GM = AChickenGame::Get(GetWorld());
	if (!GM) return;
	if (GM->IsIntro()) { GM->SkipIntro(); return; }
	if (GM->IsHandoff()) { GM->DismissHandoff(); return; }
	if (GM->IsMenuOpen()) { GM->MenuActivate(); return; }
	if (NotMyTurn()) return;
	if (GM->IsInBattle())
	{
		if (GM->GetBattle()->Winner >= 0) { GM->CloseBattle(); ClearSelection(); }
		else GM->BattleEndUnitTurn();
		return;
	}
	GM->EndTurn();
	// Po zmianie tury zaznaczenie moze byc nieaktualne.
	const AChickenGameState* GS = GM->GetChickenState();
	if (GS)
	{
		if (SelectedUnitId != 0 && !CivRules::FindUnit(GS->Civ, SelectedUnitId)) ClearSelection();
		if (SelectedCityId != 0 && !CivRules::FindCity(GS->Civ, SelectedCityId)) ClearSelection();
	}
	// Podpowiedz: nastepna wolna jednostka.
	const int32 Next = GM->NextIdleUnit(0);
	if (Next != 0 && SelectedUnitId == 0 && SelectedCityId == 0) SelectUnit(Next);
}

void AStrategyPlayerController::OnSave()
{
	if (InBattle() || InMenu()) return;
	if (AChickenGame* NG = AChickenGame::Get(GetWorld())) if (NG->IsNetworked()) { ShowNotice(TEXT("Niedostępne w grze LAN")); return; }
	if (AChickenGame* GM = AChickenGame::Get(GetWorld()))
	{
		ShowNotice(GM->SaveGame() ? TEXT("Zapisano grę (F9 wczyta)") : TEXT("Zapis nie powiódł się"));
	}
}

void AStrategyPlayerController::OnLoad()
{
	if (InBattle() || InMenu()) return;
	if (AChickenGame* NG = AChickenGame::Get(GetWorld())) if (NG->IsNetworked()) { ShowNotice(TEXT("Niedostępne w grze LAN")); return; }
	if (AChickenGame* GM = AChickenGame::Get(GetWorld()))
	{
		const bool bOk = GM->LoadGame();
		ClearSelection();
		ShowNotice(bOk ? TEXT("Wczytano grę") : TEXT("Brak zapisu do wczytania"));
	}
}

void AStrategyPlayerController::OnNewMap()
{
	if (InBattle() || InMenu()) return;
	if (AChickenGame* NG = AChickenGame::Get(GetWorld())) if (NG->IsNetworked()) { ShowNotice(TEXT("Niedostępne w grze LAN")); return; }
	if (AChickenGame* GM = AChickenGame::Get(GetWorld()))
	{
		FMapSettings S = GM->GetActiveSettings();
		S.Seed = 0; // nowe losowe ziarno
		GM->StartNewGame(S);
		ClearSelection();
	}
}

void AStrategyPlayerController::OnCreateArmy()
{
	if (InBattle() || InMenu() || NotMyTurn()) return;
	AChickenGame* GM = AChickenGame::Get(GetWorld());
	if (!GM || SelectedUnitId == 0) return;
	FString Why;
	if (GM->CreateArmy(SelectedUnitId, &Why)) ShowNotice(TEXT("Założono armię – J: dołącz jednostki z tego pola"));
	else ShowNotice(Why.IsEmpty() ? FString(TEXT("Nie można założyć armii")) : Why);
}

void AStrategyPlayerController::OnJoinArmy()
{
	if (InBattle() || InMenu() || NotMyTurn()) return;
	AChickenGame* GM = AChickenGame::Get(GetWorld());
	if (!GM || SelectedUnitId == 0) return;
	FString Why;
	if (GM->JoinArmy(SelectedUnitId, &Why))
	{
		ShowNotice(TEXT("Jednostka dołączyła do armii"));
		ClearSelection();
	}
	else ShowNotice(Why.IsEmpty() ? FString(TEXT("Nie można dołączyć do armii")) : Why);
}

void AStrategyPlayerController::OnLeaveArmy()
{
	if (InBattle() || InMenu() || NotMyTurn()) return;
	AChickenGame* GM = AChickenGame::Get(GetWorld());
	if (!GM || SelectedUnitId == 0) return;
	FString Why;
	if (!GM->LeaveArmy(SelectedUnitId, &Why)) ShowNotice(Why.IsEmpty() ? FString(TEXT("Nie można opuścić armii")) : Why);
}

void AStrategyPlayerController::OnPeace()
{
	if (InBattle() || InMenu() || NotMyTurn()) return;
	AChickenGame* GM = AChickenGame::Get(GetWorld());
	if (!GM) return;
	ShowNotice(GM->ProposePeace() ? TEXT("Zaproponowano pokój (odpowiedź AI przy jego turze)") : TEXT("Nie jesteś z nikim na wojnie"));
}

void AStrategyPlayerController::OnImprove()
{
	if (InBattle() || InMenu() || NotMyTurn()) return;
	AChickenGame* GM = AChickenGame::Get(GetWorld());
	if (!GM || SelectedUnitId == 0) return;
	FString Why;
	if (GM->BuildImprovement(SelectedUnitId, &Why)) ShowNotice(TEXT("Robotnik buduje ulepszenie (3 tury)"));
	else ShowNotice(Why.IsEmpty() ? FString(TEXT("Nie można ulepszyć")) : Why);
}

void AStrategyPlayerController::OnToggleTech()
{
	if (InBattle() || InMenu()) return;
	bTechPanel = !bTechPanel;
	Page = 0;
}

void AStrategyPlayerController::OnPageUp() { Page = FMath::Max(0, Page - 1); }
void AStrategyPlayerController::OnPageDown() { ++Page; }

void AStrategyPlayerController::OnDigit(int32 Index)
{
	AChickenGame* GM = AChickenGame::Get(GetWorld());
	const AChickenGameState* GS = GM ? GM->GetChickenState() : nullptr;
	if (GM && GM->IsMenuOpen()) { GM->MenuDigit(Index + 1); return; }
	if (bTechPanel)
	{
		const FCivPlayerState* PS = GS ? GS->GetPlayerState(GM->GetViewer()) : nullptr;
		if (!PS) return;
		TArray<ETech> Av;
		CivTech::GetAvailable(*PS, Av);
		const int32 I = Page * 9 + Index;
		if (!Av.IsValidIndex(I)) return;
		if (GM->SetResearch(Av[I])) ShowNotice(FString::Printf(TEXT("Badanie: %s"), CivTech::Def(Av[I]).Name));
		return;
	}
	const FCity* City = (GS && SelectedCityId != 0) ? CivRules::FindCity(GS->Civ, SelectedCityId) : nullptr;
	if (!City) return;
	TArray<FProductionOption> Options;
	CivRules::GetProductionOptions(GS->Grid, GS->Civ, *City, Options);
	const int32 I = Page * 9 + Index;
	if (!Options.IsValidIndex(I)) return;
	if (GM->SetProduction(SelectedCityId, Options[I].Item)) ShowNotice(FString::Printf(TEXT("%s: produkcja – %s"), *City->Name, *Options[I].Name));
}

void AStrategyPlayerController::OnZoomIn()
{
	if (AStrategyPawn* P = Cast<AStrategyPawn>(GetPawn())) P->ZoomBy(1.f);
}

void AStrategyPlayerController::OnZoomOut()
{
	if (AStrategyPawn* P = Cast<AStrategyPawn>(GetPawn())) P->ZoomBy(-1.f);
}
