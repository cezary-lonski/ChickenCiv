#pragma once

#include "CoreMinimal.h"
#include "Map/CivTypes.h"

/** Deterministyczny generator mapy: to samo ziarno i rozmiar daja te same pola. */
struct FMapGenerator
{
	/** Zamienia kolumne/wiersz prostokata na wspolrzedne osiowe (uklad "odd-r"). */
	static FHexCoord OffsetToAxial(int32 Col, int32 Row)
	{
		return FHexCoord(Col - (Row - (Row & 1)) / 2, Row);
	}

	/** Generuje cala mape. Seed musi byc != 0 (0 rozwiazuje wywolujacy). */
	static TArray<FMapTile> Generate(const FMapSettings& Settings);

	/** Losuje niezerowe ziarno. */
	static int32 RandomSeed();
};
