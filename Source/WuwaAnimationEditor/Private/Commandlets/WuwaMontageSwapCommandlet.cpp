#include "Commandlets/WuwaMontageSwapCommandlet.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimTypes.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AnimationModifiersAssetUserData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modifiers/WuwaPromoteRootMotionModifier.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaMontageSwap, Log, All);

namespace WuwaMontageSwap
{
	bool HasRootModifier(UAnimSequence* Sequence)
	{
		const auto* Data = Cast<UAnimationModifiersAssetUserData>(
			Sequence->GetAssetUserDataOfClass(UAnimationModifiersAssetUserData::StaticClass()));
		if (Data)
		{
			for (const UAnimationModifier* Modifier : Data->GetAnimationModifierInstances())
			{
				if (Modifier && Modifier->IsA<UWuwaPromoteRootMotionModifier>()) { return true; }
			}
		}
		return false;
	}

	FString DescribeNotifies(const TArray<FAnimNotifyEvent>& Notifies)
	{
		FString Out;
		for (const FAnimNotifyEvent& N : Notifies)
		{
			const FString Name = N.Notify ? N.Notify->GetClass()->GetName() : N.NotifyStateClass ? N.NotifyStateClass->GetClass()->GetName() : N.NotifyName.ToString();
			Out += FString::Printf(TEXT(" %s@%.3f+%.3f"), *Name, N.GetTime(), N.GetDuration());
		}
		return Out.IsEmpty() ? TEXT(" -") : Out;
	}

	void Relink(FAnimNotifyEvent& Event, UAnimSequenceBase* Owner, float Time, float Duration)
	{
		const int32 Slot = Event.GetSlotIndex();
		Event.Link(Owner, Time, Slot);
		Event.TriggerTimeOffset = GetTriggerTimeOffsetForType(Owner->CalculateOffsetForNotify(Time));
		if (Duration > 0.f)
		{
			Event.SetDuration(Duration);
			Event.EndLink.Link(Owner, Time + Duration, Slot);
			Event.EndTriggerTimeOffset = GetTriggerTimeOffsetForType(Owner->CalculateOffsetForNotify(Time + Duration));
		}
	}
}

