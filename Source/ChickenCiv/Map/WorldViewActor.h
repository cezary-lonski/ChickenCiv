#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Map/CivTypes.h"
#include "Model/CivState.h"
#include "WorldViewActor.generated.h"

class AChickenGameState;
class AHexMapActor;
class UProceduralMeshComponent;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class USkeletalMesh;
class UStaticMesh;
class UAnimSequence;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UInstancedStaticMeshComponent;

/** Dodatek przypiety do kosci kurczaka (bron, tarcza, helm...). Wartosci z Content/UI/models.json. */
struct FPropSpec
{
	FName MeshKey;
	FName Bone;
	FVector Loc = FVector::ZeroVector;
	FRotator Rot = FRotator::ZeroRotator;
	float Scale = 1.f;
};

/** Umieszczenie statycznego modelu (pojazd, element miasta). Klucz meshu: "Katalog/Nazwa" pod /Game/Models. */
struct FPlace
{
	FName MeshKey;
	FName Mat; // "Village", "Castle" lub "Tint" (material barwiony kolorem gracza z tekstura Air)
	FVector Loc = FVector::ZeroVector;
	FRotator Rot = FRotator::ZeroRotator;
	float Scale = 1.f;
	int32 MinPop = 0;
	int32 Building = -1; // wymagany budynek w miescie (EBuildingType) albo -1
};

/** Dekoracja terenu: obiekt zlozony z kilku siatek (np. drzewo = pien + korony), ustawiany na polu. */
struct FDecorItem
{
	FVector At = FVector::ZeroVector;
	TArray<FPlace> Pieces;
};

/** Model 3D miasta: zestaw statycznych siatek zalezny od wielkosci miasta. */
struct FCityVisual
{
	TArray<UStaticMeshComponent*> Comps;
	int32 Sig = -1;
};

/** Model 3D jednostki (kurczak szkieletowy z animacja albo pojazd). */
struct FUnitVisual
{
	USkeletalMeshComponent* Skel = nullptr;
	UStaticMeshComponent* Stat = nullptr;
	TArray<UStaticMeshComponent*> Props;
	UMaterialInstanceDynamic* Mid = nullptr;
	float Yaw = 0.f;
	int32 Walking = -1; // -1 jeszcze nie ustawiono, 0 postoj, 1 chodzenie
};

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

	// ---- Modele 3D (Content/Models, ustawienia w Content/UI/models.json) ----
	void LoadKit();
	bool UsesModel(EUnitType T) const;
	void UpdateVisual(const FUnit& U, const FVector& Pos, const FVector& MoveDir, bool bMoving, const FLinearColor& Col);
	bool bKitTried = false;
	bool bKitReady = false;
	UPROPERTY() TObjectPtr<USkeletalMesh> KitChicken;
	UPROPERTY() TObjectPtr<UAnimSequence> KitIdle;
	UPROPERTY() TObjectPtr<UAnimSequence> KitWalk;
	UPROPERTY() TObjectPtr<UMaterialInterface> KitTintMat;
	UPROPERTY() TMap<FName, TObjectPtr<UStaticMesh>> KitProps;
	UPROPERTY() TMap<FName, TObjectPtr<UStaticMesh>> KitMeshes;
	UPROPERTY() TObjectPtr<UMaterialInterface> KitVillageMat;
	UPROPERTY() TObjectPtr<UMaterialInterface> KitCastleMat;
	UPROPERTY() TObjectPtr<UMaterialInterface> KitStationMat;
	UPROPERTY() TObjectPtr<UMaterialInterface> KitHillMat;
	UPROPERTY() TObjectPtr<UTexture> KitAirTex;
	TMap<FName, TArray<FPropSpec>> PropTable;
	TMap<FName, FPlace> VehSpec;      // klucz: ship, shuttle, xwing, tie
	TArray<FPlace> CitySpec;
	float ChickenScale = 1.4f;
	float ChickenYaw = 0.f;
	TMap<int32, FUnitVisual> Visuals;
	TMap<int32, FCityVisual> CityVis;
	UStaticMesh* GetKitMesh(const FName& Key);
	// ---- Dekoracje terenu i ulepszen (instancje); przebudowa tylko gdy zmieni sie widocznosc/ulepszenia ----
	TMap<FName, TArray<FDecorItem>> DecorSpec; // klucz: Forest, Hills, Mountain, Farm
	UPROPERTY() TMap<FName, TObjectPtr<UInstancedStaticMeshComponent>> DecorISM;
	uint32 DecorSig = 0;
	UInstancedStaticMeshComponent* GetDecorISM(const FPlace& P);
	void RebuildDecor(const AChickenGameState& GS, const AHexMapActor& Map, const TArray<uint8>* Vis);
	UMaterialInterface* MatFor(const FName& Mat) const;
	void UpdateCityVisual(const FCity& C, const FVector& Pos, const FLinearColor& Col);

	// Plynny ruch: pozycja rysowana kazdej jednostki dazy do docelowej; widok przebudowuje sie w Tick, dopoki cos sie rusza.
	TMap<int32, FVector> DrawPos;
	TWeakObjectPtr<const AChickenGameState> LastGS;
	TWeakObjectPtr<const AHexMapActor> LastMap;
	int32 LastViewer = 0;
	double LastSyncTime = 0.0;
	bool bAnimating = false;
};
