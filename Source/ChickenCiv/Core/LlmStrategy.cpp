#include "Core/LlmStrategy.h"
#include "Core/CivAI.h"
#include "Model/TechTree.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"
#include "Misc/App.h"

namespace LlmStrategy
{
	static const TCHAR* GSection = TEXT("ChickenCiv.Llm");

	FString GetApiKey()
	{
		FString Key = FPlatformMisc::GetEnvironmentVariable(TEXT("ANTHROPIC_API_KEY"));
		if (Key.IsEmpty() && GConfig) GConfig->GetString(GSection, TEXT("ApiKey"), Key, GGameUserSettingsIni);
		Key.TrimStartAndEndInline();
		return Key;
	}

	bool HasApiKey() { return !GetApiKey().IsEmpty(); }

	void SetApiKey(const FString& Key)
	{
		if (!GConfig) return;
		GConfig->SetString(GSection, TEXT("ApiKey"), *Key, GGameUserSettingsIni);
		GConfig->Flush(false, GGameUserSettingsIni);
	}

	FString GetModel()
	{
		FString Model = FPlatformMisc::GetEnvironmentVariable(TEXT("CHICKEN_LLM_MODEL"));
		if (Model.IsEmpty() && GConfig) GConfig->GetString(GSection, TEXT("Model"), Model, GGameUserSettingsIni);
		return Model.IsEmpty() ? FString(TEXT("claude-haiku-5-5")) : Model;
	}

	FString BuildPrompt(const FHexMap& M, const FCivState& S, const TArray<FPlayerInfo>& Players, int32 Player, int32 Turn)
	{
		static const TCHAR* PathNames[] = { TEXT("main"), TEXT("ships"), TEXT("lasers"), TEXT("elements"), TEXT("advanced") };
		FString P;
		P += TEXT("You steer the strategy of one civilization of chickens in a turn-based 4X game (Civilization-like). ");
		P += TEXT("A classic AI handles tactics; you only set high level strategy.\n");
		P += FString::Printf(TEXT("Turn %d. You are civilization %d.\n"), Turn, Player + 1);
		if (S.PlayerStates.IsValidIndex(Player))
		{
			const FCivPlayerState& PS = S.PlayerStates[Player];
			P += FString::Printf(TEXT("Your race: %s. Gold %d, science points %d, nuggets %d. Techs known %d of %d. Current aggression %d/100, wants %d cities, research focus '%s'.\n"),
				*CivNames::RaceName(PS.Race), PS.Gold, PS.Science, PS.Nuggets, PS.Known.Num(), NumTechs - 1, PS.AIAggression, PS.AITargetCities, PathNames[FMath::Clamp(PS.AIFocusPath, 0, 4)]);
		}
		for (int32 p = 0; p < S.PlayerStates.Num(); ++p)
		{
			int32 Cities = 0, Units = 0;
			for (const FCity& C : S.Cities) if (C.Owner == p) ++Cities;
			for (const FUnit& U : S.Units) if (U.Owner == p && !U.IsArmyMember()) ++Units;
			P += FString::Printf(TEXT("Civilization %d%s: race %s, %d cities, %d units%s.\n"), p + 1, p == Player ? TEXT(" (you)") : TEXT(""),
				Players.IsValidIndex(p) ? *CivNames::RaceName(Players[p].Race) : TEXT("?"), Cities, Units,
				(p != Player && CivAI::IsAtWar(S, Player, p)) ? TEXT(", AT WAR with you") : TEXT(""));
		}
		P += TEXT("Race perks: red science x3, yellow strength x2, blue ships fast/strong/cheap, green guardian wipes armies attacking its cities. ");
		P += TEXT("Tech paths after space flight: ships, lasers, elements, then advanced tech.\n");
		P += TEXT("Reply with ONLY a JSON object, no prose: {\"aggression\": 0-100, \"target_cities\": 1-8, \"focus\": \"main|ships|lasers|elements\", \"war_target\": <civilization number or null>, \"note\": \"max 12 words\"}");
		return P;
	}

