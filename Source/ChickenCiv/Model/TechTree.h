#pragma once

#include "CoreMinimal.h"
#include "Model/CivState.h"

/** Sciezka drzewka (do wyswietlania). */
enum class ETechPath : uint8 { Main, Ships, Lasers, Elements, Advanced };

struct FTechDef
{
	ETech Id;
	const TCHAR* Name;
	int32 Cost;          // punkty nauki
	ETechPath Path;
	ETech Req1;
	ETech Req2;
	const TCHAR* Unlocks; // opis odblokowan (do interfejsu)
};

/** Drzewko technologii i badania. Czyste funkcje na stanie gracza. */
namespace CivTech
{
	const FTechDef& Def(ETech T);
	FString PathName(ETechPath P);

	bool Knows(const FCivPlayerState& PS, ETech T);
	/** Technologia nieznana, a wszystkie wymagania poznane. */
	bool CanResearch(const FCivPlayerState& PS, ETech T);
	/** Technologie dostepne do badania, posortowane wg sciezki i kosztu. */
	void GetAvailable(const FCivPlayerState& PS, TArray<ETech>& Out);

	bool SetResearch(FCivPlayerState& PS, ETech T);
	/** Dodaje punkty nauki do badania; konczy badanie, gdy wystarczy punktow. Zwraca true, gdy poznano technologie. */
	bool AddScience(FCivPlayerState& PS, int32 Points, TArray<FString>& Log);
	/** Wybiera najtansze dostepne badanie (dla AI i automatu). */
	ETech AutoPick(const FCivPlayerState& PS);
	/** Ile tur do konca badania przy danym przyroscie nauki (999 gdy brak). */
	int32 TurnsLeft(const FCivPlayerState& PS, int32 SciencePerTurn);
}
