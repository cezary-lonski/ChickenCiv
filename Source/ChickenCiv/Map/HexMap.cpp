#include "Map/HexMap.h"
#include "Map/MapGenerator.h"

void FHexMap::Build(const FMapSettings& Settings)
{
	Tiles = FMapGenerator::Generate(Settings);
	Index.Reset();
	Bounds = FBox2D(ForceInit);
	for (int32 i = 0; i < Tiles.Num(); ++i)
	{
		Index.Add(Tiles[i].Coord, i);
		Bounds += Tiles[i].Coord.ToWorld(HexSize);
	}
}

void FHexMap::GetInRadius(const FHexCoord& C, int32 R, TArray<FHexCoord>& Out) const
{
	Out.Reset();
	for (int32 DQ = -R; DQ <= R; ++DQ)
	{
		for (int32 DR = FMath::Max(-R, -DQ - R); DR <= FMath::Min(R, -DQ + R); ++DR)
		{
			const FHexCoord H(C.Q + DQ, C.R + DR);
			if (Contains(H)) Out.Add(H);
		}
	}
}

int32 FHexMap::CountLandInRadius(const FHexCoord& C, int32 R) const
{
	TArray<FHexCoord> Cells;
	GetInRadius(C, R, Cells);
	int32 N = 0;
	for (const FHexCoord& H : Cells)
	{
		const FMapTile* T = GetTile(H);
		if (T && !T->IsWater()) ++N;
	}
	return N;
}
