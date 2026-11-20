#pragma once

#include "CoreMinimal.h"
#include "Model/CivState.h"

/** Zapis i odczyt gry jako JSON w Saved/SaveGames. */
struct FChickenSave
{
	static FString SlotPath(const FString& Slot);
	static bool Save(const FChickenSaveData& Data, const FString& Slot);
	static bool Load(FChickenSaveData& OutData, const FString& Slot);

	static FString ToJson(const FChickenSaveData& Data);
	static bool FromJson(const FString& Json, FChickenSaveData& OutData);
};
