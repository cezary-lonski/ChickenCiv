#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Map/CivTypes.h"
#include "Map/HexMap.h"
#include "HexMapActor.generated.h"

class UProceduralMeshComponent;

/** Rysuje mape heksow jako jeden mesh proceduralny (kolor wierzcholkow = teren, mgla wojny) i obwodke zaznaczenia. */
UCLASS()
class CHICKENCIV_API AHexMapActor : public AActor
{
	GENERATED_BODY()

public:
	AHexMapActor();

	static constexpr float HexSize = FHexMap::HexSize;

	/** Rysuje mape. Visibility (opcjonalnie): 0 nieznane, 1 odkryte, 2 widoczne; nullptr = wszystko widoczne. */
	void ShowMap(const FHexMap* InGrid, const TArray<uint8>* Visibility);

	/** Odswieza tylko kolory (mgla wojny) po zmianie widocznosci. */
	void UpdateVisibility(const TArray<uint8>* Visibility);

	static float TerrainHeight(ETerrainType T);
	static FLinearColor TerrainColor(const FMapTile& T);

	/** Srodek pola w swiecie (z wysokoscia). */
	FVector TileCenter(const FHexCoord& C) const;

	FBox2D GetBounds() const { return Grid ? Grid->Bounds : FBox2D(ForceInit); }

	/** Zaznacza pole (obwodka); pusty wskaznik = brak zaznaczenia. */
	void SetSelection(const FHexCoord* C);

private:
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> TerrainMesh;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> SelectionMesh;

	const FHexMap* Grid = nullptr;

	void RebuildMesh(const TArray<uint8>* Visibility);
};
