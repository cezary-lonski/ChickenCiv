#pragma once

#include "CoreMinimal.h"
#include "Map/CivTypes.h"
#include "ChickenCommand.generated.h"

/** Polecenia gracza. W LAN rozsylane do wszystkich maszyn, ktore licza te sama deterministyczna gre. */
UENUM()
enum class ECmd : uint8
{
	EndTurn,
	OrderMove,          // A = jednostka, B = Q, C = R
	FoundCity,          // A = jednostka
	SkipUnit,           // A = jednostka
	SetProduction,      // A = miasto, B = rodzaj (EProductionKind), C = pozycja
	SetResearch,        // A = technologia (ETech)
	CreateArmy,         // A = dowodca
	JoinArmy,           // A = jednostka
	LeaveArmy,          // A = jednostka
	StartAttack,        // A = jednostka, B = Q, C = R
	BattleClick,        // A = Q, B = R, C = 1 gdy tryb lasera Gwiazdy Smierci
	BattleEndUnitTurn,
	BattleAutoResolve,
	CloseBattle,
	BuildImprovement,   // A = robotnik
	ProposePeace,       // proponuje pokoj wszystkim wrogom
	NewGame             // Settings (Seed ustalony przez gospodarza)
};

USTRUCT()
struct FChickenCommand
{
	GENERATED_BODY()

	UPROPERTY() ECmd Type = ECmd::EndTurn;
	UPROPERTY() int32 A = 0;
	UPROPERTY() int32 B = 0;
	UPROPERTY() int32 C = 0;
	UPROPERTY() FMapSettings Settings;

	static FChickenCommand Make(ECmd InType, int32 InA = 0, int32 InB = 0, int32 InC = 0)
	{
		FChickenCommand Cmd;
		Cmd.Type = InType;
		Cmd.A = InA;
		Cmd.B = InB;
		Cmd.C = InC;
		return Cmd;
	}
};
