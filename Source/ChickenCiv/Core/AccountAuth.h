#pragma once

#include "CoreMinimal.h"

/** Zalogowane konto gracza. Provider: google, microsoft, email, guest. */
struct FAccount
{
	FString Provider;
	FString DisplayName;
	FString Email;
	bool IsLoggedIn() const { return !Provider.IsEmpty(); }
};

/**
 * Logowanie: Google i Microsoft przez OAuth 2.0 "device code" (gracz wpisuje kod na stronie w przegladarce,
 * gra nie przechowuje hasel ani tokenow), konto lokalne e-mail + haslo (skrot z sola w Saved/Accounts.json)
 * oraz tryb goscia. Identyfikatory aplikacji (client id) ustawia sie zmiennymi srodowiskowymi lub w ustawieniach
 * uzytkownika; bez nich dany dostawca jest niedostepny. Nic z tego nie trafia do repozytorium ani do zapisow gry.
 */
namespace Auth
{
	enum class EProvider : uint8 { Google, Microsoft };

	struct FDeviceCode
	{
		FString DeviceCode;
		FString UserCode;
		FString VerificationUrl;
		int32 Interval = 5;
		int32 ExpiresIn = 900;
	};

	// ---- Biezace konto ----
	const FAccount& Current();
	void SetCurrent(const FAccount& Account);
	void Logout();
	/** Nazwa do pokazania jako imie gracza (pusta, gdy nikt nie jest zalogowany). */
	FString DisplayName();
	/** Zapamietuje / wczytuje konto miedzy uruchomieniami (tylko nazwa, dostawca i e-mail; bez hasel i tokenow). */
	void SaveRemembered();
	bool LoadRemembered();

	// ---- Konto lokalne e-mail ----
	/** Do testow: inny plik kont (pusty = domyslny Saved/Accounts.json). */
	void SetAccountsFileOverride(const FString& Path);
	bool IsValidEmail(const FString& Email);
	FString HashPassword(const FString& Password, const FString& Salt);
	bool Register(const FString& Email, const FString& Password, FString& OutError);
	bool Login(const FString& Email, const FString& Password, FString& OutError);

	// ---- OAuth (Google, Microsoft) ----
	FString ClientId(EProvider P);
	bool HasClientId(EProvider P);
	FString ProviderName(EProvider P);
	/** Czysta funkcja: odczyt odpowiedzi "device code" i tokena (testowalne bez sieci). */
	bool ParseDeviceCodeResponse(const FString& Json, FDeviceCode& Out);
	/** Czysta funkcja: odczyt danych z id_token (JWT) – imie i e-mail. */
	bool ParseIdToken(const FString& Jwt, const FString& Provider, FAccount& Out);

	void RequestDeviceCode(EProvider P, TFunction<void(bool bOk, const FDeviceCode& Code, const FString& Error)> Done);
	/** Jedno odpytanie o token. State: 0 = jeszcze czeka, 1 = zalogowano, 2 = blad/odmowa. */
	void PollToken(EProvider P, const FDeviceCode& Code, TFunction<void(int32 State, const FAccount& Account, const FString& Error)> Done);
}
