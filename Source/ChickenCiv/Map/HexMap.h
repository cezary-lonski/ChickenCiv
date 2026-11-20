#pragma once

#include "CoreMinimal.h"
#include "Map/CivTypes.h"

/** Dane mapy (bez aktorow): pola, indeks po wspolrzednych, obrys. Uzywane przez zasady gry i testy. */
struct FHexMap
{
	static constexpr float HexSize = 100.f;

	TArray<FMapTile> Tiles;
	TMap<FHexCoord, int32> Index;
	FBox2D Bounds = FBox2D(ForceInit);

	/** Generuje mape z ustawien (Seed musi byc != 0). */
	void Build(const FMapSettings& Settings);

	int32 Num() const { return Tiles.Num(); }
	int32 IndexOf(const FHexCoord& C) const { const int32* I = Index.Find(C); return I ? *I : INDEX_NONE; }
	bool Contains(const FHexCoord& C) const { return Index.Contains(C); }
	const FMapTile* GetTile(const FHexCoord& C) const { const int32* I = Index.Find(C); return I ? &Tiles[*I] : nullptr; }

	/** Ile pol ladu (nie woda) w promieniu R wokol C, lacznie z C. */
	int32 CountLandInRadius(const FHexCoord& C, int32 R) const;

	/** Wszystkie pola mapy w odleglosci <= R od C. */
	void GetInRadius(const FHexCoord& C, int32 R, TArray<FHexCoord>& Out) const;
};
