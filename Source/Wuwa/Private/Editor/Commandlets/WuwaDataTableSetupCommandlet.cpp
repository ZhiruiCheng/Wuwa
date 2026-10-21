#include "Editor/Commandlets/WuwaDataTableSetupCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Engine/DataTable.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogWuwaDataTableSetup, Log, All);

UWuwaDataTableSetupCommandlet::UWuwaDataTableSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpUsage = TEXT("-run=WuwaDataTableSetup -Recipe=ArtSource/<...>.json [-OnlyFields=A,B] [-Apply]");
}

int32 UWuwaDataTableSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	FString RecipeArg;
	if (!FParse::Value(*Params, TEXT("Recipe="), RecipeArg))
	{
		UE_LOG(LogWuwaDataTableSetup, Error, TEXT("Usage: %s"), *HelpUsage);
		return 1;
	}
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	const FString RecipePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / RecipeArg);
	FString RecipeText;
	TSharedPtr<FJsonObject> Recipe;
	if (!FFileHelper::LoadFileToString(RecipeText, *RecipePath)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(RecipeText), Recipe) || !Recipe.IsValid())
	{
		UE_LOG(LogWuwaDataTableSetup, Error, TEXT("Cannot read %s"), *RecipePath);
		return 1;
	}
	const FString TablePackage = Recipe->GetStringField(TEXT("table"));
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (TablePackage.IsEmpty() || !Recipe->TryGetArrayField(TEXT("rows"), Rows))
	{
		UE_LOG(LogWuwaDataTableSetup, Error, TEXT("%s has no table/rows."), *RecipePath);
		return 1;
	}
	const FString ObjectPath = TablePackage + TEXT(".") + FPackageName::GetShortName(TablePackage);
	UDataTable* Table = LoadObject<UDataTable>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	const UScriptStruct* RowStruct = Table ? Table->GetRowStruct() : FindObject<UScriptStruct>(nullptr, *Recipe->GetStringField(TEXT("struct")));
	if (!RowStruct)
	{
		UE_LOG(LogWuwaDataTableSetup, Error, TEXT("%s does not exist and struct '%s' is unknown."), *TablePackage, *Recipe->GetStringField(TEXT("struct")));
		return 1;
	}
	FString RecipeStruct;
	if (Recipe->TryGetStringField(TEXT("struct"), RecipeStruct) && !RecipeStruct.IsEmpty() && RecipeStruct != RowStruct->GetPathName())
	{
		UE_LOG(LogWuwaDataTableSetup, Error, TEXT("%s has row struct %s, recipe says %s."), *TablePackage, *RowStruct->GetPathName(), *RecipeStruct);
		return 1;
	}
	FString OnlyFieldsText;
	const bool bOnlyFields = FParse::Value(*Params, TEXT("OnlyFields="), OnlyFieldsText, false);
	if (bOnlyFields && !Table)
	{
		UE_LOG(LogWuwaDataTableSetup, Error, TEXT("%s does not exist; -OnlyFields needs an existing table."), *TablePackage);
		return 1;
	}

	// 先在临时表里解析：字段名 / 枚举名写错时这里报错，不动资产（只更新部分字段时，配方行缺的字段不算错）
	FString RowsJson;
	FJsonSerializer::Serialize(*Rows, TJsonWriterFactory<>::Create(&RowsJson));
	UDataTable* Parsed = NewObject<UDataTable>(GetTransientPackage());
	Parsed->RowStruct = const_cast<UScriptStruct*>(RowStruct);
	int32 Errors = 0;
	for (const FString& Problem : Parsed->CreateTableFromJSONString(RowsJson))
	{
		if (bOnlyFields && Problem.Contains(TEXT("is missing an entry for"))) continue;
		UE_LOG(LogWuwaDataTableSetup, Error, TEXT("%s"), *Problem);
		++Errors;
	}
	if (Errors > 0) return 1;

	const FString BackupDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/DataTableSetup") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	auto BackupAndSave = [&](UDataTable* Target) -> bool
	{
		const FString Filename = FPackageName::LongPackageNameToFilename(TablePackage, FPackageName::GetAssetPackageExtension());
		if (IFileManager::Get().FileExists(*Filename) && IFileManager::Get().Copy(*(BackupDir / FPaths::GetCleanFilename(Filename)), *Filename) != COPY_OK)
		{
			UE_LOG(LogWuwaDataTableSetup, Error, TEXT("Failed to back up %s"), *Filename);
			return false;
		}
		UPackage* Package = Target->GetOutermost();
		Package->MarkPackageDirty();
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		Args.Error = GWarn;
		if (!UPackage::SavePackage(Package, Target, *Filename, Args))
		{
			UE_LOG(LogWuwaDataTableSetup, Error, TEXT("Failed to save %s (is the editor open?)"), *Filename);
			return false;
		}
		UE_LOG(LogWuwaDataTableSetup, Display, TEXT("Saved %s (backup in %s)"), *TablePackage, *BackupDir);
		return true;
	};

	if (bOnlyFields)
	{
		TArray<FString> FieldNames;
		OnlyFieldsText.ParseIntoArray(FieldNames, TEXT(","));
		TArray<const FProperty*> Fields;
		for (const FString& FieldName : FieldNames)
		{
			const FProperty* Field = RowStruct->FindPropertyByName(FName(*FieldName.TrimStartAndEnd()));
			if (!Field)
			{
				UE_LOG(LogWuwaDataTableSetup, Error, TEXT("%s has no field %s."), *RowStruct->GetName(), *FieldName);
				return 1;
			}
			Fields.Add(Field);
		}
		TArray<TPair<uint8*, const uint8*>> Updates;
		int32 Missing = 0;
		for (const TPair<FName, uint8*>& Pair : Parsed->GetRowMap())
		{
			uint8* Row = Table->FindRowUnchecked(Pair.Key);
			if (!Row) { ++Missing; continue; }
			bool bChanged = false;
			for (const FProperty* Field : Fields)
			{
				if (Field->Identical_InContainer(Row, Pair.Value)) continue;
				FString Old, New;
				Field->ExportText_InContainer(0, Old, Row, Row, nullptr, PPF_None);
				Field->ExportText_InContainer(0, New, Pair.Value, Pair.Value, nullptr, PPF_None);
				UE_LOG(LogWuwaDataTableSetup, Display, TEXT("  %s.%s: %s -> %s"), *Pair.Key.ToString(), *Field->GetName(), *Old, *New);
				bChanged = true;
			}
			if (bChanged) Updates.Emplace(Row, Pair.Value);
		}
		UE_LOG(LogWuwaDataTableSetup, Display, TEXT("%s: %s changes %d rows (%d recipe rows not in the table, left out)%s"), *TablePackage, *OnlyFieldsText,
			Updates.Num(), Missing, bApply ? TEXT("") : TEXT(" — read-only, pass -Apply to write"));
		if (!bApply || Updates.IsEmpty()) return 0;
		Table->Modify();
		for (const TPair<uint8*, const uint8*>& Update : Updates)
		{
			for (const FProperty* Field : Fields) Field->CopyCompleteValue_InContainer(Update.Key, Update.Value);
		}
		Table->HandleDataTableChanged();
		return BackupAndSave(Table) ? 0 : 1;
	}

	int32 Added = 0, Changed = 0, Removed = 0;
	for (const TPair<FName, uint8*>& Pair : Parsed->GetRowMap())
	{
		const uint8* Old = Table ? Table->FindRowUnchecked(Pair.Key) : nullptr;
		if (!Old) ++Added;
		else if (!RowStruct->CompareScriptStruct(Old, Pair.Value, PPF_None)) ++Changed;
	}
	if (Table) for (const FName& Name : Table->GetRowNames()) Removed += Parsed->FindRowUnchecked(Name) ? 0 : 1;
	UE_LOG(LogWuwaDataTableSetup, Display, TEXT("%s: %d rows (%s); added %d, changed %d, removed %d%s"), *TablePackage, Parsed->GetRowMap().Num(),
		*Recipe->GetStringField(TEXT("source")), Added, Changed, Removed, bApply ? TEXT("") : TEXT(" — read-only, pass -Apply to write"));
	if (!bApply) return 0;
	if (!Table)
	{
		UPackage* Package = CreatePackage(*TablePackage);
		Table = NewObject<UDataTable>(Package, *FPackageName::GetShortName(TablePackage), RF_Public | RF_Standalone | RF_Transactional);
		Table->RowStruct = const_cast<UScriptStruct*>(RowStruct);
		FAssetRegistryModule::AssetCreated(Table);
	}
	Table->CreateTableFromJSONString(RowsJson);
	return BackupAndSave(Table) ? 0 : 1;
#else
	return 1;
#endif
}
