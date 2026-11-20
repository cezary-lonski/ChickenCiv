#pragma once

#include "CoreMinimal.h"
#include "Hex/HexCoord.h"
#include "CivTypes.generated.h"

UENUM(BlueprintType)
enum class ETerrainType : uint8
{
	Ocean,
	Coast,
	Grassland,
	Plains,
	Forest,
	Desert,
	Hills,
	Mountain
};

UENUM(BlueprintType)
enum class EResourceType : uint8
{
	None,
	Wheat,
	Cattle,
	Fish,
	Horses,
	Iron,
	Stone,
	Gold
};

UENUM(BlueprintType)
enum class EChickenRace : uint8
{
	Red,     // nauka x3
	Yellow,  // sila x2
	Blue,    // statki x10
	Green    // jednostka specjalna
};

UENUM(BlueprintType)
enum class EMapSize : uint8
{
	Small,
	Medium,
	Large
};

UENUM(BlueprintType)
enum class EDifficulty : uint8
{
	Easy,
	Normal,
	Hard
};

/** Ustawienia nowej gry (pozniej wypelniane z menu). Seed 0 = losowy. */
USTRUCT(BlueprintType)
struct FMapSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Seed = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EMapSize Size = EMapSize::Medium;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 NumPlayers = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EDifficulty Difficulty = EDifficulty::Normal;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 NumHumans = 1;                       // gracze ludzcy (hot seat), pierwsi w kolejnosci
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EChickenRace PlayerRace = EChickenRace::Red; // rasa gracza 1
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PlayerColorIndex = -1;                // kolor gracza 1: -1 = kolor rasy, 0..7 paleta

	int32 GetWidth() const
	{
		switch (Size) { case EMapSize::Small: return 40; case EMapSize::Large: return 80; default: return 60; }
	}
	int32 GetHeight() const
	{
		switch (Size) { case EMapSize::Small: return 24; case EMapSize::Large: return 52; default: return 38; }
	}
};

/** Jeden gracz (czlowiek lub AI). */
USTRUCT(BlueprintType)
struct FPlayerInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) FString Name;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) EChickenRace Race = EChickenRace::Red;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIsAI = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ColorIndex = -1; // -1 = kolor rasy, 0..7 paleta
};

/** Kolory kurczakow: paleta 8 kolorow (cztery pierwsze to kolory ras) i kolor domyslny rasy. */
namespace CivColors
{
	constexpr int32 NumPalette = 8;

	inline FLinearColor Palette(int32 I)
	{
		switch (((I % NumPalette) + NumPalette) % NumPalette)
		{
		case 0: return FLinearColor(0.9f, 0.08f, 0.08f);
		case 1: return FLinearColor(0.95f, 0.8f, 0.05f);
		case 2: return FLinearColor(0.1f, 0.3f, 0.95f);
		case 3: return FLinearColor(0.1f, 0.75f, 0.15f);
		case 4: return FLinearColor(1.f, 0.5f, 0.05f);
		case 5: return FLinearColor(0.6f, 0.2f, 0.85f);
		case 6: return FLinearColor(0.1f, 0.85f, 0.85f);
		default: return FLinearColor(0.95f, 0.95f, 0.95f);
		}
	}

	inline const TCHAR* PaletteName(int32 I)
	{
		static const TCHAR* Names[] = { TEXT("Czerwony"), TEXT("Żółty"), TEXT("Niebieski"), TEXT("Zielony"), TEXT("Pomarańczowy"), TEXT("Fioletowy"), TEXT("Turkusowy"), TEXT("Biały") };
		if (I < 0) return TEXT("Kolor rasy");
		return Names[I % NumPalette];
	}

	inline FLinearColor RaceColor(EChickenRace R) { return Palette((int32)R); }

	inline FLinearColor ForPlayer(const FPlayerInfo& P)
	{
		return P.ColorIndex >= 0 ? Palette(P.ColorIndex) : RaceColor(P.Race);
	}
}

/** Pole mapy. Zwykla struktura (mapa jest odtwarzana z ziarna, nie zapisywana pole po polu). */
struct FMapTile
{
	FHexCoord Coord;
	ETerrainType Terrain = ETerrainType::Ocean;
	EResourceType Resource = EResourceType::None;
	float Elevation = 0.f;

	bool IsWater() const { return Terrain == ETerrainType::Ocean || Terrain == ETerrainType::Coast; }
	bool IsPassableLand() const { return !IsWater() && Terrain != ETerrainType::Mountain; }
};

/** Cechy rasy (mnozniki). Sila i statki zadzialaja w Etapach 3-5. */
struct FRaceDef
{
	int32 ScienceMult = 1;      // czerwone x3
	int32 StrengthMult = 1;     // zolte x2 (sila jednostek w walce)
	int32 ShipSpeedMult = 1;    // niebieskie x10
	int32 ShipStrengthMult = 1; // niebieskie x3
	int32 ShipCostDiv = 1;      // niebieskie: koszt / 5
	bool bCityDefenseWipe = false; // zielone: jednostka specjalna niszczy armie atakujaca miasto
};

namespace CivRaces
{
	inline FRaceDef Get(EChickenRace R)
	{
		FRaceDef D;
		switch (R)
		{
		case EChickenRace::Red: D.ScienceMult = 3; break;
		case EChickenRace::Yellow: D.StrengthMult = 2; break;
		case EChickenRace::Blue: D.ShipSpeedMult = 10; D.ShipStrengthMult = 3; D.ShipCostDiv = 5; break;
		case EChickenRace::Green: D.bCityDefenseWipe = true; break;
		}
		return D;
	}
}

namespace CivNames
{
	inline FString TerrainName(ETerrainType T)
	{
		switch (T)
		{
		case ETerrainType::Ocean: return TEXT("Ocean");
		case ETerrainType::Coast: return TEXT("Wybrzeże");
		case ETerrainType::Grassland: return TEXT("Łąka");
		case ETerrainType::Plains: return TEXT("Równina");
		case ETerrainType::Forest: return TEXT("Las");
		case ETerrainType::Desert: return TEXT("Pustynia");
		case ETerrainType::Hills: return TEXT("Wzgórza");
		case ETerrainType::Mountain: return TEXT("Góry");
		}
		return TEXT("?");
	}

	inline FString ResourceName(EResourceType R)
	{
		switch (R)
		{
		case EResourceType::None: return TEXT("brak");
		case EResourceType::Wheat: return TEXT("Pszenica");
		case EResourceType::Cattle: return TEXT("Bydło");
		case EResourceType::Fish: return TEXT("Ryby");
		case EResourceType::Horses: return TEXT("Konie");
		case EResourceType::Iron: return TEXT("Żelazo");
		case EResourceType::Stone: return TEXT("Kamień");
		case EResourceType::Gold: return TEXT("Złoto");
		}
		return TEXT("?");
	}

	inline FString RaceName(EChickenRace R)
	{
		switch (R)
		{
		case EChickenRace::Red: return TEXT("Czerwone");
		case EChickenRace::Yellow: return TEXT("Żółte");
		case EChickenRace::Blue: return TEXT("Niebieskie");
		case EChickenRace::Green: return TEXT("Zielone");
		}
		return TEXT("?");
	}
}
