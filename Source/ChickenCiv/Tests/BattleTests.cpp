#include "Misc/AutomationTest.h"
#include "Core/BattleRules.h"
#include "Core/CivRules.h"
#include "Core/ArmyRules.h"
#include "Map/MapGenerator.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FHexMap FlatMap(int32 W, int32 H)
	{
		FHexMap M;
		for (int32 Row = 0; Row < H; ++Row)
			for (int32 Col = 0; Col < W; ++Col)
			{
				FMapTile T;
				T.Coord = FMapGenerator::OffsetToAxial(Col, Row);
				T.Terrain = ETerrainType::Grassland;
				M.Index.Add(T.Coord, M.Tiles.Add(T));
				M.Bounds += T.Coord.ToWorld(FHexMap::HexSize);
			}
		return M;
	}

	FCivState State(int32 N = 2)
	{
		FCivState S;
		S.PlayerStates.SetNum(N);
		for (FCivPlayerState& P : S.PlayerStates) P.Race = EChickenRace::Red;
		return S;
	}

	// Reczne budowanie bitwy do testow pojedynczych umiejetnosci.
	int32 Add(FBattle& B, int32 Side, EUnitType Type, int32 Col, int32 Row, int32 Initiative)
	{
		const FUnitDef& D = CivDefs::Unit(Type);
		FBattleUnit U;
		U.Id = B.Units.Num();
		U.Side = Side;
		U.Type = Type;
		U.Pos = BattleRules::CellToHex(Col, Row);
		U.Strength = D.Strength;
		U.Defense = D.Strength;
		U.Moves = D.Moves;
		U.Range = 1;
		U.Initiative = Initiative;
		U.bSpecial = D.bSpecial;
		U.bDeputy = Type == EUnitType::Deputy;
		return B.Units.Add(U), U.Id;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleBoardTest, "ChickenCiv.Battle.Board", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBattleBoardTest::RunTest(const FString&)
{
	for (int32 Row = 0; Row < FBattle::Height; ++Row)
		for (int32 Col = 0; Col < FBattle::Width; ++Col)
		{
			int32 C, R;
			const FHexCoord H = BattleRules::CellToHex(Col, Row);
			BattleRules::HexToCell(H, C, R);
			TestEqual(TEXT("kolumna"), C, Col);
			TestEqual(TEXT("rzad"), R, Row);
			TestTrue(TEXT("w planszy"), BattleRules::InBoard(H));
			TestEqual(TEXT("zgodne z mapa"), H, FMapGenerator::OffsetToAxial(Col, Row));
		}
	TestFalse(TEXT("poza plansza"), BattleRules::InBoard(BattleRules::CellToHex(-1, 3)));
	TestFalse(TEXT("poza plansza 2"), BattleRules::InBoard(BattleRules::CellToHex(3, FBattle::Height)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleFightTest, "ChickenCiv.Battle.FightAndApply", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBattleFightTest::RunTest(const FString&)
{
	FHexMap M = FlatMap(14, 14);
	FCivState S = State();
	S.PlayerStates[0].Race = EChickenRace::Yellow;

	const FHexCoord Home = FMapGenerator::OffsetToAxial(4, 6);
	const FHexCoord Target = FMapGenerator::OffsetToAxial(6, 6);

	// Obroncy: wojownik w miescie gracza 1.
	const int32 Settler = CivRules::SpawnUnit(S, 1, EUnitType::Settler, Target).Id;
	const int32 CityId = CivRules::FoundCity(M, S, Settler, nullptr);
	TestTrue(TEXT("miasto obroncy"), CityId != INDEX_NONE);
	CivRules::SpawnUnit(S, 1, EUnitType::Warrior, Target);

	// Atakujacy: dowodca + 6 wojownikow zolte (sila x2).
	const int32 Cmd = CivRules::SpawnUnit(S, 0, EUnitType::Commander, Home).Id;
	ArmyRules::CreateArmy(S, Cmd);
	const int32 ArmyId = CivRules::FindUnit(S, Cmd)->ArmyId;
	for (int32 i = 0; i < 6; ++i) ArmyRules::JoinArmy(S, ArmyId, CivRules::SpawnUnit(S, 0, EUnitType::Warrior, Home).Id);

	FBattle B;
	FString Why;
	TestTrue(TEXT("setup"), BattleRules::Setup(S, Cmd, Target, B, &Why));
	TestEqual(TEXT("7 atakujacych + 1 obronca"), B.Units.Num(), 8);
	TestTrue(TEXT("obrona miasta"), B.bCityDefense);
	TestFalse(TEXT("bez zielonego straznika"), B.bGreenWipe);
	for (const FBattleUnit& U : B.Units)
	{
		TestTrue(TEXT("jednostka na planszy"), BattleRules::InBoard(U.Pos));
		if (U.Type == EUnitType::Warrior && U.Side == 0) TestEqual(TEXT("zolta sila x2"), U.Strength, 12);
		if (U.Side == 1) TestEqual(TEXT("obrona miasta x1.5"), U.Strength, 9);
	}
	TSet<FHexCoord> Cells;
	for (const FBattleUnit& U : B.Units) { TestFalse(TEXT("rozne pola"), Cells.Contains(U.Pos)); Cells.Add(U.Pos); }

	BattleRules::AutoResolve(B);
	TestEqual(TEXT("atakujacy wygrywa"), B.Winner, 0);
	TestEqual(TEXT("5 nuggetsow za zabitego"), B.Nuggets[0], 5);

	BattleRules::Apply(M, S, B);
	TestEqual(TEXT("nuggetsy dla gracza"), S.PlayerStates[0].Nuggets, 5);
	TestEqual(TEXT("miasto zdobyte"), CivRules::FindCity(S, CityId)->Owner, 0);
	TestEqual(TEXT("dowodca w miescie"), CivRules::FindUnit(S, Cmd)->Pos, Target);
	TestFalse(TEXT("obronca zginal"), S.Units.ContainsByPredicate([](const FUnit& U) { return U.Owner == 1; }));
	TestTrue(TEXT("armia przetrwala"), ArmyRules::FindArmy(S, ArmyId) != nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleSpecialsTest, "ChickenCiv.Battle.Specials", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBattleSpecialsTest::RunTest(const FString&)
{
	// Medyk leczy.
	{
		FBattle B;
		const int32 Medic = Add(B, 0, EUnitType::Medic, 3, 4, 100);
		const int32 Ally = Add(B, 0, EUnitType::Warrior, 4, 4, 10);
		Add(B, 1, EUnitType::Warrior, 9, 4, 5);
		B.Units[Ally].Hp = 40;
		BattleRules::Begin(B);
		TestEqual(TEXT("medyk pierwszy"), B.Current()->Id, Medic);
		TestFalse(TEXT("nie leczy pelnego"), BattleRules::Heal(B, Medic, Medic));
		TestTrue(TEXT("leczy"), BattleRules::Heal(B, Medic, Ally));
		TestEqual(TEXT("+30 zdrowia"), B.Units[Ally].Hp, 70);
		TestTrue(TEXT("tura przeszla"), B.Current()->Id != Medic);
	}

	// Wskrzeszacz: raz na gre.
	{
		FBattle B;
		const int32 Res = Add(B, 0, EUnitType::Resurrector, 3, 4, 100);
		const int32 Dead1 = Add(B, 0, EUnitType::Warrior, 2, 4, 10);
		const int32 Dead2 = Add(B, 0, EUnitType::Warrior, 2, 5, 10);
		Add(B, 1, EUnitType::Warrior, 9, 4, 5);
		B.Units[Dead1].bAlive = false; B.Units[Dead1].Hp = 0;
		B.Units[Dead2].bAlive = false; B.Units[Dead2].Hp = 0;
		BattleRules::Begin(B);
		TestTrue(TEXT("wskrzesza"), BattleRules::Resurrect(B, Res, Dead1));
		TestTrue(TEXT("zyje"), B.Units[Dead1].bAlive);
		TestEqual(TEXT("polowa zdrowia"), B.Units[Dead1].Hp, 50);
		TestTrue(TEXT("flaga uzycia"), B.bResurrectUsed[0]);
		// Kolejna runda: ponowne wskrzeszenie jest zablokowane.
		while (B.Current() && B.Current()->Id != Res && B.Winner < 0) BattleRules::EndUnitTurn(B);
		TestFalse(TEXT("tylko 1 wskrzeszenie na gre"), BattleRules::Resurrect(B, Res, Dead2));
	}

	// Czarna dziura: celowanie 2 tury, potem niszczy wszystko w promieniu 2.
	{
		FBattle B;
		const int32 Thrower = Add(B, 0, EUnitType::BlackHoleThrower, 1, 4, 100);
		const int32 E1 = Add(B, 1, EUnitType::Warrior, 8, 4, 5);
		const int32 E2 = Add(B, 1, EUnitType::Warrior, 9, 4, 4);
		const int32 Far = Add(B, 1, EUnitType::Warrior, 9, 8, 3);
		const FHexCoord Aim = BattleRules::CellToHex(8, 4);
		BattleRules::Begin(B);
		TestTrue(TEXT("celuje"), BattleRules::BlackHole(B, Thrower, Aim));
		TestEqual(TEXT("2 tury celowania"), B.Units[Thrower].AimTurns, 2);
		for (int32 Guard = 0; Guard < 60 && B.Winner < 0 && B.Round <= 4; ++Guard) BattleRules::EndUnitTurn(B);
		TestFalse(TEXT("E1 zniszczony"), B.Units[E1].bAlive);
		TestFalse(TEXT("E2 zniszczony"), B.Units[E2].bAlive);
		TestTrue(TEXT("daleki cel przezyl"), B.Units[Far].bAlive);
		TestEqual(TEXT("nuggetsy za 2 zabitych"), B.Nuggets[0], 10);
		TestEqual(TEXT("miotacz przestal celowac"), B.Units[Thrower].AimTurns, 0);
	}

	// Laser Gwiazdy Smierci: raz na runde, niszczy dowodce wroga => wygrana.
	{
		FBattle B;
		B.bDeathStar[0] = true;
		const int32 Mine = Add(B, 0, EUnitType::Warrior, 2, 4, 100);
		const int32 EnemyCmd = Add(B, 1, EUnitType::Commander, 9, 4, 5);
		Add(B, 1, EUnitType::Warrior, 9, 5, 4);
		B.Units[EnemyCmd].bCommander = true;
		B.bHasCommander[1] = true;
		BattleRules::Begin(B);
		TestEqual(TEXT("moja tura"), B.Current()->Id, Mine);
		TestFalse(TEXT("nie wlasna jednostke"), BattleRules::DeathStarLaser(B, 0, Mine));
		TestTrue(TEXT("laser"), BattleRules::DeathStarLaser(B, 0, EnemyCmd));
		TestEqual(TEXT("wygrana po smierci dowodcy"), B.Winner, 0);
	}

	// Zastepca przejmuje dowodzenie, bitwa trwa.
	{
		FBattle B;
		B.bDeathStar[0] = true;
		Add(B, 0, EUnitType::Warrior, 2, 4, 100);
		const int32 Cmd = Add(B, 1, EUnitType::Commander, 9, 4, 5);
		const int32 Dep = Add(B, 1, EUnitType::Deputy, 9, 5, 4);
		B.Units[Cmd].bCommander = true;
		B.bHasCommander[1] = true;
		BattleRules::Begin(B);
		BattleRules::DeathStarLaser(B, 0, Cmd);
		TestEqual(TEXT("bitwa trwa"), B.Winner, -1);
		TestTrue(TEXT("zastepca dowodzi"), B.Units[Dep].bCommander);
		TestFalse(TEXT("laser tylko raz na runde"), BattleRules::DeathStarLaser(B, 0, Dep));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBattleGreenTest, "ChickenCiv.Battle.GreenGuardian", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FBattleGreenTest::RunTest(const FString&)
{
	FHexMap M = FlatMap(14, 14);
	FCivState S = State();
	S.PlayerStates[1].Race = EChickenRace::Green;
	const FHexCoord Home = FMapGenerator::OffsetToAxial(4, 6);
	const FHexCoord Target = FMapGenerator::OffsetToAxial(6, 6);

	const int32 Settler = CivRules::SpawnUnit(S, 1, EUnitType::Settler, Target).Id;
	const int32 CityId = CivRules::FoundCity(M, S, Settler, nullptr);
	CivRules::SpawnUnit(S, 1, EUnitType::GreenGuardian, Target);

	const int32 Cmd = CivRules::SpawnUnit(S, 0, EUnitType::Commander, Home).Id;
	ArmyRules::CreateArmy(S, Cmd);
	const int32 ArmyId = CivRules::FindUnit(S, Cmd)->ArmyId;
	for (int32 i = 0; i < 4; ++i) ArmyRules::JoinArmy(S, ArmyId, CivRules::SpawnUnit(S, 0, EUnitType::Warrior, Home).Id);

	FBattle B;
	TestTrue(TEXT("setup"), BattleRules::Setup(S, Cmd, Target, B));
	TestTrue(TEXT("zielony niszczy armie"), B.bGreenWipe);
	TestEqual(TEXT("wygrywa obronca"), B.Winner, 1);
	TestEqual(TEXT("nuggetsy za 5 jednostek"), B.Nuggets[1], 25);
	BattleRules::Apply(M, S, B);
	TestFalse(TEXT("cala armia zniszczona"), S.Units.ContainsByPredicate([](const FUnit& U) { return U.Owner == 0; }));
	TestTrue(TEXT("armia usunieta"), S.Armies.Num() == 0);
	TestEqual(TEXT("miasto nietkniete"), CivRules::FindCity(S, CityId)->Owner, 1);

	// Poza miastem zielony straznik nie dziala (tylko obrona miasta).
	FCivState S2 = State();
	S2.PlayerStates[1].Race = EChickenRace::Green;
	CivRules::SpawnUnit(S2, 1, EUnitType::GreenGuardian, Target);
	const int32 W = CivRules::SpawnUnit(S2, 0, EUnitType::Warrior, Home).Id;
	FBattle B2;
	TestTrue(TEXT("setup 2"), BattleRules::Setup(S2, W, Target, B2));
	TestFalse(TEXT("brak miasta, brak zniszczenia"), B2.bGreenWipe);
	TestEqual(TEXT("bitwa trwa"), B2.Winner, -1);
	return true;
}

#endif