	bool ApplyReply(const FString& ReplyText, FCivState& S, int32 Player)
	{
		if (!S.PlayerStates.IsValidIndex(Player)) return false;
		int32 A = INDEX_NONE, B = INDEX_NONE;
		ReplyText.FindChar(TEXT('{'), A);
		ReplyText.FindLastChar(TEXT('}'), B);
		if (A == INDEX_NONE || B == INDEX_NONE || B <= A) return false;
		const FString Json = ReplyText.Mid(A, B - A + 1);

		TSharedPtr<FJsonObject> Obj;
		TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Obj) || !Obj.IsValid()) return false;

		FCivPlayerState& PS = S.PlayerStates[Player];
		bool bAny = false;
		double V = 0.0;
		if (Obj->TryGetNumberField(TEXT("aggression"), V)) { PS.AIAggression = FMath::Clamp((int32)V, 0, 100); bAny = true; }
		if (Obj->TryGetNumberField(TEXT("target_cities"), V)) { PS.AITargetCities = FMath::Clamp((int32)V, 1, 8); bAny = true; }
		FString Focus;
		if (Obj->TryGetStringField(TEXT("focus"), Focus))
		{
			Focus = Focus.ToLower();
			if (Focus == TEXT("main")) { PS.AIFocusPath = 0; bAny = true; }
			else if (Focus == TEXT("ships")) { PS.AIFocusPath = 1; bAny = true; }
			else if (Focus == TEXT("lasers")) { PS.AIFocusPath = 2; bAny = true; }
			else if (Focus == TEXT("elements")) { PS.AIFocusPath = 3; bAny = true; }
		}
		if (Obj->TryGetNumberField(TEXT("war_target"), V))
		{
			const int32 T = (int32)V - 1; // w odpowiedzi numeracja od 1
			if (T >= 0 && T < S.PlayerStates.Num() && T != Player) { PS.AIWarTarget = T; bAny = true; }
		}
		else if (Obj->HasTypedField<EJson::Null>(TEXT("war_target")))
		{
			PS.AIWarTarget = -1;
			bAny = true;
		}
		return bAny;
	}

	void RequestStrategy(const FString& Prompt, TFunction<void(bool, const FString&)> Callback)
	{
		const FString Key = GetApiKey();
		if (Key.IsEmpty()) { Callback(false, TEXT("brak klucza API")); return; }

		TSharedRef<FJsonObject> Msg = MakeShared<FJsonObject>();
		Msg->SetStringField(TEXT("role"), TEXT("user"));
		Msg->SetStringField(TEXT("content"), Prompt);
		TSharedRef<FJsonObject> Body = MakeShared<FJsonObject>();
		Body->SetStringField(TEXT("model"), GetModel());
		Body->SetNumberField(TEXT("max_tokens"), 300);
		TArray<TSharedPtr<FJsonValue>> Messages;
		Messages.Add(MakeShared<FJsonValueObject>(Msg));
		Body->SetArrayField(TEXT("messages"), Messages);
		FString BodyStr;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
		FJsonSerializer::Serialize(Body, Writer);

		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Req = FHttpModule::Get().CreateRequest();
		Req->SetURL(TEXT("https://api.anthropic.com/v1/messages"));
		Req->SetVerb(TEXT("POST"));
		Req->SetHeader(TEXT("content-type"), TEXT("application/json"));
		Req->SetHeader(TEXT("x-api-key"), Key);
		Req->SetHeader(TEXT("anthropic-version"), TEXT("2023-06-01"));
		Req->SetTimeout(30.f);
		Req->SetContentAsString(BodyStr);
		Req->OnProcessRequestComplete().BindLambda([Callback](FHttpRequestPtr, FHttpResponsePtr Res, bool bOk)
		{
			if (!bOk || !Res.IsValid() || Res->GetResponseCode() != 200)
			{
				Callback(false, Res.IsValid() ? FString::Printf(TEXT("HTTP %d"), Res->GetResponseCode()) : FString(TEXT("brak odpowiedzi")));
				return;
			}
			TSharedPtr<FJsonObject> Root;
			TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Res->GetContentAsString());
			if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) { Callback(false, TEXT("nieczytelna odpowiedz")); return; }
			const TArray<TSharedPtr<FJsonValue>>* Content = nullptr;
			if (!Root->TryGetArrayField(TEXT("content"), Content) || Content->Num() == 0) { Callback(false, TEXT("pusta odpowiedz")); return; }
			FString Text;
			for (const TSharedPtr<FJsonValue>& V : *Content)
			{
				const TSharedPtr<FJsonObject>* O = nullptr;
				FString T;
				if (V->TryGetObject(O) && (*O)->TryGetStringField(TEXT("text"), T)) Text += T;
			}
			Callback(!Text.IsEmpty(), Text);
		});
		Req->ProcessRequest();
	}
}
