#include "Editor/Commandlets/WuwaMotionNotifySetupCommandlet.h"
#include "Editor/WuwaAnimNotifyEditorUtils.h"
#include "Game/Animation/Notifies/WuwaAnimNotify_CameraModify.h"
#include "Game/Animation/Notifies/WuwaAnimNotify_SequenceCamera.h"
#include "Game/Animation/Notifies/WuwaAnimNotifyState_Burst.h"
#include "Game/Animation/Notifies/WuwaAnimNotifyState_AbsoluteTimeStop.h"

#if WITH_EDITOR
#include "Animation/AnimMontage.h"
#include "Dom/JsonObject.h"
#include "Game/Animation/Notifies/WuwaAnimNotifyState_KeepAwayFromGround.h"
#include "Game/Animation/Notifies/WuwaAnimNotifyState_MoveToTarget.h"
#include "Game/Animation/Notifies/WuwaAnimNotifyState_RoleRotate.h"
#include "Game/Animation/Notifies/WuwaAnimNotifyState_SkillRotate.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogWuwaMotionNotifySetup, Log, All);

UWuwaMotionNotifySetupCommandlet::UWuwaMotionNotifySetupCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
	HelpDescription = TEXT("Installs Move To Target / Skill Rotate / Role Rotate windows from ArtSource/Combat/<character>.motion_notifies.json. Read-only unless -Apply.");
	HelpUsage = TEXT("-run=WuwaMotionNotifySetup -Character=changli|jiabei [-Apply]");
}

#if WITH_EDITOR
namespace WuwaMotionNotifySetup
{
	const FName TrackName(TEXT("Motion"));
	constexpr double AllowedRatios[] = { 1.0, 1.25 };
	constexpr double RatioTolerance = 0.04;  // 嘉贝 Attack_H_0201 导出只有 86 帧（原作 89 帧，2.833 / 2.930 = 0.967），只差末尾 0.1 秒：按 1:1 原时间装

	bool IsMotionNotify(const FAnimNotifyEvent& Event)
	{
		// 瞬时通知：技能特写镜头、镜头修改
		if (Event.Notify && (Event.Notify->IsA<UWuwaAnimNotify_SequenceCamera>() || Event.Notify->IsA<UWuwaAnimNotify_CameraModify>())) return true;
		const UAnimNotifyState* State = Event.NotifyStateClass;
		return State && (State->IsA<UWuwaAnimNotifyState_MoveToTarget>() || State->IsA<UWuwaAnimNotifyState_SkillRotate>() || State->IsA<UWuwaAnimNotifyState_RoleRotate>()
			|| State->IsA<UWuwaAnimNotifyState_KeepAwayFromGround>() || State->IsA<UWuwaAnimNotifyState_AbsoluteTimeStop>()
			|| State->IsA<UWuwaAnimNotifyState_Burst>() || State->IsA<UWuwaAnimNotifyState_CameraModify>());
	}

	FString NotifyName(const UObject* Object)
	{
		if (const UAnimNotify* Instant = Cast<UAnimNotify>(Object)) return Instant->GetNotifyName();
		if (const UAnimNotifyState* State = Cast<UAnimNotifyState>(Object)) return State->GetNotifyName();
		return GetNameSafe(Object);
	}

	UClass* ClassFor(const FString& Type)
	{
		if (Type == TEXT("MoveToTarget")) return UWuwaAnimNotifyState_MoveToTarget::StaticClass();
		if (Type == TEXT("SkillRotate")) return UWuwaAnimNotifyState_SkillRotate::StaticClass();
		if (Type == TEXT("RoleRotate")) return UWuwaAnimNotifyState_RoleRotate::StaticClass();
		if (Type == TEXT("KeepAwayFromGround")) return UWuwaAnimNotifyState_KeepAwayFromGround::StaticClass();
		if (Type == TEXT("AbsoluteTimeStop")) return UWuwaAnimNotifyState_AbsoluteTimeStop::StaticClass();
		if (Type == TEXT("BurstState")) return UWuwaAnimNotifyState_Burst::StaticClass();
		if (Type == TEXT("SequenceCamera")) return UWuwaAnimNotify_SequenceCamera::StaticClass();
		if (Type == TEXT("CameraModify")) return UWuwaAnimNotify_CameraModify::StaticClass();
		if (Type == TEXT("CameraModifyWindow")) return UWuwaAnimNotifyState_CameraModify::StaticClass();
		return nullptr;
	}

