#include "Misc/AutomationTest.h"
#include "Core/CivAI.h"
#include "Core/CivRules.h"
#include "Core/ArmyRules.h"
#include "Core/LlmStrategy.h"
#include "Map/MapGenerator.h"
#include "Model/TechTree.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FHexMap Flat(int32 W, int32 H)
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
		for (FCivPlayerState& P : S.PlayerStates) P.Race = EChickenRace::Yellow;
		return S;
	}

	struct FSimResult
	{
		int32 Cities = 0, Units = 0, Techs = 0, Wars = 0;
		int64 Hash = 0;
		bool bInvariantsOk = true;
		FString Problem;
	};

	FSimResult Simulate(int32 Seed, int32 NumPlayers, int32 Turns, EDifficulty Difficulty)
	{
		FMapSettings St;
		St.Seed = Seed;
		St.Size = EMapSize::Medium;
		St.NumPlayers = NumPlayers;
		St.Difficulty = Difficulty;
		FHexMap M;
		M.Build(St);

		FCivState S;
		S.PlayerStates.SetNum(NumPlayers);
		for (int32 i = 0; i < NumPlayers; ++i)
		{
			S.PlayerStates[i].Race = (EChickenRace)(i % 4);
			CivAI::InitPlayer(S.PlayerStates[i], i, Seed, Difficulty, true);
		}
		CivRules::PlaceStartingUnits(M, S, NumPlayers, Seed);
		CivRules::BeginPlayerTurn(M, S, 0);

		for (int32 Turn = 1; Turn <= Turns; ++Turn)
		{
			for (int32 p = 0; p < NumPlayers; ++p)
			{
				TArray<FString> Log;
				CivAI::PlayTurn(M, S, p, Difficulty, Turn, &Log);
				CivRules::EndPlayerTurn(M, S, p, Log);
				CivRules::BeginPlayerTurn(M, S, (p + 1) % NumPlayers);
			}
		}

		FSimResult R;
		R.Cities = S.Cities.Num();
		R.Units = S.Units.Num();
		for (const FCivPlayerState& P : S.PlayerStates) { R.Techs += P.Known.Num(); R.Wars += P.AtWarWith.Num(); R.Hash = R.Hash * 31 + P.Gold * 7 + P.Science + P.Known.Num() * 1000 + P.Nuggets; }
		for (const FUnit& U : S.Units) R.Hash = R.Hash * 31 + U.Pos.Q * 131 + U.Pos.R * 17 + (int32)U.Type + U.Owner;
		for (const FCity& C : S.Cities) R.Hash = R.Hash * 31 + C.Pos.Q * 131 + C.Pos.R * 17 + C.Population + C.Owner;

		// Niezmienniki.
		TMap<FHexCoord, int32> Civil, Mil;
		for (const FUnit& U : S.Units)
		{
			const FMapTile* T = M.GetTile(U.Pos);
			if (!T) { R.bInvariantsOk = false; R.Problem = TEXT("jednostka poza mapa"); break; }
			if (U.Def().Domain == EDomain::Land && !T->IsPassableLand()) { R.bInvariantsOk = false; R.Problem = FString::Printf(TEXT("jednostka ladowa na niedostepnym polu %s"), *U.Pos.ToString()); break; }
			if (U.IsArmyMember()) continue;
			int32& Cnt = U.IsCivilian() ? Civil.FindOrAdd(U.Pos) : Mil.FindOrAdd(U.Pos);
			if (++Cnt > 1) { R.bInvariantsOk = false; R.Problem = FString::Printf(TEXT("dwie jednostki tej samej klasy na %s"), *U.Pos.ToString()); break; }
		}
		for (const FArmy& A : S.Armies)
		{
			const FUnit* C = CivRules::FindUnit(S, A.CommanderId);
			if (!C) { R.bInvariantsOk = false; R.Problem = TEXT("armia bez dowodcy"); break; }
			for (int32 Id : A.Members)
			{
				const FUnit* Mem = CivRules::FindUnit(S, Id);
				if (!Mem || Mem->Pos != C->Pos || Mem->ArmyId != A.Id) { R.bInvariantsOk = false; R.Problem = TEXT("czlonek armii poza dowodca"); break; }
			}
		}
		return R;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAiLlmReplyTest, "ChickenCiv.AI.LlmReply", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAiLlmReplyTest::RunTest(const FString&)
{
	FCivState S = State(3);
	TestTrue(TEXT("poprawna odpowiedz z gadanina"), LlmStrategy::ApplyReply(TEXT("Sure! {\"aggression\": 80, \"target_cities\": 5, \"focus\": \"Lasers\", \"war_target\": 3, \"note\": \"attack\"} Good luck."), S, 0));
	TestEqual(TEXT("agresja"), S.PlayerStates[0].AIAggression, 80);
	TestEqual(TEXT("miasta"), S.PlayerStates[0].AITargetCities, 5);
	TestEqual(TEXT("sciezka lasery"), S.PlayerStates[0].AIFocusPath, 2);
	TestEqual(TEXT("cel wojny (numer od 1)"), S.PlayerStates[0].AIWarTarget, 2);

	TestTrue(TEXT("wartosci poza zakresem sa przycinane"), LlmStrategy::ApplyReply(TEXT("{\"aggression\": 500, \"target_cities\": 99}"), S, 1));
	TestEqual(TEXT("agresja max 100"), S.PlayerStates[1].AIAggression, 100);
	TestEqual(TEXT("miasta max 8"), S.PlayerStates[1].AITargetCities, 8);

	TestTrue(TEXT("null kasuje cel"), LlmStrategy::ApplyReply(TEXT("{\"war_target\": null}"), S, 0));
	TestEqual(TEXT("brak celu"), S.PlayerStates[0].AIWarTarget, -1);

	TestFalse(TEXT("nie wojna ze soba"), LlmStrategy::ApplyReply(TEXT("{\"war_target\": 1}"), S, 0));
	TestFalse(TEXT("smieci"), LlmStrategy::ApplyReply(TEXT("nie wiem"), S, 0));
	TestFalse(TEXT("zly json"), LlmStrategy::ApplyReply(TEXT("{aggression: }"), S, 0));
	TestFalse(TEXT("zly gracz"), LlmStrategy::ApplyReply(TEXT("{\"aggression\": 5}"), S, 9));

	const FString Prompt = LlmStrategy::BuildPrompt(Flat(4, 4), S, { FPlayerInfo(), FPlayerInfo(), FPlayerInfo() }, 0, 11);
	TestTrue(TEXT("prompt zawiera format odpowiedzi"), Prompt.Contains(TEXT("JSON")));
	TestTrue(TEXT("prompt zawiera numer tury"), Prompt.Contains(TEXT("Turn 11")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAiRulesTest, "ChickenCiv.AI.WarAndArmies", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAiRulesTest::RunTest(const FString&)
{
	FHexMap M = Flat(14, 14);
	FCivState S = State();
	const FHexCoord Home = FMapGenerator::OffsetToAxial(4, 6);
	const FHexCoord Target = FMapGenerator::OffsetToAxial(5, 6);

	// Dyplomacja symetryczna.
	TestFalse(TEXT("poczatkowy pokoj"), CivAI::IsAtWar(S, 0, 1));
	TestTrue(TEXT("wypowiedzenie wojny"), CivAI::DeclareWar(S, 0, 1));
	TestTrue(TEXT("wojna w obie strony"), CivAI::IsAtWar(S, 1, 0) && CivAI::IsAtWar(S, 0, 1));
	TestFalse(TEXT("nie dwa razy"), CivAI::DeclareWar(S, 1, 0));
	TestFalse(TEXT("nie ze soba"), CivAI::DeclareWar(S, 0, 0));
	S.PlayerStates[0].AtWarWith.Reset();
	S.PlayerStates[1].AtWarWith.Reset();

	// Wchodzenie na pole wlasnego dowodcy = dolaczenie do armii.
	const int32 Cmd = CivRules::SpawnUnit(S, 0, EUnitType::Commander, Home).Id;
	ArmyRules::CreateArmy(S, Cmd);
	const int32 ArmyId = CivRules::FindUnit(S, Cmd)->ArmyId;
	const int32 W = CivRules::SpawnUnit(S, 0, EUnitType::Warrior, FMapGenerator::OffsetToAxial(4, 8)).Id;
	TestTrue(TEXT("mozna wejsc na pole dowodcy"), CivRules::CanEnter(M, S, *CivRules::FindUnit(S, W), Home));
	TestTrue(TEXT("ruch do dowodcy"), CivRules::OrderMove(M, S, W, Home));
	TestEqual(TEXT("dolaczyl"), CivRules::FindUnit(S, W)->ArmyId, ArmyId);
	TestEqual(TEXT("stoi z dowodca"), CivRules::FindUnit(S, W)->Pos, Home);
	// Pelna armia nie przyjmuje kolejnych.
	for (int32 i = 0; i < 9; ++i) ArmyRules::JoinArmy(S, ArmyId, CivRules::SpawnUnit(S, 0, EUnitType::Warrior, Home).Id);
	const int32 Extra = CivRules::SpawnUnit(S, 0, EUnitType::Warrior, FMapGenerator::OffsetToAxial(4, 9)).Id;
	TestFalse(TEXT("pelna armia blokuje pole"), CivRules::CanEnter(M, S, *CivRules::FindUnit(S, Extra), Home));

	// Atak automatyczny: armia 0 kontra samotny wojownik 1; wojna wypowiadana automatycznie.
	const int32 Enemy = CivRules::SpawnUnit(S, 1, EUnitType::Warrior, Target).Id;
	TArray<FString> Log;
	TestFalse(TEXT("za daleko"), CivAI::AttackAuto(M, S, Cmd, FMapGenerator::OffsetToAxial(9, 9), &Log));
	TestTrue(TEXT("atak"), CivAI::AttackAuto(M, S, Cmd, Target, &Log));
	TestTrue(TEXT("wojna po ataku"), CivAI::IsAtWar(S, 0, 1));
	TestTrue(TEXT("obronca zniszczony"), CivRules::FindUnit(S, Enemy) == nullptr);
	TestTrue(TEXT("dowodca zyje"), CivRules::FindUnit(S, Cmd) != nullptr);
	TestTrue(TEXT("komunikat o wojnie"), Log.ContainsByPredicate([](const FString& L) { return L.Contains(TEXT("wypowiada wojnę")); }));

	// Zdobycie pustego miasta.
	FCivState S2 = State();
	const int32 Settler = CivRules::SpawnUnit(S2, 1, EUnitType::Settler, Target).Id;
	const int32 CityId = CivRules::FoundCity(M, S2, Settler, nullptr);
	const int32 Raider = CivRules::SpawnUnit(S2, 0, EUnitType::Warrior, Home).Id;
	TestTrue(TEXT("zajecie miasta"), CivAI::AttackAuto(M, S2, Raider, Target, nullptr));
	TestEqual(TEXT("miasto zmienilo wlasciciela"), CivRules::FindCity(S2, CityId)->Owner, 0);

	// Produkcja: miasto bez obroncy buduje wojownika; z obronca i miejscem na ekspansje - osadnika.
	FCivState S3 = State();
	S3.PlayerStates[0].AITargetCities = 4;
	const int32 Set3 = CivRules::SpawnUnit(S3, 0, EUnitType::Settler, Home).Id;
	const int32 City3 = CivRules::FoundCity(M, S3, Set3, nullptr);
	FCity& C3 = *CivRules::FindCity(S3, City3);
	FProduction P = CivAI::ChooseProduction(M, S3, C3);
	TestTrue(TEXT("najpierw obronca"), P.Kind == EProductionKind::Unit && P.Item == (int32)EUnitType::Warrior);
	CivRules::SpawnUnit(S3, 0, EUnitType::Warrior, Home);
	C3.Population = 3;
	P = CivAI::ChooseProduction(M, S3, C3);
	TestTrue(TEXT("potem ekspansja"), P.Kind == EProductionKind::Unit && P.Item == (int32)EUnitType::Settler);
	S3.PlayerStates[0].AITargetCities = 1;
	P = CivAI::ChooseProduction(M, S3, C3);
	TestFalse(TEXT("bez ekspansji brak osadnika"), P.Kind == EProductionKind::Unit && P.Item == (int32)EUnitType::Settler);
	TestTrue(TEXT("cos jest do zrobienia"), P.Kind != EProductionKind::None);

	// Bonus trudnosci.
	C3.Population = 6;
	S3.PlayerStates[0].YieldBonus = -20;
	const int32 Easy = CivRules::ComputeCityReport(M, S3, C3).Yield.Science;
	S3.PlayerStates[0].YieldBonus = 25;
	const int32 Hard = CivRules::ComputeCityReport(M, S3, C3).Yield.Science;
	TestTrue(TEXT("trudny AI ma wiecej nauki"), Hard > Easy);

	// Osobowosc deterministyczna.
	FCivPlayerState A, B;
	CivAI::InitPlayer(A, 1, 42, EDifficulty::Hard, true);
	CivAI::InitPlayer(B, 1, 42, EDifficulty::Hard, true);
	TestEqual(TEXT("ta sama osobowosc"), A.AIAggression, B.AIAggression);
	TestEqual(TEXT("bonus Hard"), A.YieldBonus, 25);
	CivAI::InitPlayer(B, 1, 42, EDifficulty::Easy, true);
	TestEqual(TEXT("kara Easy"), B.YieldBonus, -20);
	CivAI::InitPlayer(B, 1, 42, EDifficulty::Hard, false);
	TestEqual(TEXT("czlowiek bez bonusu"), B.YieldBonus, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAiPeaceAndRoadsTest, "ChickenCiv.AI.PeaceAndRoads", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAiPeaceAndRoadsTest::RunTest(const FString&)
{
	FHexMap M = Flat(12, 12);
	FCivState S = State(3);
	TArray<FString> Log;

	// Pokoj miedzy dwoma graczami: propozycja, potem przyjecie.
	CivAI::DeclareWar(S, 0, 1);
	TestTrue(TEXT("propozycja"), CivAI::ProposePeace(S, 0, 1, 5, &Log));
	TestTrue(TEXT("oferta u adresata"), S.PlayerStates[1].PeaceOffers.Contains(0));
	TestTrue(TEXT("wciaz wojna"), CivAI::IsAtWar(S, 0, 1));
	TestTrue(TEXT("przyjecie"), CivAI::ProposePeace(S, 1, 0, 6, &Log));
	TestFalse(TEXT("pokoj"), CivAI::IsAtWar(S, 0, 1));
	TestEqual(TEXT("blokada wojny"), *S.PlayerStates[0].PeaceSince.Find(1), 6);
	TestFalse(TEXT("brak propozycji bez wojny"), CivAI::ProposePeace(S, 0, 1, 7, &Log));

	// AI przyjmuje pokoj, gdy jest slabsze; agresywne i silniejsze odrzuca.
	CivAI::DeclareWar(S, 0, 2);
	S.PlayerStates[2].AIAggression = 90;
	for (int32 i = 0; i < 4; ++i) CivRules::SpawnUnit(S, 2, EUnitType::Warrior, FMapGenerator::OffsetToAxial(1 + i, 1));
	CivRules::SpawnUnit(S, 0, EUnitType::Warrior, FMapGenerator::OffsetToAxial(1, 9));
	TestFalse(TEXT("silne agresywne AI odrzuca"), CivAI::WouldAcceptPeace(S, 2, 0));
	S.PlayerStates[2].AIAggression = 20;
	TestTrue(TEXT("lagodne AI przyjmuje"), CivAI::WouldAcceptPeace(S, 2, 0));
	CivAI::ProposePeace(S, 0, 2, 20, &Log);
	CivAI::PlayTurn(M, S, 2, EDifficulty::Normal, 20, &Log);
	TestFalse(TEXT("AI zawarlo pokoj w swojej turze"), CivAI::IsAtWar(S, 0, 2));

	// Droga: robotnik buduje farme, potem droge; las z droga kosztuje 1.
	FHexMap M2 = Flat(10, 10);
	M2.Tiles[M2.IndexOf(FMapGenerator::OffsetToAxial(5, 5))].Terrain = ETerrainType::Forest;
	FCivState S2 = State();
	const FHexCoord Forest = FMapGenerator::OffsetToAxial(5, 5);
	const int32 Wk = CivRules::SpawnUnit(S2, 0, EUnitType::Worker, Forest).Id;
	TestEqual(TEXT("najpierw kopalnia"), (int32)CivRules::BestImprovementFor(M2, S2, Forest), (int32)EImprovement::Mine);
	CivRules::StartImprovement(M2, S2, Wk);
	for (int32 t = 0; t < CivRules::ImprovementTurns; ++t) CivRules::BeginPlayerTurn(M2, S2, 0);
	TestEqual(TEXT("kopalnia gotowa"), (int32)CivRules::ImprovementAt(S2, Forest), (int32)EImprovement::Mine);
	CivRules::BeginPlayerTurn(M2, S2, 0); // robotnik odzyskuje ruch w nastepnej turze
	TestEqual(TEXT("potem droga"), (int32)CivRules::BestImprovementFor(M2, S2, Forest), (int32)EImprovement::Road);
	TestFalse(TEXT("jeszcze bez drogi"), CivRules::HasRoad(S2, Forest));
	CivRules::StartImprovement(M2, S2, Wk);
	for (int32 t = 0; t < CivRules::ImprovementTurns; ++t) CivRules::BeginPlayerTurn(M2, S2, 0);
	TestTrue(TEXT("droga gotowa"), CivRules::HasRoad(S2, Forest));
	TestEqual(TEXT("kopalnia nadal jest"), (int32)CivRules::ImprovementAt(S2, Forest), (int32)EImprovement::Mine);
	TestEqual(TEXT("nic wiecej do zbudowania"), (int32)CivRules::BestImprovementFor(M2, S2, Forest), (int32)EImprovement::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAiSimulationTest, "ChickenCiv.AI.FullGameSimulation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAiSimulationTest::RunTest(const FString&)
{
	const FSimResult R1 = Simulate(7, 3, 120, EDifficulty::Normal);
	TestTrue(*FString::Printf(TEXT("niezmienniki gry (%s)"), *R1.Problem), R1.bInvariantsOk);
	TestTrue(TEXT("AI zalozylo miasta"), R1.Cities >= 3);
	TestTrue(TEXT("AI bada technologie"), R1.Techs >= 6);
	TestTrue(TEXT("AI ma jednostki"), R1.Units >= 3);
	AddInfo(FString::Printf(TEXT("Po 120 turach: %d miast, %d jednostek, %d technologii, wojen: %d"), R1.Cities, R1.Units, R1.Techs, R1.Wars));

	// Determinizm: ta sama gra daje ten sam wynik.
	const FSimResult R2 = Simulate(7, 3, 120, EDifficulty::Normal);
	TestEqual(TEXT("deterministyczny wynik"), R2.Hash, R1.Hash);

	// Inne ziarno i poziom trudnosci tez zachowuja niezmienniki.
	const FSimResult R3 = Simulate(21, 4, 80, EDifficulty::Hard);
	TestTrue(*FString::Printf(TEXT("niezmienniki 2 (%s)"), *R3.Problem), R3.bInvariantsOk);
	TestTrue(TEXT("miasta w drugiej grze"), R3.Cities >= 4);
	return true;
}

#endif
