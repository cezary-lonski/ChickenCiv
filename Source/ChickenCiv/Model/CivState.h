#pragma once

#include "CoreMinimal.h"
#include "Hex/HexCoord.h"
#include "Map/CivTypes.h"
#include "CivState.generated.h"

/** Technologie. None = brak wymagan. Kolejnosc musi zgadzac sie z tabela w TechTree.cpp. */
UENUM(BlueprintType)
enum class ETech : uint8
{
	None,
	Agriculture, Husbandry, Writing, Bronze, Mathematics, Sailing, IronWorking, Gunpowder,
	Electricity, Engines, Computers, Spaceflight,
	SpaceStations, Shuttles, DeathStar,   // sciezka statkow
	Lasers,                               // sciezka laserow
	SmallWarships,                        // statki + lasery
	Elements, ParticleAccelerator, BlackHoles, Teleportation, // sciezka pierwiastkow
	AdvancedTech                          // po obu sciezkach kosmosu i pierwiastkach
};
constexpr int32 NumTechs = 23;

UENUM(BlueprintType)
enum class EUnitType : uint8
{
	Settler,
	Worker,
	Warrior,
	Archer,
	Scout,
	Ship,
	LaserGunslinger,
	SpaceShuttle,
	XWing,
	TIE,
	BlackHoleThrower,
	Droid,
	AcidThrower,
	MeteorDroid,
	Commander,       // dowodca armii
	Medic,           // jednostka specjalna: leczy
	Resurrector,     // jednostka specjalna: 1 wskrzeszenie na gre
	Deputy,          // jednostka specjalna: zastepca dowodcy
	GreenGuardian    // zielona jednostka specjalna: niszczy armie atakujaca miasto
};

UENUM(BlueprintType)
enum class EBuildingType : uint8
{
	Granary,
	Workshop,
	Library,
	Market,
	Barracks,
	SpaceStation,
	ParticleAccelerator,
	Teleporter,
	DeathStar
};

/** Ulepszenia pol budowane przez robotnikow. */
UENUM(BlueprintType)
enum class EImprovement : uint8
{
	None,
	Farm,   // +1 jedzenia (laka, rownina, pustynia)
	Mine,   // +1 produkcji (wzgorza, las)
	Road    // ruch po polu kosztuje co najwyzej 1 (osobna warstwa obok farmy/kopalni)
};

/** Gdzie jednostka sie porusza. */
enum class EDomain : uint8 { Land, Sea, Air };

/** Rodzaj produkcji w miescie. */
UENUM(BlueprintType)
enum class EProductionKind : uint8
{
	None,
	Unit,
	Building
};

struct FUnitDef
{
	const TCHAR* Name;
	int32 Moves;
	int32 Cost;
	int32 Vision;
	int32 Strength;
	bool bCivilian;
	EDomain Domain = EDomain::Land;
	ETech Req = ETech::None;
	bool bSpecial = false;   // jednostka specjalna (limit 10 w armii)
	int32 RaceLock = -1;     // >= 0: tylko ta rasa (EChickenRace) moze ja budowac
};

struct FBuildingDef
{
	const TCHAR* Name;
	int32 Cost;
	int32 Food;
	int32 Prod;
	int32 Gold;
	int32 Science;
	ETech Req = ETech::None;
	int32 NuggetCost = 0;  // koszt w nuggetsach (cuda)
	bool bWonder = false;  // cud: jeden na gre
};

namespace CivDefs
{
	inline const FUnitDef& Unit(EUnitType T)
	{
		static const FUnitDef Defs[] = {
			{ TEXT("Osadnik"),   2, 50, 2, 0, true  },
			{ TEXT("Robotnik"),  2, 30, 2, 0, true  },
			{ TEXT("Wojownik"),  2, 20, 2, 6, false },
			{ TEXT("Łucznik"),   2, 30, 2, 5, false, EDomain::Land, ETech::Husbandry },
			{ TEXT("Zwiadowca"), 3, 15, 3, 0, false },
			{ TEXT("Statek"),    4, 60, 3, 12, false, EDomain::Sea, ETech::Sailing },
			{ TEXT("Laserowy pistoleciarz"), 2, 80, 2, 14, false, EDomain::Land, ETech::Lasers },
			{ TEXT("Prom kosmiczny"), 5, 120, 3, 0, true, EDomain::Air, ETech::Shuttles },
			{ TEXT("X-wing"),    6, 140, 3, 20, false, EDomain::Air, ETech::SmallWarships },
			{ TEXT("TIE"),       6, 140, 3, 20, false, EDomain::Air, ETech::SmallWarships },
			{ TEXT("Miotacz czarnej dziury"), 1, 200, 2, 30, false, EDomain::Land, ETech::BlackHoles, true },
			{ TEXT("Droid"),     3, 180, 2, 25, false, EDomain::Land, ETech::AdvancedTech },
			{ TEXT("Kwasomiot"), 2, 190, 2, 28, false, EDomain::Land, ETech::AdvancedTech },
			{ TEXT("Droid meteorowy"), 2, 250, 2, 35, false, EDomain::Land, ETech::AdvancedTech },
			{ TEXT("Dowódca"),   2, 60, 2, 10, false, EDomain::Land, ETech::Bronze },
			{ TEXT("Medyk"),     2, 40, 2, 2, false, EDomain::Land, ETech::Husbandry, true },
			{ TEXT("Wskrzeszacz"), 2, 150, 2, 1, false, EDomain::Land, ETech::Electricity, true },
			{ TEXT("Zastępca"),  2, 70, 2, 8, false, EDomain::Land, ETech::IronWorking, true },
			{ TEXT("Zielony strażnik"), 2, 120, 2, 25, false, EDomain::Land, ETech::Bronze, true, 3 },
		};
		return Defs[(int32)T];
	}

