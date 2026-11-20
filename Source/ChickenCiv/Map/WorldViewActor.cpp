#include "Map/WorldViewActor.h"
#include "Map/HexMapActor.h"
#include "Map/MeshBuilder.h"
#include "Core/ChickenGameState.h"
#include "Core/CivRules.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Animation/AnimSequence.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

AWorldViewActor::AWorldViewActor()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	Mesh = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Units"));
	SetRootComponent(Mesh);
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Mesh->SetCastShadow(false);
	Mesh->bUseAsyncCooking = false;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> VertexColorMat(TEXT("/Engine/EngineDebugMaterials/VertexColorMaterial.VertexColorMaterial"));
	if (VertexColorMat.Succeeded()) Mesh->SetMaterial(0, VertexColorMat.Object);
}

namespace
{
	const FLinearColor Orange(0.95f, 0.55f, 0.1f);
	const FLinearColor DarkOrange(0.8f, 0.4f, 0.05f);
	const FLinearColor ComboRed(0.9f, 0.1f, 0.1f);
	const FLinearColor Steel(0.62f, 0.64f, 0.7f);
	const FLinearColor Dark(0.12f, 0.12f, 0.15f);
	const FLinearColor Wood(0.5f, 0.32f, 0.15f);
	const FLinearColor Gold(1.f, 0.82f, 0.15f);
	const FLinearColor White(0.95f, 0.95f, 0.95f);

	FLinearColor Lighter(const FLinearColor& C, float T) { return FLinearColor(FMath::Lerp(C.R, 1.f, T), FMath::Lerp(C.G, 1.f, T), FMath::Lerp(C.B, 1.f, T), 1.f); }
	FLinearColor Darker(const FLinearColor& C, float T) { return FLinearColor(C.R * (1.f - T), C.G * (1.f - T), C.B * (1.f - T), 1.f); }

	/** Podstawowy kurczak: korpus, glowa, dziob, grzebien, ogon, nogi, oczy. S = skala, Face = kierunek (+1 lub -1 po X). */
	void AddChickenBody(FMeshBuilder& B, const FVector& P, float S, float Face, const FLinearColor& Col, bool bLegs = true)
	{
		const FLinearColor Head = Lighter(Col, 0.18f);
		B.AddEllipsoid(P + FVector(0, 0, 22 * S), FVector(15 * S, 12 * S, 15 * S), Col);                 // korpus
		B.AddSphere(P + FVector(11 * S * Face, 0, 39 * S), 8 * S, Head);                                   // glowa
		B.AddCone(P + FVector(17 * S * Face, 0, 39 * S), FVector(Face, 0, -0.1), 3.5f * S, 9 * S, Orange);  // dziob
		B.AddCone(P + FVector(11 * S * Face, 0, 46 * S), FVector(0, 0, 1), 3.f * S, 8 * S, ComboRed);       // grzebien
		B.AddCone(P + FVector(-13 * S * Face, 0, 26 * S), FVector(-Face, 0, 0.8), 6.f * S, 17 * S, Darker(Col, 0.15f)); // ogon
		B.AddSphere(P + FVector(15 * S * Face, 5.5f * S, 42 * S), 1.6f * S, Dark, 3, 5);                   // oczy
		B.AddSphere(P + FVector(15 * S * Face, -5.5f * S, 42 * S), 1.6f * S, Dark, 3, 5);
		if (bLegs)
		{
			B.AddCylinder(P + FVector(0, 5 * S, 0), 1.7f * S, 9 * S, DarkOrange, 5);
			B.AddCylinder(P + FVector(0, -5 * S, 0), 1.7f * S, 9 * S, DarkOrange, 5);
		}
	}

	void AddRobot(FMeshBuilder& B, const FVector& P, float S, const FLinearColor& Col)
	{
		B.AddBox(P + FVector(0, 0, 14 * S), FVector(9 * S, 8 * S, 7 * S), Steel);               // nogi/podwozie
		B.AddBox(P + FVector(0, 0, 30 * S), FVector(12 * S, 10 * S, 11 * S), Col);              // tors
		B.AddSphere(P + FVector(0, 0, 47 * S), 7 * S, Steel);                                     // glowa
		B.AddSphere(P + FVector(5 * S, 0, 48 * S), 2.2f * S, FLinearColor(0.3f, 1.f, 1.f), 3, 5); // oko
		B.AddBar(P + FVector(0, 0, 53 * S), P + FVector(0, 0, 62 * S), 0.9f * S, Dark);           // antena
	}

