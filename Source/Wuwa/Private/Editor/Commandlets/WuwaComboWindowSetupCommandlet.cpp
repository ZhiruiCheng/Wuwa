#include "Editor/Commandlets/WuwaComboWindowSetupCommandlet.h"
#include "Editor/WuwaAnimNotifyEditorUtils.h"

#if WITH_EDITOR
#include "Animation/AnimMontage.h"
#include "Engine/Blueprint.h"
#include "Engine/DataTable.h"
#include "Game/Animation/Notifies/WuwaAnimNotifyState_AddLooseTag.h"
#include "Game/Animation/Notifies/WuwaAnimNotify_SkillReadyEndBridge.h"
#include "Game/Common/WuwaGameTags.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputCommandConfig.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillTypes.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaComboWindowSetup, Log, All);

UWuwaComboWindowSetupCommandlet::UWuwaComboWindowSetupCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
	HelpDescription = TEXT("Adds State.Combat.ComboWindow windows to a role's attack montages and requires the tag on its combo rules. Read-only unless -Apply.");
	HelpUsage = TEXT("-run=WuwaComboWindowSetup -Character=changli|jiabei [-After=0.6] [-Apply]");
}

#if WITH_EDITOR
namespace WuwaComboWindowSetup
{
	const FName TrackName(TEXT("Combo"));

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
			UE_LOG(LogWuwaComboWindowSetup, Error, TEXT("Could not save %s (is the editor open with it loaded?)"), *Filename);
			return false;
		}
		UE_LOG(LogWuwaComboWindowSetup, Display, TEXT("Saved %s"), *Package->GetName());
		return true;
	}

	float ReadyEndTime(const UAnimMontage* Montage)
	{
		for (const FAnimNotifyEvent& Event : Montage->Notifies)
		{
			if (Event.Notify && Event.Notify->IsA<UWuwaAnimNotify_SkillReadyEndBridge>()) return Event.GetTime();
		}
		return -1.f;
	}

	const FAnimNotifyEvent* FindWindow(const UAnimMontage* Montage, const FGameplayTag& Tag)
	{
		return Montage->Notifies.FindByPredicate([&Tag](const FAnimNotifyEvent& Event)
		{
			const UWuwaAnimNotifyState_AddLooseTag* Window = Cast<UWuwaAnimNotifyState_AddLooseTag>(Event.NotifyStateClass);
			return Window && Window->Tag == Tag;
		});
	}

	FString Describe(const FGameplayTagQuery& Query)
	{
		return Query.IsEmpty() ? TEXT("-") : Query.GetDescription().IsEmpty() ? TEXT("(set)") : Query.GetDescription();
	}
}
#endif

