#include "Core/CivAI.h"
#include "Core/CivRules.h"
#include "Core/ArmyRules.h"
#include "Core/BattleRules.h"
#include "Model/TechTree.h"
#include "Math/RandomStream.h"

namespace CivAI
{
	// ===== Dyplomacja =====
	bool IsAtWar(const FCivState& S, int32 A, int32 B)
	{
		return S.PlayerStates.IsValidIndex(A) && S.PlayerStates[A].AtWarWith.Contains(B);
	}

	bool DeclareWar(FCivState& S, int32 A, int32 B)
	{
		if (A == B || !S.PlayerStates.IsValidIndex(A) || !S.PlayerStates.IsValidIndex(B)) return false;
		if (IsAtWar(S, A, B)) return false;
		S.PlayerStates[A].AtWarWith.AddUnique(B);
		S.PlayerStates[B].AtWarWith.AddUnique(A);
		return true;
	}

	bool MakePeace(FCivState& S, int32 A, int32 B, int32 Turn)
	{
		if (!IsAtWar(S, A, B) || !S.PlayerStates.IsValidIndex(A) || !S.PlayerStates.IsValidIndex(B)) return false;
		S.PlayerStates[A].AtWarWith.Remove(B);
		S.PlayerStates[B].AtWarWith.Remove(A);
		S.PlayerStates[A].PeaceOffers.Remove(B);
		S.PlayerStates[B].PeaceOffers.Remove(A);
		S.PlayerStates[A].PeaceSince.Add(B, Turn);
		S.PlayerStates[B].PeaceSince.Add(A, Turn);
		return true;
	}

	int32 TotalPower(const FCivState& S, int32 Player)
	{
		int32 Sum = 0;
		const EChickenRace Race = S.PlayerStates.IsValidIndex(Player) ? S.PlayerStates[Player].Race : EChickenRace::Red;
		for (const FUnit& U : S.Units) if (U.Owner == Player) Sum += U.Def().Strength * CivRaces::Get(Race).StrengthMult;
		return Sum;
	}

	bool WouldAcceptPeace(const FCivState& S, int32 AI, int32 Other)
	{
		if (!S.PlayerStates.IsValidIndex(AI)) return false;
		const int32 Mine = TotalPower(S, AI), Theirs = TotalPower(S, Other);
		return S.PlayerStates[AI].AIAggression < 50 || Mine * 10 <= Theirs * 12;
	}

	bool ProposePeace(FCivState& S, int32 From, int32 To, int32 Turn, TArray<FString>* Log)
	{
		if (!IsAtWar(S, From, To)) return false;
		FCivPlayerState& F = S.PlayerStates[From];
		if (F.PeaceOffers.Contains(To))
		{
			// Druga strona juz proponowala: przyjmujemy.
			if (MakePeace(S, From, To, Turn) && Log) Log->Add(FString::Printf(TEXT("Zawarto pokój: cywilizacje %d i %d"), From + 1, To + 1));
			return true;
		}
		S.PlayerStates[To].PeaceOffers.AddUnique(From);
		if (Log) Log->Add(FString::Printf(TEXT("Cywilizacja %d proponuje pokój cywilizacji %d"), From + 1, To + 1));
		return true;
	}

	void InitPlayer(FCivPlayerState& PS, int32 PlayerIndex, int32 Seed, EDifficulty Difficulty, bool bAI)
	{
		FRandomStream R(Seed * 131 + PlayerIndex * 7919 + 1);
		PS.AIAggression = 20 + R.RandRange(0, 60);
		PS.AITargetCities = 3 + R.RandRange(0, 3);
		PS.AIFocusPath = 1 + R.RandRange(0, 2); // Statki / Lasery / Pierwiastki
		PS.AIWarTarget = -1;
		PS.YieldBonus = 0;
		if (bAI)
		{
			switch (Difficulty)
			{
			case EDifficulty::Easy: PS.YieldBonus = -20; break;
			case EDifficulty::Normal: PS.YieldBonus = 0; break;
			case EDifficulty::Hard: PS.YieldBonus = 25; break;
			}
		}
	}

