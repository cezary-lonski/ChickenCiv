#include "Core/BattleRules.h"
#include "Core/CivRules.h"
#include "Core/ArmyRules.h"

namespace BattleRules
{
	// ===== Plansza =====
	// Plansza to prostokat w ukladzie "odd-r"; kolumna 0 po lewej (atakujacy), Width-1 po prawej (obronca).
	FHexCoord CellToHex(int32 Col, int32 Row)
	{
		return FHexCoord(Col - (Row - (Row & 1)) / 2, Row);
	}

	void HexToCell(const FHexCoord& H, int32& Col, int32& Row)
	{
		Row = H.R;
		Col = H.Q + (H.R - (H.R & 1)) / 2;
	}

	bool InBoard(const FHexCoord& H)
	{
		int32 C, R;
		HexToCell(H, C, R);
		return C >= 0 && C < FBattle::Width && R >= 0 && R < FBattle::Height;
	}

	const FBattleUnit* UnitAt(const FBattle& B, const FHexCoord& H, bool bAliveOnly)
	{
		for (const FBattleUnit& U : B.Units) if (U.Pos == H && (U.bAlive || !bAliveOnly)) return &U;
		return nullptr;
	}

	const FBattleUnit* CorpseAt(const FBattle& B, const FHexCoord& H, int32 Side)
	{
		for (const FBattleUnit& U : B.Units) if (!U.bAlive && U.Side == Side && U.Pos == H) return &U;
		return nullptr;
	}

	int32 CountAlive(const FBattle& B, int32 Side)
	{
		int32 N = 0;
		for (const FBattleUnit& U : B.Units) if (U.bAlive && U.Side == Side) ++N;
		return N;
	}

	int32 Distance(const FBattleUnit& A, const FBattleUnit& T) { return A.Pos.Distance(T.Pos); }

	// ===== Przygotowanie =====
	static int32 RangeOf(EUnitType T)
	{
		switch (T)
		{
		case EUnitType::Archer:
		case EUnitType::LaserGunslinger:
		case EUnitType::XWing:
		case EUnitType::TIE: return 3;
		case EUnitType::AcidThrower: return 2;
		case EUnitType::MeteorDroid: return 4;
		default: return 1;
		}
	}

	static FBattleUnit MakeUnit(const FUnit& U, int32 Side, EChickenRace Race, bool bCityBonus)
	{
		const FUnitDef& D = U.Def();
		FBattleUnit B;
		B.MapUnitId = U.Id;
		B.Side = Side;
		B.Type = U.Type;
		B.MaxHp = D.bCivilian ? 30 : 100;
		B.Hp = FMath::Clamp(U.Hp, 1, B.MaxHp);
		int32 Str = D.Strength * CivRaces::Get(Race).StrengthMult;
		if (bCityBonus) Str = Str * 3 / 2;
		B.Strength = Str;
		B.Defense = Str;
		B.Moves = D.Moves;
		B.Range = RangeOf(U.Type);
		B.bFlying = D.Domain == EDomain::Air;
		B.bCommander = U.bCommander;
		B.bDeputy = U.Type == EUnitType::Deputy;
		B.bSpecial = D.bSpecial;
		B.bCivilian = D.bCivilian;
		B.Initiative = D.Moves * 10 + Str;
		return B;
	}

	static void PlaceSide(FBattle& B, int32 Side)
	{
		static const int32 RowOrder[FBattle::Height] = { 4, 3, 5, 2, 6, 1, 7, 0, 8 };
		// Kolejnosc: wojownicy z przodu, dowodca w srodku, specjalni z tylu.
		TArray<int32> Idx;
		for (const FBattleUnit& U : B.Units) if (U.Side == Side && !U.bSpecial && !U.bCommander) Idx.Add(U.Id);
		for (const FBattleUnit& U : B.Units) if (U.Side == Side && U.bCommander) Idx.Add(U.Id);
		for (const FBattleUnit& U : B.Units) if (U.Side == Side && U.bSpecial && !U.bCommander) Idx.Add(U.Id);

		const int32 Cols[3] = { Side == 0 ? 2 : FBattle::Width - 3, Side == 0 ? 1 : FBattle::Width - 2, Side == 0 ? 0 : FBattle::Width - 1 };
		int32 N = 0;
		for (int32 Id : Idx)
		{
			if (N >= 3 * FBattle::Height) { B.Units[Id].bAlive = false; continue; } // brak miejsca (nie powinno sie zdarzyc: max 21)
			const int32 Col = Cols[N / FBattle::Height];
			const int32 Row = RowOrder[N % FBattle::Height];
			B.Units[Id].Pos = CellToHex(Col, Row);
			++N;
		}
	}