int32 UWuwaComboWindowSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaComboWindowSetup;
	FString Character;
	if (!FParse::Value(*Params, TEXT("Character="), Character) || Character.IsEmpty())
	{
		UE_LOG(LogWuwaComboWindowSetup, Error, TEXT("Usage: %s"), *HelpUsage);
		return 1;
	}
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	float After = 0.6f;
	FParse::Value(*Params, TEXT("After="), After);
	const FGameplayTag ComboTag = FGameplayTag::RequestGameplayTag(TEXT("State.Combat.ComboWindow"));
	const FGameplayTag AttackInput = FWuwaGameTags::Get().Input_Combat_Attack;

	// 角色蓝图：/Game/Characters/Role/<角色>/BP_Role_<角色>
	const FString Title = Character.Left(1).ToUpper() + Character.Mid(1);
	const FString BlueprintPath = FString::Printf(TEXT("/Game/Characters/Role/%s/BP_Role_%s.BP_Role_%s_C"), *Character, *Title, *Title);
	UClass* RoleClass = LoadClass<AWuwaCharacter>(nullptr, *BlueprintPath);
	if (!RoleClass)
	{
		const FString Lower = FString::Printf(TEXT("/Game/Characters/Role/%s/BP_Role_%s.BP_Role_%s_C"), *Character, *Character, *Character);
		RoleClass = LoadClass<AWuwaCharacter>(nullptr, *Lower);
	}
	const AWuwaCharacter* Defaults = RoleClass ? RoleClass->GetDefaultObject<AWuwaCharacter>() : nullptr;
	UWuwaInputCommandConfig* Config = Defaults ? Defaults->InputCommandConfig.Get() : nullptr;
	const UDataTable* SkillTable = Defaults ? Defaults->SkillTable.Get() : nullptr;
	if (!Config || !SkillTable)
	{
		UE_LOG(LogWuwaComboWindowSetup, Error, TEXT("Character blueprint for %s with InputCommandConfig and SkillTable not found."), *Character);
		return 1;
	}
	UE_LOG(LogWuwaComboWindowSetup, Display, TEXT("%s: input %s, skills %s"), *RoleClass->GetName(), *Config->GetName(), *SkillTable->GetName());

	auto MontageOf = [SkillTable](int32 SkillId) -> UAnimMontage*
	{
		const FWuwaSkillInfo* Info = SkillTable->FindRow<FWuwaSkillInfo>(FName(*FString::FromInt(SkillId)), TEXT("ComboWindowSetup"), false);
		return Info && Info->Montages.Num() > 0 ? Info->Montages[0].LoadSynchronous() : nullptr;
	};

	// 输入规则一览；接段规则 = 攻击输入且要求当前技能
	TArray<int32> ComboRules;
	TArray<UAnimMontage*> SourceMontages;
	for (int32 Index = 0; Index < Config->Rules.Num(); ++Index)
	{
		const FWuwaInputCommandRule& Rule = Config->Rules[Index];
		const bool bCombo = Rule.InputTag == AttackInput && Rule.RequiredCurrentSkillId != 0;
		UE_LOG(LogWuwaComboWindowSetup, Display, TEXT("  rule %d %s: input %s -> %d, requires current %d, current tags %s, owner tags %s%s"), Index,
			*Rule.RuleName.ToString(), *Rule.InputTag.ToString(), Rule.TargetSkillId, Rule.RequiredCurrentSkillId,
			*Describe(Rule.CurrentSkillTagQuery), *Describe(Rule.OwnerTagQuery), bCombo ? TEXT("  [combo]") : TEXT(""));
		if (!bCombo) continue;
		ComboRules.Add(Index);
		if (UAnimMontage* Source = MontageOf(Rule.RequiredCurrentSkillId)) SourceMontages.AddUnique(Source);
		else UE_LOG(LogWuwaComboWindowSetup, Warning, TEXT("    skill %d has no montage in %s"), Rule.RequiredCurrentSkillId, *SkillTable->GetName());
	}

	// 蒙太奇窗口
	int32 Errors = 0;
	TArray<TPair<UAnimMontage*, float>> NewWindows;
	for (UAnimMontage* Montage : SourceMontages)
	{
		const float Ready = ReadyEndTime(Montage);
		if (const FAnimNotifyEvent* Existing = FindWindow(Montage, ComboTag))
		{
			UE_LOG(LogWuwaComboWindowSetup, Display, TEXT("  %s: ReadyEnd %.3f, window %.3f-%.3f (kept)"), *Montage->GetName(), Ready,
				Existing->GetTime(), Existing->GetTime() + Existing->GetDuration());
			continue;
		}
		if (Ready < 0.f)
		{
			UE_LOG(LogWuwaComboWindowSetup, Error, TEXT("  %s has no ReadyEnd notify; cannot place the window."), *Montage->GetName());
			++Errors;
			continue;
		}
		const float End = FMath::Min(Ready + After, Montage->GetPlayLength());
		UE_LOG(LogWuwaComboWindowSetup, Display, TEXT("  %s: ReadyEnd %.3f of %.3f, new window 0-%.3f"), *Montage->GetName(), Ready, Montage->GetPlayLength(), End);
		NewWindows.Emplace(Montage, End);
	}
	TArray<int32> RulesToChange;
	for (const int32 Index : ComboRules)
	{
		const FGameplayTagQuery& Query = Config->Rules[Index].OwnerTagQuery;
		if (Query.IsEmpty()) RulesToChange.Add(Index);
		else if (!Query.Matches(FGameplayTagContainer(ComboTag)) || Query.Matches(FGameplayTagContainer()))
		{
			UE_LOG(LogWuwaComboWindowSetup, Warning, TEXT("  rule %s already has another owner tag query; not changed."), *Config->Rules[Index].RuleName.ToString());
		}
	}
	UE_LOG(LogWuwaComboWindowSetup, Display, TEXT("%s: %d combo rules (%d to require %s), %d montages (%d new windows), %d errors%s"), *Character,
		ComboRules.Num(), RulesToChange.Num(), *ComboTag.ToString(), SourceMontages.Num(), NewWindows.Num(), Errors, bApply ? TEXT("") : TEXT(" — audit only, run with -Apply"));
	if (!bApply || Errors > 0) return Errors > 0 ? 1 : 0;

	const FString BackupRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/ComboWindowSetup") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	for (const TPair<UAnimMontage*, float>& Pair : NewWindows)
	{
		UAnimMontage* Montage = Pair.Key;
		if (!Backup(Montage, BackupRoot)) return 1;
		Montage->Modify();
		int32 Track = Montage->AnimNotifyTracks.IndexOfByPredicate([](const FAnimNotifyTrack& T) { return T.TrackName == TrackName; });
		if (Track == INDEX_NONE)
		{
			Track = Montage->AnimNotifyTracks.AddDefaulted();
			Montage->AnimNotifyTracks[Track].TrackName = TrackName;
			Montage->AnimNotifyTracks[Track].TrackColor = FLinearColor(.3f, 1.f, .4f);
		}
		UWuwaAnimNotifyState_AddLooseTag* Window = NewObject<UWuwaAnimNotifyState_AddLooseTag>(Montage, NAME_None, RF_Transactional);
		Window->Tag = ComboTag;
		FAnimNotifyEvent& Notify = Montage->Notifies.AddDefaulted_GetRef();
		Notify.TrackIndex = Track;
		Notify.NotifyTriggerChance = 1.f;
		Notify.Guid = FGuid::NewGuid();
		Notify.NotifyName = FName(*Window->GetNotifyName());
		Notify.NotifyStateClass = Window;
		// 和嘉贝的窗口一样用 BranchingPoint：开始 / 结束不会因为帧率跳过
		Notify.MontageTickType = EMontageNotifyTickType::BranchingPoint;
		Notify.Link(Montage, 0.f);
		Notify.SetDuration(Pair.Value);
		Notify.EndLink.Link(Montage, Pair.Value);
		Montage->SortNotifies();
		WuwaAnimNotifyEditor::RefreshTriggerOffsets(Montage);
		Montage->RefreshCacheData();
		if (!Save(Montage)) return 1;
	}
	if (RulesToChange.Num() > 0)
	{
		if (!Backup(Config, BackupRoot)) return 1;
		Config->Modify();
		for (const int32 Index : RulesToChange)
		{
			Config->Rules[Index].OwnerTagQuery = FGameplayTagQuery::MakeQuery_MatchTag(ComboTag);
		}
		if (!Save(Config)) return 1;
	}
	UE_LOG(LogWuwaComboWindowSetup, Display, TEXT("Backups in %s"), *BackupRoot);
	return 0;
#else
	return 1;
#endif
}
