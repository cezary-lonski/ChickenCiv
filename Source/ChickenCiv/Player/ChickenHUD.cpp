#include "Player/ChickenHUD.h"
#include "Player/StrategyPlayerController.h"
#include "Core/ChickenGameState.h"
#include "Core/ChickenGame.h"
#include "Core/CivRules.h"
#include "Model/TechTree.h"
#include "Core/ArmyRules.h"
#include "Core/BattleRules.h"
#include "Core/LlmStrategy.h"
#include "Core/AccountAuth.h"
#include "Player/BattleLayout.h"
#include "Map/HexMapActor.h"
#include "Map/WorldViewActor.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

namespace
{
	struct FTextCursor
	{
		UCanvas* Canvas;
		UFont* Font;
		float X;
		float Y;

		void Line(const FString& Text, const FLinearColor& Color, float Scale = 1.1f)
		{
			Canvas->SetDrawColor(Color.ToFColor(true));
			float W = 0.f, H = 0.f;
			Canvas->TextSize(Font, Text, W, H, Scale, Scale);
			Canvas->DrawText(Font, Text, X, Y, Scale, Scale);
			Y += H + 3.f;
		}
	};
}

void AChickenHUD::DrawLogin(const AChickenGame* GM)
{
	const FLoginState& L = GM->GetLogin();
	if (!Canvas || !GEngine) return;
	UFont* Font = GEngine->GetMediumFont();
	const float W = (float)Canvas->SizeX, H = (float)Canvas->SizeY;
	DrawRect(FLinearColor(0.03f, 0.04f, 0.07f, 0.98f), 0.f, 0.f, W, H);

	float TW = 0.f, TH = 0.f;
	Canvas->SetDrawColor(255, 205, 60);
	Canvas->TextSize(Font, TEXT("CHICKEN CIV"), TW, TH, 3.2f, 3.2f);
	Canvas->DrawText(Font, TEXT("CHICKEN CIV"), (W - TW) * 0.5f, 50.f, 3.2f, 3.2f);
	Canvas->SetDrawColor(200, 200, 210);
	Canvas->TextSize(Font, TEXT("Zaloguj się"), TW, TH, 1.5f, 1.5f);
	Canvas->DrawText(Font, TEXT("Zaloguj się"), (W - TW) * 0.5f, 130.f, 1.5f, 1.5f);

	const float X0 = W * 0.5f - 300.f;
	float Y = 200.f;
	if (L.bWaiting)
	{
		Canvas->SetDrawColor(255, 255, 255);
		Canvas->DrawText(Font, FString::Printf(TEXT("Otwarto przeglądarkę: %s"), *L.Device.VerificationUrl), X0, Y, 1.2f, 1.2f);
		Canvas->DrawText(Font, TEXT("Wpisz tam ten kod i potwierdź logowanie:"), X0, Y + 40.f, 1.2f, 1.2f);
		Canvas->SetDrawColor(255, 230, 90);
		Canvas->DrawText(Font, L.Device.UserCode, X0, Y + 90.f, 3.0f, 3.0f);
		Canvas->SetDrawColor(160, 160, 170);
		Canvas->DrawText(Font, TEXT("Esc: anuluj"), X0, Y + 190.f, 1.0f, 1.0f);
	}
	else if (L.FormMode != 0)
	{
		Canvas->SetDrawColor(255, 255, 255);
		Canvas->DrawText(Font, L.FormMode == 2 ? TEXT("Nowe konto lokalne (e-mail + hasło)") : TEXT("Logowanie e-mail"), X0, Y, 1.3f, 1.3f);
		const FString Masked = FString::ChrN(L.Password.Len(), TEXT('*'));
		for (int32 f = 0; f < 2; ++f)
		{
			const bool bSel = L.Field == f;
			if (bSel) DrawRect(FLinearColor(0.2f, 0.25f, 0.4f, 0.9f), X0 - 10.f, Y + 50.f + f * 50.f - 4.f, 640.f, 38.f);
			Canvas->SetDrawColor(bSel ? FColor(255, 255, 255) : FColor(190, 190, 200));
			Canvas->DrawText(Font, f == 0 ? TEXT("E-mail") : TEXT("Hasło"), X0, Y + 50.f + f * 50.f, 1.2f, 1.2f);
			Canvas->SetDrawColor(255, 230, 120);
			Canvas->DrawText(Font, (f == 0 ? L.Email : Masked) + (bSel ? TEXT("_") : TEXT("")), X0 + 160.f, Y + 50.f + f * 50.f, 1.2f, 1.2f);
		}
		Canvas->SetDrawColor(160, 160, 170);
		Canvas->DrawText(Font, TEXT("Tab: pole   Enter: dalej / zatwierdź   Ctrl+V: wklej   Esc: wróć"), X0, Y + 170.f, 0.95f, 0.95f);
	}
	else
	{
		struct FRow { const TCHAR* Label; bool bAvailable; const TCHAR* Hint; };
		const FRow Rows[] = {
			{ TEXT("Zaloguj przez Google"), Auth::HasClientId(Auth::EProvider::Google), TEXT("brak GOOGLE_CLIENT_ID (README)") },
			{ TEXT("Zaloguj przez Microsoft"), Auth::HasClientId(Auth::EProvider::Microsoft), TEXT("brak MICROSOFT_CLIENT_ID (README)") },
			{ TEXT("Zaloguj e-mailem (konto lokalne)"), true, TEXT("") },
			{ TEXT("Załóż konto e-mail (lokalne)"), true, TEXT("") },
			{ TEXT("Graj jako gość"), true, TEXT("") },
		};
		for (int32 i = 0; i < UE_ARRAY_COUNT(Rows); ++i)
		{
			const bool bSel = L.Row == i;
			if (bSel) DrawRect(FLinearColor(0.2f, 0.25f, 0.4f, 0.9f), X0 - 14.f, Y - 4.f, 640.f, 36.f);
			Canvas->SetDrawColor(Rows[i].bAvailable ? (bSel ? FColor(255, 255, 255) : FColor(190, 190, 200)) : FColor(110, 110, 120));
			Canvas->DrawText(Font, FString(bSel ? TEXT("> ") : TEXT("  ")) + Rows[i].Label + (Rows[i].bAvailable ? TEXT("") : *FString::Printf(TEXT("   (%s)"), Rows[i].Hint)), X0, Y, 1.2f, 1.2f);
			Y += 40.f;
		}
		Canvas->SetDrawColor(160, 160, 170);
		Canvas->DrawText(Font, TEXT("Góra/Dół: wybór   Enter: wykonaj"), X0, Y + 20.f, 0.95f, 0.95f);
	}
	if (!L.Message.IsEmpty())
	{
		Canvas->SetDrawColor(255, 160, 80);
		Canvas->DrawText(Font, L.Message, X0, H - 90.f, 1.2f, 1.2f);
	}
}