	// ===== Pomocnicze =====
	namespace
	{
		bool Alive(const FCivState& S, int32 P)
		{
			return S.Cities.ContainsByPredicate([P](const FCity& C) { return C.Owner == P; })
				|| S.Units.ContainsByPredicate([P](const FUnit& U) { return U.Owner == P; });
		}

		EChickenRace RaceOf(const FCivState& S, int32 P)
		{
			return S.PlayerStates.IsValidIndex(P) ? S.PlayerStates[P].Race : EChickenRace::Red;
		}

		int32 PowerOf(const FCivState& S, const FUnit& U)
		{
			return U.Def().Strength * CivRaces::Get(RaceOf(S, U.Owner)).StrengthMult;
		}

		bool IsFighter(const FUnit& U)
		{
			return !U.IsCivilian() && U.Def().Domain == EDomain::Land && !U.Def().bSpecial && U.Type != EUnitType::Commander;
		}

		bool IsSupport(const FUnit& U)
		{
			return !U.IsCivilian() && U.Def().Domain == EDomain::Land && U.Def().bSpecial;
		}

		// Sila wszystkich jednostek wlasciciela na polu (armia stoi na jednym polu); miasto +50%.
		int32 PowerAt(const FCivState& S, const FHexCoord& P, int32 Owner)
		{
			int32 Sum = 0;
			for (const FUnit& U : S.Units) if (U.Pos == P && U.Owner == Owner) Sum += PowerOf(S, U);
			if (const FCity* C = CivRules::CityAt(S, P)) if (C->Owner == Owner) Sum = Sum * 3 / 2;
			return Sum;
		}

		int32 ArmyPower(const FCivState& S, const FUnit& Commander)
		{
			int32 Sum = PowerOf(S, Commander);
			for (const FUnit& U : S.Units) if (U.ArmyId == Commander.ArmyId && U.IsArmyMember()) Sum += PowerOf(S, U);
			return Sum;
		}

		int32 UnitOrArmyPower(const FCivState& S, const FUnit& U)
		{
			return U.bCommander ? ArmyPower(S, U) : PowerOf(S, U);
		}

		bool HasWar(const FCivState& S, int32 P)
		{
			return S.PlayerStates.IsValidIndex(P) && S.PlayerStates[P].AtWarWith.Num() > 0;
		}

		// Najblizszy cel wroga (jednostka lub miasto) z kim jestesmy na wojnie.
		bool FindEnemyTarget(const FCivState& S, int32 Player, const FHexCoord& From, FHexCoord& OutTarget, int32& OutPower, int32 MaxDist = 1000)
		{
			bool bFound = false;
			int32 BestDist = MAX_int32;
			auto Consider = [&](const FHexCoord& P, int32 Owner)
			{
				if (!IsAtWar(S, Player, Owner)) return;
				const int32 D = From.Distance(P);
				if (D > MaxDist) return;
				const int32 Pow = PowerAt(S, P, Owner);
				if (D < BestDist || (D == BestDist && Pow < OutPower))
				{
					BestDist = D; OutTarget = P; OutPower = Pow; bFound = true;
				}
			};
			for (const FUnit& U : S.Units) if (U.Owner != Player && !U.IsArmyMember()) Consider(U.Pos, U.Owner);
			for (const FCity& C : S.Cities) if (C.Owner != Player) Consider(C.Pos, C.Owner);
			return bFound;
		}

		int32 Count(const FCivState& S, int32 P, EUnitType T)
		{
			int32 N = 0;
			for (const FUnit& U : S.Units) if (U.Owner == P && U.Type == T) ++N;
			return N;
		}

		int32 CityCount(const FCivState& S, int32 P)
		{
			int32 N = 0;
			for (const FCity& C : S.Cities) if (C.Owner == P) ++N;
			return N;
		}

		int32 ArmyCount(const FCivState& S, int32 P)
		{
			int32 N = 0;
			for (const FArmy& A : S.Armies) if (A.Owner == P) ++N;
			return N;
		}

