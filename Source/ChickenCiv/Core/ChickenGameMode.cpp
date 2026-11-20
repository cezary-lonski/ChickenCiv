#include "Core/ChickenGameMode.h"
#include "Core/ChickenGame.h"
#include "Core/ChickenGameState.h"
#include "Player/StrategyPawn.h"
#include "Player/StrategyPlayerController.h"
#include "Player/ChickenHUD.h"
#include "Engine/World.h"

AChickenGameMode::AChickenGameMode()
{
	DefaultPawnClass = AStrategyPawn::StaticClass();
	PlayerControllerClass = AStrategyPlayerController::StaticClass();
	HUDClass = AChickenHUD::StaticClass();
	GameStateClass = AChickenGameState::StaticClass();
}

void AChickenGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	// Logika gry powstaje tu (deferred spawn z opcjami), a jej BeginPlay zaczyna sie razem ze swiatem.
	AChickenGame::Ensure(GetWorld(), Options);
}

void AChickenGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
	Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
	if (!ErrorMessage.IsEmpty()) return;
	// Gra w toku: nie wpuszczamy nowych graczy (stan jest synchronizowany poleceniami od jej poczatku).
	if (const AChickenGame* Game = AChickenGame::Get(GetWorld()))
	{
		if (Game->IsNetGameStarted()) ErrorMessage = TEXT("Gra już trwa");
		else if (GetNumPlayers() >= 8) ErrorMessage = TEXT("Brak wolnych miejsc");
	}
}

void AChickenGameMode::PostLogin(APlayerController* NewPlayer)
{
	Super::PostLogin(NewPlayer);
	// Gospodarz (pierwszy sterownik) ma miejsce 0, kolejni LAN kolejne numery.
	if (AStrategyPlayerController* PC = Cast<AStrategyPlayerController>(NewPlayer))
	{
		PC->LanPlayerIndex = NextLanPlayerIndex++;
	}
}

void AChickenGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);
	if (AChickenGame* Game = AChickenGame::Get(GetWorld())) Game->FocusOnViewerStart();
}