	bool Setup(const FCivState& S, int32 AttackerUnitId, const FHexCoord& Target, FBattle& Out, FString* Why)
	{
		auto Fail = [&](const TCHAR* Msg) { if (Why) *Why = Msg; return false; };
		const FUnit* Att = CivRules::FindUnit(S, AttackerUnitId);
		if (!Att) return Fail(TEXT("Brak atakującego"));
		if (Att->IsArmyMember()) return Fail(TEXT("Atakuje dowódca, nie członek armii"));
		if (Att->IsCivilian()) return Fail(TEXT("Cywile nie atakują"));

		Out = FBattle();
		Out.MapTarget = Target;
		Out.AttackerMapUnitId = AttackerUnitId;
		Out.Owner[0] = Att->Owner;

		// Obroncy: wszystko na polu docelowym nalezace do kogos innego.
		TArray<const FUnit*> Defenders;
		for (const FUnit& U : S.Units) if (U.Pos == Target && U.Owner != Att->Owner) Defenders.Add(&U);
		if (Defenders.Num() == 0) return Fail(TEXT("Na polu nie ma obrońców"));
		Out.Owner[1] = Defenders[0]->Owner;

		auto RaceOf = [&](int32 Owner) { return S.PlayerStates.IsValidIndex(Owner) ? S.PlayerStates[Owner].Race : EChickenRace::Red; };
		Out.Race[0] = RaceOf(Out.Owner[0]);
		Out.Race[1] = RaceOf(Out.Owner[1]);
		for (int32 s = 0; s < 2; ++s)
		{
			if (S.PlayerStates.IsValidIndex(Out.Owner[s])) Out.bResurrectUsed[s] = S.PlayerStates[Out.Owner[s]].bResurrectUsed;
			Out.bDeathStar[s] = S.Cities.ContainsByPredicate([&](const FCity& C) { return C.Owner == Out.Owner[s] && C.Has(EBuildingType::DeathStar); });
		}
		const FCity* City = CivRules::CityAt(S, Target);
		Out.bCityDefense = City && City->Owner == Out.Owner[1];

		// Atakujacy: jednostka lub cala armia dowodcy.
		TArray<const FUnit*> Attackers;
		Attackers.Add(Att);
		if (Att->bCommander)
		{
			if (const FArmy* A = ArmyRules::FindArmy(S, Att->ArmyId))
			{
				for (int32 Id : A->Members) if (const FUnit* M = CivRules::FindUnit(S, Id)) Attackers.Add(M);
			}
		}
		for (const FUnit* U : Attackers) { FBattleUnit BU = MakeUnit(*U, 0, Out.Race[0], false); BU.Id = Out.Units.Num(); Out.Units.Add(BU); }
		for (const FUnit* U : Defenders) { FBattleUnit BU = MakeUnit(*U, 1, Out.Race[1], Out.bCityDefense); BU.Id = Out.Units.Num(); Out.Units.Add(BU); }
		for (const FBattleUnit& U : Out.Units) if (U.bCommander) Out.bHasCommander[U.Side] = true;
		PlaceSide(Out, 0);
		PlaceSide(Out, 1);

		// Zielony straznik w obronie miasta: niszczy cala atakujaca armie.
		const bool bGuardian = Out.bCityDefense && Out.Units.ContainsByPredicate([](const FBattleUnit& U) { return U.Side == 1 && U.Type == EUnitType::GreenGuardian; });
		if (bGuardian)
		{
			Out.bGreenWipe = true;
			int32 Killed = 0;
			for (FBattleUnit& U : Out.Units) if (U.Side == 0 && U.bAlive) { U.bAlive = false; U.Hp = 0; ++Killed; }
			Out.Nuggets[1] += Killed * FBattle::NuggetsPerKill;
			Out.Winner = 1;
			Out.Log.Add(TEXT("Zielony strażnik niszczy całą atakującą armię!"));
		}
		return true;
	}

