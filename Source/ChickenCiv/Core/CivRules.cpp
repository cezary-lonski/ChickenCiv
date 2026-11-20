#include "Core/CivRules.h"
#include "Math/RandomStream.h"
#include "Algo/Reverse.h"
#include "Model/TechTree.h"
#include "Core/ArmyRules.h"

namespace CivRules
{
	// ===== Wyszukiwanie =====
	FUnit* FindUnit(FCivState& S, int32 Id) { return S.Units.FindByPredicate([Id](const FUnit& U) { return U.Id == Id; }); }
	const FUnit* FindUnit(const FCivState& S, int32 Id) { return S.Units.FindByPredicate([Id](const FUnit& U) { return U.Id == Id; }); }
	FCity* FindCity(FCivState& S, int32 Id) { return S.Cities.FindByPredicate([Id](const FCity& C) { return C.Id == Id; }); }
	const FCity* FindCity(const FCivState& S, int32 Id) { return S.Cities.FindByPredicate([Id](const FCity& C) { return C.Id == Id; }); }
	const FCity* CityAt(const FCivState& S, const FHexCoord& P) { return S.Cities.FindByPredicate([&P](const FCity& C) { return C.Pos == P; }); }

	const FUnit* UnitAt(const FCivState& S, const FHexCoord& P, bool bCivilian)
	{
		return S.Units.FindByPredicate([&](const FUnit& U) { return U.Pos == P && U.IsCivilian() == bCivilian && !U.IsArmyMember(); });
	}

	void UnitsAt(const FCivState& S, const FHexCoord& P, TArray<const FUnit*>& Out)
	{
		Out.Reset();
		for (const FUnit& U : S.Units) if (U.Pos == P && !U.IsArmyMember()) Out.Add(&U);
	}

	// ===== Ruch =====
	int32 MoveCost(const FMapTile& T)
	{
		switch (T.Terrain)
		{
		case ETerrainType::Ocean:
		case ETerrainType::Coast:
		case ETerrainType::Mountain: return 0;
		case ETerrainType::Forest:
		case ETerrainType::Hills:    return 2;
		default:                     return 1;
		}
	}

	// Koszt wejscia na pole dla danej domeny ruchu; 0 = nie wolno. Obca jednostka lub obce miasto blokuje pole.
	static int32 EnterCost(const FHexMap& M, const FCivState& S, EDomain Domain, int32 Owner, const FHexCoord& P)
	{
		const FMapTile* T = M.GetTile(P);
		if (!T) return 0;
		for (const FUnit& U : S.Units) if (U.Pos == P && U.Owner != Owner) return 0;
		const FCity* C = CityAt(S, P);
		if (C && C->Owner != Owner) return 0;
		switch (Domain)
		{
		case EDomain::Land:
		{
			const int32 Cost = MoveCost(*T);
			return (Cost > 1 && HasRoad(S, P)) ? 1 : Cost; // droga: las i wzgorza kosztuja tyle co rownina
		}
		case EDomain::Sea:  return (T->IsWater() || C) ? 1 : 0; // statki wplywaja do wlasnych miast (port)
		case EDomain::Air:  return 1;
		}
		return 0;
	}

	static bool CanPass(const FHexMap& M, const FCivState& S, EDomain Domain, int32 Owner, const FHexCoord& P)
	{
		return EnterCost(M, S, Domain, Owner, P) > 0;
	}

	int32 MaxMoves(const FCivState& S, const FUnit& U)
	{
		int32 Moves = U.Def().Moves;
		if (U.Def().Domain == EDomain::Sea && S.PlayerStates.IsValidIndex(U.Owner))
		{
			Moves *= CivRaces::Get(S.PlayerStates[U.Owner].Race).ShipSpeedMult;
		}
		return Moves;
	}

