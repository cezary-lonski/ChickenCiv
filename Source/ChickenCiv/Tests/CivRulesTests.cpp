#include "Misc/AutomationTest.h"
#include "Core/CivRules.h"
#include "Core/ChickenSave.h"
#include "Model/TechTree.h"
#include "Core/ArmyRules.h"
#include "Map/MapGenerator.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FHexMap MakeFlatMap(int32 W, int32 H, ETerrainType Terrain)
	{
		FHexMap M;
		for (int32 Row = 0; Row < H; ++Row)
		{
			for (int32 Col = 0; Col < W; ++Col)
			{
				FMapTile T;
				T.Coord = FMapGenerator::OffsetToAxial(Col, Row);
				T.Terrain = Terrain;
				M.Index.Add(T.Coord, M.Tiles.Add(T));
				M.Bounds += T.Coord.ToWorld(FHexMap::HexSize);
			}
		}
		return M;
	}

	FCivState MakeState(int32 NumPlayers = 2)
	{
		FCivState S;
		S.PlayerStates.SetNum(NumPlayers);
		for (FCivPlayerState& P : S.PlayerStates) P.Race = EChickenRace::Yellow; // neutralna dla nauki
		return S;
	}

	void SetTerrain(FHexMap& M, const FHexCoord& C, ETerrainType T)
	{
		M.Tiles[M.IndexOf(C)].Terrain = T;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCivPathTest, "ChickenCiv.Rules.Pathfinding", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCivPathTest::RunTest(const FString&)
{
	FHexMap M = MakeFlatMap(10, 10, ETerrainType::Grassland);
	FCivState S = MakeState();
	const FHexCoord Start = FMapGenerator::OffsetToAxial(1, 4);
	const FHexCoord Goal = FMapGenerator::OffsetToAxial(6, 4);
	FUnit& U = CivRules::SpawnUnit(S, 0, EUnitType::Warrior, Start);
	const int32 Id = U.Id;

	TArray<FHexCoord> Path;
	TestTrue(TEXT("jest droga"), CivRules::FindPath(M, S, *CivRules::FindUnit(S, Id), Goal, Path));
	TestEqual(TEXT("dlugosc drogi = dystans"), Path.Num(), Start.Distance(Goal));
	TestEqual(TEXT("koniec drogi"), Path.Last(), Goal);

	// Pas gor na drodze: droga musi go ominac.
	for (int32 Row = 0; Row < 9; ++Row) SetTerrain(M, FMapGenerator::OffsetToAxial(4, Row), ETerrainType::Mountain);
	TestTrue(TEXT("jest droga naokolo"), CivRules::FindPath(M, S, *CivRules::FindUnit(S, Id), Goal, Path));
	for (const FHexCoord& H : Path) TestTrue(TEXT("droga nie wchodzi w gory"), CivRules::MoveCost(*M.GetTile(H)) > 0);

	// Caly pas zablokowany.
	SetTerrain(M, FMapGenerator::OffsetToAxial(4, 9), ETerrainType::Mountain);
	TestFalse(TEXT("brak drogi przez mur"), CivRules::FindPath(M, S, *CivRules::FindUnit(S, Id), Goal, Path));

	// Cel w wodzie jest niedostepny.
	FHexMap M2 = MakeFlatMap(10, 10, ETerrainType::Grassland);
	SetTerrain(M2, Goal, ETerrainType::Ocean);
	TestFalse(TEXT("woda nieosiagalna"), CivRules::FindPath(M2, S, *CivRules::FindUnit(S, Id), Goal, Path));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCivMoveTest, "ChickenCiv.Rules.MovementOrders", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCivMoveTest::RunTest(const FString&)
{
	FHexMap M = MakeFlatMap(12, 6, ETerrainType::Grassland);
	FCivState S = MakeState();
	const FHexCoord Start = FMapGenerator::OffsetToAxial(1, 2);
	const FHexCoord Goal = FMapGenerator::OffsetToAxial(8, 2);
	const int32 Id = CivRules::SpawnUnit(S, 0, EUnitType::Warrior, Start).Id;

	TestTrue(TEXT("rozkaz przyjety"), CivRules::OrderMove(M, S, Id, Goal));
	FUnit* U = CivRules::FindUnit(S, Id);
	TestEqual(TEXT("w jednej turze 2 pola"), U->Pos.Distance(Start), 2);
	TestEqual(TEXT("ruch wyczerpany"), U->MovesLeft, 0);
	TestTrue(TEXT("zostala droga"), U->Path.Num() > 0);

	int32 Guard = 0;
	while (CivRules::FindUnit(S, Id)->Pos != Goal && Guard++ < 10)
	{
		CivRules::BeginPlayerTurn(M, S, 0);
	}
	TestEqual(TEXT("dotarl do celu"), CivRules::FindUnit(S, Id)->Pos, Goal);
	TestEqual(TEXT("droga skonczona"), CivRules::FindUnit(S, Id)->Path.Num(), 0);

	// Teren trudny kosztuje wiecej: las zabiera dwa punkty ruchu.
	FHexMap F = MakeFlatMap(6, 3, ETerrainType::Forest);
	FCivState S2 = MakeState();
	const int32 Id2 = CivRules::SpawnUnit(S2, 0, EUnitType::Warrior, FMapGenerator::OffsetToAxial(0, 1)).Id;
	CivRules::OrderMove(F, S2, Id2, FMapGenerator::OffsetToAxial(4, 1));
	TestEqual(TEXT("w lesie 1 pole na ture"), CivRules::FindUnit(S2, Id2)->Pos.Distance(FMapGenerator::OffsetToAxial(0, 1)), 1);

	// Dwie jednostki wojskowe nie moga stac na jednym polu; cywilna i wojskowa moga.
	FCivState S3 = MakeState();
	const FHexCoord A = FMapGenerator::OffsetToAxial(2, 2);
	CivRules::SpawnUnit(S3, 0, EUnitType::Warrior, A);
	CivRules::SpawnUnit(S3, 0, EUnitType::Archer, FMapGenerator::OffsetToAxial(3, 2));
	CivRules::SpawnUnit(S3, 0, EUnitType::Worker, FMapGenerator::OffsetToAxial(3, 2));
	TestFalse(TEXT("2 wojskowe na polu"), CivRules::CanEnter(M, S3, S3.Units[1], A));
	TestTrue(TEXT("cywilny na polu wojskowego"), CivRules::CanEnter(M, S3, S3.Units[2], A));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCivCityTest, "ChickenCiv.Rules.FoundCity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCivCityTest::RunTest(const FString&)
{
	FHexMap M = MakeFlatMap(14, 8, ETerrainType::Grassland);
	FCivState S = MakeState();
	const FHexCoord Pos = FMapGenerator::OffsetToAxial(3, 3);
	const int32 SettlerId = CivRules::SpawnUnit(S, 0, EUnitType::Settler, Pos).Id;
	const int32 WarriorId = CivRules::SpawnUnit(S, 0, EUnitType::Warrior, Pos).Id;

	FString Why;
	TestFalse(TEXT("wojownik nie zaklada miast"), CivRules::CanFoundCity(M, S, *CivRules::FindUnit(S, WarriorId), &Why));

	const int32 CityId = CivRules::FoundCity(M, S, SettlerId, &Why);
	TestTrue(TEXT("miasto zalozone"), CityId != INDEX_NONE);
	TestEqual(TEXT("osadnik zuzyty"), CivRules::FindUnit(S, SettlerId) == nullptr, true);
	TestEqual(TEXT("jedno miasto"), S.Cities.Num(), 1);
	TestEqual(TEXT("miasto na polu osadnika"), S.Cities[0].Pos, Pos);
	TestEqual(TEXT("populacja 1"), S.Cities[0].Population, 1);

	// Zbyt blisko istniejacego miasta.
	const int32 S2 = CivRules::SpawnUnit(S, 0, EUnitType::Settler, FMapGenerator::OffsetToAxial(5, 3)).Id;
	TestEqual(TEXT("za blisko"), CivRules::FoundCity(M, S, S2, &Why), (int32)INDEX_NONE);
	TestFalse(TEXT("powod podany"), Why.IsEmpty());
	// Wystarczajaco daleko.
	const int32 S3 = CivRules::SpawnUnit(S, 0, EUnitType::Settler, FMapGenerator::OffsetToAxial(9, 3)).Id;
	TestTrue(TEXT("daleko od miasta"), CivRules::FoundCity(M, S, S3, &Why) != INDEX_NONE);
	TestNotEqual(TEXT("rozne nazwy miast"), S.Cities[0].Name, S.Cities[1].Name);

	// Na gorach nie wolno.
	FHexMap M2 = MakeFlatMap(8, 8, ETerrainType::Mountain);
	FCivState S4 = MakeState();
	const int32 S5 = CivRules::SpawnUnit(S4, 0, EUnitType::Settler, FMapGenerator::OffsetToAxial(3, 3)).Id;
	TestEqual(TEXT("gory"), CivRules::FoundCity(M2, S4, S5, &Why), (int32)INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCivEconomyTest, "ChickenCiv.Rules.GrowthAndProduction", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCivEconomyTest::RunTest(const FString&)
{
	FHexMap M = MakeFlatMap(14, 8, ETerrainType::Grassland);
	FCivState S = MakeState();
	const int32 SettlerId = CivRules::SpawnUnit(S, 0, EUnitType::Settler, FMapGenerator::OffsetToAxial(5, 3)).Id;
	CivRules::FoundCity(M, S, SettlerId);
	FCity& City = S.Cities[0];

	const FCityReport R = CivRules::ComputeCityReport(M, S, City);
	TestEqual(TEXT("centrum + 1 pole lakowe daja 4 jedzenia"), R.Yield.Food, 4);
	TestEqual(TEXT("zuzycie 2"), R.FoodNeeded, 2);
	TestEqual(TEXT("1 pracujace pole"), R.Worked.Num(), 1);

	// Produkcja: wojownik (20) przy 1 produkcji z centrum.
	FProduction P; P.Kind = EProductionKind::Unit; P.Item = (int32)EUnitType::Warrior;
	TestTrue(TEXT("ustawiono produkcje"), CivRules::SetProduction(M, S, City, P));
	FProduction Bad; Bad.Kind = EProductionKind::Unit; Bad.Item = (int32)EUnitType::Settler;
	TestFalse(TEXT("osadnik wymaga populacji 2"), CivRules::SetProduction(M, S, City, Bad));

	TArray<FString> Log;
	int32 Turns = 0;
	while (S.Units.Num() == 0 && Turns < 100)
	{
		CivRules::EndPlayerTurn(M, S, 0, Log);
		++Turns;
	}
	TestEqual(TEXT("wojownik wyprodukowany"), S.Units.Num(), 1);
	if (S.Units.Num() == 1) TestEqual(TEXT("typ"), (int32)S.Units[0].Type, (int32)EUnitType::Warrior);
	TestTrue(TEXT("kolejka oprozniona"), S.Cities[0].Current.Kind == EProductionKind::None);
	TestTrue(TEXT("w rozsadnej liczbie tur"), Turns <= 25);

	// Wzrost: po kilku turach populacja rosnie.
	TestTrue(TEXT("miasto urosło"), S.Cities[0].Population >= 2);
	TestTrue(TEXT("zloto rosnie"), S.PlayerStates[0].Gold > 0);
	TestTrue(TEXT("nauka rosnie"), S.PlayerStates[0].Science > 0);

	// Budynek: Biblioteka zwieksza nauke.
	const int32 SciBefore = CivRules::ComputeCityReport(M, S, S.Cities[0]).Yield.Science;
	S.Cities[0].Buildings.Add(EBuildingType::Library);
	TestEqual(TEXT("biblioteka +3 nauki"), CivRules::ComputeCityReport(M, S, S.Cities[0]).Yield.Science, SciBefore + 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCivRaceTest, "ChickenCiv.Rules.Races", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCivRaceTest::RunTest(const FString&)
{
	TestEqual(TEXT("czerwone nauka x3"), CivRaces::Get(EChickenRace::Red).ScienceMult, 3);
	TestEqual(TEXT("zolte sila x2"), CivRaces::Get(EChickenRace::Yellow).StrengthMult, 2);
	TestEqual(TEXT("niebieskie szybkosc x10"), CivRaces::Get(EChickenRace::Blue).ShipSpeedMult, 10);
	TestEqual(TEXT("niebieskie sila x3"), CivRaces::Get(EChickenRace::Blue).ShipStrengthMult, 3);
	TestEqual(TEXT("niebieskie koszt /5"), CivRaces::Get(EChickenRace::Blue).ShipCostDiv, 5);
	TestTrue(TEXT("zielone obrona miasta"), CivRaces::Get(EChickenRace::Green).bCityDefenseWipe);

	// Ta sama gospodarka: czerwone daja 3x wiecej nauki niz zolte.
	FHexMap M = MakeFlatMap(10, 10, ETerrainType::Grassland);
	FCivState S = MakeState();
	FUnit& Settler = CivRules::SpawnUnit(S, 0, EUnitType::Settler, FMapGenerator::OffsetToAxial(5, 5));
	const int32 Id = Settler.Id;
	const int32 CityId = CivRules::FoundCity(M, S, Id, nullptr);
	TestTrue(TEXT("miasto zalozone"), CityId != INDEX_NONE);
	const int32 Yellow = CivRules::ComputeCityReport(M, S, *CivRules::FindCity(S, CityId)).Yield.Science;
	S.PlayerStates[0].Race = EChickenRace::Red;
	const int32 Red = CivRules::ComputeCityReport(M, S, *CivRules::FindCity(S, CityId)).Yield.Science;
	TestTrue(TEXT("jest nauka"), Yellow > 0);
	TestEqual(TEXT("czerwone 3x"), Red, 3 * Yellow);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCivVisibilityTest, "ChickenCiv.Rules.Visibility", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCivVisibilityTest::RunTest(const FString&)
{
	FHexMap M = MakeFlatMap(20, 12, ETerrainType::Grassland);
	FCivState S = MakeState();
	const FHexCoord Pos = FMapGenerator::OffsetToAxial(4, 5);
	const int32 Id = CivRules::SpawnUnit(S, 0, EUnitType::Warrior, Pos).Id;
	CivRules::ComputeVisibility(M, S, 0);

	const TArray<uint8>& V = S.PlayerStates[0].Visibility;
	TestEqual(TEXT("pole jednostki widoczne"), (int32)V[M.IndexOf(Pos)], 2);
	TestEqual(TEXT("sasiad widoczny"), (int32)V[M.IndexOf(Pos.Neighbor(0))], 2);
	TestEqual(TEXT("w promieniu 2"), (int32)V[M.IndexOf(Pos.Neighbor(0).Neighbor(0))], 2);
	TestEqual(TEXT("w promieniu 3 nieznane"), (int32)V[M.IndexOf(Pos.Neighbor(0).Neighbor(0).Neighbor(0))], 0);
	TestEqual(TEXT("gracz 1 nic nie widzi"), (int32)(S.PlayerStates[1].Visibility.Num() == 0 ? 0 : S.PlayerStates[1].Visibility[M.IndexOf(Pos)]), 0);

	// Po odejsciu pole zostaje odkryte, ale nie widoczne.
	CivRules::FindUnit(S, Id)->Pos = FMapGenerator::OffsetToAxial(14, 5);
	CivRules::ComputeVisibility(M, S, 0);
	TestEqual(TEXT("dawne pole odkryte"), (int32)S.PlayerStates[0].Visibility[M.IndexOf(Pos)], 1);
	TestEqual(TEXT("nowe pole widoczne"), (int32)S.PlayerStates[0].Visibility[M.IndexOf(FMapGenerator::OffsetToAxial(14, 5))], 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCivStartTest, "ChickenCiv.Rules.StartingUnits", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCivStartTest::RunTest(const FString&)
{
	for (int32 Seed : { 1, 42, 777, 12345 })
	{
		FMapSettings Set;
		Set.Seed = Seed;
		Set.Size = EMapSize::Medium;
		FHexMap M;
		M.Build(Set);

		FCivState S;
		S.PlayerStates.SetNum(4);
		TestTrue(FString::Printf(TEXT("start dla ziarna %d"), Seed), CivRules::PlaceStartingUnits(M, S, 4, Seed));

		TArray<FHexCoord> SettlerPos;
		for (int32 p = 0; p < 4; ++p)
		{
			int32 Settlers = 0, Warriors = 0, Workers = 0;
			for (const FUnit& U : S.Units)
			{
				if (U.Owner != p) continue;
				const FMapTile* T = M.GetTile(U.Pos);
				TestTrue(TEXT("jednostka na ladzie"), T && CivRules::MoveCost(*T) > 0);
				if (U.Type == EUnitType::Settler) { ++Settlers; SettlerPos.Add(U.Pos); }
				if (U.Type == EUnitType::Warrior) ++Warriors;
				if (U.Type == EUnitType::Worker) ++Workers;
			}
			TestEqual(TEXT("osadnik"), Settlers, 1);
			TestEqual(TEXT("wojownik"), Warriors, 1);
			TestEqual(TEXT("robotnik"), Workers, 1);
		}
		for (int32 i = 0; i < SettlerPos.Num(); ++i)
			for (int32 j = i + 1; j < SettlerPos.Num(); ++j)
				TestTrue(TEXT("gracze rozstawieni z dala od siebie"), SettlerPos[i].Distance(SettlerPos[j]) >= 3);

		// Dwie jednostki cywilne nie stoja na jednym polu.
		for (const FUnit& A : S.Units)
			for (const FUnit& B : S.Units)
				if (A.Id < B.Id && A.Pos == B.Pos) TestTrue(TEXT("rozne klasy na jednym polu"), A.IsCivilian() != B.IsCivilian());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCivSaveTest, "ChickenCiv.Save.CivStateRoundTrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCivSaveTest::RunTest(const FString&)
{
	FHexMap M = MakeFlatMap(14, 8, ETerrainType::Grassland);
	FChickenSaveData D;
	D.Civ.PlayerStates.SetNum(2);
	CivRules::SpawnUnit(D.Civ, 1, EUnitType::Archer, FHexCoord(2, 3)).Path = { FHexCoord(3, 3), FHexCoord(4, 3) };
	const int32 Sid = CivRules::SpawnUnit(D.Civ, 0, EUnitType::Settler, FMapGenerator::OffsetToAxial(5, 3)).Id;
	CivRules::FoundCity(M, D.Civ, Sid);
	D.Civ.Cities[0].Buildings.Add(EBuildingType::Granary);
	D.Civ.Cities[0].Current.Kind = EProductionKind::Building;
	D.Civ.Cities[0].Current.Item = (int32)EBuildingType::Library;
	D.Civ.Cities[0].Population = 3;
	CivRules::ComputeVisibility(M, D.Civ, 0);
	D.Civ.PlayerStates[0].Gold = 77;

	FChickenSaveData Out;
	TestTrue(TEXT("parsowanie"), FChickenSave::FromJson(FChickenSave::ToJson(D), Out));
	TestEqual(TEXT("jednostki"), Out.Civ.Units.Num(), 1);
	if (Out.Civ.Units.Num() == 1)
	{
		TestEqual(TEXT("typ jednostki"), (int32)Out.Civ.Units[0].Type, (int32)EUnitType::Archer);
		TestEqual(TEXT("wlasciciel"), Out.Civ.Units[0].Owner, 1);
		TestEqual(TEXT("pozycja"), Out.Civ.Units[0].Pos, FHexCoord(2, 3));
		TestEqual(TEXT("droga"), Out.Civ.Units[0].Path.Num(), 2);
	}
	TestEqual(TEXT("miasta"), Out.Civ.Cities.Num(), 1);
	if (Out.Civ.Cities.Num() == 1)
	{
		TestEqual(TEXT("populacja"), Out.Civ.Cities[0].Population, 3);
		TestEqual(TEXT("budynki"), Out.Civ.Cities[0].Buildings.Num(), 1);
		TestTrue(TEXT("budynek to spichlerz"), Out.Civ.Cities[0].Buildings.Contains(EBuildingType::Granary));
		TestTrue(TEXT("produkcja"), Out.Civ.Cities[0].Current.Kind == EProductionKind::Building && Out.Civ.Cities[0].Current.Item == (int32)EBuildingType::Library);
		TestFalse(TEXT("nazwa miasta"), Out.Civ.Cities[0].Name.IsEmpty());
	}
	TestEqual(TEXT("zloto"), Out.Civ.PlayerStates[0].Gold, 77);
	TestEqual(TEXT("widocznosc zachowana"), Out.Civ.PlayerStates[0].Visibility.Num(), D.Civ.PlayerStates[0].Visibility.Num());
	TestEqual(TEXT("kolejne Id"), Out.Civ.NextId, D.Civ.NextId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCivTechTest, "ChickenCiv.Rules.TechTree", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCivTechTest::RunTest(const FString&)
{
	// Spojnosc tabeli: indeks = id, wymagania istnieja i maja nizszy numer (brak cykli).
	for (int32 i = 0; i < NumTechs; ++i)
	{
		const FTechDef& D = CivTech::Def((ETech)i);
		TestEqual(TEXT("id zgodne z indeksem"), (int32)D.Id, i);
		if (i > 0)
		{
			TestTrue(TEXT("wymaganie 1 wczesniejsze"), (int32)D.Req1 < i);
			TestTrue(TEXT("wymaganie 2 wczesniejsze"), (int32)D.Req2 < i);
			TestTrue(TEXT("koszt dodatni"), D.Cost > 0);
		}
	}

	FCivPlayerState PS;
	TArray<ETech> Av;
	CivTech::GetAvailable(PS, Av);
	TestEqual(TEXT("na start tylko Rolnictwo"), Av.Num(), 1);
	TestFalse(TEXT("nie mozna przeskoczyc"), CivTech::SetResearch(PS, ETech::Spaceflight));
	TestTrue(TEXT("Rolnictwo"), CivTech::SetResearch(PS, ETech::Agriculture));

	TArray<FString> Log;
	TestFalse(TEXT("za malo nauki"), CivTech::AddScience(PS, 4, Log));
	TestEqual(TEXT("turs left"), CivTech::TurnsLeft(PS, 3), 2);
	TestTrue(TEXT("poznane"), CivTech::AddScience(PS, 8, Log));
	TestTrue(TEXT("zna Rolnictwo"), CivTech::Knows(PS, ETech::Agriculture));
	TestEqual(TEXT("nadwyzka przechodzi dalej"), PS.ResearchProgress, 2);
	TestTrue(TEXT("badanie wyczyszczone"), PS.Researching == ETech::None);

	// Wszystko da sie zbadac: ciagle badamy najtansze dostepne.
	int32 Guard = 0;
	while (CivTech::AutoPick(PS) != ETech::None && Guard++ < 100)
	{
		CivTech::SetResearch(PS, CivTech::AutoPick(PS));
		CivTech::AddScience(PS, 100000, Log);
	}
	TestEqual(TEXT("cale drzewko poznane"), PS.Known.Num(), NumTechs - 1);

	// Odblokowania: bez technologii brak pozycji produkcji, po niej jest.
	FHexMap M = MakeFlatMap(10, 10, ETerrainType::Grassland);
	SetTerrain(M, FMapGenerator::OffsetToAxial(5, 4), ETerrainType::Ocean);
	FCivState S = MakeState();
	const int32 UId = CivRules::SpawnUnit(S, 0, EUnitType::Settler, FMapGenerator::OffsetToAxial(5, 5)).Id;
	const int32 CityId = CivRules::FoundCity(M, S, UId, nullptr);
	TestTrue(TEXT("miasto"), CityId != INDEX_NONE);
	FCity& City = *CivRules::FindCity(S, CityId);
	City.Population = 3;

	auto Has = [&](EProductionKind K, int32 Item)
	{
		TArray<FProductionOption> O;
		CivRules::GetProductionOptions(M, S, City, O);
		return O.ContainsByPredicate([&](const FProductionOption& X) { return X.Item.Kind == K && X.Item.Item == Item; });
	};
	TestFalse(TEXT("lucznik zablokowany"), Has(EProductionKind::Unit, (int32)EUnitType::Archer));
	TestFalse(TEXT("statek zablokowany"), Has(EProductionKind::Unit, (int32)EUnitType::Ship));
	TestFalse(TEXT("biblioteka zablokowana"), Has(EProductionKind::Building, (int32)EBuildingType::Library));
	S.PlayerStates[0].Known = { ETech::Agriculture, ETech::Husbandry, ETech::Writing, ETech::Mathematics, ETech::Sailing };
	TestTrue(TEXT("lucznik odblokowany"), Has(EProductionKind::Unit, (int32)EUnitType::Archer));
	TestTrue(TEXT("statek odblokowany (miasto przy wodzie)"), Has(EProductionKind::Unit, (int32)EUnitType::Ship));
	TestTrue(TEXT("biblioteka odblokowana"), Has(EProductionKind::Building, (int32)EBuildingType::Library));
	TestFalse(TEXT("Gwiazda Smierci jeszcze nie"), Has(EProductionKind::Building, (int32)EBuildingType::DeathStar));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCivArmyTest, "ChickenCiv.Rules.Armies", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCivArmyTest::RunTest(const FString&)
{
	FHexMap M = MakeFlatMap(14, 14, ETerrainType::Grassland);
	FCivState S = MakeState(2);
	const FHexCoord Home = FMapGenerator::OffsetToAxial(6, 6);

	const int32 CmdId = CivRules::SpawnUnit(S, 0, EUnitType::Commander, Home).Id;
	FString Why;
	TestFalse(TEXT("tylko dowodca zaklada armie"), ArmyRules::CreateArmy(S, CivRules::SpawnUnit(S, 0, EUnitType::Warrior, Home).Id, &Why));
	TestTrue(TEXT("dowodca zaklada armie"), ArmyRules::CreateArmy(S, CmdId, &Why));
	TestFalse(TEXT("druga armia tego samego dowodcy"), ArmyRules::CreateArmy(S, CmdId, &Why));
	const int32 ArmyId = CivRules::FindUnit(S, CmdId)->ArmyId;
	TestTrue(TEXT("jest armia"), ArmyRules::FindArmy(S, ArmyId) != nullptr);

	// Limity: 10 zwyklych + 10 specjalnych, jeden zastepca.
	int32 JoinedNormal = 0;
	for (int32 i = 0; i < 12; ++i)
	{
		const int32 Id = CivRules::SpawnUnit(S, 0, EUnitType::Warrior, Home).Id;
		if (ArmyRules::JoinArmy(S, ArmyId, Id, &Why)) ++JoinedNormal;
	}
	TestEqual(TEXT("max 10 zwyklych"), JoinedNormal, 10);
	const int32 FirstDeputy = CivRules::SpawnUnit(S, 0, EUnitType::Deputy, Home).Id;
	TestTrue(TEXT("zastepca dolacza"), ArmyRules::JoinArmy(S, ArmyId, FirstDeputy, &Why));
	TestFalse(TEXT("tylko jeden zastepca"), ArmyRules::JoinArmy(S, ArmyId, CivRules::SpawnUnit(S, 0, EUnitType::Deputy, Home).Id, &Why));
	int32 JoinedSpecial = 1; // zastepca
	for (int32 i = 0; i < 12; ++i)
	{
		const int32 Id = CivRules::SpawnUnit(S, 0, EUnitType::Medic, Home).Id;
		if (ArmyRules::JoinArmy(S, ArmyId, Id, &Why)) ++JoinedSpecial;
	}
	TestEqual(TEXT("max 10 specjalnych"), JoinedSpecial, 10);
	TestFalse(TEXT("cywil nie wchodzi"), ArmyRules::JoinArmy(S, ArmyId, CivRules::SpawnUnit(S, 0, EUnitType::Worker, Home).Id, &Why));
	TestFalse(TEXT("statek nie wchodzi"), ArmyRules::JoinArmy(S, ArmyId, CivRules::SpawnUnit(S, 0, EUnitType::Ship, Home).Id, &Why));
	const FUnit* FarAway = CivRules::FindUnit(S, CivRules::SpawnUnit(S, 0, EUnitType::Archer, FMapGenerator::OffsetToAxial(1, 1)).Id);
	TestFalse(TEXT("daleko od dowodcy"), ArmyRules::JoinArmy(S, ArmyId, FarAway->Id, &Why));

	int32 N = 0, Sp = 0;
	ArmyRules::CountMembers(S, *ArmyRules::FindArmy(S, ArmyId), N, Sp);
	TestEqual(TEXT("10 zwyklych"), N, 10);
	TestEqual(TEXT("10 specjalnych"), Sp, 10);

	// Czlonkowie sa niewidoczni dla zasad pol: cala armia zajmuje jedno pole.
	TestTrue(TEXT("czlonek nie blokuje pola"), CivRules::UnitAt(S, Home, false) != nullptr && CivRules::UnitAt(S, Home, false)->Id == CmdId);

	// Ruch dowodcy przesuwa cala armie.
	const FHexCoord Dest = FMapGenerator::OffsetToAxial(8, 6);
	CivRules::FindUnit(S, CmdId)->MovesLeft = 5;
	TestTrue(TEXT("ruch armii"), CivRules::OrderMove(M, S, CmdId, Dest));
	int32 AtDest = 0;
	for (const FUnit& U : S.Units) if (U.ArmyId == ArmyId && U.Pos == Dest) ++AtDest;
	TestEqual(TEXT("21 jednostek na polu docelowym"), AtDest, 21);

	// Smierc dowodcy z zastepca: armia trwa.
	const FHexCoord Where = CivRules::FindUnit(S, CmdId)->Pos;
	TArray<FString> Log;
	ArmyRules::RemoveUnit(M, S, CmdId, &Log);
	const FArmy* After = ArmyRules::FindArmy(S, ArmyId);
	TestTrue(TEXT("armia trwa"), After != nullptr);
	if (After)
	{
		TestEqual(TEXT("zastepca dowodzi"), After->CommanderId, FirstDeputy);
		TestTrue(TEXT("zastepca ma flage"), CivRules::FindUnit(S, FirstDeputy)->bCommander);
		TestEqual(TEXT("19 czlonkow"), After->Members.Num(), 19);
	}

	// Smierc dowodcy bez zastepcy: rozpad, jednostki na roznych polach.
	FCivState S2 = MakeState(2);
	const int32 C2 = CivRules::SpawnUnit(S2, 0, EUnitType::Commander, Home).Id;
	ArmyRules::CreateArmy(S2, C2);
	const int32 A2 = CivRules::FindUnit(S2, C2)->ArmyId;
	for (int32 i = 0; i < 5; ++i) ArmyRules::JoinArmy(S2, A2, CivRules::SpawnUnit(S2, 0, EUnitType::Warrior, Home).Id);
	ArmyRules::JoinArmy(S2, A2, CivRules::SpawnUnit(S2, 0, EUnitType::Medic, Home).Id);
	ArmyRules::RemoveUnit(M, S2, C2, &Log);
	TestTrue(TEXT("armia rozwiazana"), ArmyRules::FindArmy(S2, A2) == nullptr);
	TestEqual(TEXT("6 jednostek przezylo"), S2.Units.Num(), 6);
	TSet<FHexCoord> Tiles;
	for (const FUnit& U : S2.Units)
	{
		TestEqual(TEXT("jednostka samodzielna"), U.ArmyId, 0);
		TestFalse(TEXT("kazda na innym polu"), Tiles.Contains(U.Pos));
		Tiles.Add(U.Pos);
		TestTrue(TEXT("blisko miejsca smierci"), U.Pos.Distance(Home) <= 2);
	}

	// Opuszczenie armii.
	FCivState S3 = MakeState(2);
	const int32 C3 = CivRules::SpawnUnit(S3, 0, EUnitType::Commander, Home).Id;
	ArmyRules::CreateArmy(S3, C3);
	const int32 A3 = CivRules::FindUnit(S3, C3)->ArmyId;
	const int32 W3 = CivRules::SpawnUnit(S3, 0, EUnitType::Warrior, Home).Id;
	ArmyRules::JoinArmy(S3, A3, W3);
	TestTrue(TEXT("opuszcza"), ArmyRules::LeaveArmy(M, S3, W3, &Why));
	TestTrue(TEXT("juz nie w armii"), CivRules::FindUnit(S3, W3)->ArmyId == 0 && CivRules::FindUnit(S3, W3)->Pos != Home);

	// Zapis armii.
	FChickenSaveData D;
	D.Civ = S2;
	D.Civ.Armies = S.Armies;
	D.Civ.Units = S.Units;
	const FString Json = FChickenSave::ToJson(D);
	TestFalse(TEXT("serializacja"), Json.IsEmpty());
	FChickenSaveData Out;
	TestTrue(TEXT("deserializacja"), FChickenSave::FromJson(Json, Out));
	TestEqual(TEXT("armie zapisane"), Out.Civ.Armies.Num(), S.Armies.Num());
	if (Out.Civ.Armies.Num() > 0) TestEqual(TEXT("czlonkowie zapisani"), Out.Civ.Armies[0].Members.Num(), 19);

	// Zielony strazik tylko dla zielonych.
	FCity City; City.Id = 1; City.Owner = 0; City.Pos = Home;
	S.PlayerStates[0].Race = EChickenRace::Red;
	S.PlayerStates[0].Known = { ETech::Agriculture, ETech::Husbandry, ETech::Bronze };
	TArray<FProductionOption> Opt;
	CivRules::GetProductionOptions(M, S, City, Opt);
	TestFalse(TEXT("czerwone bez zielonego straznika"), Opt.ContainsByPredicate([](const FProductionOption& O) { return O.Item.Kind == EProductionKind::Unit && O.Item.Item == (int32)EUnitType::GreenGuardian; }));
	S.PlayerStates[0].Race = EChickenRace::Green;
	CivRules::GetProductionOptions(M, S, City, Opt);
	TestTrue(TEXT("zielone maja zielonego straznika"), Opt.ContainsByPredicate([](const FProductionOption& O) { return O.Item.Kind == EProductionKind::Unit && O.Item.Item == (int32)EUnitType::GreenGuardian; }));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCivImprovementTest, "ChickenCiv.Rules.Improvements", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCivImprovementTest::RunTest(const FString&)
{
	FHexMap M = MakeFlatMap(10, 10, ETerrainType::Grassland);
	SetTerrain(M, FMapGenerator::OffsetToAxial(6, 5), ETerrainType::Hills);
	FCivState S = MakeState();
	const int32 SettlerId = CivRules::SpawnUnit(S, 0, EUnitType::Settler, FMapGenerator::OffsetToAxial(5, 5)).Id;
	const int32 CityId = CivRules::FoundCity(M, S, SettlerId, nullptr);
	S.Cities[0].Population = 1;
	const FHexCoord Field = FMapGenerator::OffsetToAxial(5, 4);

	const int32 W = CivRules::SpawnUnit(S, 0, EUnitType::Worker, Field).Id;
	TestEqual(TEXT("laka -> farma"), (int32)CivRules::BestImprovementFor(M, S, Field), (int32)EImprovement::Farm);
	TestEqual(TEXT("wzgorza -> kopalnia"), (int32)CivRules::BestImprovementFor(M, S, FMapGenerator::OffsetToAxial(6, 5)), (int32)EImprovement::Mine);
	TestEqual(TEXT("miasto bez ulepszenia"), (int32)CivRules::BestImprovementFor(M, S, S.Cities[0].Pos), (int32)EImprovement::None);

	const int32 FoodBefore = CivRules::ComputeCityReport(M, S, *CivRules::FindCity(S, CityId)).Yield.Food;
	TestTrue(TEXT("start budowy"), CivRules::StartImprovement(M, S, W, nullptr));
	TestFalse(TEXT("nie dwa razy"), CivRules::StartImprovement(M, S, W, nullptr));
	for (int32 t = 0; t < CivRules::ImprovementTurns; ++t)
	{
		TestEqual(TEXT("jeszcze nie gotowe"), (int32)CivRules::ImprovementAt(S, Field), (int32)EImprovement::None);
		CivRules::BeginPlayerTurn(M, S, 0);
		TestEqual(TEXT("robotnik stoi"), CivRules::FindUnit(S, W)->MovesLeft, 0);
	}
	TestEqual(TEXT("ulepszenie gotowe"), (int32)CivRules::ImprovementAt(S, Field), (int32)EImprovement::Farm);
	const int32 FoodAfter = CivRules::ComputeCityReport(M, S, *CivRules::FindCity(S, CityId)).Yield.Food;
	TestTrue(TEXT("farma daje jedzenie (jesli pole pracuje)"), FoodAfter >= FoodBefore);
	TestEqual(TEXT("po farmie mozna tylko droge"), (int32)CivRules::BestImprovementFor(M, S, Field), (int32)EImprovement::Road);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCivShipsTest, "ChickenCiv.Rules.BlueShipsAndWonder", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCivShipsTest::RunTest(const FString&)
{
	FHexMap M = MakeFlatMap(12, 8, ETerrainType::Grassland);
	for (int32 Col = 0; Col < 12; ++Col) for (int32 Row = 0; Row < 3; ++Row) SetTerrain(M, FMapGenerator::OffsetToAxial(Col, Row), ETerrainType::Ocean);

	FCivState S = MakeState(2);
	S.PlayerStates[0].Race = EChickenRace::Blue;
	S.PlayerStates[1].Race = EChickenRace::Yellow;

	TestEqual(TEXT("statek niebieskich 5x tanszy"), CivRules::UnitCost(S, 0, EUnitType::Ship), CivDefs::Unit(EUnitType::Ship).Cost / 5);
	TestEqual(TEXT("statek zoltych pelna cena"), CivRules::UnitCost(S, 1, EUnitType::Ship), CivDefs::Unit(EUnitType::Ship).Cost);
	TestEqual(TEXT("wojownik niebieskich pelna cena"), CivRules::UnitCost(S, 0, EUnitType::Warrior), CivDefs::Unit(EUnitType::Warrior).Cost);

	const FHexCoord Sea = FMapGenerator::OffsetToAxial(4, 1);
	const FUnit& Blue = CivRules::SpawnUnit(S, 0, EUnitType::Ship, Sea);
	TestEqual(TEXT("niebieski statek ruch x10"), Blue.MovesLeft, 40);
	const int32 YId = CivRules::SpawnUnit(S, 1, EUnitType::Ship, FMapGenerator::OffsetToAxial(4, 0)).Id;
	TestEqual(TEXT("zolty statek ruch x1"), CivRules::FindUnit(S, YId)->MovesLeft, 4);

	// Statek plywa tylko po wodzie, a po ladzie nie.
	const int32 BId = S.Units[0].Id;
	TestFalse(TEXT("statek nie wejdzie na lad"), CivRules::OrderMove(M, S, BId, FMapGenerator::OffsetToAxial(4, 5)));
	TestTrue(TEXT("statek plynie po wodzie"), CivRules::OrderMove(M, S, BId, FMapGenerator::OffsetToAxial(10, 0)));

	// Jednostka powietrzna lata nad wszystkim.
	const int32 AId = CivRules::SpawnUnit(S, 0, EUnitType::XWing, FMapGenerator::OffsetToAxial(2, 6)).Id;
	TestTrue(TEXT("X-wing nad ladem i woda"), CivRules::OrderMove(M, S, AId, FMapGenerator::OffsetToAxial(8, 0)));

	// Cud: jeden na gre, wymaga nuggetsow.
	FHexMap M2 = MakeFlatMap(10, 10, ETerrainType::Grassland);
	FCivState S2 = MakeState(2);
	const int32 U1 = CivRules::SpawnUnit(S2, 0, EUnitType::Settler, FMapGenerator::OffsetToAxial(2, 2)).Id;
	const int32 U2 = CivRules::SpawnUnit(S2, 1, EUnitType::Settler, FMapGenerator::OffsetToAxial(7, 7)).Id;
	const int32 C1 = CivRules::FoundCity(M2, S2, U1, nullptr);
	const int32 C2 = CivRules::FoundCity(M2, S2, U2, nullptr);
	S2.PlayerStates[0].Known = { ETech::DeathStar };
	S2.PlayerStates[1].Known = { ETech::DeathStar };
	FProduction W; W.Kind = EProductionKind::Building; W.Item = (int32)EBuildingType::DeathStar;
	TestTrue(TEXT("gracz 0 moze budowac cud"), CivRules::SetProduction(M2, S2, *CivRules::FindCity(S2, C1), W));
	FCity& City1 = *CivRules::FindCity(S2, C1);
	City1.ProdStock = 1000;
	TArray<FString> Log;
	CivRules::EndPlayerTurn(M2, S2, 0, Log);
	TestFalse(TEXT("bez nuggetsow cud czeka"), City1.Has(EBuildingType::DeathStar));
	S2.PlayerStates[0].Nuggets = 150;
	CivRules::EndPlayerTurn(M2, S2, 0, Log);
	TestTrue(TEXT("z nuggetsami cud stoi"), City1.Has(EBuildingType::DeathStar));
	TestEqual(TEXT("nuggetsy zuzyte"), S2.PlayerStates[0].Nuggets, 50);
	TestFalse(TEXT("drugi gracz nie moze juz budowac cudu"), CivRules::SetProduction(M2, S2, *CivRules::FindCity(S2, C2), W));
	return true;
}

#endif