	void Apply(const FHexMap& M, FCivState& S, const FBattle& B, TArray<FString>* Log)
	{
		// Zdrowie ocalalych i nuggetsy.
		for (const FBattleUnit& BU : B.Units)
		{
			if (BU.MapUnitId == 0 || !BU.bAlive) continue;
			if (FUnit* U = CivRules::FindUnit(S, BU.MapUnitId)) U->Hp = BU.Hp;
		}
		for (int32 s = 0; s < 2; ++s)
		{
			if (!S.PlayerStates.IsValidIndex(B.Owner[s])) continue;
			S.PlayerStates[B.Owner[s]].Nuggets += B.Nuggets[s];
			S.PlayerStates[B.Owner[s]].bResurrectUsed = B.bResurrectUsed[s];
		}

		// Polegli: najpierw zwykli i zastepcy, na koncu dowodcy (zeby zastepca mogl przejac armie).
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
			for (const FBattleUnit& BU : B.Units)
			{
				if (BU.bAlive || BU.MapUnitId == 0) continue;
				const FUnit* U = CivRules::FindUnit(S, BU.MapUnitId);
				if (!U) continue;
				const bool bWasCmd = U->bCommander;
				if ((Pass == 0) == bWasCmd) continue;
				ArmyRules::RemoveUnit(M, S, BU.MapUnitId, Log);
			}
		}