	void AddUnitShape(FMeshBuilder& B, const FUnit& U, const FVector& P, const FLinearColor& Col)
	{
		const float F = 1.f;
		switch (U.Type)
		{
		case EUnitType::Settler:
			AddChickenBody(B, P, 1.f, F, Col);
			B.AddSphere(P + FVector(-11, 0, 30), 9.f, Wood);                            // tobolek
			B.AddBar(P + FVector(8, 11, 0), P + FVector(8, 11, 44), 1.4f, Wood);        // kij
			break;
		case EUnitType::Worker:
			AddChickenBody(B, P, 1.f, F, Col);
			B.AddCylinder(P + FVector(11, 0, 45), 8.f, 5.f, Gold, 8);                    // kask
			B.AddBar(P + FVector(14, -12, 12), P + FVector(14, -12, 36), 1.6f, Wood);    // mlot
			B.AddBox(P + FVector(14, -12, 38), FVector(5, 3, 3), Steel);
			break;
		case EUnitType::Warrior:
			AddChickenBody(B, P, 1.f, F, Col);
			B.AddEllipsoid(P + FVector(11, 0, 44), FVector(9, 9, 6), Steel);              // helm
			B.AddBar(P + FVector(10, 13, 14), P + FVector(10, 13, 52), 1.6f, Steel);      // miecz
			B.AddBox(P + FVector(10, 13, 20), FVector(1.5f, 5.f, 1.5f), Gold);
			break;
		case EUnitType::Archer:
			AddChickenBody(B, P, 1.f, F, Col);
			B.AddBar(P + FVector(12, 13, 14), P + FVector(12, 17, 30), 1.3f, Wood);       // luk
			B.AddBar(P + FVector(12, 17, 30), P + FVector(12, 13, 48), 1.3f, Wood);
			B.AddBar(P + FVector(12, 13, 14), P + FVector(12, 13, 48), 0.5f, White);      // cieciwa
			B.AddBox(P + FVector(-8, -12, 26), FVector(3, 3, 9), Wood);                   // kolczan
			break;
		case EUnitType::Scout:
			AddChickenBody(B, P, 0.82f, F, Col);
			B.AddCone(P + FVector(8, 0, 40), FVector(-1, 0, 1), 3.f, 18.f, Gold);         // pioro
			break;
		case EUnitType::Commander:
			AddChickenBody(B, P, 1.18f, F, Col);
			B.AddCylinder(P + FVector(13, 0, 50), 6.5f, 5.f, Gold, 8);                    // korona
			for (int32 k = 0; k < 4; ++k) B.AddCone(P + FVector(13 + 6.f * FMath::Cos(k * HALF_PI), 6.f * FMath::Sin(k * HALF_PI), 55), FVector(0, 0, 1), 1.8f, 6.f, Gold, 5);
			B.AddBox(P + FVector(-14, 0, 24), FVector(2, 14, 14), ComboRed);              // peleryna
			break;
		case EUnitType::Medic:
			AddChickenBody(B, P, 1.f, F, Col);
			B.AddBox(P + FVector(14, 0, 24), FVector(1.5f, 7, 2), White);                 // krzyz
			B.AddBox(P + FVector(14, 0, 24), FVector(1.5f, 2, 7), White);
			B.AddCylinder(P + FVector(11, 0, 45), 6.f, 4.f, White, 8);                    // czepek
			break;
		case EUnitType::Resurrector:
			AddChickenBody(B, P, 1.f, F, Col);
			B.AddCone(P + FVector(11, 0, 45), FVector(0, 0, 1), 7.f, 20.f, FLinearColor(0.55f, 0.2f, 0.9f)); // kapelusz
			B.AddSphere(P + FVector(16, 8, 30), 4.f, FLinearColor(0.7f, 0.4f, 1.f), 4, 6);                    // magiczna kula
			break;
		case EUnitType::Deputy:
			AddChickenBody(B, P, 1.08f, F, Col);
			B.AddSphere(P + FVector(15, 0, 28), 4.f, Steel, 4, 6);                        // odznaka
			B.AddBox(P + FVector(0, 15, 34), FVector(5, 3, 2), Steel);                    // naramiennik
			B.AddBox(P + FVector(0, -15, 34), FVector(5, 3, 2), Steel);
			break;
		case EUnitType::LaserGunslinger:
			AddChickenBody(B, P, 1.f, F, Col);
			B.AddBox(P + FVector(20, 12, 24), FVector(11, 2.5f, 3), Steel);               // pistolet
			B.AddSphere(P + FVector(32, 12, 24), 2.6f, FLinearColor(0.3f, 1.f, 1.f), 3, 5);
			break;
		case EUnitType::BlackHoleThrower:
			AddChickenBody(B, P, 1.1f, F, Col);
			B.AddBar(P + FVector(0, 0, 36), P + FVector(30, 0, 44), 4.f, Dark);           // wyrzutnia
			B.AddSphere(P + FVector(32, 0, 45), 5.5f, FLinearColor(0.35f, 0.05f, 0.5f), 4, 7);
			break;
		case EUnitType::GreenGuardian:
			AddChickenBody(B, P, 1.2f, F, Col);
			B.AddBox(P + FVector(16, 0, 26), FVector(3, 13, 15), FLinearColor(0.1f, 0.55f, 0.15f)); // tarcza
			B.AddCone(P + FVector(19, 0, 26), FVector(1, 0, 0), 4.f, 8.f, Gold, 6);
			break;
		case EUnitType::Droid: AddRobot(B, P, 1.f, Col); break;
		case EUnitType::AcidThrower:
			AddRobot(B, P, 1.f, Darker(Col, 0.2f));
			B.AddCylinder(P + FVector(-9, 0, 26), 6.f, 16.f, FLinearColor(0.5f, 1.f, 0.2f), 8); // zbiornik z kwasem
			B.AddBar(P + FVector(10, 0, 32), P + FVector(26, 0, 32), 2.f, Dark);                // dysza
			break;
		case EUnitType::MeteorDroid:
			AddRobot(B, P, 1.15f, Col);
			B.AddBar(P + FVector(0, 0, 50), P + FVector(18, 0, 74), 4.f, Dark);                 // wyrzutnia
			B.AddSphere(P + FVector(20, 0, 77), 6.f, FLinearColor(1.f, 0.4f, 0.1f), 4, 7);      // meteoryt
			break;
		case EUnitType::Ship:
			B.AddBox(P + FVector(0, 0, 8), FVector(32, 14, 8), Darker(Col, 0.25f));           // kadlub
			B.AddCone(P + FVector(32, 0, 8), FVector(1, 0, 0), 12.f, 16.f, Darker(Col, 0.25f), 6); // dziob
			B.AddBar(P + FVector(0, 0, 14), P + FVector(0, 0, 60), 1.8f, Wood);               // maszt
			B.AddBox(P + FVector(0, 0, 40), FVector(1, 14, 16), White);                       // zagiel
			B.AddBox(P + FVector(0, 0, 62), FVector(2, 5, 3), Col);                           // flaga
			break;
		case EUnitType::SpaceShuttle:
			B.AddEllipsoid(P + FVector(0, 0, 70), FVector(26, 9, 9), White);
			B.AddBox(P + FVector(-6, 0, 70), FVector(10, 28, 1.5f), Col);                     // skrzydla
			B.AddBox(P + FVector(-20, 0, 78), FVector(5, 1.5f, 8), Col);                      // statecznik
			break;
		case EUnitType::XWing:
			B.AddEllipsoid(P + FVector(0, 0, 70), FVector(22, 5, 5), Col);
			B.AddBox(P + FVector(-4, 18, 76), FVector(10, 16, 1), Steel);                     // skrzydla w X
			B.AddBox(P + FVector(-4, -18, 76), FVector(10, 16, 1), Steel);
			B.AddBox(P + FVector(-4, 18, 64), FVector(10, 16, 1), Steel);
			B.AddBox(P + FVector(-4, -18, 64), FVector(10, 16, 1), Steel);
			break;
		case EUnitType::TIE:
			B.AddSphere(P + FVector(0, 0, 70), 8.f, Dark);
			B.AddBox(P + FVector(0, 15, 70), FVector(1.5f, 1.5f, 15), Col);                   // dwa panele
			B.AddBox(P + FVector(0, -15, 70), FVector(1.5f, 1.5f, 15), Col);
			break;
		}
	}

