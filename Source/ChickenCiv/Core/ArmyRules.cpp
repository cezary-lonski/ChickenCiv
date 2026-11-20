#include "Core/ArmyRules.h"
#include "Core/CivRules.h"

namespace ArmyRules
{
	FArmy* FindArmy(FCivState& S, int32 Id) { return S.Armies.FindByPredicate([Id](const FArmy& A) { return A.Id == Id; }); }
	const FArmy* FindArmy(const FCivState& S, int32 Id) { return S.Armies.FindByPredicate([Id](const FArmy& A) { return A.Id == Id; }); }

	const FArmy* ArmyAt(const FCivState& S, const FHexCoord& P, int32 Owner)
	{
		for (const FArmy& A : S.Armies)
		{
			if (A.Owner != Owner) continue;
			const FUnit* C = CivRules::FindUnit(S, A.CommanderId);
			if (C && C->Pos == P) return &A;
		}
		return nullptr;
	}

	void CountMembers(const FCivState& S, const FArmy& A, int32& OutNormal, int32& OutSpecial)
	{
		OutNormal = 0;
		OutSpecial = 0;
		for (int32 Id : A.Members)
		{
			const FUnit* U = CivRules::FindUnit(S, Id);
			if (!U) continue;
			if (U->Def().bSpecial) ++OutSpecial; else ++OutNormal;
		}
	}

	bool CreateArmy(FCivState& S, int32 CommanderUnitId, FString* Why)
	{
		auto Fail = [&](const TCHAR* Msg) { if (Why) *Why = Msg; return false; };
		FUnit* U = CivRules::FindUnit(S, CommanderUnitId);
		if (!U) return Fail(TEXT("Brak jednostki"));
		if (U->Type != EUnitType::Commander) return Fail(TEXT("Armię zakłada tylko Dowódca"));
		if (U->ArmyId != 0) return Fail(TEXT("Dowódca już prowadzi armię"));

		FArmy A;
		A.Id = S.NextId++;
		A.Owner = U->Owner;
		A.CommanderId = U->Id;
		U->ArmyId = A.Id;
		U->bCommander = true;
		S.Armies.Add(A);
		return true;
	}

	bool CanJoin(const FCivState& S, const FArmy& A, const FUnit& U, FString* Why, bool bRequireSameTile)
	{
		auto Fail = [&](const TCHAR* Msg) { if (Why) *Why = Msg; return false; };
		const FUnit* Cmd = CivRules::FindUnit(S, A.CommanderId);
		if (!Cmd) return Fail(TEXT("Armia nie ma dowódcy"));
		if (U.Owner != A.Owner) return Fail(TEXT("To nie Twoja armia"));
		if (U.ArmyId != 0) return Fail(TEXT("Jednostka jest już w armii"));
		if (U.Type == EUnitType::Commander) return Fail(TEXT("Dowódca nie dołącza do armii"));
		if (U.IsCivilian()) return Fail(TEXT("Cywile nie wstępują do armii"));
		if (U.Def().Domain != EDomain::Land) return Fail(TEXT("Do armii wchodzą tylko jednostki lądowe"));
		if (bRequireSameTile && U.Pos != Cmd->Pos) return Fail(TEXT("Jednostka musi stać na polu dowódcy"));

		int32 Normal = 0, Special = 0;
		CountMembers(S, A, Normal, Special);
		if (U.Def().bSpecial)
		{
			if (Special >= FArmy::MaxSpecial) return Fail(TEXT("Armia ma już 10 jednostek specjalnych"));
			if (U.Type == EUnitType::Deputy)
			{
				for (int32 Id : A.Members)
				{
					const FUnit* M = CivRules::FindUnit(S, Id);
					if (M && M->Type == EUnitType::Deputy) return Fail(TEXT("Armia ma już zastępcę"));
				}
			}
		}
		else if (Normal >= FArmy::MaxNormal)
		{
			return Fail(TEXT("Armia ma już 10 jednostek zwykłych"));
		}
		return true;
	}

	bool JoinArmy(FCivState& S, int32 ArmyId, int32 UnitId, FString* Why)
	{
		auto Fail = [&](const TCHAR* Msg) { if (Why) *Why = Msg; return false; };
		FArmy* A = FindArmy(S, ArmyId);
		FUnit* U = CivRules::FindUnit(S, UnitId);
		if (!A || !U) return Fail(TEXT("Brak armii lub jednostki"));
		if (!CanJoin(S, *A, *U, Why, true)) return false;

		U->ArmyId = A->Id;
		U->bCommander = false;
		U->Path.Reset();
		U->MovesLeft = 0;
		A->Members.Add(U->Id);
		return true;
	}