		int32 DefendersAt(const FCivState& S, const FHexCoord& P, int32 Owner)
		{
			int32 N = 0;
			for (const FUnit& U : S.Units) if (U.Pos == P && U.Owner == Owner && !U.IsCivilian() && U.Def().Domain == EDomain::Land && !U.IsArmyMember()) ++N;
			return N;
		}
	}

	// ===== Produkcja =====
	FProduction ChooseProduction(const FHexMap& M, const FCivState& S, const FCity& City)
	{
		TArray<FProductionOption> Opts;
		CivRules::GetProductionOptions(M, S, City, Opts);
		const FCivPlayerState& PS = S.PlayerStates[City.Owner];
		const int32 Player = City.Owner;

		const int32 Cities = CityCount(S, Player);
		const int32 SettlersInField = Count(S, Player, EUnitType::Settler);
		const bool bNeedExpand = (Cities + SettlersInField) < PS.AITargetCities;
		const int32 Armies = ArmyCount(S, Player);
		const int32 Commanders = Count(S, Player, EUnitType::Commander);
		const int32 Defenders = DefendersAt(S, City.Pos, Player);
		int32 Fighters = 0;
		for (const FUnit& U : S.Units) if (U.Owner == Player && IsFighter(U)) ++Fighters;
		const bool bGuardianHere = S.Units.ContainsByPredicate([&](const FUnit& U) { return U.Pos == City.Pos && U.Owner == Player && U.Type == EUnitType::GreenGuardian; });

		FProduction Best;
		float BestScore = -500.f;
		for (const FProductionOption& O : Opts)
		{
			float Score = -1000.f;
			if (O.Item.Kind == EProductionKind::Unit)
			{
				const EUnitType T = (EUnitType)O.Item.Item;
				const FUnitDef& D = CivDefs::Unit(T);
				if (D.Domain == EDomain::Land)
				{
					switch (T)
					{
					case EUnitType::Settler: Score = (bNeedExpand && City.Population >= 2) ? 100.f : -1000.f; break;
					case EUnitType::Worker: Score = (Count(S, Player, EUnitType::Worker) < Cities) ? 25.f : -1000.f; break;
					case EUnitType::Scout: Score = (Count(S, Player, EUnitType::Scout) < 1) ? 30.f : -1000.f; break;
					case EUnitType::Commander: Score = (PS.AIAggression >= 30 && Commanders < 1 + Cities / 3) ? 50.f : -1000.f; break;
					case EUnitType::Medic: Score = (Armies > 0 && Count(S, Player, EUnitType::Medic) < Armies) ? 35.f : -1000.f; break;
					case EUnitType::Deputy: Score = (Armies > 0 && Count(S, Player, EUnitType::Deputy) < Armies) ? 45.f : -1000.f; break;
					case EUnitType::Resurrector: Score = (Armies > 0 && !PS.bResurrectUsed && Count(S, Player, EUnitType::Resurrector) < 1) ? 30.f : -1000.f; break;
					case EUnitType::BlackHoleThrower: Score = (Armies > 0 && Count(S, Player, EUnitType::BlackHoleThrower) < Armies) ? 40.f : -1000.f; break;
					case EUnitType::GreenGuardian: Score = !bGuardianHere ? 75.f : -1000.f; break;
					default:
						if (Fighters < 6 * FMath::Max(1, Cities))
						{
							Score = D.Strength * 3.f - O.Cost / 8.f + (Defenders == 0 ? 80.f : 0.f) + (PS.AIAggression >= 50 ? 10.f : 0.f);
						}
						break;
					}
				}
			}
			else
			{
				switch ((EBuildingType)O.Item.Item)
				{
				case EBuildingType::Granary: Score = City.Population >= 5 ? 10.f : 40.f; break;
				case EBuildingType::Workshop: Score = 45.f; break;
				case EBuildingType::Library: Score = 60.f; break;
				case EBuildingType::Market: Score = 35.f; break;
				case EBuildingType::Barracks: Score = 5.f; break;
				case EBuildingType::SpaceStation: Score = 50.f; break;
				case EBuildingType::ParticleAccelerator: Score = 70.f; break;
				case EBuildingType::Teleporter: Score = 3.f; break;
				case EBuildingType::DeathStar:
					Score = PS.Nuggets >= CivDefs::Building(EBuildingType::DeathStar).NuggetCost ? 90.f : -1000.f;
					break;
				}
			}
			if (Score > BestScore) { BestScore = Score; Best = O.Item; }
		}
		return Best;
	}

