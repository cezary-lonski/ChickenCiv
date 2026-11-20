#pragma once

#include "CoreMinimal.h"
#include "Map/HexMap.h"
#include "Model/CivState.h"

/**
 * Bitwa taktyczna (styl HoMM) na planszy heksowej. Czysta logika bez aktorow: latwa do testow i AI.
 * Strona 0 = atakujacy, 1 = obronca. Kolejnosc ruchow wg inicjatywy, w rundach.
 */
struct FBattleUnit
{
	int32 Id = 0;          // numer w bitwie (indeks w Units)
	int32 MapUnitId = 0;   // jednostka na mapie (0 = brak)
	int32 Side = 0;
	EUnitType Type = EUnitType::Warrior;
	FHexCoord Pos;
	int32 Hp = 100;
	int32 MaxHp = 100;
	int32 Strength = 0;    // z uwzglednieniem rasy i bonusow
	int32 Defense = 0;
	int32 Moves = 2;
	int32 Range = 1;
	int32 Initiative = 0;
	bool bAlive = true;
	bool bFlying = false;
	bool bCommander = false;
	bool bDeputy = false;
	bool bSpecial = false;
	bool bCivilian = false;

	// Stan tury
	int32 MovesLeft = 0;
	bool bActed = false;      // wykonal atak/umiejetnosc
	bool bRetaliated = false; // odpowiedzial juz kontratakiem w tej rundzie

	// Miotacz czarnej dziury
	int32 AimTurns = 0;
	FHexCoord AimTarget;
};

struct FBattle
{
	static constexpr int32 Width = 11;
	static constexpr int32 Height = 9;
	static constexpr int32 NuggetsPerKill = 5;
	static constexpr int32 HealAmount = 30;
	static constexpr int32 BlackHoleRadius = 2;

	TArray<FBattleUnit> Units;
	int32 Round = 1;
	TArray<int32> Order;      // kolejnosc ruchow w biezacej rundzie (id jednostek)
	int32 OrderIndex = 0;
	int32 Winner = -1;        // -1 trwa, 0 atakujacy, 1 obronca

	int32 Owner[2] = { 0, 0 };
	EChickenRace Race[2] = { EChickenRace::Red, EChickenRace::Red };
	bool bDeathStar[2] = { false, false };
	bool bDeathStarUsed[2] = { false, false }; // w tej rundzie
	bool bResurrectUsed[2] = { false, false };
	bool bHasCommander[2] = { false, false };
	bool bCityDefense = false;
	bool bGreenWipe = false;
	int32 Nuggets[2] = { 0, 0 };
	FHexCoord MapTarget; // pole na mapie, na ktore uderzono
	int32 AttackerMapUnitId = 0; // jednostka (lub dowodca), ktora zaatakowala
	bool bTurnStarted = false;   // czy tura biezacej jednostki juz sie zaczela
	int32 TurnSerial = 0;        // rosnie z kazda zakonczona tura (dla AI i UI)
	TArray<FString> Log;

	const FBattleUnit* Current() const { return Order.IsValidIndex(OrderIndex) ? &Units[Order[OrderIndex]] : nullptr; }
	FBattleUnit* Current() { return Order.IsValidIndex(OrderIndex) ? &Units[Order[OrderIndex]] : nullptr; }
};

namespace BattleRules
{
	// ---- Plansza ----
	FHexCoord CellToHex(int32 Col, int32 Row);
	void HexToCell(const FHexCoord& H, int32& Col, int32& Row);
	bool InBoard(const FHexCoord& H);
	const FBattleUnit* UnitAt(const FBattle& B, const FHexCoord& H, bool bAliveOnly = true);
	/** Polegly sojusznik na polu (do wskrzeszenia). */
	const FBattleUnit* CorpseAt(const FBattle& B, const FHexCoord& H, int32 Side);

	// ---- Przygotowanie ----
	/**
	 * Tworzy bitwe: atakujacy (jednostka lub armia pod dowodztwem AttackerUnitId) kontra wszystko na polu Target.
	 * Zielony straznik w obronie miasta niszczy cala atakujaca armie (bGreenWipe).
	 */
	bool Setup(const FCivState& S, int32 AttackerUnitId, const FHexCoord& Target, FBattle& Out, FString* Why = nullptr);
	/** Przenosi wynik na mape: straty, rozpad armii, nuggetsy, zdobycie miasta. */
	void Apply(const FHexMap& M, FCivState& S, const FBattle& B, TArray<FString>* Log = nullptr);

	// ---- Przebieg ----
	/** Zaczyna runde i ture pierwszej jednostki. */
	void Begin(FBattle& B);
	void EndUnitTurn(FBattle& B);

	int32 Distance(const FBattleUnit& A, const FBattleUnit& T);
	void GetReachable(const FBattle& B, const FBattleUnit& U, TArray<FHexCoord>& Out);
	bool Move(FBattle& B, int32 UnitId, const FHexCoord& To);
	bool Attack(FBattle& B, int32 AttackerId, int32 TargetId);
	bool Heal(FBattle& B, int32 MedicId, int32 TargetId);
	bool Resurrect(FBattle& B, int32 UnitId, int32 CorpseId);
	/** Miotacz zaczyna celowac (2 tury), potem strzela w to pole. */
	bool BlackHole(FBattle& B, int32 ThrowerId, const FHexCoord& Target);
	/** Laser Gwiazdy Smierci: natychmiast niszczy wybrana jednostke (raz na runde na strone). */
	bool DeathStarLaser(FBattle& B, int32 Side, int32 TargetId);

	// ---- AI ----
	/** Rozgrywa cala ture biezacej jednostki (klasyczne AI) i konczy ja. */
	void AIPlayUnit(FBattle& B);
	/** Rozgrywa bitwe do konca obiema stronami AI (limit rund: wygrywa obronca). */
	void AutoResolve(FBattle& B, int32 MaxRounds = 100);

	int32 CountAlive(const FBattle& B, int32 Side);
}
