#include "Editor/Commandlets/WuwaCampTableSetupCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Engine/DataTable.h"
#include "Game/NewWorld/Combat/WuwaCamp.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaCampTable, Log, All);

UWuwaCampTableSetupCommandlet::UWuwaCampTableSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpDescription = TEXT("Rebuilds the camp DataTable (FWuwaCampRow) from ArtSource/Combat/camps.json. Read-only unless -Apply.");
	HelpUsage = TEXT("-run=WuwaCampTableSetup [-Apply]");
}

int32 UWuwaCampTableSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	const FString RecipePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("ArtSource/Combat/camps.json"));
	FString RecipeText;
	TSharedPtr<FJsonObject> Recipe;
	if (!FFileHelper::LoadFileToString(RecipeText, *RecipePath)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(RecipeText), Recipe) || !Recipe.IsValid())
	{
		UE_LOG(LogWuwaCampTable, Error, TEXT("Cannot read %s (run ArtSource/Combat/prepare_camps.py first)."), *RecipePath);
		return 1;
	}
	const FString TablePackage = Recipe->GetStringField(TEXT("table"));
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (TablePackage.IsEmpty() || !Recipe->TryGetArrayField(TEXT("rows"), Rows))
	{
		UE_LOG(LogWuwaCampTable, Error, TEXT("%s has no table/rows."), *RecipePath);
		return 1;
	}
	FString RowsJson;
	FJsonSerializer::Serialize(*Rows, TJsonWriterFactory<>::Create(&RowsJson));

	// 先在临时表里解析：字段名 / 枚举名写错时这里报错，不动资产
	UDataTable* Parsed = NewObject<UDataTable>(GetTransientPackage());
	Parsed->RowStruct = FWuwaCampRow::StaticStruct();
	const TArray<FString> Problems = Parsed->CreateTableFromJSONString(RowsJson);
	for (const FString& Problem : Problems) UE_LOG(LogWuwaCampTable, Error, TEXT("%s"), *Problem);
	if (Problems.Num() > 0) return 1;
	// 每行引用的阵营都要在表里
	int32 Errors = 0;
	for (const TPair<FName, uint8*>& Pair : Parsed->GetRowMap())
	{
		for (const TPair<FName, EWuwaCampRelation>& Relation : reinterpret_cast<const FWuwaCampRow*>(Pair.Value)->Relations)
		{
			if (!Parsed->FindRowUnchecked(Relation.Key))
			{
				UE_LOG(LogWuwaCampTable, Error, TEXT("%s refers to unknown camp %s."), *Pair.Key.ToString(), *Relation.Key.ToString());
				++Errors;
			}
		}
	}
	if (Errors > 0) return 1;

	const FString ObjectPath = TablePackage + TEXT(".") + FPackageName::GetShortName(TablePackage);
	UDataTable* Table = LoadObject<UDataTable>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (Table && Table->GetRowStruct() != FWuwaCampRow::StaticStruct())
	{
		UE_LOG(LogWuwaCampTable, Error, TEXT("%s exists but its row struct is %s."), *TablePackage, *GetNameSafe(Table->GetRowStruct()));
		return 1;
	}
	int32 Added = 0, Changed = 0;
	for (const TPair<FName, uint8*>& Pair : Parsed->GetRowMap())
	{
		const uint8* Old = Table ? Table->FindRowUnchecked(Pair.Key) : nullptr;
		if (!Old) ++Added;
		else if (!FWuwaCampRow::StaticStruct()->CompareScriptStruct(Old, Pair.Value, PPF_None)) ++Changed;
	}
	UE_LOG(LogWuwaCampTable, Display, TEXT("%s: %d camps from %s; added %d, changed %d."), *TablePackage, Parsed->GetRowMap().Num(),
		*Recipe->GetStringField(TEXT("source")), Added, Changed);
	if (!bApply)
	{
		UE_LOG(LogWuwaCampTable, Display, TEXT("Read-only; pass -Apply to write."));
		return 0;
	}

	const FString Filename = FPackageName::LongPackageNameToFilename(TablePackage, FPackageName::GetAssetPackageExtension());
	if (IFileManager::Get().FileExists(*Filename))
	{
		const FString BackupDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/CampTableSetup") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
		if (IFileManager::Get().Copy(*(BackupDir / FPaths::GetCleanFilename(Filename)), *Filename) != COPY_OK)
		{
			UE_LOG(LogWuwaCampTable, Error, TEXT("Failed to back up %s"), *Filename);
			return 1;
		}
		UE_LOG(LogWuwaCampTable, Display, TEXT("Backed up to %s"), *BackupDir);
	}
	if (!Table)
	{
		UPackage* Package = CreatePackage(*TablePackage);
		Table = NewObject<UDataTable>(Package, *FPackageName::GetShortName(TablePackage), RF_Public | RF_Standalone | RF_Transactional);
		Table->RowStruct = FWuwaCampRow::StaticStruct();
		FAssetRegistryModule::AssetCreated(Table);
	}
	Table->CreateTableFromJSONString(RowsJson);
	UPackage* Package = Table->GetOutermost();
	Package->MarkPackageDirty();
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	Args.SaveFlags = SAVE_NoError;
	Args.Error = GWarn;
	if (!UPackage::SavePackage(Package, Table, *Filename, Args))
	{
		UE_LOG(LogWuwaCampTable, Error, TEXT("Failed to save %s (is the editor open?)"), *Filename);
		return 1;
	}
	UE_LOG(LogWuwaCampTable, Display, TEXT("Saved %s"), *TablePackage);
	return 0;
#else
	return 1;
#endif
}