	bool CanEnter(const FHexMap& M, const FCivState& S, const FUnit& U, const FHexCoord& To)
	{
		if (!CanPass(M, S, U.Def().Domain, U.Owner, To)) return false;
		// Na polu moze stac jedna jednostka cywilna i jedna wojskowa.
		const FUnit* Other = UnitAt(S, To, U.IsCivilian());
		if (!Other || Other->Id == U.Id) return true;
		// Wojskowy moze wejsc na pole wlasnego dowodcy, jesli armia ma dla niego miejsce (dolaczy do niej).
		if (!U.IsCivilian() && Other->Owner == U.Owner && Other->bCommander && U.ArmyId == 0 && U.Type != EUnitType::Commander)
		{
			const FArmy* A = ArmyRules::FindArmy(S, Other->ArmyId);
			return A && ArmyRules::CanJoin(S, *A, U, nullptr, false);
		}
		return false;
	}

	bool FindPath(const FHexMap& M, const FCivState& S, const FUnit& U, const FHexCoord& To, TArray<FHexCoord>& OutPath)
	{
		OutPath.Reset();
		if (U.Pos == To) return false;
		if (!CanEnter(M, S, U, To)) return false;

		TMap<FHexCoord, int32> Dist;
		TMap<FHexCoord, FHexCoord> Prev;
		TArray<FHexCoord> Open;
		Dist.Add(U.Pos, 0);
		Open.Add(U.Pos);

		while (Open.Num() > 0)
		{
			int32 Best = 0;
			for (int32 i = 1; i < Open.Num(); ++i) if (Dist[Open[i]] < Dist[Open[Best]]) Best = i;
			const FHexCoord Cur = Open[Best];
			Open.RemoveAtSwap(Best);
			if (Cur == To) break;

			const int32 CurDist = Dist[Cur];
			for (int32 d = 0; d < 6; ++d)
			{
				const FHexCoord N = Cur.Neighbor(d);
				const int32 Cost = EnterCost(M, S, U.Def().Domain, U.Owner, N);
				if (Cost <= 0) continue;
				// Nie przechodzimy przez pole zajete przez inna wlasna jednostke wojskowa (poza celem).
				if (N != To && !U.IsCivilian())
				{
					const FUnit* Occ = UnitAt(S, N, false);
					if (Occ && Occ->Id != U.Id && Occ->Owner == U.Owner) continue;
				}
				const int32 ND = CurDist + Cost;
				const int32* Old = Dist.Find(N);
				if (!Old || ND < *Old)
				{
					Dist.Add(N, ND);
					Prev.Add(N, Cur);
					Open.AddUnique(N);
				}
			}
		}

		if (!Prev.Contains(To)) return false;
		FHexCoord Step = To;
		while (Step != U.Pos)
		{
			OutPath.Add(Step);
			Step = Prev[Step];
		}
		Algo::Reverse(OutPath);
		return true;
	}

	void AdvanceUnit(const FHexMap& M, FCivState& S, FUnit& U)
	{
		while (U.MovesLeft > 0 && U.Path.Num() > 0)
		{
			const FHexCoord Next = U.Path[0];
			if (!CanEnter(M, S, U, Next)) { U.Path.Reset(); break; }
			const int32 Cost = EnterCost(M, S, U.Def().Domain, U.Owner, Next);
			U.Pos = Next;
			// Wejscie na pole wlasnego dowodcy = dolaczenie do jego armii.
			if (!U.bCommander && !U.IsCivilian() && U.ArmyId == 0)
			{
				if (const FArmy* Army = ArmyRules::ArmyAt(S, Next, U.Owner))
				{
					const int32 ArmyId = Army->Id;
					if (ArmyRules::JoinArmy(S, ArmyId, U.Id)) { U.Path.Reset(); break; }
				}
			}
			if (U.bCommander)
			{
				for (FUnit& Member : S.Units) if (Member.ArmyId == U.ArmyId && Member.IsArmyMember()) Member.Pos = Next;
			}
			U.Path.RemoveAt(0);
			U.MovesLeft = FMath::Max(0, U.MovesLeft - Cost);
		}
	}

	bool OrderMove(const FHexMap& M, FCivState& S, int32 UnitId, const FHexCoord& To)
	{
		FUnit* U = FindUnit(S, UnitId);
		if (!U) return false;
		TArray<FHexCoord> Path;
		if (!FindPath(M, S, *U, To, Path)) return false;
		U->Path = Path;
		AdvanceUnit(M, S, *U);
		return true;
	}

