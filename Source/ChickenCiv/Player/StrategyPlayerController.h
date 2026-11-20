#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "Hex/HexCoord.h"
#include "Core/ChickenGame.h"
#include "Core/ChickenCommand.h"
#include "StrategyPlayerController.generated.h"

/**
 * Sterowanie: LPM wybiera jednostke/miasto/heks, PPM wydaje rozkaz ruchu, B zaklada miasto,
 * K pomija jednostke, Tab nastepna jednostka, 1-9 wybor produkcji w miescie, Enter konczy ture,
 * F5/F9 zapis/odczyt, N nowa mapa, kolo = zoom.
 */
UCLASS()
class CHICKENCIV_API AStrategyPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AStrategyPlayerController();

	virtual void SetupInputComponent() override;
	virtual void BeginPlay() override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Miejsce gracza w grze LAN (nadawane przez serwer). */
	UPROPERTY(Replicated) int32 LanPlayerIndex = 0;

	UFUNCTION(Server, Reliable) void ServerSubmitCommand(const FChickenCommand& Cmd);
	UFUNCTION(Client, Reliable) void ClientExecuteCommand(const FChickenCommand& Cmd);
	UFUNCTION(Client, Reliable) void ClientCheckHash(int32 Turn, int32 ServerHash);

	bool HasSelection() const { return bHasSelection; }
	FHexCoord GetSelectedHex() const { return SelectedHex; }
	int32 GetSelectedUnitId() const { return SelectedUnitId; }
	int32 GetSelectedCityId() const { return SelectedCityId; }

	/** Heks pod kursorem; false, gdy kursor poza swiatem. */
	bool GetHexUnderCursor(FHexCoord& Out) const;

	void SelectHex(const FHexCoord& C);
	void SelectUnit(int32 UnitId);
	void SelectCity(int32 CityId);
	void ClearSelection();

	/** Komunikat chwilowy (np. powod odmowy), pokazywany w HUD. */
	bool IsTechPanelOpen() const { return bTechPanel; }
	int32 GetPage() const { return Page; }

	FString Notice;
	double NoticeUntil = 0.0;
	void ShowNotice(const FString& Text);

private:
	bool bHasSelection = false;
	FHexCoord SelectedHex;
	int32 SelectedUnitId = 0;
	int32 SelectedCityId = 0;
	bool bTechPanel = false;
	int32 Page = 0;

	void OnToggleTech();
	void OnImprove();
	void OnPeace();
	void OnBattleAuto();
	void OnDeathStar();
	bool InBattle() const;
	bool InMenu() const;
	void OnEscape();
	void OnMenuUp();
	void OnMenuDown();
	void OnMenuLeft();
	void OnMenuRight();
	void OnBackspace();
	void OnDelete();
	void OnPasteKey();
	void OnPeriod();
	bool NotMyTurn();
	void OnDigit0() { if (InMenu()) { if (AChickenGame* GM = AChickenGame::Get(GetWorld())) GM->MenuDigit(0); } }
	void OnCreateArmy();
	void OnJoinArmy();
	void OnLeaveArmy();
	void OnPageUp();
	void OnPageDown();
	void OnClick();
	void OnRightClick();
	void OnFoundCity();
	void OnSkipUnit();
	void OnNextUnit();
	void OnEndTurn();
	void OnSave();
	void OnLoad();
	void OnNewMap();
	void OnZoomIn();
	void OnZoomOut();
	void OnDigit(int32 Index);
	void OnDigit1() { OnDigit(0); }
	void OnDigit2() { OnDigit(1); }
	void OnDigit3() { OnDigit(2); }
	void OnDigit4() { OnDigit(3); }
	void OnDigit5() { OnDigit(4); }
	void OnDigit6() { OnDigit(5); }
	void OnDigit7() { OnDigit(6); }
	void OnDigit8() { OnDigit(7); }
	void OnDigit9() { OnDigit(8); }
};
