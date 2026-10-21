#include "Commandlets/WuwaBossSetupCommandlet.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Engine/DataTable.h"
#include "FileHelpers.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillTypes.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modifiers/WuwaPromoteRootMotionModifier.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaBossSetup, Log, All);

UWuwaBossSetupCommandlet::UWuwaBossSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpUsage = TEXT("-run=WuwaBossSetup -Recipe=ArtSource/Boss/<boss>.boss.json [-Apply]");
}

namespace WuwaBossSetup
{
	FString AssetPath(const FString& PackagePath)
	{
		return PackagePath + TEXT(".") + FPackageName::GetShortName(PackagePath);
	}

	/** 已有资产的包文件复制到备份目录（保持 /Game 下的相对路径）。 */
	void Backup(const FString& PackagePath, const FString& BackupRoot)
	{
		FString File;
		if (FPackageName::DoesPackageExist(PackagePath, &File))
		{
			IFileManager::Get().Copy(*(BackupRoot / PackagePath.RightChop(6) + FPackageName::GetAssetPackageExtension()), *File);
		}
	}

	/** 找到或新建资产（同名同类的直接复用，保持引用不断）。 */
	template <typename T>
	T* FindOrCreate(const FString& PackagePath)
	{
		if (T* Existing = LoadObject<T>(nullptr, *AssetPath(PackagePath), nullptr, LOAD_NoWarn | LOAD_Quiet)) return Existing;
		UPackage* Package = CreatePackage(*PackagePath);
		Package->FullyLoad();
		T* Created = NewObject<T>(Package, *FPackageName::GetShortName(PackagePath), RF_Public | RF_Standalone);
		FAssetRegistryModule::AssetCreated(Created);
		return Created;
	}
}