	void AddCityShape(FMeshBuilder& B, const FVector& P, const FLinearColor& Col, int32 Pop)
	{
		B.AddCylinder(P, 40.f, 6.f, Darker(Col, 0.35f), 12);                                  // podstawa
		const int32 Houses = FMath::Clamp(1 + Pop / 2, 1, 5);
		static const FVector2D Spots[5] = { FVector2D(-18, -14), FVector2D(-18, 16), FVector2D(14, -22), FVector2D(14, 24), FVector2D(-34, 0) };
		for (int32 i = 0; i < Houses; ++i)
		{
			const FVector H = P + FVector(Spots[i].X, Spots[i].Y, 6.f);
			B.AddBox(H + FVector(0, 0, 9), FVector(8, 8, 9), Lighter(Col, 0.25f), 12.f * i);
			B.AddCone(H + FVector(0, 0, 18), FVector(0, 0, 1), 11.f, 11.f, Darker(Col, 0.1f), 4);
		}
		B.AddBox(P + FVector(20, 0, 28), FVector(7, 7, 22), Lighter(Col, 0.1f));              // wieza
		B.AddCone(P + FVector(20, 0, 50), FVector(0, 0, 1), 10.f, 14.f, Col, 6);
		B.AddBar(P + FVector(20, 0, 64), P + FVector(20, 0, 80), 1.2f, Dark);                 // maszt z flaga
		B.AddBox(P + FVector(20, 8, 76), FVector(1, 8, 4), Gold);
	}

	void AddResourceShape(FMeshBuilder& B, const FVector& P, EResourceType R)
	{
		switch (R)
		{
		case EResourceType::Wheat:
			for (int32 k = 0; k < 4; ++k) B.AddCone(P + FVector(-3.f * k, 3.f * (k % 2), 0), FVector(0.1f * (k - 1.5f), 0, 1), 2.8f, 22.f, Gold, 5);
			break;
		case EResourceType::Cattle:
			B.AddBox(P + FVector(0, 0, 11), FVector(10, 6, 5), Wood);
			B.AddSphere(P + FVector(11, 0, 14), 4.f, Wood, 3, 6);
			B.AddBar(P + FVector(-6, 4, 0), P + FVector(-6, 4, 7), 1.3f, Dark);
			B.AddBar(P + FVector(6, -4, 0), P + FVector(6, -4, 7), 1.3f, Dark);
			break;
		case EResourceType::Fish:
			B.AddEllipsoid(P + FVector(0, 0, 8), FVector(11, 4, 4), FLinearColor(0.2f, 0.9f, 1.f), 4, 7);
			B.AddCone(P + FVector(-10, 0, 8), FVector(-1, 0, 0), 4.f, 7.f, FLinearColor(0.2f, 0.7f, 1.f), 5);
			break;
		case EResourceType::Horses:
			B.AddBox(P + FVector(0, 0, 13), FVector(11, 5, 6), FLinearColor(0.85f, 0.65f, 0.4f));
			B.AddBar(P + FVector(9, 0, 15), P + FVector(14, 0, 26), 2.f, FLinearColor(0.85f, 0.65f, 0.4f));
			B.AddBar(P + FVector(-7, 3, 0), P + FVector(-7, 3, 9), 1.2f, Dark);
			B.AddBar(P + FVector(7, -3, 0), P + FVector(7, -3, 9), 1.2f, Dark);
			break;
		case EResourceType::Iron:
			B.AddEllipsoid(P + FVector(0, 0, 7), FVector(10, 8, 7), FLinearColor(0.3f, 0.3f, 0.36f), 3, 6);
			B.AddEllipsoid(P + FVector(9, 5, 4), FVector(6, 5, 4), FLinearColor(0.38f, 0.28f, 0.28f), 3, 6);
			break;
		case EResourceType::Stone:
			B.AddBox(P + FVector(0, 0, 6), FVector(9, 7, 6), FLinearColor(0.72f, 0.72f, 0.72f), 20.f);
			B.AddBox(P + FVector(9, 6, 4), FVector(5, 4, 4), FLinearColor(0.6f, 0.6f, 0.6f), -15.f);
			break;
		case EResourceType::Gold:
			B.AddSphere(P + FVector(0, 0, 6), 7.f, Gold, 4, 7);
			B.AddSphere(P + FVector(9, 5, 4), 4.5f, Gold, 3, 6);
			B.AddSphere(P + FVector(-7, 6, 3), 4.f, Gold, 3, 6);
			break;
		default: break;
		}
	}
}

