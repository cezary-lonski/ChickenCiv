#include "Misc/AutomationTest.h"
#include "Hex/HexCoord.h"
#include "Map/MapGenerator.h"
#include "Map/CivTypes.h"
#include "Core/ChickenSave.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexNeighborsTest, "ChickenCiv.Hex.Neighbors", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHexNeighborsTest::RunTest(const FString&)
{
	const FHexCoord C(3, -2);
	for (int32 i = 0; i < 6; ++i)
	{
		TestEqual(FString::Printf(TEXT("sasiad %d w odleglosci 1"), i), C.Distance(C.Neighbor(i)), 1);
		// Sasiad w kierunku i+3 to sasiad "z przeciwka".
		TestEqual(TEXT("przeciwny kierunek wraca"), C.Neighbor(i).Neighbor(i + 3), C);
	}
	TestEqual(TEXT("odleglosc 0"), C.Distance(C), 0);
	TestEqual(TEXT("odleglosc 4"), FHexCoord(0, 0).Distance(FHexCoord(2, 2)), 4);
	TestEqual(TEXT("odleglosc 3"), FHexCoord(0, 0).Distance(FHexCoord(3, -3)), 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHexWorldRoundTripTest, "ChickenCiv.Hex.WorldRoundTrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHexWorldRoundTripTest::RunTest(const FString&)
{
	for (int32 Q = -6; Q <= 6; ++Q)
	{
		for (int32 R = -6; R <= 6; ++R)
		{
			const FHexCoord C(Q, R);
			const FVector2D P = C.ToWorld(100.f);
			TestEqual(TEXT("srodek wraca do tego samego heksa"), FHexCoord::FromWorld(P, 100.f), C);
			// Punkt blisko rogu wciaz nalezy do tego heksa (promien wpisany = 86.6).
			const FVector2D Near = P + FVector2D(0.f, 80.f);
			TestEqual(TEXT("punkt w srodku heksa"), FHexCoord::FromWorld(Near, 100.f), C);
		}
	}
	// Dwa sasiednie heksy sa oddalone o sqrt(3)*Size.
	const float D = FVector2D::Distance(FHexCoord(0, 0).ToWorld(100.f), FHexCoord(1, 0).ToWorld(100.f));
	TestTrue(TEXT("odleglosc srodkow sasiadow"), FMath::IsNearlyEqual(D, 100.f * FMath::Sqrt(3.f), 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMapGeneratorTest, "ChickenCiv.Map.Generator", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMapGeneratorTest::RunTest(const FString&)
{
	FMapSettings S;
	S.Seed = 12345;
	S.Size = EMapSize::Small;

	const TArray<FMapTile> A = FMapGenerator::Generate(S);
	const TArray<FMapTile> B = FMapGenerator::Generate(S);
	TestEqual(TEXT("liczba pol"), A.Num(), S.GetWidth() * S.GetHeight());
	TestEqual(TEXT("ta sama liczba przy tym samym ziarnie"), A.Num(), B.Num());

	bool bSame = A.Num() == B.Num();
	int32 Land = 0, Water = 0;
	TSet<FHexCoord> Seen;
	for (int32 i = 0; bSame && i < A.Num(); ++i)
	{
		if (A[i].Coord != B[i].Coord || A[i].Terrain != B[i].Terrain || A[i].Resource != B[i].Resource) bSame = false;
		Seen.Add(A[i].Coord);
		(A[i].IsWater() ? Water : Land)++;
	}
	TestTrue(TEXT("mapa deterministyczna"), bSame);
	TestEqual(TEXT("wspolrzedne unikalne"), Seen.Num(), A.Num());
	TestTrue(TEXT("jest lad"), Land > A.Num() / 10);
	TestTrue(TEXT("jest woda"), Water > A.Num() / 10);

	FMapSettings S2 = S;
	S2.Seed = 54321;
	const TArray<FMapTile> C = FMapGenerator::Generate(S2);
	bool bDiff = false;
	for (int32 i = 0; i < C.Num() && i < A.Num(); ++i) if (C[i].Terrain != A[i].Terrain) { bDiff = true; break; }
	TestTrue(TEXT("inne ziarno = inna mapa"), bDiff);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOffsetToAxialTest, "ChickenCiv.Map.OffsetToAxial", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FOffsetToAxialTest::RunTest(const FString&)
{
	// Sasiedzi w prostokacie odd-r musza byc sasiadami w osiowych.
	TestEqual(TEXT("(0,0)"), FMapGenerator::OffsetToAxial(0, 0), FHexCoord(0, 0));
	const FHexCoord A = FMapGenerator::OffsetToAxial(5, 4);
	const FHexCoord B = FMapGenerator::OffsetToAxial(6, 4);
	TestEqual(TEXT("sasiad w wierszu"), A.Distance(B), 1);
	const FHexCoord C = FMapGenerator::OffsetToAxial(5, 5);
	TestEqual(TEXT("sasiad w wierszu nizej"), A.Distance(C), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSaveJsonTest, "ChickenCiv.Save.JsonRoundTrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSaveJsonTest::RunTest(const FString&)
{
	FChickenSaveData D;
	D.Settings.Seed = 777;
	D.Settings.Size = EMapSize::Large;
	D.Settings.NumPlayers = 4;
	D.Turn = 12;
	D.CurrentPlayer = 2;
	FPlayerInfo P; P.Name = TEXT("Test"); P.Race = EChickenRace::Blue; P.bIsAI = true;
	D.Players.Add(P);

	const FString Json = FChickenSave::ToJson(D);
	FChickenSaveData Out;
	TestTrue(TEXT("parsowanie"), FChickenSave::FromJson(Json, Out));
	TestEqual(TEXT("ziarno"), Out.Settings.Seed, 777);
	TestEqual(TEXT("rozmiar"), (int32)Out.Settings.Size, (int32)EMapSize::Large);
	TestEqual(TEXT("tura"), Out.Turn, 12);
	TestEqual(TEXT("gracz"), Out.CurrentPlayer, 2);
	TestEqual(TEXT("gracze"), Out.Players.Num(), 1);
	if (Out.Players.Num() == 1)
	{
		TestEqual(TEXT("rasa"), (int32)Out.Players[0].Race, (int32)EChickenRace::Blue);
		TestTrue(TEXT("AI"), Out.Players[0].bIsAI);
	}
	return true;
}

#endif