	// ===== Osadnicy =====
	namespace
	{
		float SiteScore(const FHexMap& M, const FHexCoord& P)
		{
			TArray<FHexCoord> Cells;
			M.GetInRadius(P, 2, Cells);
			float Sum = 0.f;
			for (const FHexCoord& C : Cells)
			{
				const FMapTile* T = M.GetTile(C);
				if (!T) continue;
				const FYield Y = CivRules::TileYield(*T);
				Sum += Y.Food * 2.f + Y.Prod * 1.5f + Y.Gold * 1.f;
			}
			return Sum;
		}

		bool BestCitySite(const FHexMap& M, const FCivState& S, const FUnit& Settler, FHexCoord& Out)
		{
			TArray<FHexCoord> Cells;
			M.GetInRadius(Settler.Pos, 10, Cells);
			struct FCand { FHexCoord P; float Score; };
			TArray<FCand> Cands;
			for (const FHexCoord& P : Cells)
			{
				const FMapTile* T = M.GetTile(P);
				if (!T || !T->IsPassableLand()) continue;
				bool bOk = true;
				for (const FCity& C : S.Cities)
				{
					const int32 D = C.Pos.Distance(P);
					if (D < 3 || (C.Owner == Settler.Owner && D < 4)) { bOk = false; break; }
				}
				if (!bOk) continue;
				if (S.Units.ContainsByPredicate([&](const FUnit& U) { return U.Pos == P && U.Owner != Settler.Owner; })) continue;
				Cands.Add({ P, SiteScore(M, P) - 2.f * Settler.Pos.Distance(P) });
			}
			Cands.Sort([](const FCand& A, const FCand& B)
			{
				if (A.Score != B.Score) return A.Score > B.Score;
				return A.P.Q != B.P.Q ? A.P.Q < B.P.Q : A.P.R < B.P.R;
			});
			for (int32 i = 0; i < Cands.Num() && i < 4; ++i)
			{
				if (Cands[i].P == Settler.Pos) { Out = Cands[i].P; return true; }
				TArray<FHexCoord> Path;
				if (CivRules::FindPath(M, S, Settler, Cands[i].P, Path)) { Out = Cands[i].P; return true; }
			}
			return false;
		}

		void SettlerTurn(const FHexMap& M, FCivState& S, int32 UnitId, TArray<FString>* Log)
		{
			FUnit* U = CivRules::FindUnit(S, UnitId);
			if (!U) return;
			FHexCoord Site;
			if (!BestCitySite(M, S, *U, Site))
			{
				if (CivRules::CanFoundCity(M, S, *U)) CivRules::FoundCity(M, S, UnitId);
				return;
			}
			if (U->Pos != Site) CivRules::OrderMove(M, S, UnitId, Site);
			U = CivRules::FindUnit(S, UnitId);
			if (U && U->Pos == Site && CivRules::CanFoundCity(M, S, *U)) CivRules::FoundCity(M, S, UnitId);
		}
	}