UWuwaMontageSwapCommandlet::UWuwaMontageSwapCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UWuwaMontageSwapCommandlet::Main(const FString& Params)
{
	using namespace WuwaMontageSwap;

	FString PairsText;
	FParse::Value(*Params, TEXT("Pairs="), PairsText, false);
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	TArray<FString> Pairs;
	PairsText.ParseIntoArray(Pairs, TEXT(";"));
	if (Pairs.IsEmpty())
	{
		UE_LOG(LogWuwaMontageSwap, Error, TEXT("需要 -Pairs=<蒙太奇>=<新动画>;..."));
		return 1;
	}

	const FString BackupRoot = FPaths::ProjectSavedDir() / TEXT("Backups/MontageSwap") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"));
	TArray<UPackage*> ToSave;
	int32 Problems = 0;
	for (const FString& Pair : Pairs)
	{
		FString MontagePath, NewPath;
		if (!Pair.Split(TEXT("="), &MontagePath, &NewPath))
		{
			UE_LOG(LogWuwaMontageSwap, Error, TEXT("格式不对：%s"), *Pair);
			++Problems;
			continue;
		}
		// <蒙太奇>=<动画>@<模板蒙太奇>：蒙太奇不存在时复制模板（插槽、混合、通知都沿用，通知随后按比例缩放）
		FString TemplatePath;
		if (NewPath.Contains(TEXT("@")))
		{
			FString AnimPart;
			NewPath.Split(TEXT("@"), &AnimPart, &TemplatePath);
			NewPath = AnimPart;
		}
		UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Montage && !TemplatePath.IsEmpty())
		{
			UAnimMontage* Template = LoadObject<UAnimMontage>(nullptr, *TemplatePath);
			const FString PackageName = FPackageName::ObjectPathToPackageName(MontagePath);
			if (Template && !FPackageName::DoesPackageExist(PackageName))
			{
				if (!bApply)
				{
					UE_LOG(LogWuwaMontageSwap, Display, TEXT("== %s 不存在，-Apply 时会从 %s 复制"), *MontagePath, *Template->GetName());
					continue;
				}
				Montage = DuplicateObject<UAnimMontage>(Template, CreatePackage(*PackageName), *FPackageName::GetLongPackageAssetName(PackageName));
				Montage->SetFlags(RF_Public | RF_Standalone);
				FAssetRegistryModule::AssetCreated(Montage);
				UE_LOG(LogWuwaMontageSwap, Display, TEXT("== 从 %s 复制出 %s"), *Template->GetName(), *Montage->GetPathName());
			}
		}
		UAnimSequence* New = LoadObject<UAnimSequence>(nullptr, *NewPath);
		if (!Montage || !New)
		{
			UE_LOG(LogWuwaMontageSwap, Error, TEXT("加载失败：montage=%s(%d) anim=%s(%d)"), *MontagePath, Montage != nullptr, *NewPath, New != nullptr);
			++Problems;
			continue;
		}

		// 只处理单片段蒙太奇：时间映射才是简单的整体缩放
		TArray<FAnimSegment*> Segments;
		for (FSlotAnimationTrack& Slot : Montage->SlotAnimTracks)
		{
			for (FAnimSegment& Segment : Slot.AnimTrack.AnimSegments)
			{
				Segments.Add(&Segment);
			}
		}
		if (Segments.Num() != 1 || !Cast<UAnimSequence>(Segments[0]->GetAnimReference()))
		{
			UE_LOG(LogWuwaMontageSwap, Error, TEXT("%s 有 %d 个片段（只支持一个动画序列片段），跳过"), *Montage->GetName(), Segments.Num());
			++Problems;
			continue;
		}
		FAnimSegment& Segment = *Segments[0];
		UAnimSequence* Old = Cast<UAnimSequence>(Segment.GetAnimReference().Get());
		const float OldLength = Old->GetPlayLength();
		const float NewLength = New->GetPlayLength();
		const float Ratio = NewLength / OldLength;
		const bool bOldModifier = HasRootModifier(Old);
		const bool bNewModifier = HasRootModifier(New);

		UE_LOG(LogWuwaMontageSwap, Display, TEXT("== %s"), *Montage->GetPathName());
		UE_LOG(LogWuwaMontageSwap, Display, TEXT("   旧：%s  %.3fs@%.0f  片段 %.3f~%.3f 起点 %.3f 速率 %.2f 循环 %d  rootMotion=%d lock=%d modifier=%d"),
			*Old->GetPathName(), OldLength, Old->GetSamplingFrameRate().AsDecimal(), Segment.AnimStartTime, Segment.AnimEndTime, Segment.StartPos,
			Segment.AnimPlayRate, Segment.LoopingCount, Old->bEnableRootMotion, Old->bForceRootLock, bOldModifier);
		UE_LOG(LogWuwaMontageSwap, Display, TEXT("   新：%s  %.3fs@%.0f  rootMotion=%d modifier=%d  比例 x%.4f"),
			*New->GetPathName(), NewLength, New->GetSamplingFrameRate().AsDecimal(), New->bEnableRootMotion, bNewModifier, Ratio);
		UE_LOG(LogWuwaMontageSwap, Display, TEXT("   蒙太奇长度 %.3f，分段：%s"), Montage->GetPlayLength(),
			*FString::JoinBy(Montage->CompositeSections, TEXT(" "), [](const FCompositeSection& S) { return FString::Printf(TEXT("%s@%.3f"), *S.SectionName.ToString(), S.GetTime()); }));
		UE_LOG(LogWuwaMontageSwap, Display, TEXT("   蒙太奇通知：%s"), *DescribeNotifies(Montage->Notifies));
		UE_LOG(LogWuwaMontageSwap, Display, TEXT("   旧动画通知：%s | 新动画通知：%s"), *DescribeNotifies(Old->Notifies), *DescribeNotifies(New->Notifies));
		const bool bFullSegment = FMath::IsNearlyZero(Segment.AnimStartTime, 0.002f) && FMath::IsNearlyEqual(Segment.AnimEndTime, OldLength, 0.002f);
		if (!bFullSegment)
		{
			UE_LOG(LogWuwaMontageSwap, Warning, TEXT("   片段不是整段动画，起止时间也按比例缩放"));
		}
		if (Old == New)
		{
			UE_LOG(LogWuwaMontageSwap, Display, TEXT("   已经是新动画，跳过"));
			continue;
		}
		if (!bApply)
		{
			continue;
		}

		// 备份
		for (UObject* Asset : {static_cast<UObject*>(Montage), static_cast<UObject*>(New)})
		{
			const FString Package = Asset->GetOutermost()->GetName();
			const FString File = FPackageName::LongPackageNameToFilename(Package, FPackageName::GetAssetPackageExtension());
			IFileManager::Get().Copy(*(BackupRoot / Package.RightChop(6) + FPackageName::GetAssetPackageExtension()), *File);
		}

		// 1. 新动画：根运动设置、修改器、序列通知
		New->Modify();
		New->bEnableRootMotion = Old->bEnableRootMotion;
		New->RootMotionRootLock = Old->RootMotionRootLock;
		New->bForceRootLock = Old->bForceRootLock;
		if (bOldModifier && !bNewModifier && !UWuwaPromoteRootMotionModifier::ApplyToSequence(New))
		{
			UE_LOG(LogWuwaMontageSwap, Error, TEXT("   %s 应用根运动修改器失败"), *New->GetName());
			++Problems;
			continue;
		}
		if (New->Notifies.IsEmpty() && !Old->Notifies.IsEmpty())
		{
			if (New->AnimNotifyTracks.IsEmpty())
			{
				for (const FAnimNotifyTrack& Track : Old->AnimNotifyTracks)
				{
					New->AnimNotifyTracks.Emplace(Track.TrackName, Track.TrackColor);
				}
			}
			for (const FAnimNotifyEvent& OldEvent : Old->Notifies)
			{
				FAnimNotifyEvent Event = OldEvent;
				Event.Notify = OldEvent.Notify ? DuplicateObject<UAnimNotify>(OldEvent.Notify.Get(), New) : nullptr;
				Event.NotifyStateClass = OldEvent.NotifyStateClass ? DuplicateObject<UAnimNotifyState>(OldEvent.NotifyStateClass.Get(), New) : nullptr;
				Event.TrackIndex = FMath::Clamp(Event.TrackIndex, 0, New->AnimNotifyTracks.Num() - 1);
				Relink(Event, New, OldEvent.GetTime() * Ratio, OldEvent.GetDuration() * Ratio);
				New->Notifies.Add(MoveTemp(Event));
			}
			New->SortNotifies();
			New->RefreshCacheData();
		}
		New->PostEditChange();
		New->MarkPackageDirty();

		// 2. 蒙太奇：换片段、缩放时间（片段从 StartPos 开始，蒙太奇时间 t -> StartPos + (t - StartPos) * Ratio）
		Montage->Modify();
		const float StartPos = Segment.StartPos;
		auto Map = [StartPos, Ratio](float T) { return T <= StartPos ? T : StartPos + (T - StartPos) * Ratio; };
		// 换片段之前先记下绝对时间：通知 / 分段可能是「相对片段」链接的，还缓存着旧片段长度，换完再读会算偏
		// （实测取消窗口结束点从 1.333 偏到 1.067）。
		TArray<float> SectionTimes;
		for (const FCompositeSection& Section : Montage->CompositeSections)
		{
			SectionTimes.Add(Section.GetTime());
		}
		TArray<TPair<float, float>> NotifyTimes; // 起点、终点（绝对时间）
		for (const FAnimNotifyEvent& Event : Montage->Notifies)
		{
			NotifyTimes.Emplace(Event.GetTime(), Event.GetTime() + Event.GetDuration());
		}
		const float NewStart = Segment.AnimStartTime * Ratio;
		const float NewEnd = Segment.AnimEndTime * Ratio;
		Segment.SetAnimReference(New, false);
		Segment.AnimStartTime = NewStart;
		Segment.AnimEndTime = FMath::Min(NewEnd, NewLength);
		Montage->SetCompositeLength(Montage->CalculateSequenceLength());
		for (int32 I = 0; I < Montage->CompositeSections.Num(); ++I)
		{
			FCompositeSection& Section = Montage->CompositeSections[I];
			const EAnimLinkMethod::Type Method = Section.GetLinkMethod();
			Section.ChangeLinkMethod(EAnimLinkMethod::Absolute);
			Section.Link(Montage, Map(SectionTimes[I]), Section.GetSlotIndex());
			Section.ChangeLinkMethod(Method);
		}
		for (int32 I = 0; I < Montage->Notifies.Num(); ++I)
		{
			FAnimNotifyEvent& Event = Montage->Notifies[I];
			const EAnimLinkMethod::Type Method = Event.GetLinkMethod();
			const EAnimLinkMethod::Type EndMethod = Event.EndLink.GetLinkMethod();
			Event.ChangeLinkMethod(EAnimLinkMethod::Absolute);
			Event.EndLink.ChangeLinkMethod(EAnimLinkMethod::Absolute);
			const float Start = Map(NotifyTimes[I].Key);
			const float End = Map(NotifyTimes[I].Value);
			Relink(Event, Montage, Start, End - Start);
			// 切回原来的链接方式：引擎用新片段重新换算，绝对位置不变
			Event.ChangeLinkMethod(Method);
			Event.EndLink.ChangeLinkMethod(EndMethod);
		}
		Montage->RefreshCacheData();
		Montage->PostEditChange();
		// 读回校验：引擎刷新时会按时间重排通知（同一时间的两个会互换位置），所以不按下标对，
		// 而是给每个读回的「起点~终点」找一个还没配对、且吻合的预期值
		TArray<TPair<float, float>> Expected;
		for (const TPair<float, float>& Times : NotifyTimes)
		{
			Expected.Emplace(Map(Times.Key), Map(Times.Value));
		}
		TArray<bool> Used;
		Used.Init(false, Expected.Num());
		bool bTimesOk = Expected.Num() == Montage->Notifies.Num();
		for (int32 I = 0; I < Montage->Notifies.Num(); ++I)
		{
			const FAnimNotifyEvent& Event = Montage->Notifies[I];
			const float Start = Event.GetTime();
			const float End = Start + Event.GetDuration();
			bool bMatched = false;
			for (int32 K = 0; K < Expected.Num() && !bMatched; ++K)
			{
				if (!Used[K] && FMath::IsNearlyEqual(Start, Expected[K].Key, 0.002f) && FMath::IsNearlyEqual(End, Expected[K].Value, 0.002f))
				{
					Used[K] = bMatched = true;
				}
			}
			if (!bMatched)
			{
				UE_LOG(LogWuwaMontageSwap, Error, TEXT("   %s 通知 %d 读回 %.3f~%.3f，和预期都对不上，不保存"), *Montage->GetName(), I, Start, End);
				bTimesOk = false;
			}
		}
		if (!bTimesOk)
		{
			++Problems;
			continue;
		}
		Montage->SortNotifies();
		Montage->MarkPackageDirty();

		UE_LOG(LogWuwaMontageSwap, Display, TEXT("   换好：蒙太奇长度 %.3f，分段：%s"), Montage->GetPlayLength(),
			*FString::JoinBy(Montage->CompositeSections, TEXT(" "), [](const FCompositeSection& S) { return FString::Printf(TEXT("%s@%.3f"), *S.SectionName.ToString(), S.GetTime()); }));
		UE_LOG(LogWuwaMontageSwap, Display, TEXT("   蒙太奇通知：%s"), *DescribeNotifies(Montage->Notifies));
		UE_LOG(LogWuwaMontageSwap, Display, TEXT("   新动画：rootMotion=%d modifier=%d 通知：%s"), New->bEnableRootMotion, HasRootModifier(New), *DescribeNotifies(New->Notifies));
		ToSave.AddUnique(Montage->GetOutermost());
		ToSave.AddUnique(New->GetOutermost());
	}

	if (bApply && !ToSave.IsEmpty())
	{
		UE_LOG(LogWuwaMontageSwap, Display, TEXT("已备份到 %s"), *BackupRoot);
		if (!UEditorLoadingAndSavingUtils::SavePackages(ToSave, false))
		{
			UE_LOG(LogWuwaMontageSwap, Error, TEXT("保存失败"));
			return 1;
		}
		UE_LOG(LogWuwaMontageSwap, Display, TEXT("已保存 %d 个包"), ToSave.Num());
	}
	return Problems ? 1 : 0;
}