	// ===== Jednostki i miasta =====
	FUnit& SpawnUnit(FCivState& S, int32 Owner, EUnitType Type, const FHexCoord& Pos)
	{
		FUnit U;
		U.Id = S.NextId++;
		U.Owner = Owner;
		U.Type = Type;
		U.Pos = Pos;
		U.MovesLeft = MaxMoves(S, U);
		return S.Units.Add_GetRef(U);
	}

	bool CanFoundCity(const FHexMap& M, const FCivState& S, const FUnit& U, FString* Why)
	{
		auto Fail = [&](const TCHAR* Msg) { if (Why) *Why = Msg; return false; };
		if (U.Type != EUnitType::Settler) return Fail(TEXT("Tylko osadnik zakłada miasta"));
		if (U.MovesLeft <= 0) return Fail(TEXT("Osadnik nie ma już ruchu"));
		const FMapTile* T = M.GetTile(U.Pos);
		if (!T || T->IsWater() || T->Terrain == ETerrainType::Mountain) return Fail(TEXT("Tu nie można założyć miasta"));
		for (const FCity& C : S.Cities)
		{
			if (C.Pos.Distance(U.Pos) < 3) return Fail(TEXT("Za blisko innego miasta (min. 3 pola)"));
		}
		return true;
	}

	int32 FoundCity(const FHexMap& M, FCivState& S, int32 UnitId, FString* Why)
	{
		const FUnit* U = FindUnit(S, UnitId);
		if (!U) return INDEX_NONE;
		if (!CanFoundCity(M, S, *U, Why)) return INDEX_NONE;

		static const TCHAR* Names[] = { TEXT("Kurnik"), TEXT("Grzęda"), TEXT("Jajowo"), TEXT("Kogutowo"), TEXT("Dziobowo"), TEXT("Nioska"), TEXT("Ziarnowo"), TEXT("Pióropusz") };
		const int32 NumNames = UE_ARRAY_COUNT(Names);

		FCity C;
		C.Id = S.NextId++;
		C.Owner = U->Owner;
		C.Pos = U->Pos;
		int32 N = S.Cities.Num();
		for (int32 Try = 0; Try < 1000; ++Try, ++N)
		{
			FString Name = Names[N % NumNames];
			if (N >= NumNames) Name += FString::Printf(TEXT(" %d"), N / NumNames + 1);
			if (!S.Cities.ContainsByPredicate([&](const FCity& O) { return O.Name == Name; })) { C.Name = Name; break; }
		}
		const int32 NewId = C.Id;
		S.Cities.Add(C);
		S.Units.RemoveAll([UnitId](const FUnit& X) { return X.Id == UnitId; });
		return NewId;
	}

	// ===== Ulepszenia pol =====
	EImprovement ImprovementAt(const FCivState& S, const FHexCoord& P)
	{
		for (const FImprovement& I : S.Improvements) if (I.Pos == P && I.Type != EImprovement::Road) return I.Type;
		return EImprovement::None;
	}

	bool HasRoad(const FCivState& S, const FHexCoord& P)
	{
		for (const FImprovement& I : S.Improvements) if (I.Pos == P && I.Type == EImprovement::Road) return true;
		return false;
	}

	EImprovement BestImprovementFor(const FHexMap& M, const FCivState& S, const FHexCoord& P)
	{
		const FMapTile* T = M.GetTile(P);
		if (!T || !T->IsPassableLand() || CityAt(S, P)) return EImprovement::None;
		if (ImprovementAt(S, P) == EImprovement::None)
		{
			switch (T->Terrain)
			{
			case ETerrainType::Grassland:
			case ETerrainType::Plains:
			case ETerrainType::Desert: return EImprovement::Farm;
			case ETerrainType::Hills:
			case ETerrainType::Forest: return EImprovement::Mine;
			default: break;
			}
		}
		return HasRoad(S, P) ? EImprovement::None : EImprovement::Road;
	}