	// ===== Walka =====
	bool AttackAuto(const FHexMap& M, FCivState& S, int32 UnitId, const FHexCoord& Target, TArray<FString>* Log)
	{
		FUnit* U = CivRules::FindUnit(S, UnitId);
		if (!U || U->IsCivilian() || U->IsArmyMember() || U->Pos.Distance(Target) != 1) return false;
		const int32 Attacker = U->Owner;

		int32 Enemy = -1;
		for (const FUnit& D : S.Units) if (D.Pos == Target && D.Owner != Attacker) { Enemy = D.Owner; break; }
		const FCity* City = CivRules::CityAt(S, Target);
		if (Enemy < 0 && City && City->Owner != Attacker) Enemy = City->Owner;
		if (Enemy < 0) return false;

		if (DeclareWar(S, Attacker, Enemy) && Log)
		{
			Log->Add(FString::Printf(TEXT("Cywilizacja %d wypowiada wojnę cywilizacji %d"), Attacker + 1, Enemy + 1));
		}

		const bool bDefenders = S.Units.ContainsByPredicate([&](const FUnit& D) { return D.Pos == Target && D.Owner != Attacker; });
		if (!bDefenders)
		{
			FCity* C = S.Cities.FindByPredicate([&](const FCity& X) { return X.Pos == Target && X.Owner != Attacker; });
			if (!C) return false;
			C->Owner = Attacker;
			C->Current = FProduction();
			U->Pos = Target;
			U->MovesLeft = 0;
			if (U->bCommander) for (FUnit& Mem : S.Units) if (Mem.ArmyId == U->ArmyId && Mem.IsArmyMember()) Mem.Pos = Target;
			if (Log) Log->Add(FString::Printf(TEXT("Cywilizacja %d zdobywa miasto %s"), Attacker + 1, *C->Name));
			return true;
		}

		FBattle B;
		if (!BattleRules::Setup(S, UnitId, Target, B)) return false;
		BattleRules::Begin(B);
		BattleRules::AutoResolve(B);
		TArray<FString> BattleLog;
		BattleRules::Apply(M, S, B, &BattleLog);
		if (Log)
		{
			Log->Add(FString::Printf(TEXT("Bitwa: cywilizacja %d atakuje cywilizację %d – wygrywa %s"), Attacker + 1, Enemy + 1, B.Winner == 0 ? TEXT("atakujący") : TEXT("obrońca")));
			for (const FString& L : BattleLog) if (L.StartsWith(TEXT("Zdobyto"))) Log->Add(L);
		}
		return true;
	}

	// ===== Ruch jednostek =====
	namespace
	{
		// Idz w kierunku pola docelowego (przylegajacego do wroga); zwraca true, gdy jednostka jest teraz obok celu.
		bool ApproachAndMaybeAttack(const FHexMap& M, FCivState& S, int32 UnitId, const FHexCoord& Target, TArray<FString>* Log)
		{
			FUnit* U = CivRules::FindUnit(S, UnitId);
			if (!U) return false;
			if (U->Pos.Distance(Target) != 1)
			{
				TArray<FHexCoord> Spots;
				for (int32 d = 0; d < 6; ++d)
				{
					const FHexCoord N = Target.Neighbor(d);
					if (M.Contains(N) && CivRules::CanEnter(M, S, *U, N)) Spots.Add(N);
				}
				const FHexCoord From = U->Pos;
				Spots.Sort([&](const FHexCoord& A, const FHexCoord& B)
				{
					const int32 DA = A.Distance(From), DB = B.Distance(From);
					if (DA != DB) return DA < DB;
					return A.Q != B.Q ? A.Q < B.Q : A.R < B.R;
				});
				bool bMoved = false;
				for (int32 i = 0; i < Spots.Num() && i < 3 && !bMoved; ++i) bMoved = CivRules::OrderMove(M, S, UnitId, Spots[i]);
				U = CivRules::FindUnit(S, UnitId);
				if (!U) return false;
			}
			if (U->Pos.Distance(Target) == 1 && U->MovesLeft > 0)
			{
				AttackAuto(M, S, UnitId, Target, Log);
				return true;
			}
			return false;
		}

		void OpportunisticAttack(const FHexMap& M, FCivState& S, int32 UnitId, TArray<FString>* Log)
		{
			FUnit* U = CivRules::FindUnit(S, UnitId);
			if (!U || U->MovesLeft <= 0) return;
			const int32 My = UnitOrArmyPower(S, *U);
			for (int32 d = 0; d < 6; ++d)
			{
				const FHexCoord N = U->Pos.Neighbor(d);
				int32 Enemy = -1;
				for (const FUnit& E : S.Units) if (E.Pos == N && E.Owner != U->Owner && IsAtWar(S, U->Owner, E.Owner)) { Enemy = E.Owner; break; }
				if (Enemy < 0)
				{
					const FCity* C = CivRules::CityAt(S, N);
					if (C && C->Owner != U->Owner && IsAtWar(S, U->Owner, C->Owner)) Enemy = C->Owner;
				}
				if (Enemy < 0) continue;
				if (My * 2 >= PowerAt(S, N, Enemy) * 3) { AttackAuto(M, S, UnitId, N, Log); return; }
			}
		}

