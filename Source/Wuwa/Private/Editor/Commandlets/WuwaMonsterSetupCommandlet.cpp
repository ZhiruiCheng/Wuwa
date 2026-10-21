#include "Editor/Commandlets/WuwaMonsterSetupCommandlet.h"

#if WITH_EDITOR
#include "Animation/AnimSequence.h"
#include "AttributeSet.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/DataTable.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "Game/NewWorld/Character/Common/WuwaCharactorBase.h"
#include "Game/NewWorld/Character/Monster/WuwaMonsterAnimInstance.h"
#include "Animation/AnimMontage.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "SkinnedAssetCompiler.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaMonsterSetup, Log, All);

UWuwaMonsterSetupCommandlet::UWuwaMonsterSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpUsage = TEXT("-run=WuwaMonsterSetup -Recipe=ArtSource/Boss/<monster>.monster.json [-Apply]");
}

int32 UWuwaMonsterSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	FString RecipeArg;
	if (!FParse::Value(*Params, TEXT("Recipe="), RecipeArg))
	{
		UE_LOG(LogWuwaMonsterSetup, Error, TEXT("Usage: %s"), *HelpUsage);
		return 1;
	}
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	FString Text;
	TSharedPtr<FJsonObject> Recipe;
	if (!FFileHelper::LoadFileToString(Text, *(FPaths::ProjectDir() / RecipeArg)) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Recipe) || !Recipe)
	{
		UE_LOG(LogWuwaMonsterSetup, Error, TEXT("Cannot read %s"), *RecipeArg);
		return 1;
	}
	const FString PackageName = Recipe->GetStringField(TEXT("blueprint"));
	const FString AssetName = FPackageName::GetShortName(PackageName);
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *Recipe->GetStringField(TEXT("mesh")));
	UAnimSequence* Idle = LoadObject<UAnimSequence>(nullptr, *Recipe->GetStringField(TEXT("idle")));
	UDataTable* BulletTable = LoadObject<UDataTable>(nullptr, *Recipe->GetStringField(TEXT("bulletTable")));
	if (!Mesh || !Idle || !BulletTable)
	{
		UE_LOG(LogWuwaMonsterSetup, Error, TEXT("Missing mesh / idle / bullet table (%s, %s, %s)."), *GetNameSafe(Mesh), *GetNameSafe(Idle), *GetNameSafe(BulletTable));
		return 1;
	}
	// 初始属性："类.属性"（如 WuwaAttributeSet.MaxHealth）→ 值；先全部解析，写错名字时不动资产
	TArray<FWuwaInitialAttribute> Attributes;
	const TSharedPtr<FJsonObject>* AttributesJson = nullptr;
	if (Recipe->TryGetObjectField(TEXT("attributes"), AttributesJson))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : (*AttributesJson)->Values)
		{
			FString ClassName, PropertyName;
			const UClass* SetClass = Entry.Key.Split(TEXT("."), &ClassName, &PropertyName)
				? FindObject<UClass>(nullptr, *(TEXT("/Script/Wuwa.") + ClassName)) : nullptr;
			FProperty* Property = SetClass && SetClass->IsChildOf<UAttributeSet>() ? FindFProperty<FProperty>(SetClass, *PropertyName) : nullptr;
			if (!Property || !FGameplayAttribute::IsGameplayAttributeDataProperty(Property))
			{
				UE_LOG(LogWuwaMonsterSetup, Error, TEXT("Unknown attribute '%s' (expected <AttributeSetClass>.<Attribute>)."), *Entry.Key);
				return 1;
			}
			FWuwaInitialAttribute& Attribute = Attributes.AddDefaulted_GetRef();
			Attribute.Attribute = FGameplayAttribute(Property);
			Attribute.Value = static_cast<float>(Entry.Value->AsNumber());
			UE_LOG(LogWuwaMonsterSetup, Display, TEXT("  attribute %s = %g"), *Entry.Key, Attribute.Value);
		}
	}

	FSkinnedAssetCompilingManager::Get().FinishAllCompilation();
	UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *(PackageName + TEXT(".") + AssetName), nullptr, LOAD_NoWarn | LOAD_Quiet);
	UE_LOG(LogWuwaMonsterSetup, Display, TEXT("%s: %s; camp %s, capsule %.0f/%.0f, mesh %s, idle %s, bullets %s"), *PackageName,
		Blueprint ? TEXT("update") : TEXT("create"), *Recipe->GetStringField(TEXT("camp")), Recipe->GetNumberField(TEXT("capsuleRadius")),
		Recipe->GetNumberField(TEXT("capsuleHalfHeight")), *Mesh->GetName(), *Idle->GetName(), *BulletTable->GetName());
	if (!bApply)
	{
		UE_LOG(LogWuwaMonsterSetup, Display, TEXT("Read-only; pass -Apply to write."));
		return 0;
	}

	const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
	if (Blueprint)
	{
		const FString Backup = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/MonsterSetup") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"))
			/ FPaths::GetCleanFilename(Filename));
		if (IFileManager::Get().Copy(*Backup, *Filename) != COPY_OK)
		{
			UE_LOG(LogWuwaMonsterSetup, Error, TEXT("Could not back up %s"), *Filename);
			return 1;
		}
	}
	else
	{
		UPackage* Package = CreatePackage(*PackageName);
		Blueprint = FKismetEditorUtilities::CreateBlueprint(AWuwaCharactorBase::StaticClass(), Package, *AssetName, BPTYPE_Normal,
			UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
		FAssetRegistryModule::AssetCreated(Blueprint);
	}
	auto Compile = [Blueprint]() -> bool
	{
		FCompilerResultsLog Results;
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Results);
		return Results.NumErrors == 0 && Blueprint->Status != BS_Error && Blueprint->GeneratedClass;
	};
	if (!Compile())
	{
		UE_LOG(LogWuwaMonsterSetup, Error, TEXT("%s failed to compile."), *AssetName);
		return 1;
	}
	AWuwaCharactorBase* Defaults = CastChecked<AWuwaCharactorBase>(Blueprint->GeneratedClass->GetDefaultObject());

	// 武器：构造脚本里的骨骼网格组件，挂在角色网格的插槽上（同名的先删掉，可以重复运行）
	USimpleConstructionScript* Construction = Blueprint->SimpleConstructionScript;
	for (const TSharedPtr<FJsonValue>& Value : Recipe->GetArrayField(TEXT("weapons")))
	{
		const TSharedPtr<FJsonObject> Weapon = Value->AsObject();
		const FName Name(*Weapon->GetStringField(TEXT("name")));
		USkeletalMesh* WeaponMesh = LoadObject<USkeletalMesh>(nullptr, *Weapon->GetStringField(TEXT("mesh")));
		if (!WeaponMesh)
		{
			UE_LOG(LogWuwaMonsterSetup, Error, TEXT("Weapon mesh %s not found."), *Weapon->GetStringField(TEXT("mesh")));
			return 1;
		}
		if (USCS_Node* Existing = Construction->FindSCSNode(Name)) Construction->RemoveNode(Existing);
		USCS_Node* Node = Construction->CreateNode(USkeletalMeshComponent::StaticClass(), Name);
		USkeletalMeshComponent* Template = CastChecked<USkeletalMeshComponent>(Node->ComponentTemplate);
		Template->SetSkeletalMesh(WeaponMesh);
		Template->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Node->SetParent(Defaults->GetMesh());
		Node->AttachToName = FName(*Weapon->GetStringField(TEXT("socket")));
		Construction->AddNode(Node);
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	if (!Compile())
	{
		UE_LOG(LogWuwaMonsterSetup, Error, TEXT("%s failed to compile after adding weapons."), *AssetName);
		return 1;
	}
	Defaults = CastChecked<AWuwaCharactorBase>(Blueprint->GeneratedClass->GetDefaultObject());

	// 类默认值
	Defaults->Modify();
	Defaults->Camp = FName(*Recipe->GetStringField(TEXT("camp")));
	Defaults->BulletTable = BulletTable;
	Defaults->InitialAttributes = Attributes;
	FString DisplayName;
	if (Recipe->TryGetStringField(TEXT("displayName"), DisplayName)) Defaults->DisplayName = FText::FromString(DisplayName);
	const TSharedPtr<FJsonObject>* BossView = nullptr;
	if (Recipe->TryGetObjectField(TEXT("bossViewConfig"), BossView))
	{
		Defaults->BossViewConfig.BossStateViewType = (*BossView)->GetIntegerField(TEXT("BossStateViewType"));
		Defaults->BossViewConfig.ShowDistance = (*BossView)->GetNumberField(TEXT("ShowDistance"));
		Defaults->BossViewConfig.bOnlyShowInBattleState = (*BossView)->GetBoolField(TEXT("bOnlyShowInBattleState"));
	}
	Defaults->AutoPossessAI = EAutoPossessAI::Disabled;   // 还没有 AI
	Defaults->GetCapsuleComponent()->SetCapsuleSize(Recipe->GetNumberField(TEXT("capsuleRadius")), Recipe->GetNumberField(TEXT("capsuleHalfHeight")));
	USkeletalMeshComponent* Body = Defaults->GetMesh();
	Body->SetSkeletalMesh(Mesh);
	const TArray<TSharedPtr<FJsonValue>>& Offset = Recipe->GetArrayField(TEXT("meshOffset"));
	Body->SetRelativeLocationAndRotation(FVector(Offset[0]->AsNumber(), Offset[1]->AsNumber(), Offset[2]->AsNumber()),
		FRotator(0.0, Recipe->GetNumberField(TEXT("meshYaw")), 0.0));
	// 临时动画实例：循环待机 + DefaultSlot 播蒙太奇（受击）。直接写属性：命令行里调 SetAnimationMode 会初始化动画实例
	if (FByteProperty* ModeProperty = FindFProperty<FByteProperty>(USkeletalMeshComponent::StaticClass(), TEXT("AnimationMode")))
	{
		ModeProperty->SetPropertyValue_InContainer(Body, static_cast<uint8>(EAnimationMode::AnimationBlueprint));
	}
	Body->AnimClass = UWuwaMonsterAnimInstance::StaticClass();
	Body->AnimationData.AnimToPlay = nullptr;
	Defaults->IdleAnimation = Idle;
	// 受击蒙太奇（原作 AI 状态机 TaskBeHitMontage 的 MontageMap）和受击效果表（原作 Data/DT_HitEffect）
	Defaults->BeHitMontages.Reset();
	const TSharedPtr<FJsonObject>* BeHit = nullptr;
	if (Recipe->TryGetObjectField(TEXT("beHitMontages"), BeHit))
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : (*BeHit)->Values)
		{
			UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *Entry.Value->AsString());
			if (!Montage)
			{
				UE_LOG(LogWuwaMonsterSetup, Error, TEXT("Hit montage %s not found."), *Entry.Value->AsString());
				return 1;
			}
			Defaults->BeHitMontages.Add(FCString::Atoi(*Entry.Key), Montage);
		}
	}
	FString HitEffectTable;
	if (Recipe->TryGetStringField(TEXT("hitEffectTable"), HitEffectTable))
	{
		Defaults->HitEffectTable = LoadObject<UDataTable>(nullptr, *HitEffectTable);
		if (!Defaults->HitEffectTable)
		{
			UE_LOG(LogWuwaMonsterSetup, Error, TEXT("Hit effect table %s not found."), *HitEffectTable);
			return 1;
		}
	}

	UPackage* Package = Blueprint->GetOutermost();
	Package->MarkPackageDirty();
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	Args.SaveFlags = SAVE_NoError;
	Args.Error = GWarn;
	if (!UPackage::SavePackage(Package, Blueprint, *Filename, Args))
	{
		UE_LOG(LogWuwaMonsterSetup, Error, TEXT("Could not save %s (is the editor open?)"), *Filename);
		return 1;
	}
	UE_LOG(LogWuwaMonsterSetup, Display, TEXT("Saved %s"), *PackageName);
	return 0;
#else
	return 1;
#endif
}
