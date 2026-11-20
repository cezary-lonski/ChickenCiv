#include "Map/MapGenerator.h"
#include "Math/RandomStream.h"

int32 FMapGenerator::RandomSeed()
{
	int32 S = FMath::RandRange(1, 2000000000);
	return S == 0 ? 1 : S;
}

namespace
{
	// Szum fraktalny w zakresie ok. -1..1.
	float Fbm(float X, float Y, float OffX, float OffY, int32 Octaves)
	{
		float Sum = 0.f, Amp = 1.f, Norm = 0.f, Freq = 1.f;
		for (int32 i = 0; i < Octaves; ++i)
		{
			Sum += Amp * FMath::PerlinNoise2D(FVector2D(X * Freq + OffX, Y * Freq + OffY));
			Norm += Amp;
			Amp *= 0.5f;
			Freq *= 2.f;
		}
		return Sum / Norm;
	}

	EResourceType RollResource(ETerrainType T, FRandomStream& Rng)
	{
		if (Rng.FRand() > 0.12f) return EResourceType::None;
		const float R = Rng.FRand();
		switch (T)
		{
		case ETerrainType::Coast:     return EResourceType::Fish;
		case ETerrainType::Plains:    return R < 0.5f ? EResourceType::Wheat : EResourceType::Horses;
		case ETerrainType::Grassland: return R < 0.6f ? EResourceType::Cattle : EResourceType::Wheat;
		case ETerrainType::Hills:     return R < 0.4f ? EResourceType::Iron : (R < 0.8f ? EResourceType::Stone : EResourceType::Gold);
		case ETerrainType::Mountain:  return R < 0.5f ? EResourceType::Stone : EResourceType::Gold;
		case ETerrainType::Desert:    return R < 0.3f ? EResourceType::Gold : EResourceType::None;
		default:                      return EResourceType::None;
		}
	}
}

TArray<FMapTile> FMapGenerator::Generate(const FMapSettings& Settings)
{
	const int32 W = Settings.GetWidth();
	const int32 H = Settings.GetHeight();

	FRandomStream Rng(Settings.Seed);
	const float OffX = Rng.FRandRange(-1000.f, 1000.f);
	const float OffY = Rng.FRandRange(-1000.f, 1000.f);
	const float MoistX = Rng.FRandRange(-1000.f, 1000.f);
	const float MoistY = Rng.FRandRange(-1000.f, 1000.f);

	TArray<FMapTile> Tiles;
	Tiles.Reserve(W * H);

	for (int32 Row = 0; Row < H; ++Row)
	{
		for (int32 Col = 0; Col < W; ++Col)
		{
			FMapTile Tile;
			Tile.Coord = OffsetToAxial(Col, Row);

			// Wysokosc: szum minus opadanie ku krawedziom, zeby brzeg mapy byl morzem.
			const float NX = (float)Col / (float)W;
			const float NY = (float)Row / (float)H;
			const float EdgeDist = FMath::Min(FMath::Min(NX, 1.f - NX), FMath::Min(NY, 1.f - NY)); // 0..0.5
			const float Falloff = FMath::Clamp(1.f - EdgeDist / 0.18f, 0.f, 1.f);

			float E = Fbm(Col * 0.07f, Row * 0.07f, OffX, OffY, 4) * 1.5f + 0.12f - Falloff * 0.9f;
			const float M = Fbm(Col * 0.09f, Row * 0.09f, MoistX, MoistY, 3) * 1.5f;
			Tile.Elevation = E;

			if (E < -0.30f)      Tile.Terrain = ETerrainType::Ocean;
			else if (E < -0.08f) Tile.Terrain = ETerrainType::Coast;
			else if (E > 0.62f)  Tile.Terrain = ETerrainType::Mountain;
			else if (E > 0.40f)  Tile.Terrain = ETerrainType::Hills;
			else if (M < -0.28f) Tile.Terrain = ETerrainType::Desert;
			else if (M < 0.05f)  Tile.Terrain = ETerrainType::Plains;
			else if (M < 0.32f)  Tile.Terrain = ETerrainType::Grassland;
			else                 Tile.Terrain = ETerrainType::Forest;

			Tile.Resource = RollResource(Tile.Terrain, Rng);
			Tiles.Add(Tile);
		}
	}
	return Tiles;
}