		const FUnit* NearestOwn(const FCivState& S, int32 Player, const FHexCoord& From, TFunctionRef<bool(const FUnit&)> Pred)
		{
			const FUnit* Best = nullptr;
			int32 BestD = MAX_int32;
			for (const FUnit& U : S.Units)
			{
				if (U.Owner != Player || !Pred(U)) continue;
				const int32 D = From.Distance(U.Pos);
				if (D < BestD || (D == BestD && Best && U.Id < Best->Id)) { BestD = D; Best = &U; }
			}
			return Best;
		}

		void CommanderTurn(const FHexMap& M, FCivState& S, int32 Id, EDifficulty Difficulty, TArray<FString>* Log)
		{
			FUnit* U = CivRules::FindUnit(S, Id);
			if (!U) return;
			if (U->ArmyId == 0) { ArmyRules::CreateArmy(S, Id); U = CivRules::FindUnit(S, Id); if (!U) return; }
			const int32 Player = U->Owner;

			const FArmy* A = ArmyRules::FindArmy(S, U->ArmyId);
			const int32 Size = A ? A->Members.Num() + 1 : 1;
			const int32 Threshold = Difficulty == EDifficulty::Easy ? 6 : (Difficulty == EDifficulty::Normal ? 4 : 3);
			if (Size < Threshold || !HasWar(S, Player)) return;

			FHexCoord Target;
			int32 EnemyPower = 0;
			if (!FindEnemyTarget(S, Player, U->Pos, Target, EnemyPower)) return;
			const int32 Ratio = Difficulty == EDifficulty::Easy ? 16 : (Difficulty == EDifficulty::Normal ? 13 : 11);
			if (ArmyPower(S, *U) * 10 < EnemyPower * Ratio) return;
			ApproachAndMaybeAttack(M, S, Id, Target, Log);
		}

		void JoinToward(const FHexMap& M, FCivState& S, int32 Id)
		{
			FUnit* U = CivRules::FindUnit(S, Id);
			if (!U) return;
			const FUnit* Cmd = NearestOwn(S, U->Owner, U->Pos, [&](const FUnit& C)
			{
				if (!C.bCommander) return false;
				const FArmy* A = ArmyRules::FindArmy(S, C.ArmyId);
				return A && ArmyRules::CanJoin(S, *A, *U, nullptr, false);
			});
			if (!Cmd) return;
			CivRules::OrderMove(M, S, Id, Cmd->Pos);
		}

		void FighterTurn(const FHexMap& M, FCivState& S, int32 Id, TArray<FString>* Log)
		{
			FUnit* U = CivRules::FindUnit(S, Id);
			if (!U) return;
			const int32 Player = U->Owner;

			OpportunisticAttack(M, S, Id, Log);
			U = CivRules::FindUnit(S, Id);
			if (!U || U->IsArmyMember() || U->MovesLeft <= 0) return;

			// Garnizon: miasto bez obroncy ma pierwszenstwo (jesli juz nie stoimy w jednym).
			const FCity* Here = CivRules::CityAt(S, U->Pos);
			const bool bGuardingHere = Here && Here->Owner == Player && DefendersAt(S, U->Pos, Player) <= 1;
			if (bGuardingHere) return;
			const FCity* Needy = nullptr;
			int32 BestD = MAX_int32;
			for (const FCity& C : S.Cities)
			{
				if (C.Owner != Player || DefendersAt(S, C.Pos, Player) > 0) continue;
				const int32 D = U->Pos.Distance(C.Pos);
				if (D < BestD) { BestD = D; Needy = &C; }
			}
			if (Needy) { CivRules::OrderMove(M, S, Id, Needy->Pos); return; }

			JoinToward(M, S, Id);
		}

