#include "Editor/Commandlets/WuwaCurveSetupCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Curves/CurveFloat.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaCurveSetup, Log, All);

UWuwaCurveSetupCommandlet::UWuwaCurveSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpUsage = TEXT("-run=WuwaCurveSetup -Recipe=ArtSource/<...>.json [-Apply]");
}

#if WITH_EDITOR
namespace WuwaCurveSetup
{
	template <typename TEnum>
	bool EnumByName(const FString& Name, TEnumAsByte<TEnum>& Out)
	{
		const int64 Value = StaticEnum<TEnum>()->GetValueByNameString(Name);
		if (Value == INDEX_NONE) return false;
		Out = static_cast<TEnum>(Value);
		return true;
	}

	/** 配方里的一条曲线 → FRichCurve；枚举名写错返回 false。 */
	bool Parse(const TSharedPtr<FJsonObject>& Def, FRichCurve& Out)
	{
		for (const TSharedPtr<FJsonValue>& Value : Def->GetArrayField(TEXT("keys")))
		{
			const TSharedPtr<FJsonObject> Key = Value->AsObject();
			FRichCurveKey& Data = Out.GetKey(Out.AddKey(Key->GetNumberField(TEXT("time")), Key->GetNumberField(TEXT("value"))));
			if (!EnumByName(Key->GetStringField(TEXT("interp")), Data.InterpMode) || !EnumByName(Key->GetStringField(TEXT("tangentMode")), Data.TangentMode)) return false;
			Data.ArriveTangent = Key->GetNumberField(TEXT("arrive"));
			Data.LeaveTangent = Key->GetNumberField(TEXT("leave"));
		}
		return EnumByName(Def->GetStringField(TEXT("preInfinity")), Out.PreInfinityExtrap)
			&& EnumByName(Def->GetStringField(TEXT("postInfinity")), Out.PostInfinityExtrap);
	}

	bool Same(const FRichCurve& A, const FRichCurve& B)
	{
		if (A.GetNumKeys() != B.GetNumKeys() || A.PreInfinityExtrap != B.PreInfinityExtrap || A.PostInfinityExtrap != B.PostInfinityExtrap) return false;
		const TArray<FRichCurveKey>& KeysA = A.GetConstRefOfKeys();
		const TArray<FRichCurveKey>& KeysB = B.GetConstRefOfKeys();
		for (int32 i = 0; i < KeysA.Num(); ++i)
		{
			if (!(KeysA[i] == KeysB[i])) return false;
		}
		return true;
	}
}
#endif

int32 UWuwaCurveSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaCurveSetup;
	FString RecipeArg;
	if (!FParse::Value(*Params, TEXT("Recipe="), RecipeArg))
	{
		UE_LOG(LogWuwaCurveSetup, Error, TEXT("Usage: %s"), *HelpUsage);
		return 1;
	}
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	const FString RecipePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / RecipeArg);
	FString RecipeText;
	TSharedPtr<FJsonObject> Recipe;
	const TArray<TSharedPtr<FJsonValue>>* Curves = nullptr;
	if (!FFileHelper::LoadFileToString(RecipeText, *RecipePath)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(RecipeText), Recipe) || !Recipe.IsValid()
		|| !Recipe->TryGetArrayField(TEXT("curves"), Curves))
	{
		UE_LOG(LogWuwaCurveSetup, Error, TEXT("Cannot read curves from %s"), *RecipePath);
		return 1;
	}

	// 先全部解析：写错时不动资产
	TArray<TPair<FString, FRichCurve>> Parsed;
	for (const TSharedPtr<FJsonValue>& Value : *Curves)
	{
		const TSharedPtr<FJsonObject> Def = Value->AsObject();
		TPair<FString, FRichCurve>& Entry = Parsed.AddDefaulted_GetRef();
		Entry.Key = Def->GetStringField(TEXT("path"));
		if (!FPackageName::IsValidLongPackageName(Entry.Key) || !Parse(Def, Entry.Value))
		{
			UE_LOG(LogWuwaCurveSetup, Error, TEXT("Bad curve %s (path or enum name)."), *Entry.Key);
			return 1;
		}
	}

	const FString BackupDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/CurveSetup") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	int32 Errors = 0;
	for (const TPair<FString, FRichCurve>& Entry : Parsed)
	{
		const FString& PackageName = Entry.Key;
		const FString AssetName = FPackageName::GetShortName(PackageName);
		UCurveFloat* Curve = LoadObject<UCurveFloat>(nullptr, *(PackageName + TEXT(".") + AssetName), nullptr, LOAD_NoWarn | LOAD_Quiet);
		const TCHAR* State = !Curve ? TEXT("create") : Same(Curve->FloatCurve, Entry.Value) ? TEXT("unchanged") : TEXT("update");
		UE_LOG(LogWuwaCurveSetup, Display, TEXT("  %s: %s (%d keys)"), *PackageName, State, Entry.Value.GetNumKeys());
		if (!bApply || (Curve && Same(Curve->FloatCurve, Entry.Value))) continue;

		const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		if (Curve)
		{
			if (IFileManager::Get().Copy(*(BackupDir / FPaths::GetCleanFilename(Filename)), *Filename) != COPY_OK)
			{
				UE_LOG(LogWuwaCurveSetup, Error, TEXT("Failed to back up %s"), *Filename);
				++Errors;
				continue;
			}
			Curve->Modify();
		}
		else
		{
			Curve = NewObject<UCurveFloat>(CreatePackage(*PackageName), *AssetName, RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Curve);
		}
		Curve->FloatCurve = Entry.Value;
		UPackage* Package = Curve->GetOutermost();
		Package->MarkPackageDirty();
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		Args.Error = GWarn;
		if (!UPackage::SavePackage(Package, Curve, *Filename, Args))
		{
			UE_LOG(LogWuwaCurveSetup, Error, TEXT("Failed to save %s (is the editor open?)"), *Filename);
			++Errors;
		}
	}
	UE_LOG(LogWuwaCurveSetup, Display, TEXT("%d curves (%s)%s"), Parsed.Num(), *Recipe->GetStringField(TEXT("source")),
		bApply ? TEXT("") : TEXT(" — read-only, pass -Apply to write"));
	return Errors > 0 ? 1 : 0;
#else
	return 1;
#endif
}
