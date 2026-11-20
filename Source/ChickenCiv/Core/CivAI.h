#pragma once

#include "CoreMinimal.h"
#include "Map/HexMap.h"
#include "Model/CivState.h"

/**
 * Klasyczne AI cywilizacji: badania, produkcja, osadnicy, armie, wojna.
 * Czyste funkcje na (mapa, stan): bez aktorow, wiec deterministyczne i testowalne.
 * Strategie wyzszego poziomu (osobowosc, cel wojny, kierunek badan) siedza w FCivPlayerState
 * i moga byc nadpisywane przez warstwe LLM (LlmStrategy) bez zmiany taktyki.
 */
namespace CivAI
{
	// ---- Dyplomacja ----
	bool IsAtWar(const FCivState& S, int32 A, int32 B);
	/** Wypowiada wojne (symetrycznie). false, gdy juz trwa. */
	bool DeclareWar(FCivState& S, int32 A, int32 B);

	/** Zawiera pokoj (symetrycznie), ustawia blokade ponownej wojny. false, gdy nie bylo wojny. */
	bool MakePeace(FCivState& S, int32 A, int32 B, int32 Turn);
	/** Gracz From proponuje pokoj graczowi To; jesli To wczesniej zaproponowal pokoj Fromowi, pokoj zostaje zawarty od razu. */
	bool ProposePeace(FCivState& S, int32 From, int32 To, int32 Turn, TArray<FString>* Log = nullptr);
	/** Czy AI przyjmie pokoj: gdy jest slabsze od wroga albo malo agresywne. */
	bool WouldAcceptPeace(const FCivState& S, int32 AI, int32 Other);
	/** Laczna sila jednostek gracza (do ocen AI). */
	int32 TotalPower(const FCivState& S, int32 Player);

	/** Osobowosc AI z ziarna (deterministyczna) oraz bonus poziomu trudnosci. */
	void InitPlayer(FCivPlayerState& PS, int32 PlayerIndex, int32 Seed, EDifficulty Difficulty, bool bAI);

	/** Rozgrywa cala ture gracza AI (przed EndPlayerTurn). Wazne zdarzenia trafiaja do Log. */
	void PlayTurn(const FHexMap& M, FCivState& S, int32 Player, EDifficulty Difficulty, int32 Turn, TArray<FString>* Log = nullptr);

	/**
	 * Atak jednostki/armii na sasiednie pole rozstrzygany automatycznie (bitwa AI vs AI lub AI vs gracz).
	 * Wypowiada wojne, jesli jej nie ma. Zwraca true, gdy do ataku doszlo.
	 */
	bool AttackAuto(const FHexMap& M, FCivState& S, int32 UnitId, const FHexCoord& Target, TArray<FString>* Log = nullptr);

	/** Wybor kolejnej pozycji produkcji dla miasta wedlug priorytetow AI (Item.Kind None, gdy brak). */
	FProduction ChooseProduction(const FHexMap& M, const FCivState& S, const FCity& City);
}