// ===== Modele 3D =====
namespace
{
	const TCHAR* TypeKey(EUnitType T)
	{
		switch (T)
		{
		case EUnitType::Settler: return TEXT("Settler");
		case EUnitType::Worker: return TEXT("Worker");
		case EUnitType::Warrior: return TEXT("Warrior");
		case EUnitType::Archer: return TEXT("Archer");
		case EUnitType::Scout: return TEXT("Scout");
		case EUnitType::Commander: return TEXT("Commander");
		case EUnitType::Medic: return TEXT("Medic");
		case EUnitType::Resurrector: return TEXT("Resurrector");
		case EUnitType::Deputy: return TEXT("Deputy");
		case EUnitType::LaserGunslinger: return TEXT("LaserGunslinger");
		case EUnitType::BlackHoleThrower: return TEXT("BlackHoleThrower");
		case EUnitType::GreenGuardian: return TEXT("GreenGuardian");
		default: return TEXT("");
		}
	}

	const TCHAR* VehicleKey(EUnitType T)
	{
		switch (T)
		{
		case EUnitType::Ship: return TEXT("ship");
		case EUnitType::SpaceShuttle: return TEXT("shuttle");
		case EUnitType::XWing: return TEXT("xwing");
		case EUnitType::TIE: return TEXT("tie");
		default: return TEXT("");
		}
	}

	FVector ReadVec(const TSharedPtr<FJsonObject>& O, const TCHAR* Field, const FVector& Def)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O->TryGetArrayField(Field, A) && A->Num() >= 3) return FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
		return Def;
	}

	FPlace ReadPlace(const TSharedPtr<FJsonObject>& O)
	{
		FPlace P;
		FString M, Mat;
		O->TryGetStringField(TEXT("mesh"), M);
		O->TryGetStringField(TEXT("mat"), Mat);
		P.MeshKey = FName(*M);
		P.Mat = FName(*Mat);
		P.Loc = ReadVec(O, TEXT("loc"), FVector::ZeroVector);
		const FVector R = ReadVec(O, TEXT("rot"), FVector::ZeroVector);
		P.Rot = FRotator(R.X, R.Y, R.Z);
		double V = 1.0;
		O->TryGetNumberField(TEXT("scale"), V);
		P.Scale = (float)V;
		V = 0.0;
		O->TryGetNumberField(TEXT("minPop"), V);
		P.MinPop = (int32)V;
		FString B;
		if (O->TryGetStringField(TEXT("building"), B))
		{
			if (B == TEXT("SpaceStation")) P.Building = (int32)EBuildingType::SpaceStation;
			else if (B == TEXT("ParticleAccelerator")) P.Building = (int32)EBuildingType::ParticleAccelerator;
			else if (B == TEXT("Teleporter")) P.Building = (int32)EBuildingType::Teleporter;
			else if (B == TEXT("DeathStar")) P.Building = (int32)EBuildingType::DeathStar;
		}
		return P;
	}
}

UStaticMesh* AWorldViewActor::GetKitMesh(const FName& Key)
{
	if (TObjectPtr<UStaticMesh>* Found = KitMeshes.Find(Key)) return Found->Get();
	const FString K = Key.ToString();
	FString Name = K;
	const int32 Slash = K.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
	if (Slash != INDEX_NONE) Name = K.Mid(Slash + 1);
	UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Models/%s.%s"), *K, *Name));
	KitMeshes.Add(Key, SM);
	return SM;
}

UMaterialInterface* AWorldViewActor::MatFor(const FName& Mat) const
{
	if (Mat == FName(TEXT("Village"))) return KitVillageMat;
	if (Mat == FName(TEXT("Castle"))) return KitCastleMat;
	if (Mat == FName(TEXT("Station"))) return KitStationMat;
	if (Mat == FName(TEXT("Hill"))) return KitHillMat;
	return nullptr;
}