	bool StartImprovement(const FHexMap& M, FCivState& S, int32 UnitId, FString* Why)
	{
		auto Fail = [&](const TCHAR* Msg) { if (Why) *Why = Msg; return false; };
		FUnit* U = FindUnit(S, UnitId);
		if (!U || U->Type != EUnitType::Worker) return Fail(TEXT("Tylko robotnik buduje ulepszenia"));
		if (U->MovesLeft <= 0) return Fail(TEXT("Robotnik nie ma już ruchu"));
		if (U->WorkTurns > 0) return Fail(TEXT("Robotnik już pracuje"));
		const EImprovement Best = BestImprovementFor(M, S, U->Pos);
		if (Best == EImprovement::None) return Fail(TEXT("Tu nie ma czego ulepszać"));
		U->WorkType = Best;
		U->WorkTurns = ImprovementTurns;
		U->MovesLeft = 0;
		U->Path.Reset();
		return true;
	}

	// ===== Plony =====
	FYield TileYield(const FMapTile& T)
	{
		FYield Y;
		switch (T.Terrain)
		{
		case ETerrainType::Ocean:     Y.Food = 1; Y.Gold = 1; break;
		case ETerrainType::Coast:     Y.Food = 1; Y.Gold = 1; break;
		case ETerrainType::Grassland: Y.Food = 2; break;
		case ETerrainType::Plains:    Y.Food = 1; Y.Prod = 1; break;
		case ETerrainType::Forest:    Y.Food = 1; Y.Prod = 2; break;
		case ETerrainType::Desert:    break;
		case ETerrainType::Hills:     Y.Prod = 2; break;
		case ETerrainType::Mountain:  break;
		}
		switch (T.Resource)
		{
		case EResourceType::Wheat:  Y.Food += 1; break;
		case EResourceType::Cattle: Y.Food += 1; break;
		case EResourceType::Fish:   Y.Food += 1; break;
		case EResourceType::Horses: Y.Prod += 1; break;
		case EResourceType::Iron:   Y.Prod += 1; break;
		case EResourceType::Stone:  Y.Prod += 1; break;
		case EResourceType::Gold:   Y.Gold += 2; break;
		default: break;
		}
		return Y;
	}

	static float TileScore(const FYield& Y) { return Y.Food * 2.f + Y.Prod * 1.5f + Y.Gold * 1.f; }

	// Przydziela pola kolejnym miastom (wedlug Id); kazde pole pracuje najwyzej jedno miasto.
	static void AssignWorkedTiles(const FHexMap& M, const FCivState& S, TMap<int32, TArray<FHexCoord>>& Out)
	{
		TSet<FHexCoord> Taken;
		for (const FCity& C : S.Cities) Taken.Add(C.Pos);

		TArray<const FCity*> Order;
		for (const FCity& C : S.Cities) Order.Add(&C);
		Order.Sort([](const FCity& A, const FCity& B) { return A.Id < B.Id; });

		for (const FCity* C : Order)
		{
			TArray<FHexCoord> Cells;
			M.GetInRadius(C->Pos, 2, Cells);
			Cells.RemoveAll([&](const FHexCoord& H) { return Taken.Contains(H) || H == C->Pos; });
			Cells.Sort([&](const FHexCoord& A, const FHexCoord& B)
			{
				const float SA = TileScore(TileYield(*M.GetTile(A))) - 0.01f * A.Distance(C->Pos);
				const float SB = TileScore(TileYield(*M.GetTile(B))) - 0.01f * B.Distance(C->Pos);
				return SA > SB;
			});
			TArray<FHexCoord>& Worked = Out.Add(C->Id);
			for (int32 i = 0; i < Cells.Num() && Worked.Num() < C->Population; ++i)
			{
				Worked.Add(Cells[i]);
				Taken.Add(Cells[i]);
			}
		}
	}

