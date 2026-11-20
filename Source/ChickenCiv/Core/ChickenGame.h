#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Map/CivTypes.h"
#include "Model/CivState.h"
#include "Core/BattleRules.h"
#include "Core/ChickenCommand.h"
#include "Core/AccountAuth.h"
#include "ChickenGame.generated.h"

class AHexMapActor;
class AWorldViewActor;
class AChickenGameState;
class AStrategyPlayerController;
class UMediaPlayer;
class UMediaTexture;
class UMediaSoundComponent;
class UTexture;

/** Wiersze menu glownego / pauzy. */
enum class EMenuRow : uint8 { Resume, NewGame, Size, Civs, Humans, Difficulty, Race, Color, Seed, ApiKey, Account, HostLan, JoinAddress, JoinLan, Load, Save, Quit, Count };

/** Ekran logowania (przed menu). */
enum class ELoginRow : uint8 { Google, Microsoft, EmailLogin, EmailRegister, Guest, Count };

struct FLoginState
{
	bool bOpen = false;
	int32 Row = 0;
	int32 FormMode = 0;            // 0 lista, 1 logowanie e-mail, 2 rejestracja e-mail
	int32 Field = 0;               // 0 e-mail, 1 haslo
	FString Email;
	FString Password;
	FString Message;
	// Logowanie przez przegladarke (device code)
	bool bWaiting = false;
	bool bPollInFlight = false;
	int32 Provider = 0;            // Auth::EProvider
	Auth::FDeviceCode Device;
	double WaitStart = 0.0;
	double NextPoll = 0.0;
};

struct FMenuState
{
	bool bOpen = false;
	bool bGameStarted = false;  // czy gracz rozpoczal gre (Wznow/Zapisz tylko wtedy)
	int32 Row = (int32)EMenuRow::NewGame;
	FMapSettings Settings;      // ustawienia nowej gry edytowane w menu
	FString Message;            // krotka informacja zwrotna (np. "Wczytano grę")
	FString JoinAddress = TEXT("127.0.0.1"); // adres gospodarza LAN
};

/**
 * Logika gry (menu, bitwa, polecenia, widoki): zwykly aktor tworzony na KAZDEJ maszynie,
 * takze na klientach LAN (AGameModeBase istnieje tylko na serwerze). Jedyne miejsce, przez ktore gracz zmienia stan.
 * Opcje URL: ?Size=Small|Medium|Large ?Players=2..8 ?Seed=123 ?Difficulty=Easy|Normal|Hard
 */
UCLASS()
class CHICKENCIV_API AChickenGame : public AActor
{
	GENERATED_BODY()

public:
	AChickenGame();

	UPROPERTY(EditAnywhere, Category = "Chicken") FMapSettings DefaultSettings;

	/** Opcje z URL mapy (?Size ?Players ?Seed ?Difficulty ?NoMenu); wywolac przed BeginPlay (deferred spawn). */
	void ApplyUrlOptions(const FString& Options);

	/** Instancja gry na tej maszynie (serwer, klient LAN lub gra pojedyncza); nullptr, gdy jeszcze nie ma. */
	static AChickenGame* Get(const UWorld* World);
	/** Tworzy instancje, jesli jej nie ma. */
	static AChickenGame* Ensure(UWorld* World, const FString& Options = FString());

	virtual void BeginPlay() override;

	/** Nowa gra z ustawieniami (Seed 0 = losowe ziarno). */
	void StartNewGame(const FMapSettings& Settings);

	AChickenGameState* GetChickenState() const;
	AHexMapActor* GetMap() const { return Map; }
	const FMapSettings& GetActiveSettings() const;

