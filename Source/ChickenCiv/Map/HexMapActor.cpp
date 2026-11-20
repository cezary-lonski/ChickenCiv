#include "Map/HexMapActor.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

AHexMapActor::AHexMapActor()
{
	PrimaryActorTick.bCanEverTick = false;

	TerrainMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("TerrainMesh"));
	SetRootComponent(TerrainMesh);
	TerrainMesh->bUseAsyncCooking = false;
	TerrainMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TerrainMesh->SetCastShadow(false);

	SelectionMesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("SelectionMesh"));
	SelectionMesh->SetupAttachment(TerrainMesh);
	SelectionMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SelectionMesh->SetCastShadow(false);

	// Material rysujacy kolor wierzcholkow (bez oswietlenia), z zasobow silnika.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> VertexColorMat(TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	if (VertexColorMat.Succeeded())
	{
		TerrainMesh->SetMaterial(0, VertexColorMat.Object);
		SelectionMesh->SetMaterial(0, VertexColorMat.Object);
	}
}

float AHexMapActor::TerrainHeight(ETerrainType T)
{
	switch (T)
	{
	case ETerrainType::Ocean:     return 0.f;
	case ETerrainType::Coast:     return 0.f;
	case ETerrainType::Hills:     return 70.f;
	case ETerrainType::Mountain:  return 140.f;
	case ETerrainType::Forest:    return 25.f;
	default:                      return 20.f;
	}
}

FLinearColor AHexMapActor::TerrainColor(const FMapTile& T)
{
	switch (T.Terrain)
	{
	case ETerrainType::Ocean:     return FLinearColor(0.02f, 0.10f, 0.35f);
	case ETerrainType::Coast:     return FLinearColor(0.05f, 0.30f, 0.55f);
	case ETerrainType::Grassland: return FLinearColor(0.12f, 0.55f, 0.12f);
	case ETerrainType::Plains:    return FLinearColor(0.55f, 0.60f, 0.20f);
	case ETerrainType::Forest:    return FLinearColor(0.03f, 0.28f, 0.06f);
	case ETerrainType::Desert:    return FLinearColor(0.85f, 0.72f, 0.35f);
	case ETerrainType::Hills:     return FLinearColor(0.42f, 0.32f, 0.16f);
	case ETerrainType::Mountain:  return FLinearColor(0.55f, 0.55f, 0.58f);
	}
	return FLinearColor::Black;
}

FVector AHexMapActor::TileCenter(const FHexCoord& C) const
{
	const FVector2D P = C.ToWorld(HexSize);
	const FMapTile* T = Grid ? Grid->GetTile(C) : nullptr;
	return FVector(P.X, P.Y, T ? TerrainHeight(T->Terrain) : 0.f) + GetActorLocation();
}

void AHexMapActor::ShowMap(const FHexMap* InGrid, const TArray<uint8>* Visibility)
{
	Grid = InGrid;
	RebuildMesh(Visibility);
	SetSelection(nullptr);
}

void AHexMapActor::UpdateVisibility(const TArray<uint8>* Visibility)
{
	RebuildMesh(Visibility);
}

