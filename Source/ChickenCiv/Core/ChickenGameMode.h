#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ChickenGameMode.generated.h"

/**
 * Cienka powloka serwerowa: klasy domyslne, tworzenie AChickenGame (logiki gry) z opcjami URL,
 * przydzial miejsc graczom LAN. Cala logika jest w AChickenGame, ktory istnieje tez na klientach.
 * Opcje URL: ?Size=Small|Medium|Large ?Players=2..8 ?Seed=123 ?Difficulty=Easy|Normal|Hard ?NoMenu
 */
UCLASS()
class CHICKENCIV_API AChickenGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AChickenGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

private:
	int32 NextLanPlayerIndex = 0;
};