		void WorkerTurn(const FHexMap& M, FCivState& S, int32 Id)
		{
			FUnit* U = CivRules::FindUnit(S, Id);
			if (!U || U->WorkTurns > 0) return;
			if (CivRules::BestImprovementFor(M, S, U->Pos) != EImprovement::Road && CivRules::StartImprovement(M, S, Id)) return;
			// Najblizsze nieulepszone pole w obszarze wlasnych miast.
			FHexCoord Best;
			int32 BestD = MAX_int32;
			for (const FCity& C : S.Cities)
			{
				if (C.Owner != U->Owner) continue;
				TArray<FHexCoord> Cells;
				M.GetInRadius(C.Pos, 2, Cells);
				for (const FHexCoord& H : Cells)
				{
					const EImprovement Imp = CivRules::BestImprovementFor(M, S, H);
					if (Imp == EImprovement::None || Imp == EImprovement::Road) continue;
					if (S.Units.ContainsByPredicate([&](const FUnit& O) { return O.Id != Id && O.Pos == H && O.IsCivilian(); })) continue;
					const int32 D = U->Pos.Distance(H);
					if (D < BestD || (D == BestD && (H.Q < Best.Q || (H.Q == Best.Q && H.R < Best.R)))) { BestD = D; Best = H; }
				}
			}
			if (BestD == MAX_int32) return;
			CivRules::OrderMove(M, S, Id, Best);
			U = CivRules::FindUnit(S, Id);
			if (U && U->Pos == Best && CivRules::BestImprovementFor(M, S, U->Pos) != EImprovement::Road) CivRules::StartImprovement(M, S, Id);
		}

		void ScoutTurn(const FHexMap& M, FCivState& S, int32 Id)
		{
			FUnit* U = CivRules::FindUnit(S, Id);
			if (!U || !S.PlayerStates.IsValidIndex(U->Owner)) return;
			const TArray<uint8>& Vis = S.PlayerStates[U->Owner].Visibility;
			TArray<FHexCoord> Cells;
			M.GetInRadius(U->Pos, 12, Cells);
			FHexCoord Best;
			int32 BestD = MAX_int32;
			for (const FHexCoord& C : Cells)
			{
				const int32 Idx = M.IndexOf(C);
				if (!Vis.IsValidIndex(Idx) || Vis[Idx] != 0) continue;
				const FMapTile* T = M.GetTile(C);
				if (!T || !T->IsPassableLand()) continue;
				const int32 D = U->Pos.Distance(C);
				if (D < BestD || (D == BestD && (C.Q < Best.Q || (C.Q == Best.Q && C.R < Best.R)))) { BestD = D; Best = C; }
			}
			if (BestD != MAX_int32) CivRules::OrderMove(M, S, Id, Best);
		}
	}