void AHexMapActor::RebuildMesh(const TArray<uint8>* Visibility)
{
	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	if (!Grid) { TerrainMesh->ClearAllMeshSections(); return; }
	Verts.Reserve(Grid->Num() * 8);

	const float Inset = 0.96f; // szczelina miedzy heksami = widoczna siatka
	const float S = HexSize * Inset;

	for (int32 TileIdx = 0; TileIdx < Grid->Num(); ++TileIdx)
	{
		const FMapTile& Tile = Grid->Tiles[TileIdx];
		const uint8 Vis = (Visibility && Visibility->IsValidIndex(TileIdx)) ? (*Visibility)[TileIdx] : 2;
		const FVector2D C = Tile.Coord.ToWorld(HexSize);
		const float H = Vis == 0 ? 0.f : TerrainHeight(Tile.Terrain);
		FLinearColor Col = TerrainColor(Tile);
		if (Vis == 0) Col = FLinearColor(0.f, 0.f, 0.f);
		else if (Vis == 1) Col = Col * 0.45f;

		// Gora: wachlarz od srodka.
		const int32 Base = Verts.Num();
		Verts.Add(FVector(C.X, C.Y, H));
		Normals.Add(FVector::UpVector);
		UVs.Add(FVector2D::ZeroVector);
		Colors.Add(Col);
		for (int32 i = 0; i < 6; ++i)
		{
			const FVector2D O = FHexCoord::CornerOffset(i, S);
			Verts.Add(FVector(C.X + O.X, C.Y + O.Y, H));
			Normals.Add(FVector::UpVector);
			UVs.Add(FVector2D::ZeroVector);
			Colors.Add(Col);
		}
		for (int32 i = 0; i < 6; ++i)
		{
			Tris.Add(Base);
			Tris.Add(Base + 1 + i);
			Tris.Add(Base + 1 + (i + 1) % 6);
		}

		// Boki: tylko tam, gdzie sasiad jest nizszy (lub brak sasiada).
		for (int32 i = 0; i < 6; ++i)
		{
			const FMapTile* N = Grid->GetTile(Tile.Coord.Neighbor(i));
			const int32 NIdx = N ? Grid->IndexOf(N->Coord) : INDEX_NONE;
			const uint8 NVis = (Visibility && Visibility->IsValidIndex(NIdx)) ? (*Visibility)[NIdx] : 2;
			const float NH = (N && NVis != 0) ? TerrainHeight(N->Terrain) : 0.f;
			if (H <= NH) continue;

			const FVector2D OA = FHexCoord::CornerOffset(i, S);
			const FVector2D OB = FHexCoord::CornerOffset((i + 1) % 6, S);
			const FVector A(C.X + OA.X, C.Y + OA.Y, 0.f);
			const FVector B(C.X + OB.X, C.Y + OB.Y, 0.f);
			const FVector Out = FVector((A.X + B.X) * 0.5f - C.X, (A.Y + B.Y) * 0.5f - C.Y, 0.f).GetSafeNormal();
			const FLinearColor Dark = Col * 0.55f;

			const int32 SB = Verts.Num();
			Verts.Add(FVector(A.X, A.Y, H));  // 0 A gora
			Verts.Add(FVector(B.X, B.Y, H));  // 1 B gora
			Verts.Add(FVector(B.X, B.Y, NH)); // 2 B dol
			Verts.Add(FVector(A.X, A.Y, NH)); // 3 A dol
			for (int32 k = 0; k < 4; ++k) { Normals.Add(Out); UVs.Add(FVector2D::ZeroVector); Colors.Add(Dark); }
			// Patrzac z zewnatrz zgodnie z ruchem wskazowek: B gora, A gora, A dol.
			Tris.Add(SB + 1); Tris.Add(SB + 0); Tris.Add(SB + 3);
			Tris.Add(SB + 1); Tris.Add(SB + 3); Tris.Add(SB + 2);
		}
	}

	TerrainMesh->ClearAllMeshSections();
	
	if (Verts.Num() > 0)
	{
		TerrainMesh->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, UVs, Colors, TArray<FProcMeshTangent>(), false);
	}
}

void AHexMapActor::SetSelection(const FHexCoord* C)
{
	SelectionMesh->ClearAllMeshSections();
	if (!C) return;
	const FMapTile* Tile = Grid ? Grid->GetTile(*C) : nullptr;
	if (!Tile) return;

	const FVector2D Ctr = C->ToWorld(HexSize);
	const float Z = TerrainHeight(Tile->Terrain) + 3.f;
	const float RIn = HexSize * 0.80f;
	const float ROut = HexSize * 0.93f;

	TArray<FVector> Verts;
	TArray<int32> Tris;
	TArray<FVector> Normals;
	TArray<FVector2D> UVs;
	TArray<FLinearColor> Colors;
	const FLinearColor White(1.f, 1.f, 0.2f);

	for (int32 i = 0; i < 6; ++i)
	{
		const FVector2D A0 = FHexCoord::CornerOffset(i, ROut);
		const FVector2D A1 = FHexCoord::CornerOffset((i + 1) % 6, ROut);
		const FVector2D B0 = FHexCoord::CornerOffset(i, RIn);
		const FVector2D B1 = FHexCoord::CornerOffset((i + 1) % 6, RIn);
		const int32 Base = Verts.Num();
		Verts.Add(FVector(Ctr.X + A0.X, Ctr.Y + A0.Y, Z));
		Verts.Add(FVector(Ctr.X + A1.X, Ctr.Y + A1.Y, Z));
		Verts.Add(FVector(Ctr.X + B1.X, Ctr.Y + B1.Y, Z));
		Verts.Add(FVector(Ctr.X + B0.X, Ctr.Y + B0.Y, Z));
		for (int32 k = 0; k < 4; ++k) { Normals.Add(FVector::UpVector); UVs.Add(FVector2D::ZeroVector); Colors.Add(White); }
		// Obie strony, zeby obwodka byla widoczna niezaleznie od kierunku.
		Tris.Add(Base); Tris.Add(Base + 1); Tris.Add(Base + 2);
		Tris.Add(Base); Tris.Add(Base + 2); Tris.Add(Base + 3);
		Tris.Add(Base); Tris.Add(Base + 2); Tris.Add(Base + 1);
		Tris.Add(Base); Tris.Add(Base + 3); Tris.Add(Base + 2);
	}
	SelectionMesh->CreateMeshSection_LinearColor(0, Verts, Tris, Normals, UVs, Colors, TArray<FProcMeshTangent>(), false);
}
