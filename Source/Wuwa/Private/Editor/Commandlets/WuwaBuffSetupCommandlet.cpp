#include "Editor/Commandlets/WuwaBuffSetupCommandlet.h"
#include "Editor/WuwaAnimNotifyEditorUtils.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAttributeSet.h"

#if WITH_EDITOR
#include "Animation/AnimMontage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/DataTable.h"
#include "Engine/Texture2D.h"
#include "GameplayEffect.h"
#include "GameplayEffectComponents/AdditionalEffectsGameplayEffectComponent.h"
#include "GameplayEffectComponents/RemoveOtherGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"
#include "GameplayEffectComponents/TargetTagRequirementsGameplayEffectComponent.h"
#include "Game/Animation/Notifies/WuwaAnimNotify_AddBuff.h"
#include "Game/Animation/Notifies/WuwaAnimNotifyState_AddLooseTag.h"
#include "Game/NewWorld/Buff/WuwaAttributeEventComponent.h"
#include "Game/NewWorld/Buff/WuwaBuffTypes.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaBuffSetup, Log, All);

UWuwaBuffSetupCommandlet::UWuwaBuffSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpUsage = TEXT("-run=WuwaBuffSetup -Recipe=ArtSource/Combat/buffs.recipe.json [-Apply]");
}

#if WITH_EDITOR
namespace WuwaBuffSetup
{
	struct FContext
	{
		bool bApply = false;
		FString BackupDir;
		int32 Errors = 0;
	};

	FString ObjectPath(const FString& PackageName) { return PackageName + TEXT(".") + FPackageName::GetShortName(PackageName); }

	bool Backup(FContext& Ctx, const FString& PackageName)
	{
		const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		if (!IFileManager::Get().FileExists(*Filename)) return true;
		if (IFileManager::Get().Copy(*(Ctx.BackupDir / FPaths::GetCleanFilename(Filename)), *Filename) != COPY_OK)
		{
			UE_LOG(LogWuwaBuffSetup, Error, TEXT("Failed to back up %s"), *Filename);
			++Ctx.Errors;
			return false;
		}
		return true;
	}

