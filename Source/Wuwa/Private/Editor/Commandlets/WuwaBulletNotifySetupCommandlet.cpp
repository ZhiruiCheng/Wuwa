#include "Editor/Commandlets/WuwaBulletNotifySetupCommandlet.h"
#include "Editor/WuwaAnimNotifyEditorUtils.h"

#if WITH_EDITOR
#include "Animation/AnimMontage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Game/Animation/Notifies/WuwaAnimNotify_SpawnBullet.h"
#include "Game/NewWorld/Bullet/WuwaBulletTypes.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillTypes.h"
#include "Game/NewWorld/Character/Common/WuwaCharactorBase.h"
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

DEFINE_LOG_CATEGORY_STATIC(LogWuwaBulletNotifySetup, Log, All);

UWuwaBulletNotifySetupCommandlet::UWuwaBulletNotifySetupCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
	HelpDescription = TEXT("Installs Spawn Bullet notifies on project montages from ArtSource/Bullet/<character>.bullet_notifies.json. Read-only unless -Apply.");
	HelpUsage = TEXT("-run=WuwaBulletNotifySetup -Character=changli|jiabei [-Apply]");
}

#if WITH_EDITOR
namespace WuwaBulletNotifySetup
{
	const FName TrackName(TEXT("Bullet"));

	/** 允许的 项目时长 / 原作时长：同长，或 24 帧导入的 30 帧原作动画整段拉长 1.25 倍。 */
	constexpr double AllowedRatios[] = { 1.0, 1.25 };
	constexpr double RatioTolerance = 0.04;  // 嘉贝 Attack_H_0201 导出只有 86 帧（原作 89 帧，2.833 / 2.930 = 0.967），只差末尾 0.1 秒：按 1:1 原时间装

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
			UE_LOG(LogWuwaBulletNotifySetup, Error, TEXT("Could not save %s (is the editor open with it loaded?)"), *Filename);
			return false;
		}
		UE_LOG(LogWuwaBulletNotifySetup, Display, TEXT("Saved %s"), *Package->GetName());
		return true;
	}

	/** 蒙太奇在 <角色目录>/AnimMontage/ 下；角色目录里继承 AWuwaCharactorBase 的蓝图（取它的子弹表和技能表）。 */
	const AWuwaCharactorBase* FindCharacterDefaults(const FString& MontagePackage)
	{
		const FString RoleFolder = FPackageName::GetLongPackagePath(FPackageName::GetLongPackagePath(MontagePackage));
		IAssetRegistry& Registry = FAssetRegistryModule::GetRegistry();
		Registry.ScanPathsSynchronous({RoleFolder}, false);
		TArray<FAssetData> Assets;
		Registry.GetAssetsByPath(*RoleFolder, Assets, false);
		for (const FAssetData& Asset : Assets)
		{
			if (Asset.AssetClassPath != UBlueprint::StaticClass()->GetClassPathName()) continue;
			const UBlueprint* Blueprint = Cast<UBlueprint>(Asset.GetAsset());
			if (const AWuwaCharactorBase* Defaults = Blueprint && Blueprint->GeneratedClass ? Cast<AWuwaCharactorBase>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr)
			{
				return Defaults;
			}
		}
		return nullptr;
	}

	/** 技能表里 Montages 含这个蒙太奇的技能 ID。 */
	TArray<int32> SkillsUsing(const UDataTable* SkillTable, const UAnimMontage* Montage)
	{
		TArray<int32> Skills;
		if (!SkillTable || SkillTable->GetRowStruct() != FWuwaSkillInfo::StaticStruct()) return Skills;
		for (const TPair<FName, uint8*>& Pair : SkillTable->GetRowMap())
		{
			const FWuwaSkillInfo* Info = reinterpret_cast<const FWuwaSkillInfo*>(Pair.Value);
			if (Info->Montages.ContainsByPredicate([Montage](const TSoftObjectPtr<UAnimMontage>& Ref) { return Ref.ToSoftObjectPath() == FSoftObjectPath(Montage); }))
			{
				Skills.Add(FCString::Atoi(*Pair.Key.ToString()));
			}
		}
		return Skills;
	}
}
#endif

int32 UWuwaBulletNotifySetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaBulletNotifySetup;
	FString Character;
	if (!FParse::Value(*Params, TEXT("Character="), Character) || Character.IsEmpty())
	{
		UE_LOG(LogWuwaBulletNotifySetup, Error, TEXT("Usage: %s"), *HelpUsage);
		return 1;
	}
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	const FString RecipePath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("ArtSource/Bullet") / (Character + TEXT(".bullet_notifies.json")));
	FString Text;
	TSharedPtr<FJsonObject> Recipe;
	if (!FFileHelper::LoadFileToString(Text, *RecipePath) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Recipe) || !Recipe)
	{
		UE_LOG(LogWuwaBulletNotifySetup, Error, TEXT("Cannot read %s (run ArtSource/Bullet/prepare_bullet_notifies.py --character %s first)."), *RecipePath, *Character);
		return 1;
	}

	// 按项目蒙太奇分组（保持配方顺序）
	TArray<FString> Order;
	TMap<FString, TArray<TSharedPtr<FJsonObject>>> ByAsset;
	for (const TSharedPtr<FJsonValue>& Value : Recipe->GetArrayField(TEXT("notifies")))
	{
		const TSharedPtr<FJsonObject> Entry = Value->AsObject();
		const FString Asset = Entry->GetStringField(TEXT("asset"));
		if (!ByAsset.Contains(Asset)) Order.Add(Asset);
		ByAsset.FindOrAdd(Asset).Add(Entry);
	}
	if (Order.IsEmpty())
	{
		UE_LOG(LogWuwaBulletNotifySetup, Error, TEXT("%s has no notifies."), *RecipePath);
		return 1;
	}
	const AWuwaCharactorBase* Defaults = FindCharacterDefaults(Order[0]);
	const UDataTable* BulletTable = Defaults ? Defaults->BulletTable.Get() : nullptr;
	if (!BulletTable)
	{
		UE_LOG(LogWuwaBulletNotifySetup, Error, TEXT("No character blueprint with a BulletTable next to %s (run WuwaBulletTableSetup -AssignOnly -Apply first)."), *Order[0]);
		return 1;
	}
	UE_LOG(LogWuwaBulletNotifySetup, Display, TEXT("Character %s, bullet table %s, skill table %s"), *GetNameSafe(Defaults->GetClass()),
		*BulletTable->GetName(), *GetNameSafe(Defaults->SkillTable));

	const FString BackupRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/BulletNotifySetup")
		/ FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	int32 Errors = 0, Warnings = 0, Installed = 0, Removed = 0;
	for (const FString& AssetPath : Order)
	{
		const TArray<TSharedPtr<FJsonObject>>& Entries = ByAsset[AssetPath];
		UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *AssetPath);
		if (!Montage)
		{
			UE_LOG(LogWuwaBulletNotifySetup, Error, TEXT("Missing montage %s"), *AssetPath);
			++Errors;
			continue;
		}
		// 时长核对：原作秒数 × 比值 = 项目秒数
		const double SourceLength = Entries[0]->GetNumberField(TEXT("sourceLength"));
		const double RawRatio = Montage->GetPlayLength() / SourceLength;
		double Ratio = 0.0;
		for (const double Allowed : AllowedRatios)
		{
			if (FMath::Abs(RawRatio - Allowed) <= RatioTolerance) { Ratio = Allowed; break; }
		}
		// 循环段（下落循环等）项目只截一个周期，时长对不上：通知都落在截出的这段里就按 1:1 原时间装
		if (Ratio == 0.0 && Entries[0]->GetStringField(TEXT("sourceAsset")).EndsWith(TEXT("_Loop")))
		{
			bool bAllInside = true;
			for (const TSharedPtr<FJsonObject>& Entry : Entries) bAllInside &= Entry->GetNumberField(TEXT("time")) < Montage->GetPlayLength();
			if (bAllInside) Ratio = 1.0;
		}
		if (Ratio == 0.0)
		{
			UE_LOG(LogWuwaBulletNotifySetup, Error, TEXT("%s is %.3fs but original %s is %.3fs (ratio %.3f); not installed."), *Montage->GetName(),
				Montage->GetPlayLength(), *Entries[0]->GetStringField(TEXT("sourceAsset")), SourceLength, RawRatio);
			++Errors;
			continue;
		}

		// 子弹 ID 在表里、所属技能与技能表一致
		const TArray<int32> Skills = SkillsUsing(Defaults->SkillTable, Montage);
		bool bMontageOk = true;
		for (const TSharedPtr<FJsonObject>& Entry : Entries)
		{
			TArray<FString> Ids;
			for (const TSharedPtr<FJsonValue>& Id : Entry->GetArrayField(TEXT("bulletIds"))) Ids.Add(Id->AsString());
			const double Time = Entry->GetNumberField(TEXT("time")) * Ratio;
			UE_LOG(LogWuwaBulletNotifySetup, Display, TEXT("%s @ %.3fs: %s (%s @ %.3fs%s)"), *Montage->GetName(), Time, *FString::Join(Ids, TEXT(", ")),
				*Entry->GetStringField(TEXT("sourceAsset")), Entry->GetNumberField(TEXT("time")), Ratio == 1.0 ? TEXT("") : *FString::Printf(TEXT(" x%.2f"), Ratio));
			for (const FString& Id : Ids)
			{
				if (!BulletTable->FindRowUnchecked(FName(*Id)))
				{
					UE_LOG(LogWuwaBulletNotifySetup, Error, TEXT("  bullet %s is not in %s"), *Id, *BulletTable->GetName());
					bMontageOk = false;
					continue;
				}
				const int32 BulletSkill = WuwaBullet::GetSkillId(WuwaBullet::ParseBulletId(FName(*Id)));
				if (!Skills.Contains(BulletSkill))
				{
					UE_LOG(LogWuwaBulletNotifySetup, Warning, TEXT("  bullet %s belongs to skill %d, but the skill table uses %s for [%s]"), *Id, BulletSkill,
						*Montage->GetName(), *FString::JoinBy(Skills, TEXT(", "), [](int32 Skill) { return FString::FromInt(Skill); }));
					++Warnings;
				}
			}
		}
		if (!bMontageOk)
		{
			++Errors;
			continue;
		}
		if (!bApply) continue;
		if (!Backup(Montage, BackupRoot))
		{
			UE_LOG(LogWuwaBulletNotifySetup, Error, TEXT("Could not back up %s"), *Montage->GetPathName());
			return 1;
		}

		Montage->Modify();
		Removed += Montage->Notifies.RemoveAll([](const FAnimNotifyEvent& Event) { return Event.Notify && Event.Notify->IsA<UWuwaAnimNotify_SpawnBullet>(); });
		int32 Track = Montage->AnimNotifyTracks.IndexOfByPredicate([](const FAnimNotifyTrack& T) { return T.TrackName == TrackName; });
		if (Track == INDEX_NONE)
		{
			Track = Montage->AnimNotifyTracks.AddDefaulted();
			Montage->AnimNotifyTracks[Track].TrackName = TrackName;
			Montage->AnimNotifyTracks[Track].TrackColor = FLinearColor(1.f, .3f, .2f);
		}
		for (const TSharedPtr<FJsonObject>& Entry : Entries)
		{
			UWuwaAnimNotify_SpawnBullet* Spawn = NewObject<UWuwaAnimNotify_SpawnBullet>(Montage, NAME_None, RF_Transactional);
			for (const TSharedPtr<FJsonValue>& Id : Entry->GetArrayField(TEXT("bulletIds"))) Spawn->BulletIds.Add(FName(*Id->AsString()));
			// 原作 TsAnimNotifySkillBehavior 的条件（普通 添加子弹 通知没有这些字段）
			FString TargetCondition;
			if (Entry->TryGetStringField(TEXT("skillTarget"), TargetCondition))
			{
				Spawn->SkillTargetCondition = TargetCondition == TEXT("HasTarget") ? EWuwaBulletSkillTargetCondition::HasTarget
					: TargetCondition == TEXT("NoTarget") ? EWuwaBulletSkillTargetCondition::NoTarget : EWuwaBulletSkillTargetCondition::Any;
			}
			auto ReadTags = [&Entry](const TCHAR* Field, FGameplayTagContainer& Out)
			{
				const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
				if (!Entry->TryGetArrayField(Field, Values)) return;
				for (const TSharedPtr<FJsonValue>& Value : *Values)
				{
					const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*Value->AsString()), false);
					if (Tag.IsValid()) Out.AddTag(Tag);
				}
			};
			ReadTags(TEXT("requiredTags"), Spawn->RequiredOwnerTags);
			ReadTags(TEXT("blockedTags"), Spawn->BlockedOwnerTags);
			FAnimNotifyEvent& Notify = Montage->Notifies.AddDefaulted_GetRef();
			Notify.TrackIndex = Track;
			Notify.NotifyTriggerChance = 1.f;
			Notify.TriggerTimeOffset = 0.f;
			Notify.Guid = FGuid::NewGuid();
			Notify.NotifyName = FName(*Spawn->GetNotifyName());
			Notify.Notify = Spawn;
			Notify.Link(Montage, static_cast<float>(Entry->GetNumberField(TEXT("time")) * Ratio));
			++Installed;
		}
		Montage->SortNotifies();
		WuwaAnimNotifyEditor::RefreshTriggerOffsets(Montage);
		Montage->RefreshCacheData();
		if (!Save(Montage)) return 1;
	}
	UE_LOG(LogWuwaBulletNotifySetup, Display, TEXT("%s: %d notifies on %d montages, %d errors, %d warnings%s"), *Character,
		bApply ? Installed : Recipe->GetArrayField(TEXT("notifies")).Num(), Order.Num(), Errors, Warnings,
		bApply ? *FString::Printf(TEXT(" (%d old Spawn Bullet notifies replaced; backups in %s)"), Removed, *BackupRoot) : TEXT(" — audit only, run with -Apply"));
	return Errors > 0 ? 1 : 0;
#else
	return 1;
#endif
}