	inline const FBuildingDef& Building(EBuildingType T)
	{
		static const FBuildingDef Defs[] = {
			{ TEXT("Spichlerz"),  40, 2, 0, 0, 0 },
			{ TEXT("Warsztat"),   60, 0, 2, 0, 0 },
			{ TEXT("Biblioteka"), 50, 0, 0, 0, 3, ETech::Writing },
			{ TEXT("Targ"),       60, 0, 0, 3, 0, ETech::Mathematics },
			{ TEXT("Koszary"),    45, 0, 0, 0, 0 },
			{ TEXT("Stacja kosmiczna"),   200, 0, 0, 0, 5,  ETech::SpaceStations },
			{ TEXT("Akcelerator cząstek"), 250, 0, 0, 0, 10, ETech::ParticleAccelerator },
			{ TEXT("Teleporter"),         150, 0, 0, 0, 0,  ETech::Teleportation },
			{ TEXT("Gwiazda Śmierci"),    500, 0, 0, 0, 0,  ETech::DeathStar, 100, true },
		};
		return Defs[(int32)T];
	}

	constexpr int32 NumUnitTypes = 19;
	constexpr int32 NumBuildingTypes = 9;
}

/** Jednostka na mapie. Path to dalsza droga rozkazu ruchu (wykonywana na poczatku tury). */
USTRUCT(BlueprintType)
struct FUnit
{
	GENERATED_BODY()

	UPROPERTY() int32 Id = 0;
	UPROPERTY() int32 Owner = 0;
	UPROPERTY() EUnitType Type = EUnitType::Warrior;
	UPROPERTY() FHexCoord Pos;
	UPROPERTY() int32 MovesLeft = 0;
	UPROPERTY() int32 Hp = 100;
	UPROPERTY() TArray<FHexCoord> Path;
	UPROPERTY() int32 ArmyId = 0;       // 0 = jednostka samodzielna
	UPROPERTY() bool bCommander = false; // dowodca (lub zastepca, ktory przejal dowodzenie)
	UPROPERTY() int32 WorkTurns = 0;     // pozostale tury budowy ulepszenia (robotnik)
	UPROPERTY() EImprovement WorkType = EImprovement::None;

	const FUnitDef& Def() const { return CivDefs::Unit(Type); }
	bool IsCivilian() const { return Def().bCivilian; }
	/** Czlonek armii, ktory nie jest dowodca: porusza sie z dowodca i nie zajmuje pola samodzielnie. */
	bool IsArmyMember() const { return ArmyId != 0 && !bCommander; }
};

USTRUCT(BlueprintType)
struct FImprovement
{
	GENERATED_BODY()

	UPROPERTY() FHexCoord Pos;
	UPROPERTY() EImprovement Type = EImprovement::None;
};

/** Armia: dowodca + do 10 jednostek zwyklych + do 10 specjalnych (w tym zastepca). */
USTRUCT(BlueprintType)
struct FArmy
{
	GENERATED_BODY()

	UPROPERTY() int32 Id = 0;
	UPROPERTY() int32 Owner = 0;
	UPROPERTY() int32 CommanderId = 0;
	UPROPERTY() TArray<int32> Members; // id jednostek (bez dowodcy)

	static constexpr int32 MaxNormal = 10;
	static constexpr int32 MaxSpecial = 10;
};

USTRUCT(BlueprintType)
struct FProduction
{
	GENERATED_BODY()

	UPROPERTY() EProductionKind Kind = EProductionKind::None;
	UPROPERTY() int32 Item = 0; // EUnitType lub EBuildingType zaleznie od Kind

