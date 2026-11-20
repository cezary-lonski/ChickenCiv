#pragma once

#include "CoreMinimal.h"
#include "Map/HexMap.h"
#include "Model/CivState.h"

/**
 * Warstwa strategiczna AI przez LLM (domyslnie Claude, API Anthropic). LLM nie steruje pojedynczymi
 * jednostkami (to robi klasyczne AI w CivAI), tylko co kilkanascie tur ustawia strategie:
 * agresje, liczbe miast, kierunek badan i cel wojny. Bez klucza gra dziala na samym klasycznym AI.
 *
 * Klucz API: zmienna srodowiskowa ANTHROPIC_API_KEY albo lokalny plik uzytkownika (GameUserSettings.ini).
 * Nigdy nie trafia do plikow zapisu gry. Do modelu wysylane sa tylko dane o stanie gry.
 */
namespace LlmStrategy
{
	constexpr int32 RequestEveryTurns = 10;

	FString GetApiKey();
	bool HasApiKey();
	/** Zapisuje klucz w lokalnych ustawieniach uzytkownika (puste = usun). */
	void SetApiKey(const FString& Key);
	FString GetModel();

	/** Zwiezle podsumowanie sytuacji gracza dla modelu. */
	FString BuildPrompt(const FHexMap& M, const FCivState& S, const TArray<FPlayerInfo>& Players, int32 Player, int32 Turn);

	/** Wyciaga JSON z odpowiedzi modelu i ustawia strategie gracza (z ograniczeniem wartosci). false, gdy nieczytelna. */
	bool ApplyReply(const FString& ReplyText, FCivState& S, int32 Player);

	/** Asynchroniczne zapytanie; Callback(bSukces, tekst odpowiedzi) wywolany w watku gry. */
	void RequestStrategy(const FString& Prompt, TFunction<void(bool, const FString&)> Callback);
}