	// ===== Cala tura =====
	void PlayTurn(const FHexMap& M, FCivState& S, int32 Player, EDifficulty Difficulty, int32 Turn, TArray<FString>* Log)
	{
		if (!S.PlayerStates.IsValidIndex(Player)) return;
		CivRules::EnsureVisibility(M, S, Player);
		FCivPlayerState& PS = S.PlayerStates[Player];

		// 0. Pokoj: odpowiadamy na propozycje; slabsze AI samo prosi o pokoj (raz na 10 tur).
		for (int32 From : TArray<int32>(PS.PeaceOffers))
		{
			if (WouldAcceptPeace(S, Player, From))
			{
				if (MakePeace(S, Player, From, Turn) && Log) Log->Add(FString::Printf(TEXT("Zawarto pokój: cywilizacje %d i %d"), Player + 1, From + 1));
			}
			else
			{
				PS.PeaceOffers.Remove(From);
				if (Log) Log->Add(FString::Printf(TEXT("Cywilizacja %d odrzuca pokój"), Player + 1));
			}
		}
		if (Turn % 10 == 0)
		{
			for (int32 Enemy : TArray<int32>(PS.AtWarWith))
			{
				if (TotalPower(S, Player) * 10 < TotalPower(S, Enemy) * 6) ProposePeace(S, Player, Enemy, Turn, Log);
			}
		}

		// 1. Wojna: po pewnym czasie agresywne AI wypowiada ja najblizszemu sasiadowi (lub celowi ze strategii).
		const int32 StartTurn = Difficulty == EDifficulty::Easy ? 60 : (Difficulty == EDifficulty::Normal ? 40 : 25);
		const int32 MinAggr = Difficulty == EDifficulty::Hard ? 40 : 50;
		if (!HasWar(S, Player) && Turn >= StartTurn && PS.AIAggression >= MinAggr)
		{
			int32 Target = -1;
			if (S.PlayerStates.IsValidIndex(PS.AIWarTarget) && PS.AIWarTarget != Player && Alive(S, PS.AIWarTarget)) Target = PS.AIWarTarget;
			else
			{
				int32 BestD = MAX_int32;
				for (const FCity& Mine : S.Cities)
				{
					if (Mine.Owner != Player) continue;
					for (const FCity& Other : S.Cities)
					{
						if (Other.Owner == Player) continue;
						const int32 D = Mine.Pos.Distance(Other.Pos);
						if (D < BestD || (D == BestD && Other.Owner < Target)) { BestD = D; Target = Other.Owner; }
					}
				}
			}
			if (Target >= 0)
			{
				const int32* Since = PS.PeaceSince.Find(Target);
				if (Since && Turn < *Since + 20) Target = -1; // swiezy pokoj: jeszcze nie
			}
			if (Target >= 0 && DeclareWar(S, Player, Target) && Log)
			{
				Log->Add(FString::Printf(TEXT("Cywilizacja %d wypowiada wojnę cywilizacji %d"), Player + 1, Target + 1));
			}
		}

		// 2. Badania: najtansze, z premia dla preferowanej sciezki.
		if (PS.Researching == ETech::None)
		{
			TArray<ETech> Av;
			CivTech::GetAvailable(PS, Av);
			ETech Best = ETech::None;
			float BestScore = 1e9f;
			for (ETech T : Av)
			{
				const FTechDef& D = CivTech::Def(T);
				const float Score = D.Cost * ((int32)D.Path == PS.AIFocusPath ? 0.6f : 1.f);
				if (Score < BestScore) { BestScore = Score; Best = T; }
			}
			if (Best != ETech::None) CivTech::SetResearch(PS, Best);
		}

		// 3. Jednostki (po id, bo ruch i walka zmieniaja liste).
		TArray<int32> Ids;
		for (const FUnit& U : S.Units) if (U.Owner == Player && !U.IsArmyMember()) Ids.Add(U.Id);
		for (int32 Id : Ids)
		{
			FUnit* U = CivRules::FindUnit(S, Id);
			if (!U || U->IsArmyMember()) continue;
			if (U->Type == EUnitType::Settler) { SettlerTurn(M, S, Id, Log); continue; }
			if (U->Type == EUnitType::Worker) { WorkerTurn(M, S, Id); continue; }
			if (U->IsCivilian()) continue;
			if (U->Type == EUnitType::Scout) { ScoutTurn(M, S, Id); continue; }
			if (U->Def().Domain != EDomain::Land) continue; // statki i lotnictwo: AI jeszcze ich nie uzywa
			if (U->MovesLeft <= 0) continue;
			if (U->Type == EUnitType::Commander)
			{
				OpportunisticAttack(M, S, Id, Log);
				if (CivRules::FindUnit(S, Id)) CommanderTurn(M, S, Id, Difficulty, Log);
				continue;
			}
			if (IsSupport(*U)) { JoinToward(M, S, Id); continue; }
			FighterTurn(M, S, Id, Log);
		}

		// 4. Produkcja w miastach bez zajecia.
		for (int32 ci = 0; ci < S.Cities.Num(); ++ci)
		{
			FCity& City = S.Cities[ci];
			if (City.Owner != Player) continue;
			if (City.Current.Kind != EProductionKind::None) continue;
			const FProduction P = ChooseProduction(M, S, City);
			if (P.Kind != EProductionKind::None) CivRules::SetProduction(M, S, City, P);
		}
	}
}
