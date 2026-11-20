#include "Core/AccountAuth.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Base64.h"
#include "Misc/SecureHash.h"
#include "Misc/Guid.h"
#include "GenericPlatform/GenericPlatformHttp.h"

namespace Auth
{
	static FAccount GCurrent;
	static const TCHAR* GSection = TEXT("ChickenCiv.Account");

	const FAccount& Current() { return GCurrent; }
	void SetCurrent(const FAccount& Account) { GCurrent = Account; }
	void Logout()
	{
		GCurrent = FAccount();
		if (GConfig)
		{
			GConfig->RemoveKey(GSection, TEXT("Provider"), GGameUserSettingsIni);
			GConfig->RemoveKey(GSection, TEXT("Name"), GGameUserSettingsIni);
			GConfig->RemoveKey(GSection, TEXT("Email"), GGameUserSettingsIni);
			GConfig->Flush(false, GGameUserSettingsIni);
		}
	}
	FString DisplayName() { return GCurrent.DisplayName; }

	void SaveRemembered()
	{
		if (!GConfig || !GCurrent.IsLoggedIn()) return;
		GConfig->SetString(GSection, TEXT("Provider"), *GCurrent.Provider, GGameUserSettingsIni);
		GConfig->SetString(GSection, TEXT("Name"), *GCurrent.DisplayName, GGameUserSettingsIni);
		GConfig->SetString(GSection, TEXT("Email"), *GCurrent.Email, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	bool LoadRemembered()
	{
		if (!GConfig) return false;
		FAccount A;
		GConfig->GetString(GSection, TEXT("Provider"), A.Provider, GGameUserSettingsIni);
		GConfig->GetString(GSection, TEXT("Name"), A.DisplayName, GGameUserSettingsIni);
		GConfig->GetString(GSection, TEXT("Email"), A.Email, GGameUserSettingsIni);
		if (!A.IsLoggedIn() || A.DisplayName.IsEmpty()) return false;
		GCurrent = A;
		return true;
	}

	// ===== Konto lokalne =====
	bool IsValidEmail(const FString& Email)
	{
		int32 At = INDEX_NONE;
		if (!Email.FindChar(TEXT('@'), At) || At < 1 || Email.Len() > 120) return false;
		const FString Domain = Email.Mid(At + 1);
		int32 Dot = INDEX_NONE;
		return Domain.FindLastChar(TEXT('.'), Dot) && Dot > 0 && Dot < Domain.Len() - 1 && !Email.Contains(TEXT(" ")) && !Domain.Contains(TEXT("@"));
	}

	FString HashPassword(const FString& Password, const FString& Salt)
	{
		// Rozciagany skrot SHA-1 z sola (konto lokalne, bez serwera): 20000 iteracji.
		FString Cur = Salt + Password;
		for (int32 i = 0; i < 20000; ++i)
		{
			const FTCHARToUTF8 Bytes(*(Salt + Cur));
			FSHAHash H;
			FSHA1::HashBuffer(Bytes.Get(), Bytes.Length(), H.Hash);
			Cur = H.ToString();
		}
		return Cur;
	}

	static FString GAccountsOverride;
	void SetAccountsFileOverride(const FString& Path) { GAccountsOverride = Path; }
	static FString AccountsPath() { return GAccountsOverride.IsEmpty() ? FPaths::ProjectSavedDir() / TEXT("Accounts.json") : GAccountsOverride; }

	static TSharedPtr<FJsonObject> LoadAccounts()
	{
		FString Text;
		TSharedPtr<FJsonObject> Obj;
		if (FFileHelper::LoadFileToString(Text, *AccountsPath()))
		{
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
			FJsonSerializer::Deserialize(Reader, Obj);
		}
		return Obj.IsValid() ? Obj : MakeShared<FJsonObject>();
	}

	static void SaveAccounts(const TSharedPtr<FJsonObject>& Obj)
	{
		FString Out;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
		FFileHelper::SaveStringToFile(Out, *AccountsPath());
	}

	bool Register(const FString& EmailIn, const FString& Password, FString& OutError)
	{
		const FString Email = EmailIn.TrimStartAndEnd().ToLower();
		if (!IsValidEmail(Email)) { OutError = TEXT("Niepoprawny adres e-mail"); return false; }
		if (Password.Len() < 6) { OutError = TEXT("Hasło musi mieć co najmniej 6 znaków"); return false; }
		TSharedPtr<FJsonObject> All = LoadAccounts();
		if (All->HasField(Email)) { OutError = TEXT("To konto już istnieje – zaloguj się"); return false; }

		const FString Salt = FGuid::NewGuid().ToString(EGuidFormats::Digits);
		TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("salt"), Salt);
		Entry->SetStringField(TEXT("hash"), HashPassword(Password, Salt));
		All->SetObjectField(Email, Entry);
		SaveAccounts(All);

		FAccount A;
		A.Provider = TEXT("email");
		A.Email = Email;
		A.DisplayName = Email.Left(Email.Find(TEXT("@")));
		SetCurrent(A);
		return true;
	}

	bool Login(const FString& EmailIn, const FString& Password, FString& OutError)
	{
		const FString Email = EmailIn.TrimStartAndEnd().ToLower();
		TSharedPtr<FJsonObject> All = LoadAccounts();
		const TSharedPtr<FJsonObject>* Entry = nullptr;
		if (!All->TryGetObjectField(Email, Entry) || !Entry)
		{
			OutError = TEXT("Nie ma takiego konta");
			return false;
		}
		FString Salt, Hash;
		(*Entry)->TryGetStringField(TEXT("salt"), Salt);
		(*Entry)->TryGetStringField(TEXT("hash"), Hash);
		if (HashPassword(Password, Salt) != Hash) { OutError = TEXT("Błędne hasło"); return false; }

		FAccount A;
		A.Provider = TEXT("email");
		A.Email = Email;
		A.DisplayName = Email.Left(Email.Find(TEXT("@")));
		SetCurrent(A);
		return true;
	}

	// ===== OAuth =====
	static FString Setting(const TCHAR* Env, const TCHAR* IniKey)
	{
		FString V = FPlatformMisc::GetEnvironmentVariable(Env);
		if (V.IsEmpty() && GConfig) GConfig->GetString(GSection, IniKey, V, GGameUserSettingsIni);
		V.TrimStartAndEndInline();
		return V;
	}

	FString ClientId(EProvider P)
	{
		return P == EProvider::Google ? Setting(TEXT("GOOGLE_CLIENT_ID"), TEXT("GoogleClientId")) : Setting(TEXT("MICROSOFT_CLIENT_ID"), TEXT("MicrosoftClientId"));
	}
	bool HasClientId(EProvider P) { return !ClientId(P).IsEmpty(); }
	FString ProviderName(EProvider P) { return P == EProvider::Google ? TEXT("Google") : TEXT("Microsoft"); }

	static TSharedPtr<FJsonObject> ParseJson(const FString& Json)
	{
		TSharedPtr<FJsonObject> Obj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		FJsonSerializer::Deserialize(Reader, Obj);
		return Obj;
	}

	bool ParseDeviceCodeResponse(const FString& Json, FDeviceCode& Out)
	{
		const TSharedPtr<FJsonObject> O = ParseJson(Json);
		if (!O.IsValid()) return false;
		O->TryGetStringField(TEXT("device_code"), Out.DeviceCode);
		O->TryGetStringField(TEXT("user_code"), Out.UserCode);
		if (!O->TryGetStringField(TEXT("verification_uri"), Out.VerificationUrl)) O->TryGetStringField(TEXT("verification_url"), Out.VerificationUrl);
		double V = 0.0;
		if (O->TryGetNumberField(TEXT("interval"), V)) Out.Interval = FMath::Clamp((int32)V, 1, 30);
		if (O->TryGetNumberField(TEXT("expires_in"), V)) Out.ExpiresIn = FMath::Clamp((int32)V, 30, 1800);
		return !Out.DeviceCode.IsEmpty() && !Out.UserCode.IsEmpty() && !Out.VerificationUrl.IsEmpty();
	}

	bool ParseIdToken(const FString& Jwt, const FString& Provider, FAccount& Out)
	{
		// JWT: naglowek.ladunek.podpis, ladunek to base64url JSON. Token pochodzi wprost z TLS od dostawcy.
		TArray<FString> Parts;
		Jwt.ParseIntoArray(Parts, TEXT("."));
		if (Parts.Num() < 2) return false;
		FString Payload;
		if (!FBase64::Decode(Parts[1], Payload, EBase64Mode::UrlSafe)) return false;
		const TSharedPtr<FJsonObject> O = ParseJson(Payload);
		if (!O.IsValid()) return false;

		FString Name, Email;
		O->TryGetStringField(TEXT("name"), Name);
		if (!O->TryGetStringField(TEXT("email"), Email)) O->TryGetStringField(TEXT("preferred_username"), Email);
		if (Name.IsEmpty()) Name = Email.IsEmpty() ? FString() : Email.Left(Email.Find(TEXT("@")));
		if (Name.IsEmpty() && Email.IsEmpty()) return false;
		Out.Provider = Provider;
		Out.DisplayName = Name;
		Out.Email = Email;
		return true;
	}

	static void PostForm(const FString& Url, const TMap<FString, FString>& Fields, TFunction<void(bool, int32, const FString&)> Done)
	{
		FString Body;
		for (const TPair<FString, FString>& KV : Fields)
		{
			if (!Body.IsEmpty()) Body += TEXT("&");
			Body += FGenericPlatformHttp::UrlEncode(KV.Key) + TEXT("=") + FGenericPlatformHttp::UrlEncode(KV.Value);
		}
		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
		Req->SetURL(Url);
		Req->SetVerb(TEXT("POST"));
		Req->SetHeader(TEXT("Content-Type"), TEXT("application/x-www-form-urlencoded"));
		Req->SetTimeout(20.f);
		Req->SetContentAsString(Body);
		Req->OnProcessRequestComplete().BindLambda([Done](FHttpRequestPtr, FHttpResponsePtr Res, bool bOk)
		{
			Done(bOk && Res.IsValid(), Res.IsValid() ? Res->GetResponseCode() : 0, Res.IsValid() ? Res->GetContentAsString() : FString());
		});
		Req->ProcessRequest();
	}

	static FString DeviceCodeUrl(EProvider P) { return P == EProvider::Google ? TEXT("https://oauth2.googleapis.com/device/code") : TEXT("https://login.microsoftonline.com/common/oauth2/v2.0/devicecode"); }
	static FString TokenUrl(EProvider P) { return P == EProvider::Google ? TEXT("https://oauth2.googleapis.com/token") : TEXT("https://login.microsoftonline.com/common/oauth2/v2.0/token"); }

	void RequestDeviceCode(EProvider P, TFunction<void(bool, const FDeviceCode&, const FString&)> Done)
	{
		if (!HasClientId(P)) { Done(false, FDeviceCode(), FString::Printf(TEXT("Brak identyfikatora aplikacji %s (zob. README)"), *ProviderName(P))); return; }
		TMap<FString, FString> F;
		F.Add(TEXT("client_id"), ClientId(P));
		F.Add(TEXT("scope"), TEXT("openid email profile"));
		PostForm(DeviceCodeUrl(P), F, [Done](bool bOk, int32 Code, const FString& Body)
		{
			FDeviceCode DC;
			if (bOk && Code == 200 && ParseDeviceCodeResponse(Body, DC)) Done(true, DC, FString());
			else Done(false, DC, FString::Printf(TEXT("Nie udało się rozpocząć logowania (HTTP %d)"), Code));
		});
	}

	void PollToken(EProvider P, const FDeviceCode& Code, TFunction<void(int32, const FAccount&, const FString&)> Done)
	{
		TMap<FString, FString> F;
		F.Add(TEXT("client_id"), ClientId(P));
		F.Add(TEXT("device_code"), Code.DeviceCode);
		F.Add(TEXT("grant_type"), TEXT("urn:ietf:params:oauth:grant-type:device_code"));
		const FString Secret = P == EProvider::Google ? Setting(TEXT("GOOGLE_CLIENT_SECRET"), TEXT("GoogleClientSecret")) : FString();
		if (!Secret.IsEmpty()) F.Add(TEXT("client_secret"), Secret);
		const FString Provider = P == EProvider::Google ? TEXT("google") : TEXT("microsoft");
		PostForm(TokenUrl(P), F, [Done, Provider](bool bOk, int32 Http, const FString& Body)
		{
			if (!bOk) { Done(0, FAccount(), TEXT("brak połączenia, ponawiam")); return; }
			const TSharedPtr<FJsonObject> O = ParseJson(Body);
			FString Err;
			if (O.IsValid()) O->TryGetStringField(TEXT("error"), Err);
			if (Http == 200 && O.IsValid())
			{
				FString IdToken;
				FAccount A;
				if (O->TryGetStringField(TEXT("id_token"), IdToken) && ParseIdToken(IdToken, Provider, A)) { Done(1, A, FString()); return; }
				Done(2, FAccount(), TEXT("Odpowiedź bez danych konta"));
				return;
			}
			if (Err == TEXT("authorization_pending") || Err == TEXT("slow_down")) { Done(0, FAccount(), FString()); return; }
			if (Err == TEXT("access_denied") || Err == TEXT("authorization_declined")) { Done(2, FAccount(), TEXT("Odmówiono dostępu")); return; }
			if (Err == TEXT("expired_token")) { Done(2, FAccount(), TEXT("Kod wygasł, spróbuj ponownie")); return; }
			Done(2, FAccount(), Err.IsEmpty() ? FString::Printf(TEXT("Błąd logowania (HTTP %d)"), Http) : Err);
		});
	}
}