	// ---- Polecenia gracza (zwracaja true, gdy zmienily stan) ----
	void EndTurn();
	bool OrderMove(int32 UnitId, const FHexCoord& To);
	bool FoundCity(int32 UnitId, FString* Why = nullptr);
	bool SkipUnit(int32 UnitId);
	bool SetProduction(int32 CityId, const FProduction& P);
	bool SetResearch(ETech Tech);
	/** Armie: A = zaloz (Dowodca), J = dolacz do armii na swoim polu, L = opusc armie. */
	bool CreateArmy(int32 UnitId, FString* Why = nullptr);
	bool JoinArmy(int32 UnitId, FString* Why = nullptr);
	bool LeaveArmy(int32 UnitId, FString* Why = nullptr);
	/** Robotnik buduje ulepszenie pola, na ktorym stoi. */
	bool BuildImprovement(int32 UnitId, FString* Why = nullptr);
	/** Proponuje pokoj wszystkim, z kim biezacy gracz jest na wojnie (albo przyjmuje ich propozycje). */
	bool ProposePeace();

	// ---- Siec LAN (deterministyczna replikacja polecen) ----
	bool IsNetworked() const;
	bool IsNetServer() const;
	bool IsNetClient() const;
	/** Numer gracza tej maszyny (w LAN: gospodarz 0, klienci kolejne). */
	int32 LocalPlayerIndex() const;
	/** Czy teraz tura gracza z tej maszyny (poza siecia zawsze true). */
	bool IsMyTurn() const;
	bool IsNetGameStarted() const { return Menu.bGameStarted; }
	bool HasDesync() const { return bDesync; }
	uint32 ComputeStateHash() const;
	/** Serwer: polecenie od klienta (sprawdza, czy to jego tura), wykonuje i rozglasza. */
	void ReceiveCommandFromClient(AStrategyPlayerController* Sender, const FChickenCommand& Cmd);
	/** Klient: polecenie rozeslane przez serwer. */
	void ApplyRemoteCommand(const FChickenCommand& Cmd);
	void CheckRemoteHash(int32 Turn, int32 ServerHash);
	/** Gospodarz rozpoczyna gre LAN z podanymi ustawieniami (Seed ustalany wczesniej). */
	void NewNetworkGame(const FMapSettings& Settings);
	int32 GetNumLobbyPlayers() const;

	// ---- Hot seat: ekran przekazania komputera ----
	bool IsHandoff() const { return bHandoff; }
	void DismissHandoff();

	// ---- Logowanie ----
	bool IsLoginOpen() const { return Login.bOpen; }
	const FLoginState& GetLogin() const { return Login; }
	void OpenLogin();
	/** Obsluga klawisza na ekranie logowania (wpisywanie, nawigacja). Zawsze pochlania klawisz. */
	bool LoginKey(const FKey& Key, bool bShift, bool bCtrl);

	// ---- Intro (film przed menu) ----
	bool IsIntro() const { return bIntro; }
	UTexture* GetIntroTexture() const;
	void SkipIntro();
	UFUNCTION() void OnIntroEnded();
	UFUNCTION() void OnIntroFailed(FString FailedUrl);

	// ---- Menu ----
	bool IsMenuOpen() const { return Menu.bOpen; }
	const FMenuState& GetMenu() const { return Menu; }
	bool IsMenuRowVisible(EMenuRow Row) const;
	void ToggleMenu();
	void MenuMove(int32 Delta);
	void MenuChange(int32 Delta);
	void MenuActivate();
	void MenuDigit(int32 Digit);
	void MenuBackspace();
	void MenuPeriod();
	void MenuPasteApiKey();
	void MenuClearApiKey();

	// ---- Bitwa taktyczna ----
	/** Atak jednostki/armii na sasiednie pole z wrogiem (pusty wrogi miasto jest zajmowane od reki). */
	bool StartAttack(int32 UnitId, const FHexCoord& Target, FString* Why = nullptr);
	bool IsInBattle() const { return ActiveBattle.IsValid(); }
	const FBattle* GetBattle() const { return ActiveBattle.Get(); }
	/** Klikniecie pola na planszy bitwy: ruch, atak lub umiejetnosc biezacej jednostki gracza. */
	bool BattleClick(const FHexCoord& Hex);
	void BattleEndUnitTurn();
	void BattleAutoResolve();
	/** Wlacza tryb lasera Gwiazdy Smierci (nastepne klikniecie wroga). Zwraca false, gdy niedostepny. */
	bool BattleToggleDeathStar();
	bool IsDeathStarMode() const { return bDeathStarMode; }
	/** Konczy bitwe (po wyniku) i nanosi straty na mape. */
	void CloseBattle();