	FCityReport ComputeCityReport(const FHexMap& M, const FCivState& S, const FCity& City)
	{
		FCityReport R;
		TMap<int32, TArray<FHexCoord>> All;
		AssignWorkedTiles(M, S, All);
		if (const TArray<FHexCoord>* W = All.Find(City.Id)) R.Worked = *W;

		// Centrum miasta: co najmniej 2 jedzenia, 1 produkcji, 1 zlota.
		FYield Center = TileYield(*M.GetTile(City.Pos));
		Center.Food = FMath::Max(Center.Food, 2);
		Center.Prod = FMath::Max(Center.Prod, 1);
		Center.Gold = FMath::Max(Center.Gold, 1);
		R.Yield += Center;
		for (const FHexCoord& H : R.Worked)
		{
			R.Yield += TileYield(*M.GetTile(H));
			switch (ImprovementAt(S, H))
			{
			case EImprovement::Farm: R.Yield.Food += 1; break;
			case EImprovement::Mine: R.Yield.Prod += 1; break;
			default: break;
			}
		}

		R.Yield.Science += City.Population; // 1 nauki na mieszkanca
		for (EBuildingType B : City.Buildings)
		{
			const FBuildingDef& D = CivDefs::Building(B);
			R.Yield.Food += D.Food;
			R.Yield.Prod += D.Prod;
			R.Yield.Gold += D.Gold;
			R.Yield.Science += D.Science;
		}
		if (S.PlayerStates.IsValidIndex(City.Owner))
		{
			R.Yield.Science *= CivRaces::Get(S.PlayerStates[City.Owner].Race).ScienceMult;
		}
		if (S.PlayerStates.IsValidIndex(City.Owner) && S.PlayerStates[City.Owner].YieldBonus != 0)
		{
			const int32 Pct = 100 + S.PlayerStates[City.Owner].YieldBonus;
			R.Yield.Prod = R.Yield.Prod * Pct / 100;
			R.Yield.Gold = R.Yield.Gold * Pct / 100;
			R.Yield.Science = R.Yield.Science * Pct / 100;
		}
		R.FoodNeeded = 2 * City.Population;
		R.GrowthThreshold = 14 + 6 * (City.Population - 1);
		return R;
	}

	FYield ComputeIncome(const FHexMap& M, const FCivState& S, int32 Player)
	{
		FYield Sum;
		for (const FCity& C : S.Cities) if (C.Owner == Player) Sum += ComputeCityReport(M, S, C).Yield;
		return Sum;
	}

	// ===== Produkcja =====
	int32 UnitCost(const FCivState& S, int32 Owner, EUnitType T)
	{
		const FUnitDef& D = CivDefs::Unit(T);
		int32 Cost = D.Cost;
		if (D.Domain == EDomain::Sea && S.PlayerStates.IsValidIndex(Owner))
		{
			Cost = FMath::Max(1, Cost / CivRaces::Get(S.PlayerStates[Owner].Race).ShipCostDiv);
		}
		return Cost;
	}

	static bool TechOk(const FCivState& S, int32 Owner, ETech T)
	{
		if (T == ETech::None) return true;
		return S.PlayerStates.IsValidIndex(Owner) && CivTech::Knows(S.PlayerStates[Owner], T);
	}

	static bool NextToWater(const FHexMap& M, const FHexCoord& P)
	{
		for (int32 d = 0; d < 6; ++d)
		{
			const FMapTile* T = M.GetTile(P.Neighbor(d));
			if (T && T->IsWater()) return true;
		}
		return false;
	}

	static bool WonderExists(const FCivState& S, EBuildingType B)
	{
		return S.Cities.ContainsByPredicate([B](const FCity& C) { return C.Has(B); });
	}

