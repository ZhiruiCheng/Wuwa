#include "Editor/Commandlets/WuwaBulletTableSetupCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Game/NewWorld/Bullet/WuwaBulletTypes.h"
#include "Game/NewWorld/Character/Common/WuwaCharactorBase.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogWuwaBulletTable, Log, All);

UWuwaBulletTableSetupCommandlet::UWuwaBulletTableSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpDescription = TEXT("Rebuilds a bullet DataTable (FWuwaBulletInfo) from ArtSource/Bullet/<character>.bullets.json and assigns it to the character "
		"blueprints next to it. -AssignOnly keeps the table as is (keeps edits made in the bullet editor). Read-only unless -Apply.");
	HelpUsage = TEXT("-run=WuwaBulletTableSetup -Character=changli|jiabei [-AssignOnly | -OnlyFields=A,B] [-Apply]");
}

#if WITH_EDITOR
namespace WuwaBulletTableSetup
{
	bool Backup(const FString& PackageName, FString& BackupDir)
	{
		const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		if (!IFileManager::Get().FileExists(*Filename)) return true;
		if (BackupDir.IsEmpty())
		{
			BackupDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/BulletTableSetup") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
		}
		if (IFileManager::Get().Copy(*(BackupDir / FPaths::GetCleanFilename(Filename)), *Filename) != COPY_OK)
		{
			UE_LOG(LogWuwaBulletTable, Error, TEXT("Failed to back up %s"), *Filename);
			return false;
		}
		UE_LOG(LogWuwaBulletTable, Display, TEXT("Backed up %s to %s"), *PackageName, *BackupDir);
		return true;
	}

	bool Save(UObject* Asset)
	{
		UPackage* Package = Asset->GetOutermost();
		Package->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		Args.Error = GWarn;
		if (!UPackage::SavePackage(Package, Asset, *Filename, Args))
		{
			UE_LOG(LogWuwaBulletTable, Error, TEXT("Failed to save %s (is the editor open?)"), *Filename);
			return false;
		}
		UE_LOG(LogWuwaBulletTable, Display, TEXT("Saved %s"), *Package->GetName());
		return true;
	}

	/**
	 * 表在 <角色目录>/Data/ 下；角色目录里（不含子目录）继承 AWuwaCharactorBase 的蓝图把 BulletTable 指向这张表。
	 * 芙露德莉丝没有角色蓝图，找不到时只打日志。
	 */
	bool AssignToCharacters(UDataTable* Table, bool bApply, FString& BackupDir)
	{
		const FString TableFolder = FPackageName::GetLongPackagePath(Table->GetOutermost()->GetName());
		const FString RoleFolder = FPackageName::GetLongPackagePath(TableFolder);
		IAssetRegistry& Registry = FAssetRegistryModule::GetRegistry();
		Registry.ScanPathsSynchronous({RoleFolder}, false);
		TArray<FAssetData> Assets;
		Registry.GetAssetsByPath(*RoleFolder, Assets, false);
		int32 Found = 0;
		for (const FAssetData& Asset : Assets)
		{
			if (Asset.AssetClassPath != UBlueprint::StaticClass()->GetClassPathName()) continue;
			UBlueprint* Blueprint = Cast<UBlueprint>(Asset.GetAsset());
			AWuwaCharactorBase* Defaults = Blueprint && Blueprint->GeneratedClass ? Cast<AWuwaCharactorBase>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
			if (!Defaults) continue;
			++Found;
			if (Defaults->BulletTable == Table)
			{
				UE_LOG(LogWuwaBulletTable, Display, TEXT("%s already uses %s."), *Blueprint->GetName(), *Table->GetName());
				continue;
			}
			UE_LOG(LogWuwaBulletTable, Display, TEXT("%s: BulletTable %s -> %s"), *Blueprint->GetName(), *GetNameSafe(Defaults->BulletTable), *Table->GetName());
			if (!bApply) continue;
			if (!Backup(Blueprint->GetOutermost()->GetName(), BackupDir)) return false;
			Defaults->Modify();
			Defaults->BulletTable = Table;
			if (!Save(Blueprint)) return false;
		}
		if (Found == 0) UE_LOG(LogWuwaBulletTable, Display, TEXT("No character blueprint in %s; BulletTable not assigned."), *RoleFolder);
		return true;
	}
}
#endif

