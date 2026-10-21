#include "Commandlets/WuwaRoleSkillSetupCommandlet.h"
#include "Editor/WuwaAnimNotifyEditorUtils.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/DataTable.h"
#include "FileHelpers.h"
#include "Game/Animation/Notifies/WuwaAnimNotify_ChangeSkillPriority.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaSkillMontageAbility.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillTypes.h"
#include "GameplayTagsManager.h"
#include "HAL/FileManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modifiers/WuwaPromoteRootMotionModifier.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaRoleSkillSetup, Log, All);

UWuwaRoleSkillSetupCommandlet::UWuwaRoleSkillSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpUsage = TEXT("-run=WuwaRoleSkillSetup -Recipe=ArtSource/Combat/<role>.skills.json [-RebuildMontages] [-Apply]");
}

namespace WuwaRoleSkillSetup
{
	const FName FlowTrack(TEXT("SkillFlow"));

	FString AssetPath(const FString& PackagePath)
	{
		return PackagePath + TEXT(".") + FPackageName::GetShortName(PackagePath);
	}

	struct FContext
	{
		bool bApply = false;
		FString BackupRoot;
		TSet<FString> BackedUp;
		TArray<UPackage*> ToSave;
		int32 Problems = 0;

		void Backup(const FString& PackagePath)
		{
			FString File;
			if (BackedUp.Contains(PackagePath) || !FPackageName::DoesPackageExist(PackagePath, &File)) return;
			BackedUp.Add(PackagePath);
			IFileManager::Get().Copy(*(BackupRoot / PackagePath.RightChop(6) + FPackageName::GetAssetPackageExtension()), *File);
		}
		void Save(UObject* Object)
		{
			Object->MarkPackageDirty();
			ToSave.AddUnique(Object->GetOutermost());
		}
	};