	void GetProductionOptions(const FHexMap& M, const FCivState& S, const FCity& City, TArray<FProductionOption>& Out)
	{
		Out.Reset();
		for (int32 i = 0; i < CivDefs::NumUnitTypes; ++i)
		{
			const EUnitType T = (EUnitType)i;
			const FUnitDef& D = CivDefs::Unit(T);
			if (!TechOk(S, City.Owner, D.Req)) continue;
			if (D.RaceLock >= 0 && (!S.PlayerStates.IsValidIndex(City.Owner) || (int32)S.PlayerStates[City.Owner].Race != D.RaceLock)) continue;
			if (T == EUnitType::Settler && City.Population < 2) continue; // osadnik kosztuje 1 mieszkanca
			if (D.Domain == EDomain::Sea && !NextToWater(M, City.Pos)) continue; // statki tylko w miastach przy wodzie
			FProductionOption O;
			O.Item.Kind = EProductionKind::Unit;
			O.Item.Item = i;
			O.Name = D.Name;
			O.Cost = UnitCost(S, City.Owner, T);
			Out.Add(O);
		}
		for (int32 i = 0; i < CivDefs::NumBuildingTypes; ++i)
		{
			const EBuildingType B = (EBuildingType)i;
			const FBuildingDef& D = CivDefs::Building(B);
			if (City.Has(B)) continue;
			if (!TechOk(S, City.Owner, D.Req)) continue;
			if (D.bWonder && WonderExists(S, B)) continue;
			FProductionOption O;
			O.Item.Kind = EProductionKind::Building;
			O.Item.Item = i;
			O.Name = D.Name;
			O.Cost = D.Cost;
			Out.Add(O);
		}
	}

	static int32 ProductionCost(const FCivState& S, int32 Owner, const FProduction& P)
	{
		if (P.Kind == EProductionKind::Unit) return UnitCost(S, Owner, (EUnitType)P.Item);
		if (P.Kind == EProductionKind::Building) return CivDefs::Building((EBuildingType)P.Item).Cost;
		return 0;
	}

	bool SetProduction(const FHexMap& M, const FCivState& S, FCity& City, const FProduction& P)
	{
		TArray<FProductionOption> Options;
		GetProductionOptions(M, S, City, Options);
		for (const FProductionOption& O : Options)
		{
			if (O.Item == P) { City.Current = P; return true; }
		}
		return false;
	}

	int32 TurnsToComplete(const FHexMap& M, const FCivState& S, const FCity& City)
	{
		const int32 Cost = ProductionCost(S, City.Owner, City.Current);
		if (Cost <= 0) return 0;
		const int32 Prod = ComputeCityReport(M, S, City).Yield.Prod;
		if (Prod <= 0) return 999;
		const int32 Left = FMath::Max(0, Cost - City.ProdStock);
		return FMath::Max(1, (Left + Prod - 1) / Prod);
	}

	// ===== Tury =====
	void BeginPlayerTurn(const FHexMap& M, FCivState& S, int32 Player)
	{
		for (FUnit& U : S.Units)
		{
			if (U.Owner != Player) continue;
			U.MovesLeft = MaxMoves(S, U);
		}
		// Robotnicy w trakcie pracy: pracuja cala ture; po ostatniej turze pole dostaje ulepszenie.
		for (FUnit& U : S.Units)
		{
			if (U.Owner != Player || U.WorkTurns <= 0) continue;
			U.MovesLeft = 0;
			if (--U.WorkTurns == 0)
			{
				const bool bFree = U.WorkType == EImprovement::Road ? !HasRoad(S, U.Pos) : ImprovementAt(S, U.Pos) == EImprovement::None;
				if (bFree && U.WorkType != EImprovement::None)
				{
					FImprovement I; I.Pos = U.Pos; I.Type = U.WorkType;
					S.Improvements.Add(I);
				}
				U.WorkType = EImprovement::None;
			}
		}
		for (FUnit& U : S.Units)
		{
			if (U.Owner == Player && U.Path.Num() > 0) AdvanceUnit(M, S, U);
		}
		ComputeVisibility(M, S, Player);
	}

	// Miejsce na nowa jednostke: pole miasta albo sasiednie wolne.
	static bool FindSpawnTile(const FHexMap& M, const FCivState& S, const FCity& City, EUnitType Type, FHexCoord& Out)
	{
		FUnit Probe;
		Probe.Owner = City.Owner;
		Probe.Type = Type;
		Probe.Pos = City.Pos;
		Probe.Id = -1;
		if (CanEnter(M, S, Probe, City.Pos)) { Out = City.Pos; return true; }
		for (int32 d = 0; d < 6; ++d)
		{
			const FHexCoord N = City.Pos.Neighbor(d);
			if (CanEnter(M, S, Probe, N)) { Out = N; return true; }
		}
		return false;
	}