	// Najblizsze wolne pole dla jednostki (promien do MaxR), z pominieciem zajetych.
	static bool FindFreeTile(const FHexMap& M, const FCivState& S, const FUnit& Probe, const FHexCoord& Center, int32 MaxR, FHexCoord& Out)
	{
		TArray<FHexCoord> Cells;
		M.GetInRadius(Center, MaxR, Cells);
		Cells.Sort([&](const FHexCoord& A, const FHexCoord& B)
		{
			const int32 DA = A.Distance(Center), DB = B.Distance(Center);
			if (DA != DB) return DA < DB;
			return A.Q != B.Q ? A.Q < B.Q : A.R < B.R;
		});
		for (const FHexCoord& C : Cells)
		{
			if (CivRules::CanEnter(M, S, Probe, C)) { Out = C; return true; }
		}
		return false;
	}

	bool LeaveArmy(const FHexMap& M, FCivState& S, int32 UnitId, FString* Why)
	{
		auto Fail = [&](const TCHAR* Msg) { if (Why) *Why = Msg; return false; };
		FUnit* U = CivRules::FindUnit(S, UnitId);
		if (!U || !U->IsArmyMember()) return Fail(TEXT("Jednostka nie jest czlonkiem armii"));
		FArmy* A = FindArmy(S, U->ArmyId);

		// Stoi w tym samym miejscu co dowodca, wiec szukamy innego pola (bez pola dowodcy).
		FUnit Probe = *U;
		Probe.ArmyId = 0;
		FHexCoord At;
		bool bFound = false;
		TArray<FHexCoord> Cells;
		M.GetInRadius(U->Pos, 1, Cells);
		for (const FHexCoord& C : Cells)
		{
			if (C == U->Pos) continue;
			if (CivRules::CanEnter(M, S, Probe, C)) { At = C; bFound = true; break; }
		}
		if (!bFound) return Fail(TEXT("Brak wolnego pola obok"));

		if (A) A->Members.Remove(UnitId);
		U->ArmyId = 0;
		U->bCommander = false;
		U->Pos = At;
		U->MovesLeft = 0;
		return true;
	}

	void DisbandArmy(const FHexMap& M, FCivState& S, int32 ArmyId, const FHexCoord& Where, TArray<FString>* Log)
	{
		FArmy* A = FindArmy(S, ArmyId);
		if (!A) return;
		const TArray<int32> Members = A->Members;
		S.Armies.RemoveAll([ArmyId](const FArmy& X) { return X.Id == ArmyId; });

		// Najpierw wszyscy przestaja byc czlonkami (zwalnia to pole dowodcy), potem rozchodza sie po polach.
		for (int32 Id : Members)
		{
			// Pozycja poza mapa: nikt nie blokuje pola dowodcy ani sam siebie.
			if (FUnit* U = CivRules::FindUnit(S, Id)) { U->ArmyId = 0; U->bCommander = false; U->Pos = FHexCoord(-9999, -9999); }
		}
		int32 Lost = 0;
		for (int32 Id : Members)
		{
			FUnit* U = CivRules::FindUnit(S, Id);
			if (!U) continue;
			FHexCoord At;
			if (FindFreeTile(M, S, *U, Where, 5, At)) { U->Pos = At; U->MovesLeft = 0; U->Path.Reset(); }
			else { ++Lost; S.Units.RemoveAll([Id](const FUnit& X) { return X.Id == Id; }); }
		}
		if (Log)
		{
			Log->Add(TEXT("Armia rozpadła się po śmierci dowódcy"));
			if (Lost > 0) Log->Add(FString::Printf(TEXT("Stracono %d jednostek bez miejsca na mapie"), Lost));
		}
	}

	void RemoveUnit(const FHexMap& M, FCivState& S, int32 UnitId, TArray<FString>* Log)
	{
		FUnit* U = CivRules::FindUnit(S, UnitId);
		if (!U) return;
		const int32 ArmyId = U->ArmyId;
		const bool bWasCommander = U->bCommander;
		const FHexCoord Pos = U->Pos;

		if (ArmyId != 0 && !bWasCommander)
		{
			if (FArmy* A = FindArmy(S, ArmyId)) A->Members.Remove(UnitId);
			S.Units.RemoveAll([UnitId](const FUnit& X) { return X.Id == UnitId; });
			return;
		}
		if (ArmyId == 0 || !bWasCommander)
		{
			S.Units.RemoveAll([UnitId](const FUnit& X) { return X.Id == UnitId; });
			return;
		}

		// Smierc dowodcy.
		S.Units.RemoveAll([UnitId](const FUnit& X) { return X.Id == UnitId; });
		FArmy* A = FindArmy(S, ArmyId);
		if (!A) return;

		const int32 DeputyId = [&]() -> int32
		{
			for (int32 Id : A->Members)
			{
				const FUnit* M = CivRules::FindUnit(S, Id);
				if (M && M->Type == EUnitType::Deputy) return Id;
			}
			return 0;
		}();

		if (DeputyId != 0)
		{
			FUnit* D = CivRules::FindUnit(S, DeputyId);
			D->bCommander = true;
			A->Members.Remove(DeputyId);
			A->CommanderId = DeputyId;
			if (Log) Log->Add(TEXT("Zastępca przejął dowodzenie armią"));
			return;
		}
		DisbandArmy(M, S, ArmyId, Pos, Log);
	}
}
