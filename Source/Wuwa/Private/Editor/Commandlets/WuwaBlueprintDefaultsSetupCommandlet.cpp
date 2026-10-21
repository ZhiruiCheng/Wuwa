#include "Editor/Commandlets/WuwaBlueprintDefaultsSetupCommandlet.h"

#if WITH_EDITOR
#include "Dom/JsonObject.h"
#include "Engine/Blueprint.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogWuwaBlueprintDefaults, Log, All);

UWuwaBlueprintDefaultsSetupCommandlet::UWuwaBlueprintDefaultsSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpUsage = TEXT("-run=WuwaBlueprintDefaultsSetup -Recipe=ArtSource/<...>.json [-Apply]");
}

int32 UWuwaBlueprintDefaultsSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	FString RecipeArg;
	if (!FParse::Value(*Params, TEXT("Recipe="), RecipeArg))
	{
		UE_LOG(LogWuwaBlueprintDefaults, Error, TEXT("Usage: %s"), *HelpUsage);
		return 1;
	}
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	const FString RecipePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / RecipeArg);
	FString RecipeText;
	TSharedPtr<FJsonObject> Recipe;
	const TArray<TSharedPtr<FJsonValue>>* Blueprints = nullptr;
	if (!FFileHelper::LoadFileToString(RecipeText, *RecipePath)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(RecipeText), Recipe) || !Recipe.IsValid()
		|| !Recipe->TryGetArrayField(TEXT("blueprints"), Blueprints))
	{
		UE_LOG(LogWuwaBlueprintDefaults, Error, TEXT("Cannot read blueprints from %s"), *RecipePath);
		return 1;
	}

	struct FChange
	{
		/** 要存的资产（蓝图或数据资产）。 */
		UObject* Blueprint = nullptr;
		UObject* Defaults = nullptr;
		FProperty* Property = nullptr;
		TArray<uint8> Value;   // 已解析的新值（属性内存）
	};
	TArray<FChange> Changes;
	auto Release = [&Changes]()
	{
		for (FChange& Change : Changes) Change.Property->DestroyValue(Change.Value.GetData());
	};

	// 先全部解析到临时内存：蓝图 / 属性名 / 文本值写错时不动资产
	int32 Errors = 0;
	// 实例化子对象（例：输入规则 Conditions 里的条件对象）改过的资产
	TSet<UObject*> SubobjectDirty;
	for (const TSharedPtr<FJsonValue>& Value : *Blueprints)
	{
		const TSharedPtr<FJsonObject> Def = Value->AsObject();
		// "blueprint"：改蓝图生成类的默认对象；"asset"：直接改资产本身（数据资产等）
		FString Path;
		const bool bAsset = Def->TryGetStringField(TEXT("asset"), Path);
		if (!bAsset) Path = Def->GetStringField(TEXT("blueprint"));
		UObject* Blueprint = LoadObject<UObject>(nullptr, *(Path + TEXT(".") + FPackageName::GetShortName(Path)), nullptr, LOAD_NoWarn | LOAD_Quiet);
		const UBlueprint* AsBlueprint = Cast<UBlueprint>(Blueprint);
		UObject* Defaults = bAsset ? Blueprint : AsBlueprint && AsBlueprint->GeneratedClass ? AsBlueprint->GeneratedClass->GetDefaultObject() : nullptr;
		if (!Defaults)
		{
			UE_LOG(LogWuwaBlueprintDefaults, Error, TEXT("Blueprint %s not found."), *Path);
			++Errors;
			continue;
		}
		// "subobjects"：[{ name, class, properties }]，按名字建在默认对象下（已有同名同类就复用），属性文本导入；
		// properties 里用路径 "<资产路径>.<资产名>:<name>" 引用它们（实例化属性按路径找到已有子对象）。
		// 类可以是原生类路径，也可以是 C# 类（/Script/ManagedWuwa.X，取它的生成类）。
		const TArray<TSharedPtr<FJsonValue>>* Subobjects = nullptr;
		if (Def->TryGetArrayField(TEXT("subobjects"), Subobjects))
		{
			for (const TSharedPtr<FJsonValue>& SubValue : *Subobjects)
			{
				const TSharedPtr<FJsonObject> SubDef = SubValue->AsObject();
				const FString SubName = SubDef->GetStringField(TEXT("name"));
				const FString ClassPath = SubDef->GetStringField(TEXT("class"));
				UClass* SubClass = LoadClass<UObject>(nullptr, *ClassPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
				if (!SubClass)
				{
					const UBlueprint* ClassBlueprint = LoadObject<UBlueprint>(nullptr, *ClassPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
					SubClass = ClassBlueprint ? ClassBlueprint->GeneratedClass.Get() : nullptr;
				}
				if (!SubClass)
				{
					UE_LOG(LogWuwaBlueprintDefaults, Error, TEXT("%s: subobject %s class %s not found (C# classes: publish ManagedWuwa first)."), *Path, *SubName, *ClassPath);
					++Errors;
					continue;
				}
				UObject* Sub = StaticFindObjectFast(UObject::StaticClass(), Defaults, FName(*SubName));
				bool bChanged = false;
				if (Sub && Sub->GetClass() != SubClass)
				{
					Sub->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
					Sub = nullptr;
				}
				if (!Sub)
				{
					Sub = NewObject<UObject>(Defaults, SubClass, FName(*SubName), RF_Transactional);
					bChanged = true;
				}
				const TSharedPtr<FJsonObject>* SubProperties = nullptr;
				if (SubDef->TryGetObjectField(TEXT("properties"), SubProperties))
				{
					for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : (*SubProperties)->Values)
					{
						FProperty* Property = SubClass->FindPropertyByName(FName(*Entry.Key));
						FString Before;
						if (Property) Property->ExportTextItem_InContainer(Before, Sub, nullptr, nullptr, PPF_None);
						if (!Property || !Property->ImportText_InContainer(*Entry.Value->AsString(), Sub, Sub, PPF_None))
						{
							UE_LOG(LogWuwaBlueprintDefaults, Error, TEXT("%s:%s.%s: cannot set '%s'."), *Path, *SubName, *Entry.Key, *Entry.Value->AsString());
							++Errors;
							continue;
						}
						FString After;
						Property->ExportTextItem_InContainer(After, Sub, nullptr, nullptr, PPF_None);
						bChanged |= Before != After;
					}
				}
				UE_LOG(LogWuwaBlueprintDefaults, Display, TEXT("  subobject %s:%s (%s)%s"), *Blueprint->GetName(), *SubName, *SubClass->GetName(),
					bChanged ? TEXT(" changed") : TEXT(" (unchanged)"));
				if (bChanged) SubobjectDirty.Add(Blueprint);
			}
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : Def->GetObjectField(TEXT("properties"))->Values)
		{
			FProperty* Property = Defaults->GetClass()->FindPropertyByName(FName(*Entry.Key));
			if (!Property)
			{
				UE_LOG(LogWuwaBlueprintDefaults, Error, TEXT("%s has no property %s."), *Blueprint->GetName(), *Entry.Key);
				++Errors;
				continue;
			}
			FChange& Change = Changes.AddDefaulted_GetRef();
			Change.Blueprint = Blueprint;
			Change.Defaults = Defaults;
			Change.Property = Property;
			Change.Value.SetNumZeroed(Property->GetSize());
			Property->InitializeValue(Change.Value.GetData());
			const FString Text = Entry.Value->AsString();
			if (!Property->ImportText_Direct(*Text, Change.Value.GetData(), Defaults, PPF_None))
			{
				UE_LOG(LogWuwaBlueprintDefaults, Error, TEXT("%s.%s: cannot parse '%s'."), *Blueprint->GetName(), *Entry.Key, *Text);
				++Errors;
			}
		}
	}
	if (Errors > 0)
	{
		Release();
		return 1;
	}

	TSet<UObject*> Dirty;
	for (const FChange& Change : Changes)
	{
		void* Current = Change.Property->ContainerPtrToValuePtr<void>(Change.Defaults);
		FString Old, New;
		Change.Property->ExportTextItem_Direct(Old, Current, nullptr, nullptr, PPF_None);
		Change.Property->ExportTextItem_Direct(New, Change.Value.GetData(), nullptr, nullptr, PPF_None);
		const bool bSame = Change.Property->Identical(Current, Change.Value.GetData());
		UE_LOG(LogWuwaBlueprintDefaults, Display, TEXT("  %s.%s: %s%s%s"), *Change.Blueprint->GetName(), *Change.Property->GetName(), *Old,
			bSame ? TEXT(" (unchanged)") : TEXT(" -> "), bSame ? TEXT("") : *New);
		if (!bSame) Dirty.Add(Change.Blueprint);
	}
	Dirty.Append(SubobjectDirty);
	UE_LOG(LogWuwaBlueprintDefaults, Display, TEXT("%d blueprints to change (%s)%s"), Dirty.Num(), *Recipe->GetStringField(TEXT("source")),
		bApply ? TEXT("") : TEXT(" — read-only, pass -Apply to write"));
	if (!bApply || Dirty.IsEmpty())
	{
		Release();
		return 0;
	}

	const FString BackupDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/BlueprintDefaultsSetup") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	for (UObject* Blueprint : Dirty)
	{
		const FString Filename = FPackageName::LongPackageNameToFilename(Blueprint->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		if (IFileManager::Get().Copy(*(BackupDir / FPaths::GetCleanFilename(Filename)), *Filename) != COPY_OK)
		{
			UE_LOG(LogWuwaBlueprintDefaults, Error, TEXT("Failed to back up %s"), *Filename);
			Release();
			return 1;
		}
	}
	for (const FChange& Change : Changes)
	{
		if (!Dirty.Contains(Change.Blueprint)) continue;
		Change.Defaults->Modify();
		Change.Property->CopyCompleteValue(Change.Property->ContainerPtrToValuePtr<void>(Change.Defaults), Change.Value.GetData());
	}
	Release();
	for (UObject* Blueprint : Dirty)
	{
		UPackage* Package = Blueprint->GetOutermost();
		Package->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		Args.Error = GWarn;
		if (!UPackage::SavePackage(Package, Blueprint, *Filename, Args))
		{
			UE_LOG(LogWuwaBlueprintDefaults, Error, TEXT("Failed to save %s (is the editor open?)"), *Filename);
			++Errors;
			continue;
		}
		UE_LOG(LogWuwaBlueprintDefaults, Display, TEXT("Saved %s (backup in %s)"), *Package->GetName(), *BackupDir);
	}
	return Errors > 0 ? 1 : 0;
#else
	return 1;
#endif
}