int32 UWuwaBulletTableSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	FString Character;
	if (!FParse::Value(*Params, TEXT("Character="), Character) || Character.IsEmpty())
	{
		UE_LOG(LogWuwaBulletTable, Error, TEXT("Usage: %s"), *HelpUsage);
		return 1;
	}
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));

	// 配方：{ table, source, rows: [ { Name, <FWuwaBulletInfo 字段>... } ], skipped }
	const FString RecipePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("ArtSource/Bullet") / (Character + TEXT(".bullets.json")));
	FString RecipeText;
	TSharedPtr<FJsonObject> Recipe;
	if (!FFileHelper::LoadFileToString(RecipeText, *RecipePath)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(RecipeText), Recipe) || !Recipe.IsValid())
	{
		UE_LOG(LogWuwaBulletTable, Error, TEXT("Cannot read %s (run ArtSource/Bullet/prepare_bullets.py --character %s first)."), *RecipePath, *Character);
		return 1;
	}
	const FString TablePackage = Recipe->GetStringField(TEXT("table"));
	const FString ObjectPath = TablePackage + TEXT(".") + FPackageName::GetShortName(TablePackage);
	FString BackupDir;
	if (FParse::Param(*Params, TEXT("AssignOnly")))
	{
		UDataTable* Existing = LoadObject<UDataTable>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Existing || Existing->GetRowStruct() != FWuwaBulletInfo::StaticStruct())
		{
			UE_LOG(LogWuwaBulletTable, Error, TEXT("%s is missing or not a bullet table; run without -AssignOnly first."), *TablePackage);
			return 1;
		}
		if (!WuwaBulletTableSetup::AssignToCharacters(Existing, bApply, BackupDir)) return 1;
		if (!bApply) UE_LOG(LogWuwaBulletTable, Display, TEXT("Read-only; pass -Apply to write."));
		return 0;
	}
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (TablePackage.IsEmpty() || !Recipe->TryGetArrayField(TEXT("rows"), Rows))
	{
		UE_LOG(LogWuwaBulletTable, Error, TEXT("%s has no table/rows."), *RecipePath);
		return 1;
	}
	FString RowsJson;
	FJsonSerializer::Serialize(*Rows, TJsonWriterFactory<>::Create(&RowsJson));

	// 先在临时表里解析一遍：字段名/枚举名写错时这里报错，不会动到资产
	UDataTable* Parsed = NewObject<UDataTable>(GetTransientPackage());
	Parsed->RowStruct = FWuwaBulletInfo::StaticStruct();
	const TArray<FString> Problems = Parsed->CreateTableFromJSONString(RowsJson);
	for (const FString& Problem : Problems) UE_LOG(LogWuwaBulletTable, Error, TEXT("%s"), *Problem);
	int32 Errors = Problems.Num();
	for (const FName& Name : Parsed->GetRowNames())
	{
		if (WuwaBullet::ParseBulletId(Name) <= 0)
		{
			UE_LOG(LogWuwaBulletTable, Error, TEXT("Row %s is not a positive bullet ID."), *Name.ToString());
			++Errors;
		}
	}
	if (Errors > 0) return 1;

	UDataTable* Table = LoadObject<UDataTable>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
	if (Table && Table->GetRowStruct() != FWuwaBulletInfo::StaticStruct())
	{
		UE_LOG(LogWuwaBulletTable, Error, TEXT("%s exists but its row struct is %s."), *TablePackage, *GetNameSafe(Table->GetRowStruct()));
		return 1;
	}

	// -OnlyFields=A,B：只把这几个字段从配方写进现有表的同名行，其余字段（包括子弹编辑器里改过的）不动；配方里多出的行不加
	FString OnlyFieldsText;
	if (FParse::Value(*Params, TEXT("OnlyFields="), OnlyFieldsText, false))
	{
		if (!Table)
		{
			UE_LOG(LogWuwaBulletTable, Error, TEXT("%s does not exist; -OnlyFields needs an existing table."), *TablePackage);
			return 1;
		}
		TArray<FString> FieldNames;
		OnlyFieldsText.ParseIntoArray(FieldNames, TEXT(","));
		TArray<const FProperty*> Fields;
		for (const FString& FieldName : FieldNames)
		{
			const FProperty* Field = FWuwaBulletInfo::StaticStruct()->FindPropertyByName(FName(*FieldName.TrimStartAndEnd()));
			if (!Field)
			{
				UE_LOG(LogWuwaBulletTable, Error, TEXT("FWuwaBulletInfo has no field %s."), *FieldName);
				return 1;
			}
			Fields.Add(Field);
		}
		int32 ChangedRows = 0, MissingRows = 0;
		TArray<TPair<uint8*, const uint8*>> Updates;
		for (const TPair<FName, uint8*>& Pair : Parsed->GetRowMap())
		{
			uint8* Row = Table->FindRowUnchecked(Pair.Key);
			if (!Row)
			{
				++MissingRows;
				continue;
			}
			bool bRowChanged = false;
			for (const FProperty* Field : Fields)
			{
				if (!Field->Identical_InContainer(Row, Pair.Value))
				{
					FString Old, New;
					Field->ExportText_InContainer(0, Old, Row, Row, nullptr, PPF_None);
					Field->ExportText_InContainer(0, New, Pair.Value, Pair.Value, nullptr, PPF_None);
					UE_LOG(LogWuwaBulletTable, Display, TEXT("  %s.%s: %s -> %s"), *Pair.Key.ToString(), *Field->GetName(), *Old, *New);
					bRowChanged = true;
				}
			}
			if (bRowChanged)
			{
				++ChangedRows;
				Updates.Emplace(Row, Pair.Value);
			}
		}
		UE_LOG(LogWuwaBulletTable, Display, TEXT("%s: %s changes %d rows (%d recipe rows not in the table, left out)."), *TablePackage, *OnlyFieldsText, ChangedRows, MissingRows);
		if (!bApply)
		{
			UE_LOG(LogWuwaBulletTable, Display, TEXT("Read-only; pass -Apply to write."));
			return 0;
		}
		if (Updates.IsEmpty()) return 0;
		if (!WuwaBulletTableSetup::Backup(TablePackage, BackupDir)) return 1;
		Table->Modify();
		for (const TPair<uint8*, const uint8*>& Update : Updates)
		{
			for (const FProperty* Field : Fields) Field->CopyCompleteValue_InContainer(Update.Key, Update.Value);
		}
		Table->HandleDataTableChanged();
		return WuwaBulletTableSetup::Save(Table) ? 0 : 1;
	}

	// 与现有表比较，只是为了在日志里看清这次改了什么
	int32 Added = 0, Changed = 0, Removed = 0;
	for (const TPair<FName, uint8*>& Pair : Parsed->GetRowMap())
	{
		const uint8* Old = Table ? Table->FindRowUnchecked(Pair.Key) : nullptr;
		if (!Old) ++Added;
		else if (!FWuwaBulletInfo::StaticStruct()->CompareScriptStruct(Old, Pair.Value, PPF_None)) ++Changed;
	}
	if (Table) for (const FName& Name : Table->GetRowNames()) Removed += Parsed->FindRowUnchecked(Name) ? 0 : 1;
	UE_LOG(LogWuwaBulletTable, Display, TEXT("%s: %d rows from %s (%s); added %d, changed %d, removed %d."),
		*TablePackage, Parsed->GetRowMap().Num(), *FPaths::GetCleanFilename(RecipePath), *Recipe->GetStringField(TEXT("source")), Added, Changed, Removed);
	if (!bApply)
	{
		if (Table) WuwaBulletTableSetup::AssignToCharacters(Table, false, BackupDir);
		UE_LOG(LogWuwaBulletTable, Display, TEXT("Read-only; pass -Apply to write."));
		return 0;
	}

	if (!WuwaBulletTableSetup::Backup(TablePackage, BackupDir)) return 1;
	if (!Table)
	{
		UPackage* Package = CreatePackage(*TablePackage);
		Table = NewObject<UDataTable>(Package, *FPackageName::GetShortName(TablePackage), RF_Public | RF_Standalone | RF_Transactional);
		Table->RowStruct = FWuwaBulletInfo::StaticStruct();
		FAssetRegistryModule::AssetCreated(Table);
	}
	Table->CreateTableFromJSONString(RowsJson);
	if (!WuwaBulletTableSetup::Save(Table)) return 1;
	return WuwaBulletTableSetup::AssignToCharacters(Table, true, BackupDir) ? 0 : 1;
#else
	return 1;
#endif
}