void AWorldViewActor::LoadKit()
{
	bKitTried = true;
	KitChicken = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Models/Chicken_001.Chicken_001"));
	KitIdle = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Models/Chicken_001Chicken_001_idle.Chicken_001Chicken_001_idle"));
	KitWalk = LoadObject<UAnimSequence>(nullptr, TEXT("/Game/Models/Chicken_001Chicken_003_walk.Chicken_001Chicken_003_walk"));
	KitTintMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Models/M_ChickenTint.M_ChickenTint"));
	KitVillageMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Models/MI_Village.MI_Village"));
	KitCastleMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Models/MI_Castle.MI_Castle"));
	KitStationMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Models/MI_Station.MI_Station"));
	KitHillMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Models/MI_Hill.MI_Hill"));
	KitAirTex = LoadObject<UTexture>(nullptr, TEXT("/Game/Models/Air/T_SparrowWhite.T_SparrowWhite"));
	bKitReady = KitChicken && KitIdle && KitWalk && KitTintMat;
	const bool bNoModels = FParse::Param(FCommandLine::Get(), TEXT("NoModels"));
	if (bNoModels) bKitReady = false;

	// Ustawienia z Content/UI/models.json (edytowalne bez kompilacji).
	FString Txt;
	TSharedPtr<FJsonObject> Root;
	if (!bNoModels && FFileHelper::LoadFileToString(Txt, *(FPaths::ProjectContentDir() / TEXT("UI/models.json"))) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Txt), Root) && Root.IsValid())
	{
		double V = 0.0;
		if (Root->TryGetNumberField(TEXT("chickenScale"), V)) ChickenScale = (float)V;
		if (Root->TryGetNumberField(TEXT("chickenYaw"), V)) ChickenYaw = (float)V;
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (Root->TryGetObjectField(TEXT("vehicles"), Obj))
		{
			for (const auto& Pair : (*Obj)->Values)
			{
				const TSharedPtr<FJsonObject>* O = nullptr;
				if (Pair.Value->TryGetObject(O)) { VehSpec.Add(FName(*Pair.Key), ReadPlace(*O)); }
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* CityArr = nullptr;
		if (Root->TryGetArrayField(TEXT("city"), CityArr))
		{
			for (const TSharedPtr<FJsonValue>& Item : *CityArr)
			{
				const TSharedPtr<FJsonObject>* O = nullptr;
				if (Item->TryGetObject(O)) CitySpec.Add(ReadPlace(*O));
			}
		}
		if (Root->TryGetObjectField(TEXT("decor"), Obj))
		{
			for (const auto& Pair : (*Obj)->Values)
			{
				const TArray<TSharedPtr<FJsonValue>>* Items = nullptr;
				if (!Pair.Value->TryGetArray(Items)) continue;
				TArray<FDecorItem>& Out = DecorSpec.FindOrAdd(FName(*Pair.Key));
				for (const TSharedPtr<FJsonValue>& It : *Items)
				{
					const TSharedPtr<FJsonObject>* IO = nullptr;
					if (!It->TryGetObject(IO)) continue;
					FDecorItem Item;
					Item.At = ReadVec(*IO, TEXT("at"), FVector::ZeroVector);
					const TArray<TSharedPtr<FJsonValue>>* Pcs = nullptr;
					if ((*IO)->TryGetArrayField(TEXT("pieces"), Pcs))
					{
						for (const TSharedPtr<FJsonValue>& PV : *Pcs)
						{
							const TSharedPtr<FJsonObject>* PO = nullptr;
							if (PV->TryGetObject(PO)) Item.Pieces.Add(ReadPlace(*PO));
						}
					}
					Out.Add(Item);
				}
			}
		}
		if (Root->TryGetObjectField(TEXT("types"), Obj))
		{
			for (const auto& Pair : (*Obj)->Values)
			{
				const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
				if (!Pair.Value->TryGetArray(Arr)) continue;
				TArray<FPropSpec>& Specs = PropTable.FindOrAdd(FName(*Pair.Key));
				for (const TSharedPtr<FJsonValue>& Item : *Arr)
				{
					const TSharedPtr<FJsonObject>* O = nullptr;
					if (!Item->TryGetObject(O)) continue;
					FPropSpec S;
					FString MeshName, Bone;
					(*O)->TryGetStringField(TEXT("mesh"), MeshName);
					(*O)->TryGetStringField(TEXT("bone"), Bone);
					S.MeshKey = FName(*MeshName);
					S.Bone = FName(*Bone);
					S.Loc = ReadVec(*O, TEXT("loc"), FVector::ZeroVector);
					const FVector Rv = ReadVec(*O, TEXT("rot"), FVector::ZeroVector);
					S.Rot = FRotator(Rv.X, Rv.Y, Rv.Z); // pitch, yaw, roll
					double Sc = 1.0;
					(*O)->TryGetNumberField(TEXT("scale"), Sc);
					S.Scale = (float)Sc;
					Specs.Add(S);
					if (!KitProps.Contains(S.MeshKey))
					{
						UStaticMesh* SM = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Models/Weapons/%s.%s"), *MeshName, *MeshName));
						if (SM) KitProps.Add(S.MeshKey, SM);
					}
				}
			}
		}
	}
	UE_LOG(LogTemp, Display, TEXT("MODELS kit ready=%d vehicles=%d cityParts=%d props=%d"), bKitReady ? 1 : 0, VehSpec.Num(), CitySpec.Num(), KitProps.Num());
}

UInstancedStaticMeshComponent* AWorldViewActor::GetDecorISM(const FPlace& P)
{
	const FName Key(*(P.MeshKey.ToString() + TEXT("|") + P.Mat.ToString()));
	if (TObjectPtr<UInstancedStaticMeshComponent>* F = DecorISM.Find(Key)) return F->Get();
	UStaticMesh* SM = GetKitMesh(P.MeshKey);
	if (!SM) return nullptr;
	UInstancedStaticMeshComponent* C = NewObject<UInstancedStaticMeshComponent>(this);
	C->SetStaticMesh(SM);
	C->SetupAttachment(GetRootComponent());
	C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	C->SetCastShadow(false);
	C->RegisterComponent();
	if (UMaterialInterface* M = MatFor(P.Mat)) for (int32 k = 0; k < C->GetNumMaterials(); ++k) C->SetMaterial(k, M);
	DecorISM.Add(Key, C);
	return C;
}

void AWorldViewActor::RebuildDecor(const AChickenGameState& GS, const AHexMapActor& Map, const TArray<uint8>* Vis)
{
	for (auto& Pair : DecorISM) if (Pair.Value) Pair.Value->ClearInstances();
	auto Place = [&](const FName& Key, const FHexCoord& Coord)
	{
		const TArray<FDecorItem>* Items = DecorSpec.Find(Key);
		if (!Items) return;
		const FVector Center = Map.TileCenter(Coord);
		const float Yaw = (float)(((Coord.Q * 73856093) ^ (Coord.R * 19349663)) & 1023) * (360.f / 1024.f);
		const FRotator YawRot(0.f, Yaw, 0.f);
		for (const FDecorItem& Item : *Items)
		{
			for (const FPlace& P : Item.Pieces)
			{
				UInstancedStaticMeshComponent* ISM = GetDecorISM(P);
				if (!ISM) continue;
				const FVector Loc = Center + YawRot.RotateVector(Item.At + P.Loc);
				ISM->AddInstance(FTransform(FRotator(P.Rot.Pitch, P.Rot.Yaw + Yaw, P.Rot.Roll), Loc, FVector(P.Scale)), true);
			}
		}
	};
	const FHexMap& Grid = GS.Grid;
	for (int32 i = 0; i < Grid.Tiles.Num(); ++i)
	{
		const FMapTile& T = Grid.Tiles[i];
		if (Vis && Vis->IsValidIndex(i) && (*Vis)[i] == 0) continue;
		switch (T.Terrain)
		{
		case ETerrainType::Forest: Place(TEXT("Forest"), T.Coord); break;
		case ETerrainType::Hills: Place(TEXT("Hills"), T.Coord); break;
		case ETerrainType::Mountain: Place(TEXT("Mountain"), T.Coord); break;
		default: break;
		}
	}
	for (const FImprovement& I : GS.Civ.Improvements)
	{
		const int32 Idx = Grid.IndexOf(I.Pos);
		if (Vis && Vis->IsValidIndex(Idx) && (*Vis)[Idx] == 0) continue;
		if (I.Type == EImprovement::Farm) Place(TEXT("Farm"), I.Pos);
	}
}

bool AWorldViewActor::UsesModel(EUnitType T) const
{
	if (VehicleKey(T)[0] != 0) return bKitReady && VehSpec.Contains(FName(VehicleKey(T)));
	return bKitReady && TypeKey(T)[0] != 0;
}

void AWorldViewActor::UpdateVisual(const FUnit& U, const FVector& Pos, const FVector& MoveDir, bool bMoving, const FLinearColor& Col)
{
	const FPlace* Veh = VehSpec.Find(FName(VehicleKey(U.Type)));
	FUnitVisual& V = Visuals.FindOrAdd(U.Id);
	if (!V.Skel && !V.Stat)
	{
		if (Veh)
		{
			V.Stat = NewObject<UStaticMeshComponent>(this);
			V.Stat->SetStaticMesh(GetKitMesh(Veh->MeshKey));
			V.Stat->SetupAttachment(GetRootComponent());
			V.Stat->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			V.Stat->RegisterComponent();
			if (Veh->Mat == FName(TEXT("Tint")) && KitTintMat)
			{
				V.Mid = UMaterialInstanceDynamic::Create(KitTintMat, this);
				if (KitAirTex) V.Mid->SetTextureParameterValue(TEXT("BaseTex"), KitAirTex);
				for (int32 i = 0; i < V.Stat->GetNumMaterials(); ++i) V.Stat->SetMaterial(i, V.Mid);
			}
			else if (UMaterialInterface* M = MatFor(Veh->Mat)) for (int32 i = 0; i < V.Stat->GetNumMaterials(); ++i) V.Stat->SetMaterial(i, M);
		}
		else
		{
			V.Skel = NewObject<USkeletalMeshComponent>(this);
			V.Skel->SetSkeletalMesh(KitChicken);
			V.Skel->SetupAttachment(GetRootComponent());
			V.Skel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			V.Skel->SetAnimationMode(EAnimationMode::AnimationSingleNode);
			V.Skel->RegisterComponent();
			V.Mid = UMaterialInstanceDynamic::Create(KitTintMat, this);
			V.Skel->SetMaterial(0, V.Mid);
			if (const TArray<FPropSpec>* Specs = PropTable.Find(FName(TypeKey(U.Type))))
			{
				for (const FPropSpec& S : *Specs)
				{
					TObjectPtr<UStaticMesh>* Found = KitProps.Find(S.MeshKey);
					if (!Found) continue;
					UStaticMeshComponent* P = NewObject<UStaticMeshComponent>(this);
					P->SetStaticMesh(Found->Get());
					P->SetCollisionEnabled(ECollisionEnabled::NoCollision);
					P->SetupAttachment(V.Skel, S.Bone);
					P->SetRelativeLocationAndRotation(S.Loc, S.Rot);
					P->SetRelativeScale3D(FVector(S.Scale));
					P->RegisterComponent();
					V.Props.Add(P);
				}
			}
		}
	}

	if (bMoving && MoveDir.SizeSquared2D() > 1.0) V.Yaw = MoveDir.Rotation().Yaw;
	if (V.Mid) V.Mid->SetVectorParameterValue(TEXT("Tint"), Col);
	if (V.Skel)
	{
		V.Skel->SetWorldLocationAndRotation(Pos, FRotator(0.f, V.Yaw + ChickenYaw, 0.f));
		V.Skel->SetWorldScale3D(FVector(ChickenScale * (U.Type == EUnitType::Scout ? 0.82f : (U.Type == EUnitType::Commander ? 1.18f : 1.f))));
		const int32 W = bMoving ? 1 : 0;
		if (V.Walking != W)
		{
			V.Walking = W;
			V.Skel->PlayAnimation(W ? KitWalk.Get() : KitIdle.Get(), true);
		}
	}
	else if (V.Stat && Veh)
	{
		V.Stat->SetWorldLocationAndRotation(Pos + Veh->Loc, FRotator(Veh->Rot.Pitch, V.Yaw + Veh->Rot.Yaw, Veh->Rot.Roll));
		V.Stat->SetWorldScale3D(FVector(Veh->Scale));
	}
}

void AWorldViewActor::UpdateCityVisual(const FCity& C, const FVector& Pos, const FLinearColor& Col)
{
	FCityVisual& CV = CityVis.FindOrAdd(C.Id);
	// Sygnatura: ktore elementy sa aktywne przy tej wielkosci miasta.
	int32 Sig = 0;
	auto Active = [&](const FPlace& P) { return C.Population >= P.MinPop && (P.Building < 0 || C.Buildings.Contains((EBuildingType)P.Building)); };
	for (int32 i = 0; i < CitySpec.Num(); ++i) if (Active(CitySpec[i])) Sig |= (1 << i);
	if (CV.Sig != Sig)
	{
		for (UStaticMeshComponent* Comp : CV.Comps) if (Comp) Comp->DestroyComponent();
		CV.Comps.Reset();
		CV.Sig = Sig;
		for (int32 i = 0; i < CitySpec.Num(); ++i)
		{
			if (!(Sig & (1 << i))) continue;
			UStaticMesh* SM = GetKitMesh(CitySpec[i].MeshKey);
			if (!SM) continue;
			UStaticMeshComponent* Comp = NewObject<UStaticMeshComponent>(this);
			Comp->SetStaticMesh(SM);
			Comp->SetupAttachment(GetRootComponent());
			Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			Comp->RegisterComponent();
			if (UMaterialInterface* M = MatFor(CitySpec[i].Mat)) for (int32 k = 0; k < Comp->GetNumMaterials(); ++k) Comp->SetMaterial(k, M);
			CV.Comps.Add(Comp);
		}
	}
	int32 Idx = 0;
	for (int32 i = 0; i < CitySpec.Num(); ++i)
	{
		if (!(Sig & (1 << i))) continue;
		if (!CV.Comps.IsValidIndex(Idx)) break;
		UStaticMeshComponent* Comp = CV.Comps[Idx++];
		if (!Comp) continue;
		Comp->SetWorldLocationAndRotation(Pos + CitySpec[i].Loc, CitySpec[i].Rot);
		Comp->SetWorldScale3D(FVector(CitySpec[i].Scale));
	}
}

void AWorldViewActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const AChickenGameState* GS = LastGS.Get();
	const AHexMapActor* Map = LastMap.Get();
	if (!bAnimating || !GS || !Map) { SetActorTickEnabled(false); return; }
	Sync(*GS, *Map, LastViewer);
}