	void EndPlayerTurn(const FHexMap& M, FCivState& S, int32 Player, TArray<FString>& Log)
	{
		EnsureVisibility(M, S, Player);
		FCivPlayerState& PS = S.PlayerStates[Player];
		int32 TurnScience = 0;

		for (int32 ci = 0; ci < S.Cities.Num(); ++ci)
		{
			// Kopia id, bo dodawanie jednostek nie zmienia miast, ale zachowujemy ostroznosc.
			FCity& City = S.Cities[ci];
			if (City.Owner != Player) continue;

			const FCityReport R = ComputeCityReport(M, S, City);
			PS.Gold += R.Yield.Gold;
			PS.Science += R.Yield.Science;
			TurnScience += R.Yield.Science;

			// Wzrost
			City.FoodStock += R.FoodSurplus();
			if (City.FoodStock >= R.GrowthThreshold)
			{
				City.FoodStock -= R.GrowthThreshold;
				++City.Population;
				Log.Add(FString::Printf(TEXT("%s rośnie do %d"), *City.Name, City.Population));
			}
			else if (City.FoodStock < 0)
			{
				City.FoodStock = 0;
				if (City.Population > 1) { --City.Population; Log.Add(FString::Printf(TEXT("%s głoduje (populacja %d)"), *City.Name, City.Population)); }
			}

			// Produkcja
			if (City.Current.Kind == EProductionKind::None)
			{
				City.ProdStock += R.Yield.Prod;
				Log.Add(FString::Printf(TEXT("%s nic nie produkuje"), *City.Name));
				continue;
			}
			City.ProdStock += R.Yield.Prod;
			// Cud, ktory ktos juz zbudowal, przestaje byc dostepny.
			if (City.Current.Kind == EProductionKind::Building)
			{
				const EBuildingType WB = (EBuildingType)City.Current.Item;
				if (CivDefs::Building(WB).bWonder && WonderExists(S, WB))
				{
					Log.Add(FString::Printf(TEXT("%s: cud %s został już zbudowany"), *City.Name, CivDefs::Building(WB).Name));
					City.Current = FProduction();
					continue;
				}
			}
			const int32 Cost = ProductionCost(S, Player, City.Current);
			if (City.ProdStock < Cost) continue;
			if (City.Current.Kind == EProductionKind::Building)
			{
				const FBuildingDef& BD = CivDefs::Building((EBuildingType)City.Current.Item);
				if (PS.Nuggets < BD.NuggetCost) { Log.Add(FString::Printf(TEXT("%s: %s czeka na nuggetsy (%d/%d)"), *City.Name, BD.Name, PS.Nuggets, BD.NuggetCost)); continue; }
				PS.Nuggets -= BD.NuggetCost;
			}

			if (City.Current.Kind == EProductionKind::Unit)
			{
				const EUnitType T = (EUnitType)City.Current.Item;
				if (T == EUnitType::Settler && City.Population < 2) continue;
				FHexCoord At;
				if (!FindSpawnTile(M, S, City, T, At)) continue; // brak miejsca: czekamy
				// SpawnUnit moze realokowac tablice Units, ale nie Cities.
				const int32 NewId = SpawnUnit(S, Player, T, At).Id;
				if (FUnit* NewU = FindUnit(S, NewId))
				{
					NewU->MovesLeft = 0;
					// Pole z dowodca armii: nowa jednostka od razu do niej dolacza (inaczej staloby tam dwoch wojskowych).
					if (!NewU->IsCivilian() && NewU->Type != EUnitType::Commander)
					{
						if (const FArmy* Army = ArmyRules::ArmyAt(S, At, Player)) { const int32 ArmyId = Army->Id; ArmyRules::JoinArmy(S, ArmyId, NewId); }
					}
				}
				if (T == EUnitType::Settler) --City.Population;
				Log.Add(FString::Printf(TEXT("%s: gotowe – %s"), *City.Name, CivDefs::Unit(T).Name));
			}
			else
			{
				const EBuildingType B = (EBuildingType)City.Current.Item;
				City.Buildings.AddUnique(B);
				Log.Add(FString::Printf(TEXT("%s: zbudowano – %s"), *City.Name, CivDefs::Building(B).Name));
			}
			City.ProdStock -= Cost;
			City.Current = FProduction();
		}

		// Badania: punkty nauki z tej tury.
		CivTech::AddScience(PS, TurnScience, Log);
		if (PS.Researching == ETech::None && CivTech::AutoPick(PS) != ETech::None && S.Cities.ContainsByPredicate([Player](const FCity& C) { return C.Owner == Player; }))
		{
			Log.Add(TEXT("Wybierz kolejne badanie (T)"));
		}
		ComputeVisibility(M, S, Player);
	}

