#pragma once

#include "CoreMinimal.h"
#include "Map/HexMap.h"
#include "Model/CivState.h"

/** Zasady gry jako czyste funkcje na (mapa, stan). Bez aktorow, wiec latwe do testow, AI i sieci. */
namespace CivRules
{
	// ---- Wyszukiwanie ----
	FUnit* FindUnit(FCivState& S, int32 Id);
	const FUnit* FindUnit(const FCivState& S, int32 Id);
	FCity* FindCity(FCivState& S, int32 Id);
	const FCity* FindCity(const FCivState& S, int32 Id);
	const FCity* CityAt(const FCivState& S, const FHexCoord& C);
	/** Jednostka danej klasy (cywilna / wojskowa) na polu; nullptr gdy brak. */
	const FUnit* UnitAt(const FCivState& S, const FHexCoord& C, bool bCivilian);
	void UnitsAt(const FCivState& S, const FHexCoord& C, TArray<const FUnit*>& Out);

	// ---- Ruch ----
	/** Koszt wejscia na teren; 0 = nieprzechodni (woda, gory). */
	int32 MoveCost(const FMapTile& T);
	/** Punkty ruchu na ture z uwzglednieniem rasy (niebieskie statki x10). */
	int32 MaxMoves(const FCivState& S, const FUnit& U);
	bool CanEnter(const FHexMap& M, const FCivState& S, const FUnit& U, const FHexCoord& To);
	/** Najkrotsza droga (bez pola startowego). false, gdy brak drogi lub cel zajety. */
	bool FindPath(const FHexMap& M, const FCivState& S, const FUnit& U, const FHexCoord& To, TArray<FHexCoord>& OutPath);
	/** Idzie wzdluz U.Path tak daleko, jak pozwalaja punkty ruchu. */
	void AdvanceUnit(const FHexMap& M, FCivState& S, FUnit& U);
	/** Rozkaz ruchu: wyznacza droge i od razu wykonuje, ile sie da. */
	bool OrderMove(const FHexMap& M, FCivState& S, int32 UnitId, const FHexCoord& To);

	// ---- Jednostki i miasta ----
	FUnit& SpawnUnit(FCivState& S, int32 Owner, EUnitType Type, const FHexCoord& Pos);
	bool CanFoundCity(const FHexMap& M, const FCivState& S, const FUnit& U, FString* Why = nullptr);
	/** Zaklada miasto; zuzywa osadnika. Zwraca Id miasta lub INDEX_NONE. */
	int32 FoundCity(const FHexMap& M, FCivState& S, int32 UnitId, FString* Why = nullptr);

	// ---- Ulepszenia pol ----
	constexpr int32 ImprovementTurns = 3;
	/** Ulepszenie plonow (farma/kopalnia); droga jest osobna warstwa: HasRoad. */
	EImprovement ImprovementAt(const FCivState& S, const FHexCoord& P);
	bool HasRoad(const FCivState& S, const FHexCoord& P);
	/** Najlepsze ulepszenie, jakie mozna zbudowac na polu (None, gdy brak sensu). */
	EImprovement BestImprovementFor(const FHexMap& M, const FCivState& S, const FHexCoord& P);
	/** Robotnik zaczyna budowe (trwa ImprovementTurns tur, robotnik stoi w miejscu). */
	bool StartImprovement(const FHexMap& M, FCivState& S, int32 UnitId, FString* Why = nullptr);

	// ---- Plony ----
	FYield TileYield(const FMapTile& T);
	/** Raport miasta (plony, wzrost, pracujace pola). */
	FCityReport ComputeCityReport(const FHexMap& M, const FCivState& S, const FCity& City);
	/** Przychod gracza na ture (suma miast). */
	FYield ComputeIncome(const FHexMap& M, const FCivState& S, int32 Player);

	// ---- Produkcja ----
	/** Koszt jednostki z uwzglednieniem rasy (niebieskie statki 5x tansze). */
	int32 UnitCost(const FCivState& S, int32 Owner, EUnitType T);
	/** Dostepne pozycje produkcji: wg technologii, polozenia miasta (port) i cudow. */
	void GetProductionOptions(const FHexMap& M, const FCivState& S, const FCity& City, TArray<FProductionOption>& Out);
	bool SetProduction(const FHexMap& M, const FCivState& S, FCity& City, const FProduction& P);
	int32 TurnsToComplete(const FHexMap& M, const FCivState& S, const FCity& City);

	// ---- Tury ----
	void BeginPlayerTurn(const FHexMap& M, FCivState& S, int32 Player);
	/** Konczy ture gracza: miasta (wzrost, produkcja), zloto i nauka. Komunikaty trafiaja do Log. */
	void EndPlayerTurn(const FHexMap& M, FCivState& S, int32 Player, TArray<FString>& Log);

	// ---- Widocznosc ----
	void EnsureVisibility(const FHexMap& M, FCivState& S, int32 Player);
	/** Przelicza widocznosc: pola widoczne teraz = 2, dawniej widziane = 1. */
	void ComputeVisibility(const FHexMap& M, FCivState& S, int32 Player);

	// ---- Start ----
	/** Rozstawia graczy: osadnik + wojownik na polu startowym, robotnik obok. */
	bool PlaceStartingUnits(const FHexMap& M, FCivState& S, int32 NumPlayers, int32 Seed);
}