int32 UWuwaBossSetupCommandlet::Main(const FString& Params)
{
	using namespace WuwaBossSetup;
	FString RecipePath;
	if (!FParse::Value(*Params, TEXT("Recipe="), RecipePath, false))
	{
		UE_LOG(LogWuwaBossSetup, Error, TEXT("Usage: %s"), *HelpUsage);
		return 1;
	}
	if (FPaths::IsRelative(RecipePath)) RecipePath = FPaths::ProjectDir() / RecipePath;
	FString Text;
	TSharedPtr<FJsonObject> Recipe;
	if (!FFileHelper::LoadFileToString(Text, *RecipePath) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Recipe) || !Recipe)
	{
		UE_LOG(LogWuwaBossSetup, Error, TEXT("Cannot read recipe %s"), *RecipePath);
		return 1;
	}
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	const FString BackupRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/BossSetup") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	TArray<UPackage*> ToSave;
	int32 Problems = 0;

	// 1. 动画序列：根运动
	for (const TSharedPtr<FJsonValue>& Value : Recipe->GetArrayField(TEXT("anims")))
	{
		const TSharedPtr<FJsonObject> Anim = Value->AsObject();
		const FString Path = Anim->GetStringField(TEXT("asset"));
		UAnimSequence* Sequence = LoadObject<UAnimSequence>(nullptr, *AssetPath(Path));
		if (!Sequence)
		{
			UE_LOG(LogWuwaBossSetup, Error, TEXT("Missing anim %s (import it first)"), *Path);
			++Problems;
			continue;
		}
		const bool bRootMotion = Anim->GetBoolField(TEXT("rootMotion"));
		UE_LOG(LogWuwaBossSetup, Display, TEXT("Anim %s: %.3fs, root motion %d -> %d"), *Sequence->GetName(), Sequence->GetPlayLength(), Sequence->bEnableRootMotion, bRootMotion);
		if (!bApply || !bRootMotion) continue;
		Backup(Path, BackupRoot);
		Sequence->bEnableRootMotion = true;
		if (!UWuwaPromoteRootMotionModifier::ApplyToSequence(Sequence))
		{
			UE_LOG(LogWuwaBossSetup, Error, TEXT("  %s: applying the root motion modifier failed"), *Sequence->GetName());
			++Problems;
		}
		Sequence->MarkPackageDirty();
		ToSave.AddUnique(Sequence->GetOutermost());
	}

	// 2. 蒙太奇
	const FString MontageFolder = Recipe->GetStringField(TEXT("montageFolder"));
	for (const TSharedPtr<FJsonValue>& Value : Recipe->GetArrayField(TEXT("montages")))
	{
		const TSharedPtr<FJsonObject> Def = Value->AsObject();
		const FString Path = MontageFolder / Def->GetStringField(TEXT("name"));
		TArray<FSlotAnimationTrack> Tracks;
		USkeleton* Skeleton = nullptr;
		bool bMissing = false;
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
					UE_LOG(LogWuwaBossSetup, Error, TEXT("Montage %s: missing anim %s"), *Path, *SegmentDef->GetStringField(TEXT("anim")));
					bMissing = true;
					continue;
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
		if (bMissing || !Skeleton)
		{
			++Problems;
			continue;
		}
		UE_LOG(LogWuwaBossSetup, Display, TEXT("Montage %s: %s"), *FPackageName::GetShortName(Path),
			*FString::JoinBy(Tracks[0].AnimTrack.AnimSegments, TEXT(" + "), [](const FAnimSegment& S)
			{
				return FString::Printf(TEXT("%s[%.3f~%.3f x%.2f @%.3f]"), *S.GetAnimReference()->GetName(), S.AnimStartTime, S.AnimEndTime, S.AnimPlayRate, S.StartPos);
			}));
		if (!bApply) continue;
		Backup(Path, BackupRoot);
		UAnimMontage* Montage = FindOrCreate<UAnimMontage>(Path);
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
		Montage->PostEditChange();
		Montage->MarkPackageDirty();
		ToSave.AddUnique(Montage->GetOutermost());
	}

	// 3. 技能表
	const FString TablePath = Recipe->GetStringField(TEXT("table"));
	TMap<FName, FWuwaSkillInfo> Rows;
	for (const TSharedPtr<FJsonValue>& Value : Recipe->GetArrayField(TEXT("skills")))
	{
		const TSharedPtr<FJsonObject> Def = Value->AsObject();
		FWuwaSkillInfo Row;
		Row.SkillName = FText::FromString(Def->GetStringField(TEXT("name")));
		for (const TSharedPtr<FJsonValue>& Montage : Def->GetArrayField(TEXT("montages")))
		{
			Row.Montages.Add(TSoftObjectPtr<UAnimMontage>(FSoftObjectPath(AssetPath(Montage->AsString()))));
		}
		const FName RowName = *FString::Printf(TEXT("%lld"), static_cast<int64>(Def->GetNumberField(TEXT("id"))));
		UE_LOG(LogWuwaBossSetup, Display, TEXT("Skill %s %s: %d montage(s)"), *RowName.ToString(), *Row.SkillName.ToString(), Row.Montages.Num());
		Rows.Add(RowName, Row);
	}
	if (bApply)
	{
		Backup(TablePath, BackupRoot);
		UDataTable* Table = FindOrCreate<UDataTable>(TablePath);
		Table->RowStruct = FWuwaSkillInfo::StaticStruct();
		Table->EmptyTable();
		for (const TPair<FName, FWuwaSkillInfo>& Pair : Rows) Table->AddRow(Pair.Key, Pair.Value);
		Table->MarkPackageDirty();
		ToSave.AddUnique(Table->GetOutermost());
	}

	if (!bApply)
	{
		UE_LOG(LogWuwaBossSetup, Display, TEXT("Read-only; pass -Apply to write."));
		return Problems ? 1 : 0;
	}
	if (!UEditorLoadingAndSavingUtils::SavePackages(ToSave, false))
	{
		UE_LOG(LogWuwaBossSetup, Error, TEXT("Saving failed"));
		return 1;
	}
	UE_LOG(LogWuwaBossSetup, Display, TEXT("Saved %d packages; backups (if any) in %s"), ToSave.Num(), *BackupRoot);
	return Problems ? 1 : 0;
}
