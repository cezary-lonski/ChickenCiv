#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Map/CivTypes.h"
#include "WorldViewActor.generated.h"

class AChickenGameState;
class AHexMapActor;
class UProceduralMeshComponent;

/**
 * Widok jednostek, miast i surowcow. Jedna siatka proceduralna z wypalonym cieniowaniem (bez swiatel),
 * zlozona z prostych bryl: kurczaki z akcesoriami zaleznymi od typu jednostki, miasta z domkami, male znaczniki surowcow.
 * Przebudowywany po kazdej zmianie stanu.
 */
UCLASS()
class CHICKENCIV_API AWorldViewActor : public AActor
{
	GENERATED_BODY()

public:
	AWorldViewActor();
	virtual void Tick(float DeltaSeconds) override;

	/** Odtwarza siatke dla gracza Viewer (widzi swoje rzeczy oraz to, co lezy w polu widzenia). */
	void Sync(const AChickenGameState& GS, const AHexMapActor& Map, int32 Viewer);

	static FLinearColor RaceColor(EChickenRace R) { return CivColors::RaceColor(R); }

private:
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Mesh;

	// Plynny ruch: pozycja rysowana kazdej jednostki dazy do docelowej; widok przebudowuje sie w Tick, dopoki cos sie rusza.
	TMap<int32, FVector> DrawPos;
	TWeakObjectPtr<const AChickenGameState> LastGS;
	TWeakObjectPtr<const AHexMapActor> LastMap;
	int32 LastViewer = 0;
	double LastSyncTime = 0.0;
	bool bAnimating = false;
};