	// ===== Widocznosc =====
	void EnsureVisibility(const FHexMap& M, FCivState& S, int32 Player)
	{
		while (S.PlayerStates.Num() <= Player) S.PlayerStates.AddDefaulted();
		TArray<uint8>& V = S.PlayerStates[Player].Visibility;
		if (V.Num() != M.Num()) { V.Init(0, M.Num()); }
	}

	void ComputeVisibility(const FHexMap& M, FCivState& S, int32 Player)
	{
		EnsureVisibility(M, S, Player);
		TArray<uint8>& V = S.PlayerStates[Player].Visibility;
		for (uint8& X : V) if (X == 2) X = 1;

		TArray<FHexCoord> Cells;
		auto Reveal = [&](const FHexCoord& Center, int32 R)
		{
			M.GetInRadius(Center, R, Cells);
			for (const FHexCoord& H : Cells) V[M.IndexOf(H)] = 2;
		};
		for (const FUnit& U : S.Units) if (U.Owner == Player) Reveal(U.Pos, U.Def().Vision);
		for (const FCity& C : S.Cities) if (C.Owner == Player) Reveal(C.Pos, 3);
	}

	// ===== Start =====
	bool PlaceStartingUnits(const FHexMap& M, FCivState& S, int32 NumPlayers, int32 Seed)
	{
		FRandomStream Rng(Seed ^ 0x5F3759DF);

		TArray<FHexCoord> Candidates;
		for (const FMapTile& T : M.Tiles)
		{
			if (T.Terrain != ETerrainType::Grassland && T.Terrain != ETerrainType::Plains) continue;
			if (M.CountLandInRadius(T.Coord, 2) < 10) continue;
			Candidates.Add(T.Coord);
		}
		// Przemieszaj deterministycznie.
		for (int32 i = Candidates.Num() - 1; i > 0; --i) Candidates.Swap(i, Rng.RandRange(0, i));

		TArray<FHexCoord> Starts;
		for (int32 Sep = 14; Sep >= 3 && Starts.Num() < NumPlayers; --Sep)
		{
			Starts.Reset();
			for (const FHexCoord& C : Candidates)
			{
				bool bOk = true;
				for (const FHexCoord& O : Starts) if (O.Distance(C) < Sep) { bOk = false; break; }
				if (!bOk) continue;
				Starts.Add(C);
				if (Starts.Num() >= NumPlayers) break;
			}
		}
		if (Starts.Num() < NumPlayers) return false;

		for (int32 p = 0; p < NumPlayers; ++p)
		{
			SpawnUnit(S, p, EUnitType::Settler, Starts[p]);
			SpawnUnit(S, p, EUnitType::Warrior, Starts[p]);
			for (int32 d = 0; d < 6; ++d)
			{
				const FHexCoord N = Starts[p].Neighbor(d);
				const FMapTile* T = M.GetTile(N);
				if (T && MoveCost(*T) > 0 && !UnitAt(S, N, true))
				{
					SpawnUnit(S, p, EUnitType::Worker, N);
					break;
				}
			}
			EnsureVisibility(M, S, p);
			ComputeVisibility(M, S, p);
		}
		return true;
	}
}