	bool operator==(const FProduction& O) const { return Kind == O.Kind && Item == O.Item; }
};

USTRUCT(BlueprintType)
struct FCity
{
	GENERATED_BODY()

	UPROPERTY() int32 Id = 0;
	UPROPERTY() int32 Owner = 0;
	UPROPERTY() FString Name;
	UPROPERTY() FHexCoord Pos;
	UPROPERTY() int32 Population = 1;
	UPROPERTY() int32 FoodStock = 0;
	UPROPERTY() int32 ProdStock = 0;
	UPROPERTY() TArray<EBuildingType> Buildings;
	UPROPERTY() FProduction Current;

	bool Has(EBuildingType B) const { return Buildings.Contains(B); }
};

/** Zasoby i widocznosc jednego gracza. Visibility: 0 nieznane, 1 odkryte, 2 widoczne teraz. */
USTRUCT(BlueprintType)
struct FCivPlayerState
{
	GENERATED_BODY()

	UPROPERTY() EChickenRace Race = EChickenRace::Red;
	UPROPERTY() int32 Gold = 0;
	UPROPERTY() int32 Science = 0;
	UPROPERTY() int32 Nuggets = 0; // energia, zdobywana za zabite kurczaki
	UPROPERTY() TArray<ETech> Known;        // poznane technologie
	UPROPERTY() bool bResurrectUsed = false; // wskrzeszenie: 1 na gre

	// Dyplomacja i AI (osobowosc moze byc nadpisana przez strategie LLM).
	UPROPERTY() TArray<int32> AtWarWith;   // gracze, z ktorymi jestesmy w stanie wojny
	UPROPERTY() TArray<int32> PeaceOffers; // gracze, ktorzy zaproponowali nam pokoj (oczekuja odpowiedzi)
	UPROPERTY() TMap<int32, int32> PeaceSince; // gracz -> tura zawarcia pokoju (blokada ponownej wojny)
	UPROPERTY() int32 YieldBonus = 0;      // % do produkcji/zlota/nauki (poziom trudnosci AI)
	UPROPERTY() int32 AIAggression = 30;   // 0-100
	UPROPERTY() int32 AITargetCities = 4;  // ile miast chce miec
	UPROPERTY() int32 AIFocusPath = 0;     // ETechPath preferowanej sciezki badan
	UPROPERTY() int32 AIWarTarget = -1;    // wskazany przez strategie cel wojny
	UPROPERTY() ETech Researching = ETech::None;
	UPROPERTY() int32 ResearchProgress = 0; // punkty nauki wlozone w badanie (bank, gdy nic nie badamy)
	UPROPERTY() TArray<uint8> Visibility;
};

USTRUCT(BlueprintType)
struct FCivState
{
	GENERATED_BODY()

	UPROPERTY() int32 NextId = 1;
	UPROPERTY() TArray<FUnit> Units;
	UPROPERTY() TArray<FCity> Cities;
	UPROPERTY() TArray<FArmy> Armies;
	UPROPERTY() TArray<FImprovement> Improvements;
	UPROPERTY() TArray<FCivPlayerState> PlayerStates;
};

/** Zapis gry (JSON): ziarno mapy (mapa jest odtwarzana) oraz caly stan rozgrywki. */
USTRUCT(BlueprintType)
struct FChickenSaveData
{
	GENERATED_BODY()

	UPROPERTY() int32 Version = 2;
	UPROPERTY() FMapSettings Settings;
	UPROPERTY() int32 Turn = 1;
	UPROPERTY() int32 CurrentPlayer = 0;
	UPROPERTY() TArray<FPlayerInfo> Players;
	UPROPERTY() FCivState Civ;
};

/** Plony z pola: jedzenie, produkcja, zloto, nauka. */
struct FYield
{
	int32 Food = 0;
	int32 Prod = 0;
	int32 Gold = 0;
	int32 Science = 0;

	FYield& operator+=(const FYield& O) { Food += O.Food; Prod += O.Prod; Gold += O.Gold; Science += O.Science; return *this; }
};

/** Podsumowanie miasta na biezaca ture. */
struct FCityReport
{
	FYield Yield;
	int32 FoodNeeded = 0;      // zuzycie zywnosci (2 na mieszkanca)
	int32 GrowthThreshold = 0; // zywnosc potrzebna do wzrostu
	TArray<FHexCoord> Worked;  // pracujace pola (bez centrum)
	int32 FoodSurplus() const { return Yield.Food - FoodNeeded; }
};

/** Pozycja na liscie produkcji miasta. */
struct FProductionOption
{
	FProduction Item;
	FString Name;
	int32 Cost = 0;
};