	/** 配方字段写进通知：位移吸附写进 Config，其余写通知本身的属性。错误时返回说明。 */
	FString ApplyProperties(UObject* State, const TSharedPtr<FJsonObject>& Properties)
	{
		const UStruct* Struct = State->GetClass();
		void* Container = State;
		if (UWuwaAnimNotifyState_MoveToTarget* Move = Cast<UWuwaAnimNotifyState_MoveToTarget>(State))
		{
			Struct = FWuwaMoveToTargetConfig::StaticStruct();
			Container = &Move->Config;
		}
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Properties->Values)
		{
			FProperty* Property = FindFProperty<FProperty>(Struct, *Pair.Key);
			if (!Property) return FString::Printf(TEXT("%s has no property %s"), *Struct->GetName(), *Pair.Key);
			FString Text;
			if (Pair.Value->Type == EJson::Boolean) Text = Pair.Value->AsBool() ? TEXT("True") : TEXT("False");
			else if (Pair.Value->Type == EJson::Number) Text = FString::SanitizeFloat(Pair.Value->AsNumber());
			else Text = Pair.Value->AsString();
			if (!Property->ImportText_InContainer(*Text, Container, State, PPF_None))
				return FString::Printf(TEXT("cannot set %s.%s = %s"), *Struct->GetName(), *Pair.Key, *Text);
		}
		return FString();
	}

	bool Backup(UObject* Asset, const FString& Root)
	{
		FString Relative = Asset->GetOutermost()->GetName();
		if (!Relative.RemoveFromStart(TEXT("/Game/"))) return false;
		const FString Source = FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension());
		const FString Destination = Root / (Relative + FPackageName::GetAssetPackageExtension());
		return IFileManager::Get().MakeDirectory(*FPaths::GetPath(Destination), true)
			&& IFileManager::Get().Copy(*Destination, *Source, false, true) == COPY_OK;
	}

	bool Save(UObject* Asset)
	{
		UPackage* Package = Asset->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		Args.Error = GWarn;
		Asset->MarkPackageDirty();
		if (!UPackage::SavePackage(Package, Asset, *Filename, Args))
		{
			UE_LOG(LogWuwaMotionNotifySetup, Error, TEXT("Could not save %s (is the editor open with it loaded?)"), *Filename);
			return false;
		}
		UE_LOG(LogWuwaMotionNotifySetup, Display, TEXT("Saved %s"), *Package->GetName());
		return true;
	}
}
#endif

int32 UWuwaMotionNotifySetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaMotionNotifySetup;
	FString Character;
	if (!FParse::Value(*Params, TEXT("Character="), Character) || Character.IsEmpty())
	{
		UE_LOG(LogWuwaMotionNotifySetup, Error, TEXT("Usage: %s"), *HelpUsage);
		return 1;
	}
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	const FString RecipePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("ArtSource/Combat") / (Character + TEXT(".motion_notifies.json")));
	FString Text;
	TSharedPtr<FJsonObject> Recipe;
	if (!FFileHelper::LoadFileToString(Text, *RecipePath) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Recipe) || !Recipe)
	{
		UE_LOG(LogWuwaMotionNotifySetup, Error, TEXT("Cannot read %s (run ArtSource/Combat/prepare_motion_notifies.py --character %s first)."), *RecipePath, *Character);
		return 1;
	}

	TArray<FString> Order;
	TMap<FString, TArray<TSharedPtr<FJsonObject>>> ByAsset;
	for (const TSharedPtr<FJsonValue>& Value : Recipe->GetArrayField(TEXT("notifies")))
	{
		const TSharedPtr<FJsonObject> Entry = Value->AsObject();
		const FString Asset = Entry->GetStringField(TEXT("asset"));
		if (!ByAsset.Contains(Asset)) Order.Add(Asset);
		ByAsset.FindOrAdd(Asset).Add(Entry);
	}

	const FString BackupRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/MotionNotifySetup")
		/ FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	int32 Errors = 0, Installed = 0, Removed = 0;
	for (const FString& AssetPath : Order)
	{
		const TArray<TSharedPtr<FJsonObject>>& Entries = ByAsset[AssetPath];
		UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *AssetPath);
		if (!Montage)
		{
			UE_LOG(LogWuwaMotionNotifySetup, Error, TEXT("Missing montage %s"), *AssetPath);
			++Errors;
			continue;
		}
		const double SourceLength = Entries[0]->GetNumberField(TEXT("sourceLength"));
		const double RawRatio = Montage->GetPlayLength() / SourceLength;
		double Ratio = 0.0;
		for (const double Allowed : AllowedRatios)
		{
			if (FMath::Abs(RawRatio - Allowed) <= RatioTolerance) { Ratio = Allowed; break; }
		}
		if (Ratio == 0.0)
		{
			UE_LOG(LogWuwaMotionNotifySetup, Error, TEXT("%s is %.3fs but original %s is %.3fs (ratio %.3f); not installed."), *Montage->GetName(),
				Montage->GetPlayLength(), *Entries[0]->GetStringField(TEXT("sourceAsset")), SourceLength, RawRatio);
			++Errors;
			continue;
		}

		// 先在临时对象上把字段都写一遍：字段名 / 值不对时不动资产
		TArray<UObject*> States;
		bool bMontageOk = true;
		for (const TSharedPtr<FJsonObject>& Entry : Entries)
		{
			UClass* Class = ClassFor(Entry->GetStringField(TEXT("type")));
			if (!Class)
			{
				UE_LOG(LogWuwaMotionNotifySetup, Error, TEXT("%s: unknown notify type %s"), *Montage->GetName(), *Entry->GetStringField(TEXT("type")));
				bMontageOk = false;
				continue;
			}
			UObject* State = NewObject<UObject>(bApply ? static_cast<UObject*>(Montage) : GetTransientPackage(), Class, NAME_None, RF_Transactional);
			const FString Problem = ApplyProperties(State, Entry->GetObjectField(TEXT("properties")));
			if (!Problem.IsEmpty())
			{
				UE_LOG(LogWuwaMotionNotifySetup, Error, TEXT("%s: %s"), *Montage->GetName(), *Problem);
				bMontageOk = false;
				continue;
			}
			States.Add(State);
			UE_LOG(LogWuwaMotionNotifySetup, Display, TEXT("%s @ %.3fs +%.3fs: %s (%s @ %.3fs%s)"), *Montage->GetName(), Entry->GetNumberField(TEXT("time")) * Ratio,
				Entry->GetNumberField(TEXT("duration")) * Ratio, *NotifyName(State), *Entry->GetStringField(TEXT("sourceAsset")), Entry->GetNumberField(TEXT("time")),
				Ratio == 1.0 ? TEXT("") : *FString::Printf(TEXT(" x%.2f"), Ratio));
		}
		if (!bMontageOk)
		{
			++Errors;
			continue;
		}
		if (!bApply) continue;
		if (!Backup(Montage, BackupRoot))
		{
			UE_LOG(LogWuwaMotionNotifySetup, Error, TEXT("Could not back up %s"), *Montage->GetPathName());
			return 1;
		}

		Montage->Modify();
		Removed += Montage->Notifies.RemoveAll(&IsMotionNotify);
		int32 Track = Montage->AnimNotifyTracks.IndexOfByPredicate([](const FAnimNotifyTrack& T) { return T.TrackName == TrackName; });
		if (Track == INDEX_NONE)
		{
			Track = Montage->AnimNotifyTracks.AddDefaulted();
			Montage->AnimNotifyTracks[Track].TrackName = TrackName;
			Montage->AnimNotifyTracks[Track].TrackColor = FLinearColor(.2f, .6f, 1.f);
		}
		for (int32 Index = 0; Index < Entries.Num(); ++Index)
		{
			const float Start = static_cast<float>(Entries[Index]->GetNumberField(TEXT("time")) * Ratio);
			const float Duration = static_cast<float>(Entries[Index]->GetNumberField(TEXT("duration")) * Ratio);
			FAnimNotifyEvent& Notify = Montage->Notifies.AddDefaulted_GetRef();
			Notify.TrackIndex = Track;
			Notify.NotifyTriggerChance = 1.f;
			Notify.Guid = FGuid::NewGuid();
			Notify.NotifyName = FName(*NotifyName(States[Index]));
			Notify.Link(Montage, Start);
			if (UAnimNotify* Instant = Cast<UAnimNotify>(States[Index]))
			{
				Notify.Notify = Instant;
			}
			else
			{
				Notify.NotifyStateClass = Cast<UAnimNotifyState>(States[Index]);
				Notify.SetDuration(Duration);
				Notify.EndLink.Link(Montage, Start + Duration);
			}
			++Installed;
		}
		Montage->SortNotifies();
		WuwaAnimNotifyEditor::RefreshTriggerOffsets(Montage);
		Montage->RefreshCacheData();
		if (!Save(Montage)) return 1;
	}
	UE_LOG(LogWuwaMotionNotifySetup, Display, TEXT("%s: %d notifies on %d montages, %d errors%s"), *Character,
		bApply ? Installed : Recipe->GetArrayField(TEXT("notifies")).Num(), Order.Num(), Errors,
		bApply ? *FString::Printf(TEXT(" (%d old motion notifies replaced; backups in %s)"), Removed, *BackupRoot) : TEXT(" — audit only, run with -Apply"));
	return Errors > 0 ? 1 : 0;
#else
	return 1;
#endif
}