void AWorldViewActor::Sync(const AChickenGameState& GS, const AHexMapActor& Map, int32 Viewer)
{
	LastGS = &GS;
	LastMap = &Map;
	LastViewer = Viewer;
	if (!bKitTried) LoadKit();
	const double Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	const float Dt = (float)FMath::Clamp(Now - LastSyncTime, 0.0, 0.05);
	LastSyncTime = Now;
	bool bMoving = false;
	TSet<int32> Seen;
	const FHexMap& Grid = GS.Grid;
	const TArray<uint8>* Vis = nullptr;
	if (const FCivPlayerState* PS = GS.GetPlayerState(Viewer)) Vis = &PS->Visibility;
	auto VisAt = [&](const FHexCoord& C) -> uint8
	{
		const int32 I = Grid.IndexOf(C);
		return (Vis && Vis->IsValidIndex(I)) ? (*Vis)[I] : 2;
	};
	auto OwnerColor = [&](int32 PlayerIdx)
	{
		return GS.Players.IsValidIndex(PlayerIdx) ? CivColors::ForPlayer(GS.Players[PlayerIdx]) : CivColors::RaceColor(EChickenRace::Red);
	};

	// Dekoracje terenu (las, wzgorza, gory, farmy): przebudowa tylko przy zmianie widocznosci lub ulepszen.
	if (bKitReady && DecorSpec.Num() > 0)
	{
		uint32 Sig = (uint32)GS.Civ.Improvements.Num() * 131u + (uint32)Grid.Tiles.Num();
		for (int32 i = 0; i < Grid.Tiles.Num(); ++i) Sig = Sig * 31u + (uint32)((Vis && Vis->IsValidIndex(i)) ? (*Vis)[i] : 2);
		for (const FImprovement& I : GS.Civ.Improvements) Sig = Sig * 17u + (uint32)(I.Pos.Q * 7 + I.Pos.R * 13 + (int32)I.Type);
		if (Sig != DecorSig) { DecorSig = Sig; RebuildDecor(GS, Map, Vis); }
	}

	FMeshBuilder B;

	// Surowce na odkrytych polach.
	for (const FMapTile& T : Grid.Tiles)
	{
		if (T.Resource == EResourceType::None || VisAt(T.Coord) == 0) continue;
		AddResourceShape(B, Map.TileCenter(T.Coord) + FVector(-34.f, 28.f, 4.f), T.Resource);
	}

	// Ulepszenia pol (na odkrytych polach).
	for (const FImprovement& I : GS.Civ.Improvements)
	{
		if (VisAt(I.Pos) == 0) continue;
		const FVector C = Map.TileCenter(I.Pos);
		if (I.Type == EImprovement::Road)
		{
			B.AddBox(C + FVector(0, 0, 1.5f), FVector(46, 5, 1.2f), FLinearColor(0.35f, 0.3f, 0.25f));
			B.AddBox(C + FVector(0, 0, 1.5f), FVector(5, 40, 1.2f), FLinearColor(0.35f, 0.3f, 0.25f));
		}
		else if (I.Type == EImprovement::Farm)
		{
			for (int32 k = -1; k <= 1; ++k) B.AddBox(C + FVector(0, k * 22.f, 3.f), FVector(30, 5, 2), FLinearColor(0.45f, 0.3f, 0.12f));
		}
		else if (I.Type == EImprovement::Mine)
		{
			B.AddBox(C + FVector(-26, -26, 10), FVector(7, 7, 10), Wood);
			B.AddBar(C + FVector(-26, -26, 22), C + FVector(-16, -26, 30), 2.f, Dark);
			B.AddSphere(C + FVector(-14, -22, 5), 5.f, Dark, 3, 6);
		}
	}

	// Miasta.
	TSet<int32> CitiesSeen;
	for (const FCity& C : GS.Civ.Cities)
	{
		if (VisAt(C.Pos) == 0) continue;
		if (bKitReady && CitySpec.Num() > 0)
		{
			CitiesSeen.Add(C.Id);
			const FVector CP = Map.TileCenter(C.Pos) + FVector(0, 0, 2.f);
			B.AddCylinder(CP, 40.f, 4.f, Darker(OwnerColor(C.Owner), 0.35f), 12);                              // podstawa w kolorze gracza
			B.AddBar(CP + FVector(30, 30, 0), CP + FVector(30, 30, 70), 1.2f, Dark);                           // maszt z flaga
			B.AddBox(CP + FVector(30, 40, 62), FVector(1, 10, 6), OwnerColor(C.Owner));
			UpdateCityVisual(C, CP, OwnerColor(C.Owner));
			if (C.Buildings.Contains(EBuildingType::DeathStar))
			{
				const FVector DS = CP + FVector(0, 0, 175.f);                                           // Gwiazda Smierci nad miastem
				B.AddSphere(DS, 40.f, FLinearColor(0.55f, 0.57f, 0.62f), 10, 16);
				B.AddCylinder(DS - FVector(0, 0, 1.5f), 40.6f, 3.f, FLinearColor(0.12f, 0.13f, 0.16f), 24); // rownikowy rowek
				B.AddSphere(DS + FVector(22, -18, 16), 11.f, FLinearColor(0.2f, 0.22f, 0.26f), 6, 10);      // czasza superlasera
				B.AddSphere(DS + FVector(27, -22, 20), 3.f, FLinearColor(0.3f, 1.f, 0.3f), 3, 6);
			}
		}
		else AddCityShape(B, Map.TileCenter(C.Pos) + FVector(0, 0, 2.f), OwnerColor(C.Owner), C.Population);
	}

	// Jednostki: swoje zawsze, cudze tylko na polach widocznych teraz; czlonkowie armii niewidoczni.
	for (const FUnit& U : GS.Civ.Units)
	{
		if (U.Owner != Viewer && VisAt(U.Pos) != 2) continue;
		if (U.IsArmyMember()) continue;
		FVector P = Map.TileCenter(U.Pos);
		// Dwie jednostki na polu (cywil + wojskowy) stoja obok siebie; w miescie odsuniete od budynkow.
		const bool bInCity = CivRules::CityAt(GS.Civ, U.Pos) != nullptr;
		P.X += U.IsCivilian() ? -24.f : 24.f;
		P.Y += U.IsCivilian() ? -24.f : 24.f;
		if (bInCity) P.Y += U.IsCivilian() ? -14.f : 14.f;
		if (U.Def().Domain == EDomain::Sea) { P.X -= 24.f; P.Y -= 24.f; }
		// Plynny ruch do docelowego pola (duze skoki, np. nowa gra, bez animacji).
		const FVector Goal = P;
		bool bUnitMoving = false;
		{
			Seen.Add(U.Id);
			FVector& D = DrawPos.FindOrAdd(U.Id, P);
			if (FVector::Dist2D(D, P) > 700.0) D = P;
			else if (!D.Equals(P, 0.5))
			{
				const FVector Delta = P - D;
				const double Step = 650.0 * Dt;
				D = (Delta.Size() <= Step) ? P : D + Delta.GetSafeNormal() * Step;
				bMoving = true;
			}
			bUnitMoving = !D.Equals(Goal, 0.5);
			P = D;
		}
		if (UsesModel(U.Type)) UpdateVisual(U, P + FVector(0, 0, 4.f), Goal - P, bUnitMoving, OwnerColor(U.Owner));
		else AddUnitShape(B, U, P + FVector(0, 0, 4.f), OwnerColor(U.Owner));
		if (U.WorkTurns > 0) B.AddSphere(P + FVector(0, 0, 66.f), 4.f, Gold, 3, 6); // znacznik pracy
		if (U.bCommander)
		{
			// Znacznik armii: chorągiewka w kolorze gracza ze zlotymi paskami (wielkosc armii).
			const FArmy* A = GS.Civ.Armies.FindByPredicate([&](const FArmy& X) { return X.Id == U.ArmyId; });
			const int32 Size = A ? A->Members.Num() : 0;
			B.AddBar(P + FVector(-8, 0, 60), P + FVector(-8, 0, 84), 1.2f, Dark);
			B.AddBox(P + FVector(-8, 7, 78), FVector(1, 7, 5), OwnerColor(U.Owner));
			for (int32 k = 0; k < FMath::Min(3, 1 + Size / 7); ++k) B.AddBox(P + FVector(-8, 7, 82 - 4 * k), FVector(1.2f, 7.5f, 0.9f), Gold);
		}
	}

	for (auto It = DrawPos.CreateIterator(); It; ++It) if (!Seen.Contains(It.Key())) It.RemoveCurrent();
	for (auto It = CityVis.CreateIterator(); It; ++It)
	{
		if (CitiesSeen.Contains(It.Key())) continue;
		for (UStaticMeshComponent* Comp : It.Value().Comps) if (Comp) Comp->DestroyComponent();
		It.RemoveCurrent();
	}
	for (auto It = Visuals.CreateIterator(); It; ++It)
	{
		if (Seen.Contains(It.Key())) continue;
		for (UStaticMeshComponent* P : It.Value().Props) if (P) P->DestroyComponent();
		if (It.Value().Skel) It.Value().Skel->DestroyComponent();
		if (It.Value().Stat) It.Value().Stat->DestroyComponent();
		It.RemoveCurrent();
	}
	{
		static int32 LogN = 0;
		if (LogN++ < 3)
		{
			int32 NV = 0; for (const FMapTile& T : Grid.Tiles) if (VisAt(T.Coord) != 0) ++NV;
			UE_LOG(LogTemp, Display, TEXT("SYNCDBG viewer=%d visibleTiles=%d tiles=%d units=%d seen=%d visuals=%d vis=%d"), Viewer, NV, Grid.Tiles.Num(), GS.Civ.Units.Num(), Seen.Num(), Visuals.Num(), Vis ? Vis->Num() : -1);
		}
	}
	bAnimating = bMoving;
	SetActorTickEnabled(bMoving);

	if (B.IsEmpty())
	{
		Mesh->ClearAllMeshSections();
		return;
	}
	Mesh->CreateMeshSection_LinearColor(0, B.Verts, B.Tris, B.Normals, B.UVs, B.Colors, TArray<FProcMeshTangent>(), false);
}