	/** C# 写的通知类（UnrealSharp 在编辑器里是 CSBlueprint，取它的生成类）。 */
	UClass* ManagedClass(const TCHAR* Name)
	{
		const FString Path = FString::Printf(TEXT("/Script/ManagedWuwa.%s"), Name);
		UBlueprint* Blueprint = FindObject<UBlueprint>(nullptr, *Path);
		if (!Blueprint) Blueprint = LoadObject<UBlueprint>(nullptr, *Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
		return Blueprint ? Blueprint->GeneratedClass.Get() : nullptr;
	}

	int32 FindOrAddTrack(UAnimMontage* Montage, FName TrackName)
	{
		int32 Track = Montage->AnimNotifyTracks.IndexOfByPredicate([TrackName](const FAnimNotifyTrack& T) { return T.TrackName == TrackName; });
		if (Track == INDEX_NONE)
		{
			Track = Montage->AnimNotifyTracks.AddDefaulted();
			Montage->AnimNotifyTracks[Track].TrackName = TrackName;
			Montage->AnimNotifyTracks[Track].TrackColor = FLinearColor(.3f, .8f, 1.f);
		}
		return Track;
	}

	FAnimNotifyEvent& AddEvent(UAnimMontage* Montage, int32 Track, float Time)
	{
		FAnimNotifyEvent& Event = Montage->Notifies.AddDefaulted_GetRef();
		Event.TrackIndex = Track;
		Event.NotifyTriggerChance = 1.f;
		Event.TriggerTimeOffset = 0.f;
		Event.Guid = FGuid::NewGuid();
		Event.Link(Montage, FMath::Clamp(Time, 0.f, Montage->GetPlayLength()));
		return Event;
	}

	bool AddNotify(UAnimMontage* Montage, int32 Track, float Time, UClass* Class)
	{
		if (!Class) return false;
		UAnimNotify* Notify = NewObject<UAnimNotify>(Montage, Class, NAME_None, RF_Transactional);
		FAnimNotifyEvent& Event = AddEvent(Montage, Track, Time);
		Event.Notify = Notify;
		Event.NotifyName = FName(*Notify->GetNotifyName());
		return true;
	}

	bool AddState(UAnimMontage* Montage, int32 Track, float Time, float Duration, UClass* Class)
	{
		if (!Class) return false;
		UAnimNotifyState* State = NewObject<UAnimNotifyState>(Montage, Class, NAME_None, RF_Transactional);
		FAnimNotifyEvent& Event = AddEvent(Montage, Track, Time);
		const float End = FMath::Clamp(Time + Duration, Event.GetTime(), Montage->GetPlayLength());
		Event.NotifyStateClass = State;
		Event.NotifyName = FName(*State->GetNotifyName());
		Event.SetDuration(End - Event.GetTime());
		Event.EndLink.Link(Montage, End);
		return true;
	}

	/** 原作流程通知 → 项目通知；时间按项目 / 原作长度比例换算（项目动画重导时帧率可能不同）。 */
	bool AddFlowNotifies(UAnimMontage* Montage, const TArray<TSharedPtr<FJsonValue>>& Flow, double SourceLength)
	{
		UClass* ReadyEnd = ManagedClass(TEXT("AnimNotify_SkillReadyEnd"));
		UClass* AcceptInput = ManagedClass(TEXT("AnimNotifyState_SkillAcceptInput"));
		UClass* MovementCancel = ManagedClass(TEXT("AnimNotifyState_MovementCancelWindow"));
		if (!ReadyEnd || !AcceptInput || !MovementCancel)
		{
			UE_LOG(LogWuwaRoleSkillSetup, Error, TEXT("C# notify classes not loaded (publish ManagedWuwa first)."));
			return false;
		}
		const double Ratio = SourceLength > 0.0 ? Montage->GetPlayLength() / SourceLength : 1.0;
		const int32 Track = FindOrAddTrack(Montage, FlowTrack);
		bool bOk = true;
		for (const TSharedPtr<FJsonValue>& Value : Flow)
		{
			const TSharedPtr<FJsonObject> Def = Value->AsObject();
			const FString Kind = Def->GetStringField(TEXT("kind"));
			const float Time = static_cast<float>(Def->GetNumberField(TEXT("time")) * Ratio);
			const float Duration = Def->HasField(TEXT("duration")) ? static_cast<float>(Def->GetNumberField(TEXT("duration")) * Ratio) : 0.f;
			if (Kind == TEXT("ReadyEnd")) bOk &= AddNotify(Montage, Track, Time, ReadyEnd);
			else if (Kind == TEXT("AcceptInput")) bOk &= AddState(Montage, Track, Time, Duration, AcceptInput);
			else if (Kind == TEXT("MovementCancel")) bOk &= AddState(Montage, Track, Time, Duration, MovementCancel);
			else if (Kind == TEXT("Priority"))
			{
				UWuwaAnimNotify_ChangeSkillPriority* Notify = NewObject<UWuwaAnimNotify_ChangeSkillPriority>(Montage, NAME_None, RF_Transactional);
				Notify->Priority = static_cast<int32>(Def->GetNumberField(TEXT("value")));
				FAnimNotifyEvent& Event = AddEvent(Montage, Track, Time);
				Event.Notify = Notify;
				Event.NotifyName = FName(*Notify->GetNotifyName());
			}
			else
			{
				UE_LOG(LogWuwaRoleSkillSetup, Error, TEXT("  unknown flow notify kind %s"), *Kind);
				bOk = false;
			}
		}
		Montage->SortNotifies();
		WuwaAnimNotifyEditor::RefreshTriggerOffsets(Montage);
		Montage->RefreshCacheData();
		return bOk;
	}

	UAnimMontage* BuildMontage(FContext& Ctx, const FString& Path, const TSharedPtr<FJsonObject>& Def, bool bRebuild)
	{
		UAnimMontage* Existing = LoadObject<UAnimMontage>(nullptr, *AssetPath(Path), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (Existing && !bRebuild)
		{
			UE_LOG(LogWuwaRoleSkillSetup, Display, TEXT("Montage %s exists; kept (pass -RebuildMontages to rebuild)"), *Existing->GetName());
			return Existing;
		}
		TArray<FSlotAnimationTrack> Tracks;
		USkeleton* Skeleton = nullptr;
		for (const TSharedPtr<FJsonValue>& TrackValue : Def->GetArrayField(TEXT("tracks")))
		{
			const TSharedPtr<FJsonObject> TrackDef = TrackValue->AsObject();
			FSlotAnimationTrack& Track = Tracks.AddDefaulted_GetRef();
			Track.SlotName = *TrackDef->GetStringField(TEXT("slot"));
			for (const TSharedPtr<FJsonValue>& SegmentValue : TrackDef->GetArrayField(TEXT("segments")))
			{
				const TSharedPtr<FJsonObject> SegmentDef = SegmentValue->AsObject();
				UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr, *AssetPath(SegmentDef->GetStringField(TEXT("anim"))));
				if (!Sequence)
				{
					UE_LOG(LogWuwaRoleSkillSetup, Error, TEXT("Montage %s: missing anim %s"), *Path, *SegmentDef->GetStringField(TEXT("anim")));
					return nullptr;
				}
				Skeleton = Skeleton ? Skeleton : Sequence->GetSkeleton();
				FAnimSegment Segment;
				Segment.SetAnimReference(Sequence);
				Segment.AnimStartTime = SegmentDef->GetNumberField(TEXT("start"));
				Segment.AnimEndTime = FMath::Min<float>(SegmentDef->GetNumberField(TEXT("end")), Sequence->GetPlayLength());
				Segment.AnimPlayRate = SegmentDef->GetNumberField(TEXT("rate"));
				Segment.StartPos = SegmentDef->GetNumberField(TEXT("startPos"));
				Segment.LoopingCount = 1;
				Track.AnimTrack.AnimSegments.Add(Segment);
			}
		}
		if (!Skeleton) return nullptr;
		UE_LOG(LogWuwaRoleSkillSetup, Display, TEXT("Montage %s: %s, %d flow notifies"), *FPackageName::GetShortName(Path),
			*FString::JoinBy(Tracks[0].AnimTrack.AnimSegments, TEXT(" + "), [](const FAnimSegment& S)
			{
				return FString::Printf(TEXT("%s[%.3f~%.3f x%.2f]"), *S.GetAnimReference()->GetName(), S.AnimStartTime, S.AnimEndTime, S.AnimPlayRate);
			}), Def->GetArrayField(TEXT("flowNotifies")).Num());
		if (!Ctx.bApply) return Existing;
		Ctx.Backup(Path);
		UAnimMontage* Montage = Existing;
		if (!Montage)
		{
			UPackage* Package = CreatePackage(*Path);
			Package->FullyLoad();
			Montage = NewObject<UAnimMontage>(Package, *FPackageName::GetShortName(Path), RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Montage);
		}
		Montage->PreEditChange(nullptr);
		Montage->SetSkeleton(Skeleton);
		Montage->SlotAnimTracks = Tracks;
		Montage->CompositeSections.Reset();
		for (const TSharedPtr<FJsonValue>& SectionValue : Def->GetArrayField(TEXT("sections")))
		{
			const TSharedPtr<FJsonObject> SectionDef = SectionValue->AsObject();
			FCompositeSection Section;
			Section.SectionName = *SectionDef->GetStringField(TEXT("name"));
			Section.NextSectionName = *SectionDef->GetStringField(TEXT("next"));
			Section.Link(Montage, SectionDef->GetNumberField(TEXT("time")));
			Montage->CompositeSections.Add(Section);
		}
		Montage->BlendIn.SetBlendTime(Def->GetNumberField(TEXT("blendIn")));
		Montage->BlendOut.SetBlendTime(Def->GetNumberField(TEXT("blendOut")));
		Montage->BlendOutTriggerTime = Def->GetNumberField(TEXT("blendOutTriggerTime"));
		Montage->RateScale = Def->GetNumberField(TEXT("rate"));
		Montage->SetCompositeLength(Montage->CalculateSequenceLength());
		// 重建时整份换掉通知（-RebuildMontages 时其他流水线要重跑）
		Montage->Notifies.Reset();
		Montage->AnimNotifyTracks.Reset();
		if (!AddFlowNotifies(Montage, Def->GetArrayField(TEXT("flowNotifies")), Def->GetNumberField(TEXT("sourceLength")))) ++Ctx.Problems;
		Montage->PostEditChange();
		Ctx.Save(Montage);
		return Montage;
	}

	UBlueprint* GetOrCreateAbility(FContext& Ctx, const FString& Path, UClass* Parent)
	{
		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *AssetPath(Path), nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (Blueprint)
		{
			if (Blueprint->ParentClass != Parent)
			{
				UE_LOG(LogWuwaRoleSkillSetup, Error, TEXT("GA %s exists with parent %s (expected %s); not touched"), *Path,
					*GetNameSafe(Blueprint->ParentClass), *Parent->GetName());
				++Ctx.Problems;
				return nullptr;
			}
			Ctx.Backup(Path);
			return Blueprint;
		}
		UPackage* Package = CreatePackage(*Path);
		Blueprint = FKismetEditorUtilities::CreateBlueprint(Parent, Package, *FPackageName::GetShortName(Path), BPTYPE_Normal,
			UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
		FAssetRegistryModule::AssetCreated(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		return Blueprint;
	}
}

int32 UWuwaRoleSkillSetupCommandlet::Main(const FString& Params)
{
	using namespace WuwaRoleSkillSetup;
	FString RecipePath;
	if (!FParse::Value(*Params, TEXT("Recipe="), RecipePath, false))
	{
		UE_LOG(LogWuwaRoleSkillSetup, Error, TEXT("Usage: %s"), *HelpUsage);
		return 1;
	}
	if (FPaths::IsRelative(RecipePath)) RecipePath = FPaths::ProjectDir() / RecipePath;
	FString Text;
	TSharedPtr<FJsonObject> Recipe;
	if (!FFileHelper::LoadFileToString(Text, *RecipePath) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Recipe) || !Recipe)
	{
		UE_LOG(LogWuwaRoleSkillSetup, Error, TEXT("Cannot read recipe %s"), *RecipePath);
		return 1;
	}
	FContext Ctx;
	Ctx.bApply = FParse::Param(*Params, TEXT("Apply"));
	Ctx.BackupRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/RoleSkillSetup") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	const bool bRebuild = FParse::Param(*Params, TEXT("RebuildMontages"));

	// 1. 动画序列：根运动（原作开了的才开；已经开了的不重复挂修改器）
	for (const TSharedPtr<FJsonValue>& Value : Recipe->GetArrayField(TEXT("anims")))
	{
		const TSharedPtr<FJsonObject> Anim = Value->AsObject();
		const FString Path = Anim->GetStringField(TEXT("asset"));
		UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr, *AssetPath(Path));
		if (!Sequence)
		{
			UE_LOG(LogWuwaRoleSkillSetup, Error, TEXT("Missing anim %s (import it first)"), *Path);
			++Ctx.Problems;
			continue;
		}
		const bool bRootMotion = Anim->GetBoolField(TEXT("rootMotion"));
		const bool bLock = Anim->GetBoolField(TEXT("forceRootLock"));
		UE_LOG(LogWuwaRoleSkillSetup, Display, TEXT("Anim %s: %.3fs, root motion %d -> %d, force lock %d -> %d"), *Sequence->GetName(),
			Sequence->GetPlayLength(), Sequence->bEnableRootMotion, bRootMotion, Sequence->bForceRootLock, bLock);
		if (!Ctx.bApply || (!bRootMotion && !bLock) || (Sequence->bEnableRootMotion == bRootMotion && Sequence->bForceRootLock == bLock)) continue;
		Ctx.Backup(Path);
		Sequence->Modify();
		Sequence->bEnableRootMotion = bRootMotion;
		Sequence->bForceRootLock = bLock;
		if (bRootMotion && !UWuwaPromoteRootMotionModifier::ApplyToSequence(Sequence))
		{
			UE_LOG(LogWuwaRoleSkillSetup, Error, TEXT("  %s: applying the root motion modifier failed"), *Sequence->GetName());
			++Ctx.Problems;
		}
		Ctx.Save(Sequence);
	}

	// 2. 蒙太奇
	const FString MontageFolder = Recipe->GetStringField(TEXT("montageFolder"));
	TMap<FString, UAnimMontage*> Montages;
	for (const TSharedPtr<FJsonValue>& Value : Recipe->GetArrayField(TEXT("montages")))
	{
		const TSharedPtr<FJsonObject> Def = Value->AsObject();
		const FString Path = MontageFolder / Def->GetStringField(TEXT("name"));
		UAnimMontage* Montage = BuildMontage(Ctx, Path, Def, bRebuild);
		if (!Montage && Ctx.bApply) ++Ctx.Problems;
		Montages.Add(Path, Montage);
	}

	// 3. GA 蓝图 + 4. 技能表
	const FString TablePath = Recipe->GetStringField(TEXT("table"));
	UDataTable* Table = LoadObject<UDataTable>(nullptr, *AssetPath(TablePath));
	if (!Table || Table->RowStruct != FWuwaSkillInfo::StaticStruct())
	{
		UE_LOG(LogWuwaRoleSkillSetup, Error, TEXT("Skill table %s missing or not FWuwaSkillInfo"), *TablePath);
		return 1;
	}
	if (Ctx.bApply)
	{
		Ctx.Backup(TablePath);
		Table->Modify();
	}
	for (const TSharedPtr<FJsonValue>& Value : Recipe->GetArrayField(TEXT("skills")))
	{
		const TSharedPtr<FJsonObject> Def = Value->AsObject();
		const FName RowName = *FString::Printf(TEXT("%lld"), static_cast<int64>(Def->GetNumberField(TEXT("id"))));
		const TSharedPtr<FJsonObject> GaDef = Def->GetObjectField(TEXT("ga"));
		const FString GaPath = GaDef->GetStringField(TEXT("path"));
		UClass* Parent = LoadClass<UWuwaGameplayAbilityBase>(nullptr, *GaDef->GetStringField(TEXT("parent")));
		const int32 Level = static_cast<int32>(GaDef->GetNumberField(TEXT("interruptLevel")));
		TArray<TObjectPtr<UAnimMontage>> SkillMontages;
		TArray<TSoftObjectPtr<UAnimMontage>> SoftMontages;
		for (const TSharedPtr<FJsonValue>& MontageValue : Def->GetArrayField(TEXT("montages")))
		{
			UAnimMontage* const* Found = Montages.Find(MontageValue->AsString());
			SkillMontages.Add(Found ? *Found : nullptr);
			SoftMontages.Add(TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(AssetPath(MontageValue->AsString()))));
		}
		FGameplayTagContainer Tags;
		for (const TSharedPtr<FJsonValue>& TagValue : Def->GetArrayField(TEXT("tags")))
		{
			const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(*TagValue->AsString(), false);
			if (Tag.IsValid()) Tags.AddTag(Tag);
			else UE_LOG(LogWuwaRoleSkillSetup, Warning, TEXT("  tag %s is not registered; skipped"), *TagValue->AsString());
		}
		// 技能期间加在身上的动作标识：输入 / buff 要查，没注册就是配置错误
		FGameplayTagContainer ActionTags;
		const TArray<TSharedPtr<FJsonValue>>* ActionTagValues = nullptr;
		if (Def->TryGetArrayField(TEXT("actionTags"), ActionTagValues))
		{
			for (const TSharedPtr<FJsonValue>& TagValue : *ActionTagValues)
			{
				const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(*TagValue->AsString(), false);
				if (Tag.IsValid()) ActionTags.AddTag(Tag);
				else
				{
					UE_LOG(LogWuwaRoleSkillSetup, Error, TEXT("  action tag %s is not registered"), *TagValue->AsString());
					++Ctx.Problems;
				}
			}
		}
		const bool bWalkOffLedge = Def->GetBoolField(TEXT("walkOffLedge"));
		UE_LOG(LogWuwaRoleSkillSetup, Display, TEXT("Skill %s %s: GA %s (%s) level %d, %d montage(s), ledge protect %d, tags %s, action tags %s"), *RowName.ToString(),
			*Def->GetStringField(TEXT("name")), *FPackageName::GetShortName(GaPath), *GetNameSafe(Parent), Level, SoftMontages.Num(), bWalkOffLedge,
			*Tags.ToStringSimple(), *ActionTags.ToStringSimple());
		if (!Parent)
		{
			UE_LOG(LogWuwaRoleSkillSetup, Error, TEXT("  GA parent class %s not found"), *GaDef->GetStringField(TEXT("parent")));
			++Ctx.Problems;
			continue;
		}
		if (!Ctx.bApply) continue;
		UBlueprint* Blueprint = GetOrCreateAbility(Ctx, GaPath, Parent);
		if (!Blueprint || !Blueprint->GeneratedClass) continue;
		UWuwaGameplayAbilityBase* Ability = CastChecked<UWuwaGameplayAbilityBase>(Blueprint->GeneratedClass->GetDefaultObject());
		Ability->Modify();
		Ability->bIsMainSkill = true;
		Ability->InterruptLevel = Level;
		if (UWuwaSkillMontageAbility* MontageAbility = Cast<UWuwaSkillMontageAbility>(Ability)) MontageAbility->SkillMontages = SkillMontages;
		// 父类特有的配置（例：GA_ChangLi_AirAttack → bEndIfActivatedOnGround），按属性名导入文本值
		const TSharedPtr<FJsonObject>* Properties = nullptr;
		if (GaDef->TryGetObjectField(TEXT("properties"), Properties))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Properties)->Values)
			{
				FProperty* Property = Ability->GetClass()->FindPropertyByName(*Pair.Key);
				const FString ValueText = Pair.Value->AsString();
				if (!Property || !Property->ImportText_InContainer(*ValueText, Ability, Ability, PPF_None))
				{
					UE_LOG(LogWuwaRoleSkillSetup, Error, TEXT("  GA property %s = %s could not be set"), *Pair.Key, *ValueText);
					++Ctx.Problems;
				}
			}
		}
		FKismetEditorUtilities::CompileBlueprint(Blueprint);
		Ctx.Save(Blueprint);

		FWuwaSkillInfo* Row = Table->FindRow<FWuwaSkillInfo>(RowName, TEXT("RoleSkillSetup"), false);
		if (!Row)
		{
			Table->AddRow(RowName, FWuwaSkillInfo());
			Row = Table->FindRow<FWuwaSkillInfo>(RowName, TEXT("RoleSkillSetup"), false);
		}
		Row->SkillName = FText::FromString(Def->GetStringField(TEXT("name")));
		Row->Ability = Blueprint->GeneratedClass;
		Row->SkillTags = Tags;
		Row->ActionTags = ActionTags;
		Row->Montages = SoftMontages;
		Row->bWalkOffLedge = bWalkOffLedge;
	}
	if (Ctx.bApply) Ctx.Save(Table);

	if (!Ctx.bApply)
	{
		UE_LOG(LogWuwaRoleSkillSetup, Display, TEXT("Read-only; pass -Apply to write."));
		return Ctx.Problems ? 1 : 0;
	}
	if (!UEditorLoadingAndSavingUtils::SavePackages(Ctx.ToSave, false))
	{
		UE_LOG(LogWuwaRoleSkillSetup, Error, TEXT("Saving failed"));
		return 1;
	}
	UE_LOG(LogWuwaRoleSkillSetup, Display, TEXT("Saved %d packages; backups (if any) in %s"), Ctx.ToSave.Num(), *Ctx.BackupRoot);
	return Ctx.Problems ? 1 : 0;
}
