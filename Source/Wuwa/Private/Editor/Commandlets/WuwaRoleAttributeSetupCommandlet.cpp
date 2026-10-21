#include "Editor/Commandlets/WuwaRoleAttributeSetupCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Engine/DataTable.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaRoleAttributeInit.h"
#include "Game/NewWorld/Character/Role/DataAsset/WuwaRoleDataAsset.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaRoleAttributeSetup, Log, All);

UWuwaRoleAttributeSetupCommandlet::UWuwaRoleAttributeSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpUsage = TEXT("-run=WuwaRoleAttributeSetup -Recipe=\"<abs role_attributes.recipe.json>\" [-Apply]");
}

#if WITH_EDITOR
namespace WuwaRoleAttributeSetup
{
	struct FContext
	{
		bool bApply = false;
		FString BackupDir;
		int32 Errors = 0;
	};

	template <typename T>
	T* LoadAsset(const FString& PackageName)
	{
		return LoadObject<T>(nullptr, *(PackageName + TEXT(".") + FPackageName::GetShortName(PackageName)), nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	bool SaveAsset(UObject* Asset)
	{
		UPackage* Package = Asset->GetOutermost();
		Package->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		if (!UPackage::SavePackage(Package, Asset, *Filename, Args))
		{
			UE_LOG(LogWuwaRoleAttributeSetup, Error, TEXT("Failed to save %s"), *Filename);
			return false;
		}
		return true;
	}

	bool Backup(FContext& Ctx, const FString& PackageName)
	{
		const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		if (!IFileManager::Get().FileExists(*Filename)) return true;
		if (Ctx.BackupDir.IsEmpty())
		{
			Ctx.BackupDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/RoleAttributeSetup") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
		}
		if (IFileManager::Get().Copy(*(Ctx.BackupDir / FPaths::GetCleanFilename(Filename)), *Filename) != COPY_OK)
		{
			UE_LOG(LogWuwaRoleAttributeSetup, Error, TEXT("Failed to back up %s"), *Filename);
			return false;
		}
		return true;
	}

	/** 整表重写：没有就新建（行结构 RowStruct）。-Apply 之外返回 nullptr。 */
	UDataTable* PrepareTable(FContext& Ctx, const FString& PackageName, UScriptStruct* RowStruct)
	{
		UDataTable* Table = LoadAsset<UDataTable>(PackageName);
		if (Table && Table->GetRowStruct() != RowStruct)
		{
			UE_LOG(LogWuwaRoleAttributeSetup, Error, TEXT("%s exists with another row struct."), *PackageName);
			++Ctx.Errors;
			return nullptr;
		}
		UE_LOG(LogWuwaRoleAttributeSetup, Display, TEXT("Table %s: will %s."), *PackageName, Table ? TEXT("rewrite") : TEXT("create"));
		if (!Ctx.bApply) return nullptr;
		if (Table)
		{
			if (!Backup(Ctx, PackageName)) return nullptr;
			Table->EmptyTable();
			return Table;
		}
		UPackage* Package = CreatePackage(*PackageName);
		Table = NewObject<UDataTable>(Package, *FPackageName::GetShortName(PackageName), RF_Public | RF_Standalone | RF_Transactional);
		Table->RowStruct = RowStruct;
		FAssetRegistryModule::AssetCreated(Table);
		return Table;
	}

	float Number(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field)
	{
		return static_cast<float>(Object->GetNumberField(Field));
	}

	bool SetupBaseProperties(FContext& Ctx, const FString& TablePackage, const TArray<TSharedPtr<FJsonValue>>& Properties)
	{
		const UEnum* ElementEnum = StaticEnum<EWuwaElement>();
		TMap<FName, FWuwaRoleBaseProperty> Rows;
		for (const TSharedPtr<FJsonValue>& Value : Properties)
		{
			const TSharedPtr<FJsonObject> P = Value->AsObject();
			FWuwaRoleBaseProperty Row;
			const int64 Element = ElementEnum->GetValueByNameString(P->GetStringField(TEXT("element")));
			if (Element == INDEX_NONE)
			{
				UE_LOG(LogWuwaRoleAttributeSetup, Error, TEXT("Unknown element %s"), *P->GetStringField(TEXT("element")));
				++Ctx.Errors;
			}
			Row.Element = Element == INDEX_NONE ? EWuwaElement::None : static_cast<EWuwaElement>(Element);
			Row.MaxHealth = Number(P, TEXT("maxHealth"));
			Row.Attack = Number(P, TEXT("attack"));
			Row.Defense = Number(P, TEXT("defense"));
			Row.CritRate = Number(P, TEXT("critRate"));
			Row.CritDamage = Number(P, TEXT("critDamage"));
			// 共鸣效率：配方没写（旧配方）按 100%
			Row.EnergyEfficiency = P->HasField(TEXT("energyEfficiency")) ? Number(P, TEXT("energyEfficiency")) : 1.f;
			Row.MaxEnergy = Number(P, TEXT("maxEnergy"));
			Row.MaxConcertoEnergy = Number(P, TEXT("maxConcertoEnergy"));
			Row.MaxSpecialEnergy1 = Number(P, TEXT("maxSpecialEnergy1"));
			Row.MaxSpecialEnergy2 = Number(P, TEXT("maxSpecialEnergy2"));
			Row.MaxSpecialEnergy3 = Number(P, TEXT("maxSpecialEnergy3"));
			Row.MaxSpecialEnergy4 = Number(P, TEXT("maxSpecialEnergy4"));
			Row.MaxSpecialEnergy5 = Number(P, TEXT("maxSpecialEnergy5"));
			const FString Id = FString::FromInt(static_cast<int32>(P->GetNumberField(TEXT("id"))));
			UE_LOG(LogWuwaRoleAttributeSetup, Display, TEXT("Base %s: %s HP %.0f ATK %.0f DEF %.0f"), *Id, *P->GetStringField(TEXT("element")), Row.MaxHealth, Row.Attack, Row.Defense);
			Rows.Add(*Id, Row);
		}
		UDataTable* Table = PrepareTable(Ctx, TablePackage, FWuwaRoleBaseProperty::StaticStruct());
		if (!Table) return !Ctx.bApply && !Ctx.Errors;
		for (const TPair<FName, FWuwaRoleBaseProperty>& Pair : Rows) Table->AddRow(Pair.Key, Pair.Value);
		return SaveAsset(Table);
	}

	bool SetupGrowth(FContext& Ctx, const FString& TablePackage, const TArray<TSharedPtr<FJsonValue>>& Growth)
	{
		UDataTable* Table = PrepareTable(Ctx, TablePackage, FWuwaRolePropertyGrowth::StaticStruct());
		if (!Table) return !Ctx.bApply && !Ctx.Errors;
		for (const TSharedPtr<FJsonValue>& Value : Growth)
		{
			const TSharedPtr<FJsonObject> G = Value->AsObject();
			FWuwaRolePropertyGrowth Row;
			Row.Level = static_cast<int32>(G->GetNumberField(TEXT("level")));
			Row.BreachLevel = static_cast<int32>(G->GetNumberField(TEXT("breachLevel")));
			Row.MaxHealthRatio = Number(G, TEXT("maxHealthRatio"));
			Row.AttackRatio = Number(G, TEXT("attackRatio"));
			Row.DefenseRatio = Number(G, TEXT("defenseRatio"));
			Table->AddRow(*FString::Printf(TEXT("%d_%d"), Row.Level, Row.BreachLevel), Row);
		}
		return SaveAsset(Table);
	}

	bool SetupRole(FContext& Ctx, const TSharedPtr<FJsonObject>& Recipe)
	{
		const FString AssetPackage = Recipe->GetStringField(TEXT("asset"));
		UWuwaRoleDataAsset* Role = LoadAsset<UWuwaRoleDataAsset>(AssetPackage);
		if (!Role)
		{
			UE_LOG(LogWuwaRoleAttributeSetup, Error, TEXT("Missing role asset %s"), *AssetPackage);
			++Ctx.Errors;
			return false;
		}
		const int32 Id = static_cast<int32>(Recipe->GetNumberField(TEXT("basePropertyId")));
		UE_LOG(LogWuwaRoleAttributeSetup, Display, TEXT("%s: BasePropertyId %d (was %d), level %d."), *AssetPackage, Id, Role->BasePropertyId, Role->Level);
		if (!Ctx.bApply) return true;
		if (!Backup(Ctx, AssetPackage)) return false;
		Role->Modify();
		Role->BasePropertyId = Id;
		return SaveAsset(Role);
	}
}
#endif

int32 UWuwaRoleAttributeSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaRoleAttributeSetup;
	FString RecipePath;
	if (!FParse::Value(*Params, TEXT("Recipe="), RecipePath, false))
	{
		UE_LOG(LogWuwaRoleAttributeSetup, Error, TEXT("Usage: %s"), *HelpUsage);
		return 1;
	}
	FString Text;
	TSharedPtr<FJsonObject> Recipe;
	const FString RecipeFile = FPaths::IsRelative(RecipePath) ? FPaths::ProjectDir() / RecipePath : RecipePath;
	if (!FFileHelper::LoadFileToString(Text, *RecipeFile) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Recipe) || !Recipe)
	{
		UE_LOG(LogWuwaRoleAttributeSetup, Error, TEXT("Cannot read recipe %s"), *RecipeFile);
		return 1;
	}
	FContext Ctx;
	Ctx.bApply = FParse::Param(*Params, TEXT("Apply"));
	bool bOk = SetupBaseProperties(Ctx, Recipe->GetStringField(TEXT("baseTable")), Recipe->GetArrayField(TEXT("baseProperties")));
	bOk &= SetupGrowth(Ctx, Recipe->GetStringField(TEXT("growthTable")), Recipe->GetArrayField(TEXT("growth")));
	for (const TSharedPtr<FJsonValue>& Role : Recipe->GetArrayField(TEXT("roles")))
	{
		bOk &= SetupRole(Ctx, Role->AsObject());
	}
	UE_LOG(LogWuwaRoleAttributeSetup, Display, TEXT("%s."), Ctx.bApply ? TEXT("Applied") : TEXT("Dry run; pass -Apply to write"));
	if (!Ctx.BackupDir.IsEmpty()) UE_LOG(LogWuwaRoleAttributeSetup, Display, TEXT("Backups in %s"), *Ctx.BackupDir);
	return bOk && !Ctx.Errors ? 0 : 1;
#else
	return 1;
#endif
}