	bool Save(FContext& Ctx, UObject* Asset)
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
			UE_LOG(LogWuwaBuffSetup, Error, TEXT("Failed to save %s (is the editor open?)"), *Filename);
			++Ctx.Errors;
			return false;
		}
		return true;
	}

	/** PNG → UI 贴图（不压缩、sRGB、有 mip：图标在界面上缩小显示），同 WuwaRoleUiSetup。 */
	UTexture2D* ImportIcon(FContext& Ctx, const FString& RelativeFile, const FString& Folder)
	{
		const FString File = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / RelativeFile);
		const FString PackageName = Folder / FPaths::GetBaseFilename(File);
		UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *ObjectPath(PackageName), nullptr, LOAD_NoWarn | LOAD_Quiet);
		IImageWrapperModule& Images = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
		TSharedPtr<IImageWrapper> Png = Images.CreateImageWrapper(EImageFormat::PNG);
		TArray<uint8> Compressed, Pixels;
		if (!FFileHelper::LoadFileToArray(Compressed, *File) || !Png->SetCompressed(Compressed.GetData(), Compressed.Num())
			|| !Png->GetRaw(ERGBFormat::BGRA, 8, Pixels))
		{
			UE_LOG(LogWuwaBuffSetup, Error, TEXT("Cannot read %s"), *File);
			++Ctx.Errors;
			return Texture;
		}
		UE_LOG(LogWuwaBuffSetup, Display, TEXT("  icon %s (%dx%d) -> %s"), *RelativeFile, Png->GetWidth(), Png->GetHeight(), *PackageName);
		if (!Ctx.bApply) return Texture;
		if (Texture)
		{
			Backup(Ctx, PackageName);
		}
		else
		{
			Texture = NewObject<UTexture2D>(CreatePackage(*PackageName), *FPaths::GetBaseFilename(File), RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Texture);
		}
		Texture->PreEditChange(nullptr);
		Texture->Source.Init(Png->GetWidth(), Png->GetHeight(), 1, 1, TSF_BGRA8, Pixels.GetData());
		Texture->CompressionSettings = TC_EditorIcon;
		Texture->MipGenSettings = TMGS_SimpleAverage;
		Texture->LODGroup = TEXTUREGROUP_UI;
		Texture->SRGB = true;
		Texture->NeverStream = true;
		if (!Texture->AssetImportData) Texture->AssetImportData = NewObject<UAssetImportData>(Texture);
		Texture->AssetImportData->Update(File);
		Texture->PostEditChange();
		Save(Ctx, Texture);
		return Texture;
	}

	/** 找到或新建 GE 蓝图（父类 UGameplayEffect），返回编译好的蓝图。 */
	UBlueprint* GetOrCreateEffectBlueprint(FContext& Ctx, const FString& PackageName)
	{
		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *ObjectPath(PackageName), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (Blueprint)
		{
			Backup(Ctx, PackageName);
			return Blueprint;
		}
		UPackage* Package = CreatePackage(*PackageName);
		Blueprint = FKismetEditorUtilities::CreateBlueprint(UGameplayEffect::StaticClass(), Package, *FPackageName::GetShortName(PackageName),
			BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
		FAssetRegistryModule::AssetCreated(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		return Blueprint;
	}

	/** GE 默认对象上的组件整体清空（重建）。GEComponents 是受保护的，用反射。 */
	void ClearComponents(UGameplayEffect* Effect)
	{
		if (const FArrayProperty* Array = FindFProperty<FArrayProperty>(UGameplayEffect::StaticClass(), TEXT("GEComponents")))
		{
			FScriptArrayHelper(Array, Array->ContainerPtrToValuePtr<void>(Effect)).EmptyValues();
		}
	}
}
#endif

int32 UWuwaBuffSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaBuffSetup;
	FString RecipeArg;
	if (!FParse::Value(*Params, TEXT("Recipe="), RecipeArg))
	{
		UE_LOG(LogWuwaBuffSetup, Error, TEXT("Usage: %s"), *HelpUsage);
		return 1;
	}
	FContext Ctx;
	Ctx.bApply = FParse::Param(*Params, TEXT("Apply"));
	Ctx.BackupDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/BuffSetup") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	const FString RecipePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / RecipeArg);
	FString RecipeText;
	TSharedPtr<FJsonObject> Recipe;
	if (!FFileHelper::LoadFileToString(RecipeText, *RecipePath)
		|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(RecipeText), Recipe) || !Recipe.IsValid())
	{
		UE_LOG(LogWuwaBuffSetup, Error, TEXT("Cannot read %s"), *RecipePath);
		return 1;
	}

	// 1. 图标 → 贴图，cue 表的行
	const FString IconDir = Recipe->GetStringField(TEXT("iconDir"));
	TMap<FName, FWuwaBuffCueRow> CueRows;
	for (const TSharedPtr<FJsonValue>& Value : Recipe->GetArrayField(TEXT("cues")))
	{
		const TSharedPtr<FJsonObject> Def = Value->AsObject();
		FWuwaBuffCueRow& Row = CueRows.Add(FName(*Def->GetStringField(TEXT("Name"))));
		Row.CueType = Def->GetIntegerField(TEXT("CueType"));
		Row.Priority = Def->GetIntegerField(TEXT("Priority"));
		for (const TSharedPtr<FJsonValue>& P : Def->GetArrayField(TEXT("Parameters"))) Row.Parameters.Add(P->AsString());
		Row.Icon = ImportIcon(Ctx, Def->GetStringField(TEXT("Icon")), IconDir);
	}

	// 2. GE：先全部建出来（"移除 buff"要引用别的 buff 的 GE 类），再填内容
	struct FBuffDef
	{
		int64 Id = 0;
		TSharedPtr<FJsonObject> Def;
		UBlueprint* Blueprint = nullptr;
	};
	TArray<FBuffDef> Buffs;
	TMap<int64, UClass*> EffectClasses;
	for (const TSharedPtr<FJsonValue>& Value : Recipe->GetArrayField(TEXT("buffs")))
	{
		FBuffDef& Buff = Buffs.AddDefaulted_GetRef();
		Buff.Def = Value->AsObject();
		Buff.Id = static_cast<int64>(Buff.Def->GetNumberField(TEXT("id")));
		const FString Path = Buff.Def->GetStringField(TEXT("path"));
		UE_LOG(LogWuwaBuffSetup, Display, TEXT("  buff %lld -> %s (%s %.2fs, tags %d, cues %d, removes %d)"), Buff.Id, *Path,
			*Buff.Def->GetStringField(TEXT("durationPolicy")), Buff.Def->GetNumberField(TEXT("duration")), Buff.Def->GetArrayField(TEXT("grantedTags")).Num(),
			Buff.Def->GetArrayField(TEXT("cueIds")).Num(), Buff.Def->GetArrayField(TEXT("removeBuffs")).Num());
		if (Buff.Def->GetArrayField(TEXT("addBuffs")).Num() > 0)
		{
			UE_LOG(LogWuwaBuffSetup, Error, TEXT("buff %lld: addBuffs is not supported yet."), Buff.Id);
			++Ctx.Errors;
		}
		if (!Ctx.bApply) continue;
		Buff.Blueprint = GetOrCreateEffectBlueprint(Ctx, Path);
		if (!Buff.Blueprint || !Buff.Blueprint->GeneratedClass)
		{
			UE_LOG(LogWuwaBuffSetup, Error, TEXT("Cannot create GE blueprint %s"), *Path);
			++Ctx.Errors;
			continue;
		}
		EffectClasses.Add(Buff.Id, Buff.Blueprint->GeneratedClass);
	}
	if (Ctx.Errors > 0) return 1;
	for (const FBuffDef& Buff : Buffs)
	{
		if (!Buff.Blueprint) continue;
		UGameplayEffect* Effect = CastChecked<UGameplayEffect>(Buff.Blueprint->GeneratedClass->GetDefaultObject());
		Effect->Modify();
		ClearComponents(Effect);
		const FString Policy = Buff.Def->GetStringField(TEXT("durationPolicy"));
		Effect->DurationPolicy = Policy == TEXT("Instant") ? EGameplayEffectDurationType::Instant
			: Policy == TEXT("Infinite") ? EGameplayEffectDurationType::Infinite : EGameplayEffectDurationType::HasDuration;
		Effect->DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(static_cast<float>(Buff.Def->GetNumberField(TEXT("duration")))));
		// 授予 Tag
		FInheritedTagContainer Tags;
		for (const TSharedPtr<FJsonValue>& Tag : Buff.Def->GetArrayField(TEXT("grantedTags")))
		{
			const FGameplayTag GameplayTag = FGameplayTag::RequestGameplayTag(FName(*Tag->AsString()), false);
			if (!GameplayTag.IsValid())
			{
				UE_LOG(LogWuwaBuffSetup, Error, TEXT("buff %lld: tag %s is not in DefaultGameplayTags.ini"), Buff.Id, *Tag->AsString());
				++Ctx.Errors;
				continue;
			}
			Tags.Added.AddTag(GameplayTag);
		}
		if (!Tags.Added.IsEmpty()) Effect->AddComponent<UTargetTagsGameplayEffectComponent>().SetAndApplyTargetTagChanges(Tags);
		// 原作瞬时 buff 改属性（AddBaseValue）：加 数值，或 数值/10000 × 另一个属性（持有者 / 施加者的当前值）
		Effect->Modifiers.Reset();
		const TArray<TSharedPtr<FJsonValue>>* ModifierDefs = nullptr;
		if (Buff.Def->TryGetArrayField(TEXT("modifiers"), ModifierDefs))
		{
			for (const TSharedPtr<FJsonValue>& Value : *ModifierDefs)
			{
				const TSharedPtr<FJsonObject> Mod = Value->AsObject();
				auto FindAttribute = [](const FString& Name)
				{
					FProperty* Property = FindFProperty<FProperty>(UWuwaAttributeSet::StaticClass(), *Name);
					return Property ? FGameplayAttribute(Property) : FGameplayAttribute();
				};
				FGameplayModifierInfo Info;
				Info.Attribute = FindAttribute(Mod->GetStringField(TEXT("attribute")));
				Info.ModifierOp = EGameplayModOp::Additive;
				FString BasedOn;
				if (Mod->TryGetStringField(TEXT("basedOn"), BasedOn))
				{
					FAttributeBasedFloat Based;
					Based.Coefficient = FScalableFloat(static_cast<float>(Mod->GetNumberField(TEXT("coefficient"))));
					Based.BackingAttribute = FGameplayEffectAttributeCaptureDefinition(FindAttribute(BasedOn),
						Mod->GetBoolField(TEXT("basedOnInstigator")) ? EGameplayEffectAttributeCaptureSource::Source : EGameplayEffectAttributeCaptureSource::Target, false);
					// 原作取值方式：0 基础值 / 1 当前值
					bool bBaseValue = false;
					Mod->TryGetBoolField(TEXT("basedOnBaseValue"), bBaseValue);
					Based.AttributeCalculationType = bBaseValue ? EAttributeBasedFloatCalculationType::AttributeBaseValue : EAttributeBasedFloatCalculationType::AttributeMagnitude;
					if (!Based.BackingAttribute.AttributeToCapture.IsValid())
					{
						UE_LOG(LogWuwaBuffSetup, Error, TEXT("buff %lld: attribute %s not in UWuwaAttributeSet"), Buff.Id, *BasedOn);
						++Ctx.Errors;
						continue;
					}
					Info.ModifierMagnitude = FGameplayEffectModifierMagnitude(Based);
				}
				else
				{
					Info.ModifierMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(static_cast<float>(Mod->GetNumberField(TEXT("magnitude")))));
				}
				if (!Info.Attribute.IsValid())
				{
					UE_LOG(LogWuwaBuffSetup, Error, TEXT("buff %lld: attribute %s not in UWuwaAttributeSet"), Buff.Id, *Mod->GetStringField(TEXT("attribute")));
					++Ctx.Errors;
					continue;
				}
				Effect->Modifiers.Add(Info);
			}
		}
		// 原作 RemovalTagIgnores：身上这些 Tag 一个都没有了就移除；RemovalTagRequirements：这些 Tag 全有了就移除
		// （= GAS RemovalTagRequirements 的 IgnoreTags / RequireTags，满足即移除）
		const TArray<TSharedPtr<FJsonValue>>* RemovalIgnores = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* RemovalRequires = nullptr;
		const bool bRemovalIgnores = Buff.Def->TryGetArrayField(TEXT("removalIgnoreTags"), RemovalIgnores) && RemovalIgnores->Num() > 0;
		const bool bRemovalRequires = Buff.Def->TryGetArrayField(TEXT("removalRequireTags"), RemovalRequires) && RemovalRequires->Num() > 0;
		// 原作 ApplicationTagIgnores / ApplicationTagRequirements：持有者有这些 Tag 不加 / 要有这些 Tag 才加 = ApplicationTagRequirements
		const TArray<TSharedPtr<FJsonValue>>* ApplicationIgnores = nullptr;
		const TArray<TSharedPtr<FJsonValue>>* ApplicationRequires = nullptr;
		const bool bApplicationIgnores = Buff.Def->TryGetArrayField(TEXT("applicationIgnoreTags"), ApplicationIgnores) && ApplicationIgnores->Num() > 0;
		const bool bApplicationRequires = Buff.Def->TryGetArrayField(TEXT("applicationRequireTags"), ApplicationRequires) && ApplicationRequires->Num() > 0;
		if (bRemovalIgnores || bRemovalRequires || bApplicationIgnores || bApplicationRequires)
		{
			UTargetTagRequirementsGameplayEffectComponent& Requirements = Effect->AddComponent<UTargetTagRequirementsGameplayEffectComponent>();
			auto AddTags = [&](const TArray<TSharedPtr<FJsonValue>>* Values, FGameplayTagContainer& Out)
			{
				if (!Values) return;
				for (const TSharedPtr<FJsonValue>& Tag : *Values)
				{
					const FGameplayTag GameplayTag = FGameplayTag::RequestGameplayTag(FName(*Tag->AsString()), false);
					if (!GameplayTag.IsValid())
					{
						UE_LOG(LogWuwaBuffSetup, Error, TEXT("buff %lld: removal tag %s is not registered"), Buff.Id, *Tag->AsString());
						++Ctx.Errors;
						continue;
					}
					Out.AddTag(GameplayTag);
				}
			};
			if (bRemovalIgnores) AddTags(RemovalIgnores, Requirements.RemovalTagRequirements.IgnoreTags);
			if (bRemovalRequires) AddTags(RemovalRequires, Requirements.RemovalTagRequirements.RequireTags);
			if (bApplicationIgnores) AddTags(ApplicationIgnores, Requirements.ApplicationTagRequirements.IgnoreTags);
			if (bApplicationRequires) AddTags(ApplicationRequires, Requirements.ApplicationTagRequirements.RequireTags);
		}
		// buff 信息（buff ID、cue）
		UWuwaBuffInfoComponent& Info = Effect->AddComponent<UWuwaBuffInfoComponent>();
		Info.BuffId = Buff.Id;
		for (const TSharedPtr<FJsonValue>& Cue : Buff.Def->GetArrayField(TEXT("cueIds"))) Info.CueIds.Add(static_cast<int64>(Cue->AsNumber()));
		// 原作额外效果"移除 buff"（ExecuteBulletOrBuff 目标 2）
		// 和原作 RemoveBuffWithTags（移除授予这些 Tag 的 buff：按 GE 授予的 Tag 查，OwningTagQuery 同时比对资产 Tag 和授予 Tag）
		const TArray<TSharedPtr<FJsonValue>>& Removes = Buff.Def->GetArrayField(TEXT("removeBuffs"));
		const TArray<TSharedPtr<FJsonValue>>* RemoveTags = nullptr;
		FGameplayTagContainer RemoveTagContainer;
		if (Buff.Def->TryGetArrayField(TEXT("removeBuffsWithTags"), RemoveTags))
		{
			for (const TSharedPtr<FJsonValue>& Tag : *RemoveTags)
			{
				const FGameplayTag GameplayTag = FGameplayTag::RequestGameplayTag(FName(*Tag->AsString()), false);
				if (!GameplayTag.IsValid())
				{
					UE_LOG(LogWuwaBuffSetup, Error, TEXT("buff %lld: remove-with tag %s is not registered"), Buff.Id, *Tag->AsString());
					++Ctx.Errors;
					continue;
				}
				RemoveTagContainer.AddTag(GameplayTag);
			}
		}
		if (Removes.Num() > 0 || !RemoveTagContainer.IsEmpty())
		{
			URemoveOtherGameplayEffectComponent& Remove = Effect->AddComponent<URemoveOtherGameplayEffectComponent>();
			if (!RemoveTagContainer.IsEmpty())
			{
				Remove.RemoveGameplayEffectQueries.Add(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(RemoveTagContainer));
			}
			for (const TSharedPtr<FJsonValue>& Id : Removes)
			{
				UClass* Target = EffectClasses.FindRef(static_cast<int64>(Id->AsNumber()));
				if (!Target)
				{
					UE_LOG(LogWuwaBuffSetup, Error, TEXT("buff %lld removes buff %lld, which is not in the recipe."), Buff.Id, static_cast<int64>(Id->AsNumber()));
					++Ctx.Errors;
					continue;
				}
				FGameplayEffectQuery& Query = Remove.RemoveGameplayEffectQueries.AddDefaulted_GetRef();
				Query.EffectDefinition = Target;
			}
		}
		// 原作到期效果：正常到期（RoutineExpirationEffects）/ 提前移除（PrematureExpirationEffects）时施加别的 buff
		// （例：嘉贝连段 0.1 秒 buff 到期 → 0.6 秒连段窗口 buff）= GE 的 AdditionalEffects 组件 OnCompleteNormal / OnCompletePrematurely
		const TSharedPtr<FJsonObject>* Expire = nullptr;
		if (Buff.Def->TryGetObjectField(TEXT("expireBuffs"), Expire))
		{
			TArray<TSubclassOf<UGameplayEffect>> Normal, Premature;
			auto Collect = [&](const TCHAR* Field, TArray<TSubclassOf<UGameplayEffect>>& Out)
			{
				for (const TSharedPtr<FJsonValue>& Id : (*Expire)->GetArrayField(Field))
				{
					UClass* Target = EffectClasses.FindRef(static_cast<int64>(Id->AsNumber()));
					if (!Target)
					{
						UE_LOG(LogWuwaBuffSetup, Error, TEXT("buff %lld adds buff %lld when it expires, which is not in the recipe."), Buff.Id, static_cast<int64>(Id->AsNumber()));
						++Ctx.Errors;
						continue;
					}
					Out.Add(Target);
				}
			};
			Collect(TEXT("normal"), Normal);
			Collect(TEXT("premature"), Premature);
			if (Effect->DurationPolicy == EGameplayEffectDurationType::Instant)
			{
				// 原作瞬时 buff 执行完马上算"到期"（例：嘉贝 1208003313 弹药 = 能量，之后加 1208003315 能量清空）：
				// 瞬时 GE 没有到期，改成 OnApplication（引擎先执行属性修改、再 OnApplied，顺序和原作一致）
				TSet<UClass*> Seen;
				TArray<FConditionalGameplayEffect> OnApplication;
				for (const TSubclassOf<UGameplayEffect>& Target : Normal)
				{
					if (Seen.Contains(Target.Get())) continue;
					Seen.Add(Target.Get());
					OnApplication.AddDefaulted_GetRef().EffectClass = Target;
				}
				if (OnApplication.Num() > 0)
				{
					Effect->AddComponent<UAdditionalEffectsGameplayEffectComponent>().OnApplicationGameplayEffects = OnApplication;
				}
			}
			else if (Normal.Num() > 0 || Premature.Num() > 0)
			{
				UAdditionalEffectsGameplayEffectComponent& Additional = Effect->AddComponent<UAdditionalEffectsGameplayEffectComponent>();
				Additional.OnCompleteNormal = Normal;
				Additional.OnCompletePrematurely = Premature;
			}
		}
		// 原作额外效果 6（属性进入区间就加 buff）
		const TSharedPtr<FJsonObject>* AttributeEvent = nullptr;
		if (Buff.Def->TryGetObjectField(TEXT("attributeEvent"), AttributeEvent))
		{
			auto FindAttribute = [](const FString& Name)
			{
				FProperty* Property = Name.IsEmpty() ? nullptr : FindFProperty<FProperty>(UWuwaAttributeSet::StaticClass(), *Name);
				return Property ? FGameplayAttribute(Property) : FGameplayAttribute();
			};
			UWuwaAttributeEventComponent& Event = Effect->AddComponent<UWuwaAttributeEventComponent>();
			Event.Attribute = FindAttribute((*AttributeEvent)->GetStringField(TEXT("attribute")));
			Event.bPerTenThousand = (*AttributeEvent)->GetBoolField(TEXT("perTenThousand"));
			Event.MaxAttribute = FindAttribute((*AttributeEvent)->GetStringField(TEXT("maxAttribute")));
			Event.LowerBound = static_cast<float>((*AttributeEvent)->GetNumberField(TEXT("lower")));
			Event.UpperBound = static_cast<float>((*AttributeEvent)->GetNumberField(TEXT("upper")));
			Event.bRemoveOnLeave = (*AttributeEvent)->GetBoolField(TEXT("removeOnLeave"));
			if (!Event.Attribute.IsValid() || (Event.bPerTenThousand && !Event.MaxAttribute.IsValid()))
			{
				UE_LOG(LogWuwaBuffSetup, Error, TEXT("buff %lld: attribute event attribute not in UWuwaAttributeSet"), Buff.Id);
				++Ctx.Errors;
			}
			for (const TSharedPtr<FJsonValue>& Id : (*AttributeEvent)->GetArrayField(TEXT("addBuffs")))
			{
				UClass* Target = EffectClasses.FindRef(static_cast<int64>(Id->AsNumber()));
				if (!Target)
				{
					UE_LOG(LogWuwaBuffSetup, Error, TEXT("buff %lld: attribute event adds buff %lld, which is not in the recipe."), Buff.Id, static_cast<int64>(Id->AsNumber()));
					++Ctx.Errors;
					continue;
				}
				Event.EffectsOnEnter.Add(Target);
			}
		}
		Effect->PostEditChange();
		FKismetEditorUtilities::CompileBlueprint(Buff.Blueprint);
		Save(Ctx, Buff.Blueprint);
	}

	// 3. 两张表整表重建
	auto RebuildTable = [&Ctx](const FString& PackageName, UScriptStruct* RowStruct, TFunctionRef<void(UDataTable*)> Fill)
	{
		UDataTable* Table = LoadObject<UDataTable>(nullptr, *ObjectPath(PackageName), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (Table)
		{
			Backup(Ctx, PackageName);
			Table->Modify();
			Table->EmptyTable();
		}
		else
		{
			Table = NewObject<UDataTable>(CreatePackage(*PackageName), *FPackageName::GetShortName(PackageName), RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Table);
		}
		Table->RowStruct = RowStruct;
		Fill(Table);
		Table->HandleDataTableChanged();
		Save(Ctx, Table);
	};
	if (Ctx.bApply)
	{
		RebuildTable(Recipe->GetStringField(TEXT("buffTable")), FWuwaBuffRow::StaticStruct(), [&EffectClasses](UDataTable* Table)
		{
			for (const TPair<int64, UClass*>& Pair : EffectClasses)
			{
				FWuwaBuffRow Row;
				Row.Effect = Pair.Value;
				Table->AddRow(FName(*LexToString(Pair.Key)), Row);
			}
		});
		RebuildTable(Recipe->GetStringField(TEXT("cueTable")), FWuwaBuffCueRow::StaticStruct(), [&CueRows](UDataTable* Table)
		{
			for (const TPair<FName, FWuwaBuffCueRow>& Pair : CueRows) Table->AddRow(Pair.Key, Pair.Value);
		});
	}

	// 4. 动画通知：按蒙太奇分组，整体替换 Add Buff 通知；原作 TsAnimNotifyStateAddTag 窗口（配方 tag + duration）装成 Add Loose Tag Window，
	//    只替换配方里出现过的那些 Tag 的窗口（连段窗口等别的流水线装的不动）
	TMap<FString, TArray<TSharedPtr<FJsonObject>>> ByMontage;
	FGameplayTagContainer ManagedWindowTags;
	for (const TSharedPtr<FJsonValue>& Value : Recipe->GetArrayField(TEXT("notifies")))
	{
		ByMontage.FindOrAdd(Value->AsObject()->GetStringField(TEXT("montage"))).Add(Value->AsObject());
		FString TagName;
		if (Value->AsObject()->TryGetStringField(TEXT("tag"), TagName))
		{
			const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
			if (!Tag.IsValid())
			{
				UE_LOG(LogWuwaBuffSetup, Error, TEXT("Tag window %s is not a registered tag."), *TagName);
				++Ctx.Errors;
				continue;
			}
			ManagedWindowTags.AddTag(Tag);
		}
	}
	static const FName TrackName(TEXT("Buff"));
	for (const TPair<FString, TArray<TSharedPtr<FJsonObject>>>& Pair : ByMontage)
	{
		UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *ObjectPath(Pair.Key));
		if (!Montage)
		{
			UE_LOG(LogWuwaBuffSetup, Error, TEXT("Montage %s not found."), *Pair.Key);
			++Ctx.Errors;
			continue;
		}
		for (const TSharedPtr<FJsonObject>& Entry : Pair.Value)
		{
			const double SourceLength = Entry->GetNumberField(TEXT("sourceLength"));
			const double Ratio = SourceLength > 0.0 ? Montage->GetPlayLength() / SourceLength : 1.0;
			FString TagName;
			if (Entry->TryGetStringField(TEXT("tag"), TagName))
			{
				UE_LOG(LogWuwaBuffSetup, Display, TEXT("  window %s @ %.3fs +%.3fs (original %.3fs, length %.3f / %.3f) tag %s"), *Montage->GetName(),
					Entry->GetNumberField(TEXT("time")) * Ratio, Entry->GetNumberField(TEXT("duration")) * Ratio, Entry->GetNumberField(TEXT("time")),
					Montage->GetPlayLength(), SourceLength, *TagName);
			}
			else
			{
				UE_LOG(LogWuwaBuffSetup, Display, TEXT("  notify %s @ %.3fs (original %.3fs, length %.3f / %.3f) add buff %lld"), *Montage->GetName(),
					Entry->GetNumberField(TEXT("time")) * Ratio, Entry->GetNumberField(TEXT("time")), Montage->GetPlayLength(), SourceLength,
					static_cast<int64>(Entry->GetNumberField(TEXT("buffId"))));
			}
			if (FMath::Abs(Ratio - 1.0) > 0.01)
			{
				UE_LOG(LogWuwaBuffSetup, Warning, TEXT("  %s length differs from the original by %.1f%%; time scaled."), *Montage->GetName(), (Ratio - 1.0) * 100.0);
			}
		}
		if (!Ctx.bApply) continue;
		if (!Backup(Ctx, Montage->GetOutermost()->GetName())) continue;
		Montage->Modify();
		Montage->Notifies.RemoveAll([&ManagedWindowTags](const FAnimNotifyEvent& Event)
		{
			if (Event.Notify && Event.Notify->IsA<UWuwaAnimNotify_AddBuff>()) return true;
			const UWuwaAnimNotifyState_AddLooseTag* Window = Cast<UWuwaAnimNotifyState_AddLooseTag>(Event.NotifyStateClass);
			return Window && ManagedWindowTags.HasTagExact(Window->Tag);
		});
		int32 Track = Montage->AnimNotifyTracks.IndexOfByPredicate([](const FAnimNotifyTrack& T) { return T.TrackName == TrackName; });
		if (Track == INDEX_NONE)
		{
			Track = Montage->AnimNotifyTracks.AddDefaulted();
			Montage->AnimNotifyTracks[Track].TrackName = TrackName;
			Montage->AnimNotifyTracks[Track].TrackColor = FLinearColor(1.f, .55f, .1f);
		}
		for (const TSharedPtr<FJsonObject>& Entry : Pair.Value)
		{
			const double SourceLength = Entry->GetNumberField(TEXT("sourceLength"));
			const double Ratio = SourceLength > 0.0 ? Montage->GetPlayLength() / SourceLength : 1.0;
			FString TagName;
			if (Entry->TryGetStringField(TEXT("tag"), TagName))
			{
				UWuwaAnimNotifyState_AddLooseTag* Window = NewObject<UWuwaAnimNotifyState_AddLooseTag>(Montage, NAME_None, RF_Transactional);
				Window->Tag = FGameplayTag::RequestGameplayTag(FName(*TagName), false);
				const float Start = static_cast<float>(Entry->GetNumberField(TEXT("time")) * Ratio);
				const float Duration = FMath::Min(static_cast<float>(Entry->GetNumberField(TEXT("duration")) * Ratio), Montage->GetPlayLength() - Start);
				FAnimNotifyEvent& Notify = Montage->Notifies.AddDefaulted_GetRef();
				Notify.TrackIndex = Track;
				Notify.NotifyTriggerChance = 1.f;
				Notify.Guid = FGuid::NewGuid();
				Notify.NotifyName = FName(*Window->GetNotifyName());
				Notify.NotifyStateClass = Window;
				// 和连段窗口一样用 BranchingPoint：开始 / 结束不会因为帧率跳过
				Notify.MontageTickType = EMontageNotifyTickType::BranchingPoint;
				Notify.Link(Montage, Start);
				Notify.SetDuration(Duration);
				Notify.EndLink.Link(Montage, Start + Duration);
				continue;
			}
			UWuwaAnimNotify_AddBuff* AddBuff = NewObject<UWuwaAnimNotify_AddBuff>(Montage, NAME_None, RF_Transactional);
			AddBuff->BuffId = static_cast<int64>(Entry->GetNumberField(TEXT("buffId")));
			FAnimNotifyEvent& Notify = Montage->Notifies.AddDefaulted_GetRef();
			Notify.TrackIndex = Track;
			Notify.NotifyTriggerChance = 1.f;
			Notify.TriggerTimeOffset = 0.f;
			Notify.Guid = FGuid::NewGuid();
			Notify.NotifyName = FName(*AddBuff->GetNotifyName());
			Notify.Notify = AddBuff;
			Notify.Link(Montage, static_cast<float>(Entry->GetNumberField(TEXT("time")) * Ratio));
		}
		Montage->SortNotifies();
		WuwaAnimNotifyEditor::RefreshTriggerOffsets(Montage);
		Montage->RefreshCacheData();
		Save(Ctx, Montage);
	}

	UE_LOG(LogWuwaBuffSetup, Display, TEXT("%d buffs, %d UI cues, %d montages (%s), %d errors%s"), Buffs.Num(), CueRows.Num(), ByMontage.Num(),
		*Recipe->GetStringField(TEXT("source")), Ctx.Errors, Ctx.bApply ? *FString::Printf(TEXT(" (backups in %s)"), *Ctx.BackupDir) : TEXT(" — read-only, pass -Apply to write"));
	return Ctx.Errors > 0 ? 1 : 0;
#else
	return 1;
#endif
}
