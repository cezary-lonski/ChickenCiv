#pragma once

#include "CoreMinimal.h"
#include "HexCoord.generated.h"

/**
 * Wspolrzedne osiowe (q, r) heksa typu "pointy-top".
 * Kierunki 0..5 rosna zgodnie ze wzrostem kata (0 = +q), krawedz i lezy miedzy rogami i oraz i+1.
 */
USTRUCT(BlueprintType)
struct FHexCoord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Q = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 R = 0;

	FHexCoord() = default;
	FHexCoord(int32 InQ, int32 InR) : Q(InQ), R(InR) {}

	bool operator==(const FHexCoord& O) const { return Q == O.Q && R == O.R; }
	bool operator!=(const FHexCoord& O) const { return !(*this == O); }
	friend uint32 GetTypeHash(const FHexCoord& C) { return HashCombine(::GetTypeHash(C.Q), ::GetTypeHash(C.R)); }

	static FHexCoord Direction(int32 Dir)
	{
		static const int32 DQ[6] = { 1, 0, -1, -1, 0, 1 };
		static const int32 DR[6] = { 0, 1, 1, 0, -1, -1 };
		const int32 D = ((Dir % 6) + 6) % 6;
		return FHexCoord(DQ[D], DR[D]);
	}

	FHexCoord Neighbor(int32 Dir) const
	{
		const FHexCoord D = Direction(Dir);
		return FHexCoord(Q + D.Q, R + D.R);
	}

	int32 Distance(const FHexCoord& O) const
	{
		const int32 DQ = Q - O.Q;
		const int32 DR = R - O.R;
		return (FMath::Abs(DQ) + FMath::Abs(DR) + FMath::Abs(DQ + DR)) / 2;
	}

	/** Srodek heksa w swiecie (cm), Size = odleglosc srodka od rogu. */
	FVector2D ToWorld(float Size) const
	{
		const float S3 = FMath::Sqrt(3.f);
		return FVector2D(Size * S3 * (Q + R * 0.5f), Size * 1.5f * R);
	}

	/** Heks zawierajacy punkt swiata. */
	static FHexCoord FromWorld(const FVector2D& P, float Size)
	{
		const float S3 = FMath::Sqrt(3.f);
		const float FQ = (S3 / 3.f * P.X - 1.f / 3.f * P.Y) / Size;
		const float FR = (2.f / 3.f * P.Y) / Size;
		return Round(FQ, FR);
	}

	static FHexCoord Round(float FQ, float FR)
	{
		const float FS = -FQ - FR;
		float RQ = FMath::RoundToFloat(FQ);
		float RR = FMath::RoundToFloat(FR);
		const float RS = FMath::RoundToFloat(FS);
		const float DQ = FMath::Abs(RQ - FQ);
		const float DR = FMath::Abs(RR - FR);
		const float DS = FMath::Abs(RS - FS);
		if (DQ > DR && DQ > DS) { RQ = -RR - RS; }
		else if (DR > DS) { RR = -RQ - RS; }
		return FHexCoord((int32)RQ, (int32)RR);
	}

	/** Pozycja rogu i (kat 60*i - 30 stopni) wzgledem srodka. */
	static FVector2D CornerOffset(int32 Corner, float Size)
	{
		const float A = FMath::DegreesToRadians(60.f * Corner - 30.f);
		return FVector2D(Size * FMath::Cos(A), Size * FMath::Sin(A));
	}

	FString ToString() const { return FString::Printf(TEXT("(%d, %d)"), Q, R); }
};
