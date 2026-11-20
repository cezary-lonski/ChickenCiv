#include "Core/ChickenGameState.h"
#include "Model/TechTree.h"
#include "Core/CivRules.h"
#include "Core/CivAI.h"
#include "Core/LlmStrategy.h"
#include "Map/MapGenerator.h"

bool AChickenGameState::NewGame(const FMapSettings& InSettings)
{
	Settings = InSettings;
	Settings.NumPlayers = FMath::Clamp(Settings.NumPlayers, 2, 8);
	Settings.NumHumans = FMath::Clamp(Settings.NumHumans, 1, Settings.NumPlayers);
	if (Settings.Seed == 0) Settings.Seed = FMapGenerator::RandomSeed();

	Grid.Build(Settings);

	Players.Reset();
	for (int32 i = 0; i < Settings.NumPlayers; ++i)
	{
		FPlayerInfo P;
		// Gracz 1 ma wybrana rase, kolejni kolejne (rozne dla pierwszych czterech).
		P.Race = (EChickenRace)((((int32)Settings.PlayerRace) + i) % 4);
		P.bIsAI = (i >= Settings.NumHumans);
		P.Name = P.bIsAI ? FString::Printf(TEXT("AI %d"), i + 1 - Settings.NumHumans)
			: (Settings.NumHumans == 1 ? FString(TEXT("Gracz")) : FString::Printf(TEXT("Gracz %d"), i + 1));
		// Kolor: wybrany dla gracza 1; przy wiecej niz 4 graczach reszta dostaje kolory z palety, by sie roznic.
		P.ColorIndex = (i == 0) ? Settings.PlayerColorIndex : (i >= 4 ? i : -1);
		Players.Add(P);
	}

	Civ = FCivState();
	Civ.PlayerStates.SetNum(Settings.NumPlayers);
	for (int32 i = 0; i < Players.Num(); ++i)
	{
		Civ.PlayerStates[i].Race = Players[i].Race;
		CivAI::InitPlayer(Civ.PlayerStates[i], i, Settings.Seed, Settings.Difficulty, Players[i].bIsAI);
	}
	Messages.Reset();
	Turn = 1;
	CurrentPlayer = 0;

	const bool bOk = CivRules::PlaceStartingUnits(Grid, Civ, Settings.NumPlayers, Settings.Seed);
	CivRules::BeginPlayerTurn(Grid, Civ, 0);
	OnTurnChanged.Broadcast();
	return bOk;
}

void AChickenGameState::MaybeRequestLlmStrategy(int32 Player)
{
	if (!Players.IsValidIndex(Player) || !Players[Player].bIsAI) return;
	if (GetNetMode() != NM_Standalone) return; // odpowiedzi LLM nie sa deterministyczne: w LAN wylaczone
	if (Turn % LlmStrategy::RequestEveryTurns != 1 || LlmPending.Contains(Player) || !LlmStrategy::HasApiKey()) return;

	LlmPending.Add(Player);
	const FString Prompt = LlmStrategy::BuildPrompt(Grid, Civ, Players, Player, Turn);
	TWeakObjectPtr<AChickenGameState> Weak(this);
	LlmStrategy::RequestStrategy(Prompt, [Weak, Player](bool bOk, const FString& Reply)
	{
		AChickenGameState* GS = Weak.Get();
		if (!GS) return;
		GS->LlmPending.Remove(Player);
		if (bOk && LlmStrategy::ApplyReply(Reply, GS->Civ, Player))
		{
			GS->AddMessage(FString::Printf(TEXT("Cywilizacja %d zmienia strategię"), Player + 1));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("LLM: brak uzytecznej strategii dla gracza %d: %s"), Player + 1, *Reply.Left(120));
		}
	});
}

void AChickenGameState::AddMessage(const FString& Text)
{
	Messages.Add(Text);
	while (Messages.Num() > 8) Messages.RemoveAt(0);
}

void AChickenGameState::EndTurn()
{
	if (Players.Num() == 0) return;

	for (int32 Guard = 0; Guard < Players.Num() + 1; ++Guard)
	{
		TArray<FString> Log;
		TArray<FString> AILog;
		MaybeRequestLlmStrategy(CurrentPlayer);
		if (Players[CurrentPlayer].bIsAI) CivAI::PlayTurn(Grid, Civ, CurrentPlayer, Settings.Difficulty, Turn, &AILog);
		CivRules::EndPlayerTurn(Grid, Civ, CurrentPlayer, Log);
		if (!Players[CurrentPlayer].bIsAI)
		{
			for (const FString& L : Log) AddMessage(L);
		}
		for (const FString& L : AILog) AddMessage(L);

		++CurrentPlayer;
		if (CurrentPlayer >= Players.Num())
		{
			CurrentPlayer = 0;
			++Turn;
		}
		CivRules::BeginPlayerTurn(Grid, Civ, CurrentPlayer);
		if (!Players[CurrentPlayer].bIsAI) break; // AI jeszcze nic nie robi: oddaje ture od razu
	}
	OnTurnChanged.Broadcast();
}

void AChickenGameState::ApplySave(const FChickenSaveData& Data)
{
	if (Data.Players.Num() > 0) Players = Data.Players;
	Civ = Data.Civ;
	Civ.PlayerStates.SetNum(Players.Num());
	for (int32 p = 0; p < Players.Num(); ++p)
	{
		Civ.PlayerStates[p].Race = Players[p].Race;
		CivRules::EnsureVisibility(Grid, Civ, p);
	}
	Turn = Data.Turn;
	CurrentPlayer = FMath::Clamp(Data.CurrentPlayer, 0, FMath::Max(0, Players.Num() - 1));
	CivRules::ComputeVisibility(Grid, Civ, CurrentPlayer);
	OnTurnChanged.Broadcast();
}
