#include "Model/TechTree.h"

namespace CivTech
{
	const FTechDef& Def(ETech T)
	{
		// Kolejnosc = kolejnosc w enum ETech.
		static const FTechDef Defs[] = {
			{ ETech::None,        TEXT("-"),                  0,   ETechPath::Main, ETech::None, ETech::None, TEXT("") },
			{ ETech::Agriculture, TEXT("Rolnictwo"),          10,  ETechPath::Main, ETech::None, ETech::None, TEXT("") },
			{ ETech::Husbandry,   TEXT("Hodowla"),            15,  ETechPath::Main, ETech::Agriculture, ETech::None, TEXT("Łucznik") },
			{ ETech::Writing,     TEXT("Pismo"),              15,  ETechPath::Main, ETech::Agriculture, ETech::None, TEXT("Biblioteka") },
			{ ETech::Bronze,      TEXT("Brąz"),               20,  ETechPath::Main, ETech::Husbandry, ETech::None, TEXT("") },
			{ ETech::Mathematics, TEXT("Matematyka"),         25,  ETechPath::Main, ETech::Writing, ETech::None, TEXT("Targ") },
			{ ETech::Sailing,     TEXT("Żegluga"),            30,  ETechPath::Main, ETech::Mathematics, ETech::None, TEXT("Statek") },
			{ ETech::IronWorking, TEXT("Obróbka żelaza"),     35,  ETechPath::Main, ETech::Bronze, ETech::None, TEXT("") },
			{ ETech::Gunpowder,   TEXT("Proch"),              45,  ETechPath::Main, ETech::IronWorking, ETech::Mathematics, TEXT("") },
			{ ETech::Electricity, TEXT("Elektryczność"),      60,  ETechPath::Main, ETech::Mathematics, ETech::Sailing, TEXT("") },
			{ ETech::Engines,     TEXT("Silniki"),            80,  ETechPath::Main, ETech::Gunpowder, ETech::Electricity, TEXT("") },
			{ ETech::Computers,   TEXT("Komputery"),          100, ETechPath::Main, ETech::Electricity, ETech::Engines, TEXT("") },
			{ ETech::Spaceflight, TEXT("Loty kosmiczne"),     130, ETechPath::Main, ETech::Computers, ETech::None, TEXT("Otwiera 3 ścieżki") },
			{ ETech::SpaceStations, TEXT("Stacje kosmiczne"), 150, ETechPath::Ships, ETech::Spaceflight, ETech::None, TEXT("Stacja kosmiczna") },
			{ ETech::Shuttles,    TEXT("Promy kosmiczne"),    180, ETechPath::Ships, ETech::SpaceStations, ETech::None, TEXT("Prom kosmiczny") },
			{ ETech::DeathStar,   TEXT("Gwiazda Śmierci"),    300, ETechPath::Ships, ETech::Shuttles, ETech::None, TEXT("Cud: Gwiazda Śmierci") },
			{ ETech::Lasers,      TEXT("Lasery"),             150, ETechPath::Lasers, ETech::Spaceflight, ETech::None, TEXT("Laserowy pistoleciarz") },
			{ ETech::SmallWarships, TEXT("Małe statki wojenne"), 220, ETechPath::Lasers, ETech::SpaceStations, ETech::Lasers, TEXT("X-wing, TIE") },
			{ ETech::Elements,    TEXT("Pierwiastki"),        150, ETechPath::Elements, ETech::Spaceflight, ETech::None, TEXT("") },
			{ ETech::ParticleAccelerator, TEXT("Akcelerator cząstek"), 200, ETechPath::Elements, ETech::Elements, ETech::None, TEXT("Akcelerator (+10 nauki)") },
			{ ETech::BlackHoles,  TEXT("Czarne dziury"),      260, ETechPath::Elements, ETech::ParticleAccelerator, ETech::None, TEXT("Miotacz czarnej dziury") },
			{ ETech::Teleportation, TEXT("Teleportacja"),     240, ETechPath::Elements, ETech::Elements, ETech::None, TEXT("Teleporter") },
			{ ETech::AdvancedTech, TEXT("Zaawansowana technologia"), 400, ETechPath::Advanced, ETech::SmallWarships, ETech::BlackHoles, TEXT("Droid, Kwasomiot, Droid meteorowy") },
		};
		static_assert(UE_ARRAY_COUNT(Defs) == NumTechs, "Tabela technologii musi miec NumTechs wpisow");
		return Defs[FMath::Clamp((int32)T, 0, NumTechs - 1)];
	}

	FString PathName(ETechPath P)
	{
		switch (P)
		{
		case ETechPath::Main: return TEXT("Główne");
		case ETechPath::Ships: return TEXT("Statki");
		case ETechPath::Lasers: return TEXT("Lasery");
		case ETechPath::Elements: return TEXT("Pierwiastki");
		case ETechPath::Advanced: return TEXT("Zaawansowana");
		}
		return TEXT("?");
	}

	bool Knows(const FCivPlayerState& PS, ETech T)
	{
		return T == ETech::None || PS.Known.Contains(T);
	}

	bool CanResearch(const FCivPlayerState& PS, ETech T)
	{
		if (T == ETech::None || Knows(PS, T)) return false;
		const FTechDef& D = Def(T);
		return Knows(PS, D.Req1) && Knows(PS, D.Req2);
	}

	void GetAvailable(const FCivPlayerState& PS, TArray<ETech>& Out)
	{
		Out.Reset();
		for (int32 i = 1; i < NumTechs; ++i) if (CanResearch(PS, (ETech)i)) Out.Add((ETech)i);
		Out.Sort([](const ETech A, const ETech B)
		{
			const FTechDef& DA = Def(A);
			const FTechDef& DB = Def(B);
			if (DA.Path != DB.Path) return (uint8)DA.Path < (uint8)DB.Path;
			if (DA.Cost != DB.Cost) return DA.Cost < DB.Cost;
			return (uint8)A < (uint8)B;
		});
	}

	bool SetResearch(FCivPlayerState& PS, ETech T)
	{
		if (!CanResearch(PS, T)) return false;
		PS.Researching = T;
		return true;
	}

	bool AddScience(FCivPlayerState& PS, int32 Points, TArray<FString>& Log)
	{
		PS.ResearchProgress += FMath::Max(0, Points);
		if (PS.Researching == ETech::None) return false;
		const FTechDef& D = Def(PS.Researching);
		if (PS.ResearchProgress < D.Cost) return false;

		PS.ResearchProgress -= D.Cost;
		PS.Known.AddUnique(PS.Researching);
		Log.Add(FString::Printf(TEXT("Poznano technologię: %s"), D.Name));
		PS.Researching = ETech::None;
		return true;
	}

	ETech AutoPick(const FCivPlayerState& PS)
	{
		TArray<ETech> Av;
		GetAvailable(PS, Av);
		ETech Best = ETech::None;
		int32 BestCost = MAX_int32;
		for (ETech T : Av)
		{
			if (Def(T).Cost < BestCost) { BestCost = Def(T).Cost; Best = T; }
		}
		return Best;
	}

	int32 TurnsLeft(const FCivPlayerState& PS, int32 SciencePerTurn)
	{
		if (PS.Researching == ETech::None) return 0;
		if (SciencePerTurn <= 0) return 999;
		const int32 Left = FMath::Max(0, Def(PS.Researching).Cost - PS.ResearchProgress);
		return FMath::Max(1, (Left + SciencePerTurn - 1) / SciencePerTurn);
	}
}