void AChickenHUD::DrawMenu(const AChickenGame* GM)
{
	const FMenuState& Menu = GM->GetMenu();
	if (!Canvas || !GEngine) return;
	UFont* Font = GEngine->GetMediumFont();
	const float W = (float)Canvas->SizeX;
	const float H = (float)Canvas->SizeY;

	DrawRect(FLinearColor(0.03f, 0.04f, 0.07f, Menu.bGameStarted ? 0.85f : 0.97f), 0.f, 0.f, W, H);

	// Tytul
	Canvas->SetDrawColor(255, 205, 60);
	float TW = 0.f, TH = 0.f;
	Canvas->TextSize(Font, TEXT("CHICKEN CIV"), TW, TH, 3.2f, 3.2f);
	Canvas->DrawText(Font, TEXT("CHICKEN CIV"), (W - TW) * 0.5f, 50.f, 3.2f, 3.2f);
	Canvas->SetDrawColor(180, 180, 190);
	Canvas->TextSize(Font, TEXT("kurczakowa cywilizacja"), TW, TH, 1.2f, 1.2f);
	Canvas->DrawText(Font, TEXT("kurczakowa cywilizacja"), (W - TW) * 0.5f, 50.f + 60.f, 1.2f, 1.2f);

	static const TCHAR* SizeNames[] = { TEXT("Mała (40x24)"), TEXT("Średnia (60x38)"), TEXT("Duża (80x52)") };
	static const TCHAR* DiffNames[] = { TEXT("Łatwy"), TEXT("Normalny"), TEXT("Trudny") };
	static const TCHAR* RacePerks[] = {
		TEXT("Czerwone – nauka x3"), TEXT("Żółte – siła x2"),
		TEXT("Niebieskie – statki: szybkość x10, siła x3, 5x taniej"), TEXT("Zielone – strażnik niszczy armie atakujące miasto") };

	const FMapSettings& S = Menu.Settings;
	const float X0 = W * 0.5f - 330.f;
	float Y = 170.f;
	const float Step = 34.f;
	for (int32 i = 0; i < (int32)EMenuRow::Count; ++i)
	{
		const EMenuRow Row = (EMenuRow)i;
		if (!GM->IsMenuRowVisible(Row)) continue;

		FString Label, Value;
		switch (Row)
		{
		case EMenuRow::Resume: Label = TEXT("Wznów grę"); break;
		case EMenuRow::NewGame:
			Label = GM->IsNetServer() ? TEXT("ROZPOCZNIJ GRĘ LAN") : TEXT("NOWA GRA");
			if (GM->IsNetServer()) Value = FString::Printf(TEXT("połączonych graczy: %d"), GM->GetNumLobbyPlayers());
			break;
		case EMenuRow::Size: Label = TEXT("Rozmiar mapy"); Value = SizeNames[(int32)S.Size]; break;
		case EMenuRow::Civs: Label = TEXT("Liczba cywilizacji"); Value = FString::FromInt(S.NumPlayers); break;
		case EMenuRow::Humans: Label = TEXT("Gracze ludzcy (hot seat)"); Value = FString::FromInt(S.NumHumans) + (S.NumHumans > 1 ? TEXT("  – kolejno przy jednym komputerze") : TEXT("")); break;
		case EMenuRow::Difficulty: Label = TEXT("Trudność AI"); Value = DiffNames[(int32)S.Difficulty]; break;
		case EMenuRow::Race: Label = TEXT("Rasa kurczaka"); Value = RacePerks[(int32)S.PlayerRace]; break;
		case EMenuRow::Color: Label = TEXT("Kolor kurczaka"); Value = CivColors::PaletteName(S.PlayerColorIndex); break;
		case EMenuRow::Seed: Label = TEXT("Ziarno mapy"); Value = S.Seed == 0 ? FString(TEXT("losowe  (wpisz cyfry, Backspace kasuje)")) : FString::FromInt(S.Seed); break;
		case EMenuRow::ApiKey: Label = TEXT("Klucz API LLM (AI)"); Value = LlmStrategy::HasApiKey() ? TEXT("ustawiony  (Enter/Ctrl+V: wklej nowy, Del: usuń)") : TEXT("brak – AI gra klasycznie  (Ctrl+V: wklej ze schowka)"); break;
		case EMenuRow::Account:
			Label = TEXT("Konto");
			Value = Auth::Current().IsLoggedIn() ? FString::Printf(TEXT("%s (%s)  –  Enter: zmień konto"), *Auth::DisplayName(), *Auth::Current().Provider) : FString(TEXT("niezalogowany  –  Enter: zaloguj"));
			break;
		case EMenuRow::HostLan: Label = TEXT("Hostuj grę LAN"); Value = TEXT("Enter: otwórz poczekalnię (inni dołączają po adresie IP)"); break;
		case EMenuRow::JoinAddress: Label = TEXT("Adres gospodarza"); Value = Menu.JoinAddress + (Menu.Row == i ? TEXT("_") : TEXT("")) + (Menu.Row == i ? TEXT("   (cyfry i kropka)") : TEXT("")); break;
		case EMenuRow::JoinLan: Label = TEXT("Dołącz do gry LAN"); break;
		case EMenuRow::Load: Label = TEXT("Wczytaj grę"); break;
		case EMenuRow::Save: Label = TEXT("Zapisz grę"); break;
		case EMenuRow::Quit: Label = TEXT("Wyjdź"); break;
		default: break;
		}

		const bool bSel = (Menu.Row == i);
		if (bSel) DrawRect(FLinearColor(0.2f, 0.25f, 0.4f, 0.9f), X0 - 14.f, Y - 4.f, 700.f, Step - 2.f);
		Canvas->SetDrawColor(bSel ? FColor(255, 255, 255) : FColor(190, 190, 200));
		Canvas->DrawText(Font, (bSel ? TEXT("> ") : TEXT("  ")) + Label, X0, Y, 1.2f, 1.2f);
		if (!Value.IsEmpty())
		{
			Canvas->SetDrawColor(bSel ? FColor(255, 230, 120) : FColor(170, 170, 120));
			Canvas->DrawText(Font, Value, X0 + 290.f, Y, 1.1f, 1.1f);
		}
		if (Row == EMenuRow::Color)
		{
			const FLinearColor Sw = S.PlayerColorIndex >= 0 ? CivColors::Palette(S.PlayerColorIndex) : CivColors::RaceColor(S.PlayerRace);
			DrawRect(Sw, X0 + 250.f, Y + 2.f, 26.f, 20.f);
		}
		Y += Step;
	}

	if (GM->IsNetClient() && !Menu.bGameStarted)
	{
		Canvas->SetDrawColor(120, 255, 160);
		Canvas->DrawText(Font, FString::Printf(TEXT("Połączono z gospodarzem jako gracz %d. Czekaj na rozpoczęcie gry…"), GM->LocalPlayerIndex() + 1), X0, 140.f, 1.2f, 1.2f);
	}
	if (!Menu.Message.IsEmpty())
	{
		Canvas->SetDrawColor(255, 160, 80);
		Canvas->DrawText(Font, Menu.Message, X0, Y + 10.f, 1.2f, 1.2f);
	}
	Canvas->SetDrawColor(160, 160, 170);
	Canvas->DrawText(Font, TEXT("Góra/Dół: wybór   Lewo/Prawo: zmiana   Enter: wykonaj   Esc: wróć do gry"), 16.f, H - 26.f, 0.9f, 0.9f);
}

