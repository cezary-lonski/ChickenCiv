#pragma once

#include "CoreMinimal.h"
#include "Core/BattleRules.h"

/** Uklad planszy bitwy na ekranie (wspolny dla HUD i klikniec). Heksy "pointy-top", ukl. odd-r. */
namespace BattleLayout
{
	constexpr float HexSize = 34.f;

	inline FVector2D CellCenter(int32 Col, int32 Row, float ViewW)
	{
		const float W = HexSize * 1.7320508f;
		const float OriginX = (ViewW - W * (FBattle::Width + 0.5f)) * 0.5f + W * 0.5f;
		const float OriginY = 140.f;
		return FVector2D(OriginX + W * (Col + 0.5f * (Row & 1)), OriginY + HexSize * 1.5f * Row);
	}

	inline FVector2D HexCenter(const FHexCoord& H, float ViewW)
	{
		int32 C, R;
		BattleRules::HexToCell(H, C, R);
		return CellCenter(C, R, ViewW);
	}

	/** Heks planszy pod punktem ekranu; false, gdy poza plansza. */
	inline bool HexAt(const FVector2D& P, float ViewW, FHexCoord& Out)
	{
		float Best = HexSize * 0.95f;
		bool bFound = false;
		for (int32 Row = 0; Row < FBattle::Height; ++Row)
			for (int32 Col = 0; Col < FBattle::Width; ++Col)
			{
				const float D = FVector2D::Distance(P, CellCenter(Col, Row, ViewW));
				if (D < Best) { Best = D; Out = BattleRules::CellToHex(Col, Row); bFound = true; }
			}
		return bFound;
	}
}
