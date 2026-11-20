#pragma once

#include "CoreMinimal.h"
#include "Map/HexMap.h"
#include "Model/CivState.h"

/**
 * Armie: dowodca + do 10 jednostek zwyklych + do 10 specjalnych (w tym zastepca).
 * Czlonkowie poruszaja sie razem z dowodca i nie zajmuja pola samodzielnie.
 * Smierc dowodcy: z zastepca armia trwa dalej, bez zastepcy sie rozpada
 * (jednostki zostaja pojedynczo na roznych heksach).
 */
namespace ArmyRules
{
	FArmy* FindArmy(FCivState& S, int32 ArmyId);
	const FArmy* FindArmy(const FCivState& S, int32 ArmyId);
	/** Armia, ktorej dowodca stoi na polu (nalezaca do Owner); nullptr, gdy brak. */
	const FArmy* ArmyAt(const FCivState& S, const FHexCoord& P, int32 Owner);

	void CountMembers(const FCivState& S, const FArmy& A, int32& OutNormal, int32& OutSpecial);

	/** Dowodca zaklada armie (na razie pusta). */
	bool CreateArmy(FCivState& S, int32 CommanderUnitId, FString* Why = nullptr);
	/** Czy jednostka moze dolaczyc do armii (limity, typ, wlasciciel). bRequireSameTile: musi stac na polu dowodcy. */
	bool CanJoin(const FCivState& S, const FArmy& A, const FUnit& U, FString* Why = nullptr, bool bRequireSameTile = true);
	/** Jednostka stojaca na polu dowodcy dolacza do armii. */
	bool JoinArmy(FCivState& S, int32 ArmyId, int32 UnitId, FString* Why = nullptr);
	/** Czlonek opuszcza armie i staje na wolnym sasiednim polu. */
	bool LeaveArmy(const FHexMap& M, FCivState& S, int32 UnitId, FString* Why = nullptr);

	/**
	 * Usuwa jednostke z gry (smierc w walce), uwzgledniajac skutki dla armii.
	 * Komunikaty trafiaja do Log (moze byc nullptr).
	 */
	void RemoveUnit(const FHexMap& M, FCivState& S, int32 UnitId, TArray<FString>* Log = nullptr);

	/** Rozpad armii: dowodca juz nie zyje; czlonkowie rozchodza sie po roznych polach wokol. */
	void DisbandArmy(const FHexMap& M, FCivState& S, int32 ArmyId, const FHexCoord& Where, TArray<FString>* Log = nullptr);
}