void AChickenHUD::DrawBattle(const AChickenGame* GM)
{
	const FBattle* B = GM->GetBattle();
	if (!B || !Canvas || !GEngine) return;
	UFont* Font = GEngine->GetMediumFont();
	const float ViewW = (float)Canvas->SizeX;

	// Tlo
	DrawRect(FLinearColor(0.03f, 0.04f, 0.07f, 0.96f), 0.f, 0.f, ViewW, (float)Canvas->SizeY);

	// Naglowek
	FTextCursor T{ Canvas, Font, 24.f, 14.f };
	const FBattleUnit* Cur = B->Current();
	T.Line(FString::Printf(TEXT("BITWA – runda %d   |   Nuggetsy: Ty +%d, wróg +%d"), B->Round, B->Nuggets[0], B->Nuggets[1]), FLinearColor::White, 1.4f);
	if (B->Winner >= 0)
	{
		T.Line(B->Winner == 0 ? TEXT("ZWYCIĘSTWO!  Enter: zakończ bitwę") : TEXT("PORAŻKA.  Enter: zakończ bitwę"), B->Winner == 0 ? FLinearColor(0.4f, 1.f, 0.4f) : FLinearColor(1.f, 0.4f, 0.4f), 1.3f);
	}
	else if (Cur)
	{
		T.Line(FString::Printf(TEXT("Teraz: %s (%s)   ruch: %d   zdrowie: %d/%d%s"), CivDefs::Unit(Cur->Type).Name, Cur->Side == 0 ? TEXT("Ty") : TEXT("wróg"), Cur->MovesLeft, Cur->Hp, Cur->MaxHp,
			Cur->AimTurns > 0 ? *FString::Printf(TEXT("   celuje (%d)"), Cur->AimTurns) : TEXT("")), FLinearColor(1.f, 1.f, 0.4f), 1.1f);
		if (GM->IsDeathStarMode()) T.Line(TEXT("Laser Gwiazdy Śmierci: kliknij wrogą jednostkę"), FLinearColor(1.f, 0.5f, 0.2f), 1.1f);
	}

	// Pola osiagalne dla biezacej jednostki gracza.
	TArray<FHexCoord> Reach;
	if (B->Winner < 0 && Cur && Cur->Side == 0) BattleRules::GetReachable(*B, *Cur, Reach);

	const float S = BattleLayout::HexSize;
	for (int32 Row = 0; Row < FBattle::Height; ++Row)
	{
		for (int32 Col = 0; Col < FBattle::Width; ++Col)
		{
			const FVector2D C = BattleLayout::CellCenter(Col, Row, ViewW);
			const FHexCoord H = BattleRules::CellToHex(Col, Row);
			const FLinearColor Edge = Reach.Contains(H) ? FLinearColor(0.4f, 0.9f, 0.4f) : FLinearColor(0.3f, 0.35f, 0.45f);
			for (int32 i = 0; i < 6; ++i)
			{
				const float A0 = FMath::DegreesToRadians(60.f * i - 30.f);
				const float A1 = FMath::DegreesToRadians(60.f * (i + 1) - 30.f);
				DrawLine(C.X + S * FMath::Cos(A0), C.Y + S * FMath::Sin(A0), C.X + S * FMath::Cos(A1), C.Y + S * FMath::Sin(A1), Edge, 1.5f);
			}
		}
	}

	// Jednostki (i zwloki) oraz cele czarnych dziur.
	for (const FBattleUnit& U : B->Units)
	{
		const FVector2D C = BattleLayout::HexCenter(U.Pos, ViewW);
		const AChickenGameState* BGS = GM->GetChickenState();
		const int32 BOwner = B->Owner[U.Side];
		const FLinearColor Col = (BGS && BGS->Players.IsValidIndex(BOwner)) ? CivColors::ForPlayer(BGS->Players[BOwner]) : CivColors::RaceColor(B->Race[U.Side]);
		if (!U.bAlive)
		{
			Canvas->SetDrawColor(120, 120, 120);
			Canvas->DrawText(Font, TEXT("x"), C.X - 5.f, C.Y - 9.f, 1.2f, 1.2f);
			continue;
		}
		const float Half = S * 0.55f;
		DrawRect(Col, C.X - Half, C.Y - Half, Half * 2.f, Half * 2.f);
		if (Cur && Cur->Id == U.Id && B->Winner < 0) DrawRect(FLinearColor(1.f, 1.f, 0.2f, 0.9f), C.X - Half - 3.f, C.Y - Half - 3.f, Half * 2.f + 6.f, 3.f);
		// Pasek zdrowia
		const float Frac = FMath::Clamp((float)U.Hp / (float)FMath::Max(1, U.MaxHp), 0.f, 1.f);
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.8f), C.X - Half, C.Y + Half + 2.f, Half * 2.f, 4.f);
		DrawRect(FLinearColor(1.f - Frac, Frac, 0.1f, 1.f), C.X - Half, C.Y + Half + 2.f, Half * 2.f * Frac, 4.f);
		FString Label = CivDefs::Unit(U.Type).Name;
		Label = Label.Left(3);
		Canvas->SetDrawColor(0, 0, 0);
		Canvas->DrawText(Font, Label, C.X - Half + 3.f, C.Y - 8.f, 0.8f, 0.8f);
		if (U.bCommander)
		{
			Canvas->SetDrawColor(255, 230, 40);
			Canvas->DrawText(Font, TEXT("*"), C.X + Half - 8.f, C.Y - Half - 4.f, 1.0f, 1.0f);
		}
		if (U.AimTurns > 0)
		{
			const FVector2D A = BattleLayout::HexCenter(U.AimTarget, ViewW);
			Canvas->SetDrawColor(180, 80, 255);
			Canvas->DrawText(Font, TEXT("O"), A.X - 6.f, A.Y - 10.f, 1.5f, 1.5f);
		}
	}

	// Pomoc
	Canvas->SetDrawColor(200, 200, 200);
	Canvas->DrawText(Font, TEXT("LPM: ruch / atak / umiejętność (medyk – leczy sojusznika, wskrzeszacz – zwłoki, miotacz – cel dziury)   Spacja: koniec tury jednostki   R: auto-rozstrzygnięcie   D: laser Gwiazdy Śmierci"), 16.f, Canvas->SizeY - 26.f, 0.85f, 0.85f);
}

void AChickenHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas || !GEngine) return;

	const AChickenGameState* GS = GetWorld()->GetGameState<AChickenGameState>();
	const AChickenGame* GM = AChickenGame::Get(GetWorld());
	const AStrategyPlayerController* PC = Cast<AStrategyPlayerController>(GetOwningPlayerController());
	if (!GS) return;

	if (GM && GM->IsIntro())
	{
		DrawRect(FLinearColor::Black, 0.f, 0.f, (float)Canvas->SizeX, (float)Canvas->SizeY);
		if (UTexture* Tex = GM->GetIntroTexture())
		{
			// Film 16:9 (lub inny) wpisany w ekran z zachowaniem proporcji.
			const float TW = FMath::Max(1.f, (float)Tex->GetSurfaceWidth()), TH = FMath::Max(1.f, (float)Tex->GetSurfaceHeight());
			const float Scale = FMath::Min(Canvas->SizeX / TW, Canvas->SizeY / TH);
			const float W = TW * Scale, H = TH * Scale;
			DrawTexture(Tex, (Canvas->SizeX - W) * 0.5f, (Canvas->SizeY - H) * 0.5f, W, H, 0.f, 0.f, 1.f, 1.f, FLinearColor::White, BLEND_Opaque);
		}
		Canvas->SetDrawColor(150, 150, 150);
		Canvas->DrawText(GEngine->GetMediumFont(), TEXT("Spacja / klik / Esc: pomiń"), 16.f, Canvas->SizeY - 28.f, 0.9f, 0.9f);
		return;
	}
	if (GM && GM->IsHandoff())
	{
		// Pelny ekran bez informacji o grze: nastepny gracz nie widzi planszy poprzedniego.
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 1.f), 0.f, 0.f, (float)Canvas->SizeX, (float)Canvas->SizeY);
		UFont* HF = GEngine->GetMediumFont();
		const FPlayerInfo* HP = GS->GetCurrentPlayerInfo();
		const FString Title = FString::Printf(TEXT("Tura %d – %s"), GS->Turn, HP ? *HP->Name : TEXT("?"));
		float TW = 0.f, TH = 0.f;
		Canvas->SetDrawColor(HP ? CivColors::ForPlayer(*HP).ToFColor(true) : FColor::White);
		Canvas->TextSize(HF, Title, TW, TH, 2.4f, 2.4f);
		Canvas->DrawText(HF, Title, (Canvas->SizeX - TW) * 0.5f, Canvas->SizeY * 0.4f, 2.4f, 2.4f);
		const TCHAR* Hint = TEXT("Przekaż komputer tej osobie i naciśnij Enter");
		Canvas->SetDrawColor(200, 200, 200);
		Canvas->TextSize(HF, Hint, TW, TH, 1.3f, 1.3f);
		Canvas->DrawText(HF, Hint, (Canvas->SizeX - TW) * 0.5f, Canvas->SizeY * 0.4f + 70.f, 1.3f, 1.3f);
		return;
	}
	if (GM && GM->IsLoginOpen())
	{
		DrawLogin(GM);
		return;
	}
	if (GM && GM->IsMenuOpen())
	{
		DrawMenu(GM);
		return;
	}
	if (GM && GM->IsInBattle())
	{
		DrawBattle(GM);
		return;
	}

	UFont* Font = GEngine->GetMediumFont();
	const int32 Viewer = GM ? GM->GetViewer() : GS->CurrentPlayer;
	const FLinearColor Gray(0.75f, 0.75f, 0.75f);

	// ---- Pasek gorny ----
	{
		FTextCursor T{ Canvas, Font, 16.f, 12.f };
		if (const FPlayerInfo* P = GS->GetCurrentPlayerInfo())
		{
			FString Head = FString::Printf(TEXT("Tura %d   |   %s (%s)"), GS->Turn, *P->Name, *CivNames::RaceName(P->Race));
			FString ResearchLine;
			FLinearColor ResearchColor(0.6f, 0.9f, 1.f);
			if (const FCivPlayerState* PS = GS->GetPlayerState(Viewer))
			{
				const FYield Inc = CivRules::ComputeIncome(GS->Grid, GS->Civ, Viewer);
				Head += FString::Printf(TEXT("   |   Złoto: %d (%+d)   Nauka: %d (%+d)   Nuggetsy: %d"), PS->Gold, Inc.Gold, PS->Science, Inc.Science, PS->Nuggets);
				if (PS->Researching != ETech::None)
				{
					const FTechDef& TD = CivTech::Def(PS->Researching);
					ResearchLine = FString::Printf(TEXT("Badanie: %s  %d/%d  (tur: %d)"), TD.Name, PS->ResearchProgress, TD.Cost, CivTech::TurnsLeft(*PS, Inc.Science));
				}
				else
				{
					ResearchLine = TEXT("Badanie: brak – naciśnij T");
					ResearchColor = FLinearColor(1.f, 0.5f, 0.4f);
				}
			}
			T.Line(Head, FLinearColor::White, 1.4f);
			if (!ResearchLine.IsEmpty()) T.Line(ResearchLine, ResearchColor, 1.0f);
		}
		T.Line(FString::Printf(TEXT("Ziarno mapy: %d"), GS->Settings.Seed), Gray, 0.9f);
	}

	// ---- Etykiety miast ----
	if (const FCivPlayerState* PS = GS->GetPlayerState(Viewer))
	{
		for (const FCity& C : GS->Civ.Cities)
		{
			const int32 Idx = GS->Grid.IndexOf(C.Pos);
			if (!PS->Visibility.IsValidIndex(Idx) || PS->Visibility[Idx] == 0) continue;
			const FVector W = FVector(C.Pos.ToWorld(FHexMap::HexSize), 150.f);
			const FVector S = Project(W);
			if (S.Z <= 0.f) continue;
			const FLinearColor OwnerColor = GS->Players.IsValidIndex(C.Owner) ? CivColors::ForPlayer(GS->Players[C.Owner]) : CivColors::RaceColor(EChickenRace::Red);
			const FString Label = FString::Printf(TEXT("%s (%d)"), *C.Name, C.Population);
			float TW = 0.f, TH = 0.f;
			Canvas->TextSize(Font, Label, TW, TH, 1.0f, 1.0f);
			Canvas->SetDrawColor(0, 0, 0, 170);
			Canvas->DrawTile(Canvas->DefaultTexture, S.X - TW * 0.5f - 4.f, S.Y - 2.f, TW + 8.f, TH + 4.f, 0, 0, 1, 1);
			Canvas->SetDrawColor(OwnerColor.ToFColor(true));
			Canvas->DrawText(Font, Label, S.X - TW * 0.5f, S.Y, 1.0f, 1.0f);
		}
	}

	// ---- Etykiety armii (liczba jednostek nad dowodca) ----
	if (const FCivPlayerState* PS = GS->GetPlayerState(Viewer))
	{
		for (const FArmy& A : GS->Civ.Armies)
		{
			const FUnit* Cmd = CivRules::FindUnit(GS->Civ, A.CommanderId);
			if (!Cmd) continue;
			const int32 Idx = GS->Grid.IndexOf(Cmd->Pos);
			if (!PS->Visibility.IsValidIndex(Idx) || (A.Owner != Viewer && PS->Visibility[Idx] != 2)) continue;
			const FVector S = Project(FVector(Cmd->Pos.ToWorld(FHexMap::HexSize), 130.f));
			if (S.Z <= 0.f) continue;
			Canvas->SetDrawColor(255, 255, 255);
			Canvas->DrawText(Font, FString::Printf(TEXT("Armia %d"), A.Members.Num() + 1), S.X - 22.f, S.Y, 0.9f, 0.9f);
		}
	}

	// ---- Panel wybranej jednostki / miasta / heksa (lewy dol) ----
	const float PanelY = Canvas->SizeY - 250.f;
	FTextCursor P{ Canvas, Font, 16.f, PanelY };

	// Ostatnie komunikaty
	for (const FString& M : GS->Messages) P.Line(M, FLinearColor(0.8f, 0.9f, 1.f), 0.95f);
	P.Y += 6.f;

	if (PC && PC->GetSelectedUnitId() != 0)
	{
		if (const FUnit* U = CivRules::FindUnit(GS->Civ, PC->GetSelectedUnitId()))
		{
			P.Line(FString::Printf(TEXT("%s   ruch: %d/%d%s"), U->Def().Name, U->MovesLeft, U->Def().Moves, U->Path.Num() > 0 ? TEXT("   [w drodze]") : TEXT("")), FLinearColor(1.f, 1.f, 0.4f), 1.3f);
			if (U->Type == EUnitType::Settler) P.Line(TEXT("B: załóż miasto"), FLinearColor::White);
			if (U->Type == EUnitType::Worker) P.Line(U->WorkTurns > 0 ? FString::Printf(TEXT("Pracuje: jeszcze %d tur"), U->WorkTurns) : FString(TEXT("I: ulepsz pole (farma / kopalnia, 3 tury)")), FLinearColor::White);
			if (U->Type == EUnitType::Commander && U->ArmyId == 0) P.Line(TEXT("A: załóż armię"), FLinearColor::White);
			if (U->bCommander)
			{
				if (const FArmy* Army = ArmyRules::FindArmy(GS->Civ, U->ArmyId))
				{
					int32 N = 0, Sp = 0;
					ArmyRules::CountMembers(GS->Civ, *Army, N, Sp);
					P.Line(FString::Printf(TEXT("Armia: zwykłe %d/10, specjalne %d/10   J: dołącz jednostkę z pola   L: wypuść ostatnią"), N, Sp), FLinearColor(0.6f, 1.f, 0.6f), 1.0f);
				}
			}
			else if (!U->IsCivilian() && U->Type != EUnitType::Commander)
			{
				P.Line(TEXT("J: dołącz do armii dowódcy na tym polu"), Gray, 0.95f);
			}
			P.Line(TEXT("PPM: idź w to miejsce   K: pomiń   Tab: następna jednostka"), Gray, 0.95f);
		}
	}
	else if (PC && PC->GetSelectedCityId() != 0)
	{
		if (const FCity* City = CivRules::FindCity(GS->Civ, PC->GetSelectedCityId()))
		{
			const FCityReport R = CivRules::ComputeCityReport(GS->Grid, GS->Civ, *City);
			// Panel miasta po prawej stronie ekranu.
			FTextCursor Q{ Canvas, Font, Canvas->SizeX - 420.f, 90.f };
			Q.Line(FString::Printf(TEXT("%s   (populacja %d)"), *City->Name, City->Population), FLinearColor(1.f, 1.f, 0.4f), 1.4f);
			Q.Line(FString::Printf(TEXT("Jedzenie %d (zużycie %d, %+d)   Wzrost: %d/%d"), R.Yield.Food, R.FoodNeeded, R.FoodSurplus(), City->FoodStock, R.GrowthThreshold), FLinearColor::White, 1.0f);
			Q.Line(FString::Printf(TEXT("Produkcja %d   Złoto %d   Nauka %d"), R.Yield.Prod, R.Yield.Gold, R.Yield.Science), FLinearColor::White, 1.0f);
			if (City->Buildings.Num() > 0)
			{
				FString B = TEXT("Budynki: ");
				for (int32 i = 0; i < City->Buildings.Num(); ++i) B += (i ? TEXT(", ") : TEXT("")) + FString(CivDefs::Building(City->Buildings[i]).Name);
				Q.Line(B, Gray, 0.95f);
			}
			Q.Y += 6.f;
			if (City->Current.Kind != EProductionKind::None)
			{
				const FString Name = City->Current.Kind == EProductionKind::Unit ? FString(CivDefs::Unit((EUnitType)City->Current.Item).Name) : FString(CivDefs::Building((EBuildingType)City->Current.Item).Name);
				const int32 Cost = City->Current.Kind == EProductionKind::Unit ? CivDefs::Unit((EUnitType)City->Current.Item).Cost : CivDefs::Building((EBuildingType)City->Current.Item).Cost;
				const int32 Turns = CivRules::TurnsToComplete(GS->Grid, GS->Civ, *City);
				Q.Line(FString::Printf(TEXT("Produkcja: %s  %d/%d  (tur: %d)"), *Name, City->ProdStock, Cost, Turns), FLinearColor(0.6f, 1.f, 0.6f), 1.1f);
			}
			else
			{
				Q.Line(TEXT("Produkcja: nic – wybierz klawiszem 1-9"), FLinearColor(1.f, 0.5f, 0.4f), 1.1f);
			}
			TArray<FProductionOption> Options;
			CivRules::GetProductionOptions(GS->Grid, GS->Civ, *City, Options);
			const int32 Pages = FMath::Max(1, (Options.Num() + 8) / 9);
			const int32 Pg = PC ? FMath::Clamp(PC->GetPage(), 0, Pages - 1) : 0;
			for (int32 k = 0; k < 9 && Pg * 9 + k < Options.Num(); ++k)
			{
				const FProductionOption& O = Options[Pg * 9 + k];
				Q.Line(FString::Printf(TEXT("%d. %s  (%d)"), k + 1, *O.Name, O.Cost), FLinearColor::White, 1.0f);
			}
			if (Pages > 1) Q.Line(FString::Printf(TEXT("PgUp/PgDn: strona %d/%d"), Pg + 1, Pages), Gray, 0.9f);
		}
	}
	else if (PC && PC->HasSelection())
	{
		const FHexCoord C = PC->GetSelectedHex();
		if (const FMapTile* T = GS->Grid.GetTile(C))
		{
			P.Line(FString::Printf(TEXT("Heks %s: %s, surowiec: %s"), *C.ToString(), *CivNames::TerrainName(T->Terrain), *CivNames::ResourceName(T->Resource)), FLinearColor(1.f, 1.f, 0.4f), 1.1f);
		}
	}

	// ---- Panel technologii (T) ----
	if (PC && PC->IsTechPanelOpen())
	{
		if (const FCivPlayerState* PS = GS->GetPlayerState(Viewer))
		{
			TArray<ETech> Av;
			CivTech::GetAvailable(*PS, Av);
			const int32 Pages = FMath::Max(1, (Av.Num() + 8) / 9);
			const int32 Pg = FMath::Clamp(PC->GetPage(), 0, Pages - 1);
			Canvas->SetDrawColor(0, 0, 0, 190);
			Canvas->DrawTile(Canvas->DefaultTexture, 380.f, 80.f, Canvas->SizeX - 760.f, 480.f, 0, 0, 1, 1);
			FTextCursor Q{ Canvas, Font, 400.f, 92.f };
			Q.Line(FString::Printf(TEXT("Technologie (poznane: %d/%d)   T: zamknij"), PS->Known.Num(), NumTechs - 1), FLinearColor(1.f, 1.f, 0.4f), 1.3f);
			for (int32 k = 0; k < 9 && Pg * 9 + k < Av.Num(); ++k)
			{
				const FTechDef& D = CivTech::Def(Av[Pg * 9 + k]);
				Q.Line(FString::Printf(TEXT("%d. [%s] %s  (%d)  %s"), k + 1, *CivTech::PathName(D.Path), D.Name, D.Cost, D.Unlocks), FLinearColor::White, 1.0f);
			}
			if (Av.Num() == 0) Q.Line(TEXT("Wszystko zbadane!"), FLinearColor::White, 1.0f);
			if (Pages > 1) Q.Line(FString::Printf(TEXT("PgUp/PgDn: strona %d/%d"), Pg + 1, Pages), Gray, 0.9f);
		}
	}

	if (const FCivPlayerState* DipPS = GS->GetPlayerState(Viewer))
	{
		FString Dip;
		for (int32 E : DipPS->AtWarWith) Dip += FString::Printf(TEXT(" %d"), E + 1);
		if (!Dip.IsEmpty()) { Canvas->SetDrawColor(255, 120, 120); Canvas->DrawText(Font, TEXT("Wojna z cywilizacją:") + Dip + TEXT("   (P: zaproponuj pokój)"), 16.f, 120.f, 1.0f, 1.0f); }
		FString Offers;
		for (int32 O : DipPS->PeaceOffers) Offers += FString::Printf(TEXT(" %d"), O + 1);
		if (!Offers.IsEmpty()) { Canvas->SetDrawColor(120, 255, 160); Canvas->DrawText(Font, TEXT("Propozycja pokoju od:") + Offers + TEXT("   (P: przyjmij)"), 16.f, 140.f, 1.0f, 1.0f); }
	}
	if (GM && GM->HasDesync())
	{
		Canvas->SetDrawColor(255, 60, 60);
		Canvas->DrawText(Font, TEXT("UWAGA: stan gry rozjechał się z gospodarzem (desynchronizacja)!"), 16.f, 100.f, 1.2f, 1.2f);
	}
	else if (GM && !GM->IsMyTurn())
	{
		Canvas->SetDrawColor(255, 220, 120);
		Canvas->DrawText(Font, TEXT("Tura innego gracza – czekaj"), 16.f, 100.f, 1.2f, 1.2f);
	}

	// ---- Komunikat chwilowy i pomoc ----
	if (PC && FPlatformTime::Seconds() < PC->NoticeUntil)
	{
		Canvas->SetDrawColor(255, 200, 80);
		Canvas->DrawText(Font, PC->Notice, 16.f, 70.f, 1.3f, 1.3f);
	}

	Canvas->SetDrawColor(200, 200, 200);
	Canvas->DrawText(Font, TEXT("WASD: kamera  Kółko: zoom  LPM: wybierz  PPM: ruch  B: miasto  K: pomiń  Tab: następna  1-9: produkcja  T: technologie  A/J/L: armia  I: ulepsz  P: pokój  Enter: koniec tury  F5/F9: zapis/odczyt  N: nowa mapa"), 16.f, Canvas->SizeY - 26.f, 0.9f, 0.9f);
}
