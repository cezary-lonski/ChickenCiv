#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Map/CivTypes.h"
#include "Map/HexMap.h"
#include "Model/CivState.h"
#include "ChickenGameState.generated.h"

/** Caly stan gry: mapa, gracze, jednostki, miasta, tury. Logike zmienia tylko serwer. */
UCLASS()
class CHICKENCIV_API AChickenGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly) int32 Turn = 1;
	UPROPERTY(BlueprintReadOnly) int32 CurrentPlayer = 0;
	UPROPERTY(BlueprintReadOnly) TArray<FPlayerInfo> Players;

	FMapSettings Settings;
	FHexMap Grid;
	FCivState Civ;
	TArray<FString> Messages;

	/** Nowa gra: mapa z ziarna (Seed != 0), gracze, jednostki startowe. false, gdy nie udalo sie rozstawic graczy. */
	bool NewGame(const FMapSettings& InSettings);

	/** Konczy ture biezacego gracza; gracze AI (na razie bez zachowania) sa przeskakiwani. */
	void EndTurn();

	/** Wczytuje stan z zapisu (po NewGame z tymi samymi ustawieniami). */
	void ApplySave(const struct FChickenSaveData& Data);

	const FPlayerInfo* GetCurrentPlayerInfo() const { return Players.IsValidIndex(CurrentPlayer) ? &Players[CurrentPlayer] : nullptr; }
	const FCivPlayerState* GetPlayerState(int32 P) const { return Civ.PlayerStates.IsValidIndex(P) ? &Civ.PlayerStates[P] : nullptr; }

	void AddMessage(const FString& Text);

	/** Co kilka tur pyta LLM o strategie gracza AI (tylko gdy jest klucz API); odpowiedz ustawia strategie asynchronicznie. */
	void MaybeRequestLlmStrategy(int32 Player);

private:
	TSet<int32> LlmPending;

public:

	FSimpleMulticastDelegate OnTurnChanged;
};
