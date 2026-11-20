#include "Map/WorldViewActor.h"
#include "Map/HexMapActor.h"
#include "Map/MeshBuilder.h"
#include "Core/ChickenGameState.h"
#include "Core/CivRules.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"

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
	for (const FCity& C : GS.Civ.Cities)
	{
		if (VisAt(C.Pos) == 0) continue;
		AddCityShape(B, Map.TileCenter(C.Pos) + FVector(0, 0, 2.f), OwnerColor(C.Owner), C.Population);
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
			P = D;
		}
		AddUnitShape(B, U, P + FVector(0, 0, 4.f), OwnerColor(U.Owner));
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
	bAnimating = bMoving;
	SetActorTickEnabled(bMoving);

	if (B.IsEmpty())
	{
		Mesh->ClearAllMeshSections();
		return;
	}
	Mesh->CreateMeshSection_LinearColor(0, B.Verts, B.Tris, B.Normals, B.UVs, B.Colors, TArray<FProcMeshTangent>(), false);
}