	/** Id nastepnej jednostki gracza z ruchem (po CurrentId), albo 0. */
	int32 NextIdleUnit(int32 CurrentId) const;

	bool SaveGame(const FString& Slot = TEXT("quicksave"));
	bool LoadGame(const FString& Slot = TEXT("quicksave"));

	/** Przerysowuje mape (mgla wojny) i jednostki po zmianie stanu. */
	void RefreshViews();

	void FocusCamerasOnMap();
	void FocusCamerasOn(const FHexCoord& C);
	void FocusOnViewerStart();

	/** Gracz, ktorego oczami widzimy swiat (w hot seat: biezacy gracz). */
	int32 GetViewer() const;

private:
	// Cialo kazdego polecenia (Impl_) jest wywolywane przez publiczne wrappery po routingu sieciowym.
	void Impl_EndTurn();
	bool Impl_OrderMove(int32 UnitId, const FHexCoord& To);
	bool Impl_FoundCity(int32 UnitId, FString* Why);
	bool Impl_SkipUnit(int32 UnitId);
	bool Impl_SetProduction(int32 CityId, const FProduction& P);
	bool Impl_SetResearch(ETech Tech);
	bool Impl_CreateArmy(int32 UnitId, FString* Why);
	bool Impl_JoinArmy(int32 UnitId, FString* Why);
	bool Impl_LeaveArmy(int32 UnitId, FString* Why);
	bool Impl_BuildImprovement(int32 UnitId, FString* Why);
	bool Impl_ProposePeace();
	bool Impl_StartAttack(int32 UnitId, const FHexCoord& Target, FString* Why);
	bool Impl_BattleClick(const FHexCoord& Hex);
	void Impl_BattleEndUnitTurn();
	void Impl_BattleAutoResolve();
	void Impl_CloseBattle();
	void ExecuteCommand(const FChickenCommand& Cmd);
	/** true, gdy to klient i polecenie poszlo na serwer (nie wykonujemy lokalnie). */
	bool NetRoute(const FChickenCommand& Cmd);
	void NetBroadcast(const FChickenCommand& Cmd);
	AStrategyPlayerController* LocalPC() const;
	bool bApplyingRemote = false;
	// Narzedzie testowe LAN (-AutoLan): automatyczny gracz, loguje hash stanu po kazdej turze.
	void AutoLanStep(int32 TargetTurns);
	int32 AutoLanLastTurn = -1;
	FRandomStream AutoLanRand = FRandomStream(1);
	bool bDesync = false;
	bool bClientProxy = false; // klient LAN: gra startuje dopiero na polecenie gospodarza
	bool bLobbyRequested = false;

	FLoginState Login;
	FTimerHandle LoginTimer;
	void LoginActivate();
	void LoginFinish(const FAccount& Account);
	void LoginCancelWait();
	void LoginTick();
	void StartIntro();
	bool bIntro = false;
	double IntroStartTime = 0.0;
	FTimerHandle IntroWatch;
	UPROPERTY() TObjectPtr<UMediaPlayer> IntroPlayer;
	UPROPERTY() TObjectPtr<UMediaTexture> IntroTexture;
	UPROPERTY() TObjectPtr<UMediaSoundComponent> IntroSound;

	bool bHandoff = false; // miedzy turami dwoch graczy ludzkich: ekran zaslania mape
	FMenuState Menu;
	bool bUrlOptions = false; // gra odpalona z opcjami w URL (np. do zrzutow): bez menu
	void LoadMenuPrefs();
	void SaveMenuPrefs() const;
	void ClearControllerSelection();

	TSharedPtr<FBattle> ActiveBattle;
	bool bDeathStarMode = false;
	/** Tury AI (strona 1) do momentu, gdy gracz ma ruch albo bitwa sie skonczy. */
	void BattleAdvance();

	UPROPERTY() TObjectPtr<AHexMapActor> Map;
	UPROPERTY() TObjectPtr<AWorldViewActor> View;
};
