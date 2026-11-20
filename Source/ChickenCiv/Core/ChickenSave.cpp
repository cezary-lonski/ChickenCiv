#include "Core/ChickenSave.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

FString FChickenSave::SlotPath(const FString& Slot)
{
	return FPaths::ProjectSavedDir() / TEXT("SaveGames") / (Slot + TEXT(".json"));
}

FString FChickenSave::ToJson(const FChickenSaveData& Data)
{
	FString Out;
	FJsonObjectConverter::UStructToJsonObjectString(Data, Out);
	return Out;
}

bool FChickenSave::FromJson(const FString& Json, FChickenSaveData& OutData)
{
	return FJsonObjectConverter::JsonObjectStringToUStruct(Json, &OutData, 0, 0);
}

bool FChickenSave::Save(const FChickenSaveData& Data, const FString& Slot)
{
	return FFileHelper::SaveStringToFile(ToJson(Data), *SlotPath(Slot), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}

bool FChickenSave::Load(FChickenSaveData& OutData, const FString& Slot)
{
	FString Json;
	if (!FFileHelper::LoadFileToString(Json, *SlotPath(Slot))) return false;
	return FromJson(Json, OutData);
}
