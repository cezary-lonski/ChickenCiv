#include "Misc/AutomationTest.h"
#include "Core/AccountAuth.h"
#include "Misc/Base64.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuthLocalTest, "ChickenCiv.Auth.LocalEmailAccount", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAuthLocalTest::RunTest(const FString&)
{
	const FString TempFile = FPaths::ProjectSavedDir() / TEXT("AutomationTemp/Accounts_test.json");
	IFileManager::Get().Delete(*TempFile, false, true, true);
	Auth::SetAccountsFileOverride(TempFile);
	Auth::Logout();

	TestTrue(TEXT("poprawny e-mail"), Auth::IsValidEmail(TEXT("jan@example.com")));
	TestFalse(TEXT("brak malpy"), Auth::IsValidEmail(TEXT("jan.example.com")));
	TestFalse(TEXT("brak domeny"), Auth::IsValidEmail(TEXT("jan@")));
	TestFalse(TEXT("brak kropki w domenie"), Auth::IsValidEmail(TEXT("jan@example")));
	TestFalse(TEXT("spacja"), Auth::IsValidEmail(TEXT("ja n@example.com")));

	FString Err;
	TestFalse(TEXT("krotkie haslo"), Auth::Register(TEXT("jan@example.com"), TEXT("abc"), Err));
	TestFalse(TEXT("zly e-mail"), Auth::Register(TEXT("nie-email"), TEXT("tajnehaslo"), Err));
	TestTrue(TEXT("rejestracja"), Auth::Register(TEXT("Jan@Example.com"), TEXT("tajnehaslo"), Err));
	TestEqual(TEXT("dostawca"), Auth::Current().Provider, FString(TEXT("email")));
	TestEqual(TEXT("imie z e-maila"), Auth::DisplayName(), FString(TEXT("jan")));
	TestFalse(TEXT("duplikat"), Auth::Register(TEXT("jan@example.com"), TEXT("innehaslo"), Err));

	Auth::Logout();
	TestFalse(TEXT("wylogowany"), Auth::Current().IsLoggedIn());
	TestFalse(TEXT("zle haslo"), Auth::Login(TEXT("jan@example.com"), TEXT("zlehaslo"), Err));
	TestFalse(TEXT("nieznane konto"), Auth::Login(TEXT("ktos@example.com"), TEXT("tajnehaslo"), Err));
	TestTrue(TEXT("logowanie (wielkosc liter w e-mailu bez znaczenia)"), Auth::Login(TEXT("JAN@example.com"), TEXT("tajnehaslo"), Err));

	// Skrot: ten sam dla tej samej soli, inny dla innej soli i hasla; haslo nie jest zapisane jawnie.
	TestEqual(TEXT("skrot deterministyczny"), Auth::HashPassword(TEXT("x"), TEXT("s")), Auth::HashPassword(TEXT("x"), TEXT("s")));
	TestNotEqual(TEXT("inna sol"), Auth::HashPassword(TEXT("x"), TEXT("s")), Auth::HashPassword(TEXT("x"), TEXT("t")));
	TestNotEqual(TEXT("inne haslo"), Auth::HashPassword(TEXT("x"), TEXT("s")), Auth::HashPassword(TEXT("y"), TEXT("s")));
	FString File;
	FFileHelper::LoadFileToString(File, *TempFile);
	TestFalse(TEXT("haslo nie zapisane jawnie"), File.Contains(TEXT("tajnehaslo")));

	Auth::SetAccountsFileOverride(FString());
	Auth::Logout();
	IFileManager::Get().Delete(*TempFile, false, true, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuthOAuthParseTest, "ChickenCiv.Auth.OAuthParsing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAuthOAuthParseTest::RunTest(const FString&)
{
	// Odpowiedz "device code" (Google uzywa verification_url, Microsoft verification_uri).
	Auth::FDeviceCode Code;
	TestTrue(TEXT("Microsoft"), Auth::ParseDeviceCodeResponse(TEXT("{\"device_code\":\"DC1\",\"user_code\":\"ABCD-EFGH\",\"verification_uri\":\"https://microsoft.com/devicelogin\",\"expires_in\":900,\"interval\":5}"), Code));
	TestEqual(TEXT("kod uzytkownika"), Code.UserCode, FString(TEXT("ABCD-EFGH")));
	TestEqual(TEXT("adres"), Code.VerificationUrl, FString(TEXT("https://microsoft.com/devicelogin")));
	TestEqual(TEXT("interwal"), Code.Interval, 5);
	Auth::FDeviceCode G;
	TestTrue(TEXT("Google"), Auth::ParseDeviceCodeResponse(TEXT("{\"device_code\":\"X\",\"user_code\":\"AB-CD\",\"verification_url\":\"https://www.google.com/device\",\"expires_in\":1800,\"interval\":3}"), G));
	TestEqual(TEXT("adres Google"), G.VerificationUrl, FString(TEXT("https://www.google.com/device")));
	Auth::FDeviceCode Bad;
	TestFalse(TEXT("brak pol"), Auth::ParseDeviceCodeResponse(TEXT("{\"error\":\"invalid_client\"}"), Bad));
	TestFalse(TEXT("smieci"), Auth::ParseDeviceCodeResponse(TEXT("nie json"), Bad));

	// id_token: base64url JSON w srodku.
	const FString Payload = TEXT("{\"name\":\"Jan Kowalski\",\"email\":\"jan@example.com\"}");
	const FString Jwt = FBase64::Encode(TEXT("{\"alg\":\"RS256\"}"), EBase64Mode::UrlSafe) + TEXT(".") + FBase64::Encode(Payload, EBase64Mode::UrlSafe) + TEXT(".podpis");
	FAccount A;
	TestTrue(TEXT("id_token"), Auth::ParseIdToken(Jwt, TEXT("google"), A));
	TestEqual(TEXT("imie"), A.DisplayName, FString(TEXT("Jan Kowalski")));
	TestEqual(TEXT("e-mail"), A.Email, FString(TEXT("jan@example.com")));
	TestEqual(TEXT("dostawca"), A.Provider, FString(TEXT("google")));

	// Microsoft bez pola name: bierzemy czesc przed @ z preferred_username.
	const FString P2 = TEXT("{\"preferred_username\":\"ola@firma.pl\"}");
	const FString Jwt2 = TEXT("a.") + FBase64::Encode(P2, EBase64Mode::UrlSafe) + TEXT(".b");
	FAccount B;
	TestTrue(TEXT("token Microsoft"), Auth::ParseIdToken(Jwt2, TEXT("microsoft"), B));
	TestEqual(TEXT("imie z e-maila"), B.DisplayName, FString(TEXT("ola")));
	FAccount C;
	TestFalse(TEXT("zly token"), Auth::ParseIdToken(TEXT("niejwt"), TEXT("google"), C));
	TestFalse(TEXT("pusty ladunek"), Auth::ParseIdToken(TEXT("a.") + FBase64::Encode(TEXT("{}"), EBase64Mode::UrlSafe) + TEXT(".b"), TEXT("google"), C));
	return true;
}

#endif
