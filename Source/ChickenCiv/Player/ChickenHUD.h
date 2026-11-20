#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ChickenHUD.generated.h"

/** Prosty HUD: tura, gracz, informacje o wybranym heksie, podpowiedz sterowania. */
UCLASS()
class CHICKENCIV_API AChickenHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	/** Plansza bitwy taktycznej rysowana na calym ekranie. */
	void DrawBattle(const class AChickenGame* GM);
	/** Menu glowne / pauzy. */
	void DrawMenu(const class AChickenGame* GM);
	/** Menu glowne w stylu ilustrowanym: przyciski na tle ilustracji (lub przyciemnionej gry w pauzie). */
	void DrawMainMenu(const class AChickenGame* GM);
	/** Ekran logowania. */
	void DrawLogin(const class AChickenGame* GM);
};