		// Zdobycie miasta.
		if (B.Winner == 0)
		{
			const bool bEmpty = !S.Units.ContainsByPredicate([&](const FUnit& U) { return U.Pos == B.MapTarget && U.Owner == B.Owner[1]; });
			FCity* City = S.Cities.FindByPredicate([&](const FCity& C) { return C.Pos == B.MapTarget && C.Owner == B.Owner[1]; });
			FUnit* Att = CivRules::FindUnit(S, B.AttackerMapUnitId);
			if (City && bEmpty && Att)
			{
				City->Owner = B.Owner[0];
				City->Current = FProduction();
				Att->Pos = B.MapTarget;
				if (Att->bCommander)
				{
					for (FUnit& Mem : S.Units) if (Mem.ArmyId == Att->ArmyId && Mem.IsArmyMember()) Mem.Pos = B.MapTarget;
				}
				if (Log) Log->Add(FString::Printf(TEXT("Zdobyto miasto %s"), *City->Name));
			}
		}
		if (FUnit* Att = CivRules::FindUnit(S, B.AttackerMapUnitId)) { Att->MovesLeft = 0; Att->Path.Reset(); }
	}

	// ===== Przebieg =====
	static void StartRound(FBattle& B)
	{
		for (FBattleUnit& U : B.Units)
		{
			U.bActed = false;
			U.bRetaliated = false;
			U.MovesLeft = U.Moves;
		}
		B.bDeathStarUsed[0] = B.bDeathStarUsed[1] = false;
		B.Order.Reset();
		for (const FBattleUnit& U : B.Units) if (U.bAlive) B.Order.Add(U.Id);
		B.Order.Sort([&](int32 A, int32 C)
		{
			const FBattleUnit& UA = B.Units[A];
			const FBattleUnit& UC = B.Units[C];
			if (UA.Initiative != UC.Initiative) return UA.Initiative > UC.Initiative;
			return A < C;
		});
		B.OrderIndex = 0;
		B.bTurnStarted = false;
	}

	static void Kill(FBattle& B, FBattleUnit& Target, int32 KillerSide)
	{
		if (!Target.bAlive) return;
		Target.bAlive = false;
		Target.Hp = 0;
		Target.AimTurns = 0;
		if (KillerSide != Target.Side) B.Nuggets[KillerSide] += FBattle::NuggetsPerKill;
		if (Target.bCommander)
		{
			// Zastepca przejmuje dowodzenie.
			for (FBattleUnit& U : B.Units) if (U.bAlive && U.Side == Target.Side && U.bDeputy && !U.bCommander) { U.bCommander = true; break; }
		}
	}

	static void CheckEnd(FBattle& B)
	{
		if (B.Winner >= 0) return;
		bool bLost[2] = { false, false };
		for (int32 s = 0; s < 2; ++s)
		{
			if (CountAlive(B, s) == 0) { bLost[s] = true; continue; }
			if (B.bHasCommander[s] && !B.Units.ContainsByPredicate([s](const FBattleUnit& U) { return U.Side == s && U.bAlive && U.bCommander; })) bLost[s] = true;
		}
		if (bLost[0]) B.Winner = 1;       // przy remisie wygrywa obronca
		else if (bLost[1]) B.Winner = 0;
	}

	static void FireBlackHole(FBattle& B, FBattleUnit& Thrower)
	{
		B.Log.Add(TEXT("Czarna dziura pochłania wszystko w promieniu 2 heksów!"));
		const FHexCoord Target = Thrower.AimTarget;
		for (FBattleUnit& U : B.Units)
		{
			if (U.bAlive && U.Pos.Distance(Target) <= FBattle::BlackHoleRadius) Kill(B, U, Thrower.Side);
		}
		Thrower.AimTurns = 0;
	}

	// Szuka nastepnej jednostki, ktora moze dzialac; obsluguje poczatek tury, nowe rundy i koniec bitwy.
	static void Settle(FBattle& B)
	{
		for (int32 Guard = 0; Guard < 10000 && B.Winner < 0; ++Guard)
		{
			if (B.OrderIndex >= B.Order.Num())
			{
				++B.Round;
				StartRound(B);
				if (B.Order.Num() == 0) { CheckEnd(B); return; }
				continue;
			}
			FBattleUnit& U = B.Units[B.Order[B.OrderIndex]];
			if (!U.bAlive || U.bCivilian || (U.Strength <= 0 && !U.bSpecial)) { ++B.OrderIndex; B.bTurnStarted = false; continue; }
			if (B.bTurnStarted) return;

			B.bTurnStarted = true;
			U.MovesLeft = U.Moves;
			if (U.AimTurns > 0)
			{
				--U.AimTurns;
				if (U.AimTurns == 0) { FireBlackHole(B, U); CheckEnd(B); }
				++B.OrderIndex; B.bTurnStarted = false; ++B.TurnSerial;
				continue;
			}
			return;
		}
	}

	void Begin(FBattle& B)
	{
		if (B.Winner >= 0) return;
		B.Round = 1;
		StartRound(B);
		CheckEnd(B);
		Settle(B);
	}

	void EndUnitTurn(FBattle& B)
	{
		if (B.Winner >= 0) return;
		++B.OrderIndex;
		B.bTurnStarted = false;
		++B.TurnSerial;
		Settle(B);
	}

	void GetReachable(const FBattle& B, const FBattleUnit& U, TArray<FHexCoord>& Out)
	{
		Out.Reset();
		TMap<FHexCoord, int32> Dist;
		TArray<FHexCoord> Queue;
		Dist.Add(U.Pos, 0);
		Queue.Add(U.Pos);
		for (int32 i = 0; i < Queue.Num(); ++i)
		{
			const FHexCoord Cur = Queue[i];
			const int32 D = Dist[Cur];
			if (D >= U.MovesLeft) continue;
			for (int32 d = 0; d < 6; ++d)
			{
				const FHexCoord N = Cur.Neighbor(d);
				if (!InBoard(N) || Dist.Contains(N)) continue;
				const bool bBlocked = UnitAt(B, N) != nullptr;
				if (bBlocked && !U.bFlying) continue;
				Dist.Add(N, D + 1);
				Queue.Add(N);
				if (!bBlocked) Out.Add(N);
			}
		}
	}

	bool Move(FBattle& B, int32 UnitId, const FHexCoord& To)
	{
		FBattleUnit* Cur = B.Current();
		if (B.Winner >= 0 || !Cur || Cur->Id != UnitId || Cur->bActed || Cur->AimTurns > 0) return false;
		TArray<FHexCoord> Reach;
		GetReachable(B, *Cur, Reach);
		if (!Reach.Contains(To)) return false;
		Cur->MovesLeft = FMath::Max(0, Cur->MovesLeft - Cur->Pos.Distance(To));
		Cur->Pos = To;
		return true;
	}

	static int32 Damage(const FBattleUnit& A, const FBattleUnit& T)
	{
		return FMath::Max(5, A.Strength * 4 - T.Defense * 2);
	}

	static void FinishAction(FBattle& B, FBattleUnit& U)
	{
		U.bActed = true;
		U.MovesLeft = 0;
		CheckEnd(B);
		if (B.Winner < 0) EndUnitTurn(B);
	}

	bool Attack(FBattle& B, int32 AttackerId, int32 TargetId)
	{
		FBattleUnit* Cur = B.Current();
		if (B.Winner >= 0 || !Cur || Cur->Id != AttackerId || Cur->bActed || !B.Units.IsValidIndex(TargetId)) return false;
		FBattleUnit& T = B.Units[TargetId];
		if (!T.bAlive || T.Side == Cur->Side) return false;
		if (Cur->Strength <= 0 || Distance(*Cur, T) > Cur->Range) return false;

		const int32 Dmg = Damage(*Cur, T);
		const int32 Side = Cur->Side;
		T.Hp -= Dmg;
		if (T.Hp <= 0) Kill(B, T, Side);
		else if (!T.bRetaliated && !T.bCivilian && T.Strength > 0 && Distance(*Cur, T) <= T.Range)
		{
			T.bRetaliated = true;
			Cur->Hp -= Damage(T, *Cur);
			if (Cur->Hp <= 0) Kill(B, *Cur, T.Side);
		}
		FinishAction(B, *Cur);
		return true;
	}

	bool Heal(FBattle& B, int32 MedicId, int32 TargetId)
	{
		FBattleUnit* Cur = B.Current();
		if (B.Winner >= 0 || !Cur || Cur->Id != MedicId || Cur->bActed || Cur->Type != EUnitType::Medic || !B.Units.IsValidIndex(TargetId)) return false;
		FBattleUnit& T = B.Units[TargetId];
		if (!T.bAlive || T.Side != Cur->Side || T.Hp >= T.MaxHp || Distance(*Cur, T) > 2) return false;
		T.Hp = FMath::Min(T.MaxHp, T.Hp + FBattle::HealAmount);
		FinishAction(B, *Cur);
		return true;
	}

	bool Resurrect(FBattle& B, int32 UnitId, int32 CorpseId)
	{
		FBattleUnit* Cur = B.Current();
		if (B.Winner >= 0 || !Cur || Cur->Id != UnitId || Cur->bActed || Cur->Type != EUnitType::Resurrector || !B.Units.IsValidIndex(CorpseId)) return false;
		const int32 Side = Cur->Side;
		if (B.bResurrectUsed[Side]) return false;
		FBattleUnit& C = B.Units[CorpseId];
		if (C.bAlive || C.Side != Side) return false;

		// Miejsce: pole zwlok, albo najblizsze wolne.
		FHexCoord At = C.Pos;
		if (UnitAt(B, At))
		{
			bool bFound = false;
			for (int32 R = 1; R <= 3 && !bFound; ++R)
			{
				for (int32 d = 0; d < 6 && !bFound; ++d)
				{
					FHexCoord P = C.Pos;
					for (int32 k = 0; k < R; ++k) P = P.Neighbor(d);
					if (InBoard(P) && !UnitAt(B, P)) { At = P; bFound = true; }
				}
			}
			if (!bFound) return false;
		}
		C.bAlive = true;
		C.Hp = C.MaxHp / 2;
		C.Pos = At;
		C.bActed = true;
		B.bResurrectUsed[Side] = true;
		B.Log.Add(TEXT("Wskrzeszono jednostkę"));
		FinishAction(B, *Cur);
		return true;
	}

	bool BlackHole(FBattle& B, int32 ThrowerId, const FHexCoord& Target)
	{
		FBattleUnit* Cur = B.Current();
		if (B.Winner >= 0 || !Cur || Cur->Id != ThrowerId || Cur->bActed || Cur->Type != EUnitType::BlackHoleThrower || Cur->AimTurns > 0) return false;
		if (!InBoard(Target)) return false;
		Cur->AimTurns = 2;
		Cur->AimTarget = Target;
		B.Log.Add(TEXT("Miotacz celuje czarną dziurą (2 tury)"));
		FinishAction(B, *Cur);
		return true;
	}

	bool DeathStarLaser(FBattle& B, int32 Side, int32 TargetId)
	{
		FBattleUnit* Cur = B.Current();
		if (B.Winner >= 0 || !Cur || Cur->Side != Side || !B.bDeathStar[Side] || B.bDeathStarUsed[Side] || !B.Units.IsValidIndex(TargetId)) return false;
		FBattleUnit& T = B.Units[TargetId];
		if (!T.bAlive || T.Side == Side) return false;
		B.bDeathStarUsed[Side] = true;
		Kill(B, T, Side);
		B.Log.Add(TEXT("Laser Gwiazdy Śmierci niszczy jednostkę"));
		CheckEnd(B);
		return true;
	}

	// ===== AI =====
	static FBattleUnit* NearestEnemy(FBattle& B, const FBattleUnit& U)
	{
		FBattleUnit* Best = nullptr;
		int32 BestScore = MAX_int32;
		for (FBattleUnit& T : B.Units)
		{
			if (!T.bAlive || T.Side == U.Side) continue;
			// Bliscy najpierw; przy remisie slabsi (mniej zdrowia).
			const int32 Score = Distance(U, T) * 1000 + T.Hp;
			if (Score < BestScore) { BestScore = Score; Best = &T; }
		}
		return Best;
	}

	void AIPlayUnit(FBattle& B)
	{
		FBattleUnit* Cur = B.Current();
		if (B.Winner >= 0 || !Cur) return;
		const int32 Id = Cur->Id;
		const int32 Serial = B.TurnSerial;
		const int32 Side = Cur->Side;

		// Laser Gwiazdy Smierci: w pierwszej kolejnosci dowodca wroga, potem najsilniejszy.
		if (B.bDeathStar[Side] && !B.bDeathStarUsed[Side])
		{
			FBattleUnit* Pick = nullptr;
			for (FBattleUnit& T : B.Units)
			{
				if (!T.bAlive || T.Side == Side || T.bCivilian) continue;
				if (!Pick || (T.bCommander && !Pick->bCommander) || (T.bCommander == Pick->bCommander && T.Strength > Pick->Strength)) Pick = &T;
			}
			if (Pick) DeathStarLaser(B, Side, Pick->Id);
			if (B.Winner >= 0) return;
		}

		FBattleUnit& U = B.Units[Id];
		auto Done = [&]() { return B.TurnSerial != Serial || B.Winner >= 0; };

		if (U.Type == EUnitType::Medic)
		{
			FBattleUnit* Best = nullptr;
			for (FBattleUnit& A : B.Units)
			{
				if (!A.bAlive || A.Side != Side || A.Hp >= A.MaxHp || Distance(U, A) > 2) continue;
				if (!Best || A.Hp * Best->MaxHp < Best->Hp * A.MaxHp) Best = &A;
			}
			if (Best && Heal(B, Id, Best->Id)) return;
		}
		if (U.Type == EUnitType::Resurrector && !B.bResurrectUsed[Side])
		{
			for (FBattleUnit& A : B.Units)
			{
				if (!A.bAlive && A.Side == Side && !A.bCivilian && !A.bCommander)
				{
					if (Resurrect(B, Id, A.Id)) return;
				}
			}
		}
		if (U.Type == EUnitType::BlackHoleThrower && U.AimTurns == 0)
		{
			FHexCoord BestHex;
			int32 BestValue = 0;
			for (const FBattleUnit& T : B.Units)
			{
				if (!T.bAlive || T.Side == Side) continue;
				int32 Value = 0;
				for (const FBattleUnit& X : B.Units)
				{
					if (!X.bAlive || X.Pos.Distance(T.Pos) > FBattle::BlackHoleRadius) continue;
					Value += (X.Side == Side) ? -3 : 1; // wlasne straty licza sie bardziej
				}
				if (Value > BestValue) { BestValue = Value; BestHex = T.Pos; }
			}
			if (BestValue >= 2 && BlackHole(B, Id, BestHex)) return;
		}

		// Atak: jesli cel w zasiegu, bij; inaczej podejdz najblizej i sprobuj jeszcze raz.
		for (int32 Attempt = 0; Attempt < 2 && !Done(); ++Attempt)
		{
			FBattleUnit* Target = NearestEnemy(B, B.Units[Id]);
			if (!Target) break;
			FBattleUnit& Me = B.Units[Id];
			if (Me.Strength > 0 && Distance(Me, *Target) <= Me.Range)
			{
				Attack(B, Id, Target->Id);
				return;
			}
			if (Attempt == 1) break;

			TArray<FHexCoord> Reach;
			GetReachable(B, Me, Reach);
			// Strzelcy zatrzymuja sie na granicy zasiegu, wojownicy podchodza wplotno.
			auto Score = [&](int32 D) { return D > Me.Range ? (D - Me.Range) * 10 : (Me.Range - D); };
			FHexCoord BestHex = Me.Pos;
			int32 BestScore = Score(Me.Pos.Distance(Target->Pos));
			for (const FHexCoord& H : Reach)
			{
				const int32 S = Score(H.Distance(Target->Pos));
				if (S < BestScore) { BestScore = S; BestHex = H; }
			}
			if (BestHex != Me.Pos) Move(B, Id, BestHex);
		}
		if (!Done()) EndUnitTurn(B);
	}

	void AutoResolve(FBattle& B, int32 MaxRounds)
	{
		if (B.Winner >= 0) return;
		if (!B.Current()) Begin(B);
		int32 Guard = 0;
		while (B.Winner < 0 && B.Round <= MaxRounds && Guard++ < 100000)
		{
			AIPlayUnit(B);
		}
		if (B.Winner < 0) B.Winner = 1; // przeciagnieta bitwa: obronca
	}
}
