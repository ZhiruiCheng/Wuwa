#include "Game/NewWorld/Buff/WuwaBuffLibrary.h"

#include "AbilitySystemComponent.h"
#include "Engine/DataTable.h"
#include "GameplayEffect.h"
#include "Game/NewWorld/Buff/WuwaBuffTypes.h"
#include "Game/NewWorld/Combat/WuwaCombatSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaBuff, Log, All);

namespace WuwaBuff
{
	/** 表在加载界面的预热阶段已经加载（UWuwaCombatWarmupSubsystem），这里通常只是取内存里的对象。 */
	const UDataTable* Table(const TSoftObjectPtr<UDataTable>& Soft, const UScriptStruct* RowStruct)
	{
		const UDataTable* Loaded = Soft.LoadSynchronous();
		return Loaded && Loaded->GetRowStruct() == RowStruct ? Loaded : nullptr;
	}

	FName RowName(int64 Id) { return FName(*LexToString(Id)); }
}

TSubclassOf<UGameplayEffect> UWuwaBuffLibrary::FindBuffEffect(int64 BuffId)
{
	const UDataTable* Buffs = WuwaBuff::Table(GetDefault<UWuwaCombatSettings>()->BuffTable, FWuwaBuffRow::StaticStruct());
	const FWuwaBuffRow* Row = Buffs && BuffId != 0 ? Buffs->FindRow<FWuwaBuffRow>(WuwaBuff::RowName(BuffId), TEXT("WuwaBuff"), false) : nullptr;
	return Row ? Row->Effect : nullptr;
}

FActiveGameplayEffectHandle UWuwaBuffLibrary::AddBuff(UAbilitySystemComponent* Target, int64 BuffId, UAbilitySystemComponent* Instigator)
{
	if (!Target || !Target->IsOwnerActorAuthoritative()) return FActiveGameplayEffectHandle();
	const TSubclassOf<UGameplayEffect> Effect = FindBuffEffect(BuffId);
	if (!Effect)
	{
		UE_LOG(LogWuwaBuff, Warning, TEXT("Buff %lld is not in DT_Buff; not added to %s."), BuffId, *GetNameSafe(Target->GetOwner()));
		return FActiveGameplayEffectHandle();
	}
	UAbilitySystemComponent* Source = Instigator ? Instigator : Target;
	const FGameplayEffectSpecHandle Spec = Source->MakeOutgoingSpec(Effect, 1.f, Source->MakeEffectContext());
	if (!Spec.IsValid()) return FActiveGameplayEffectHandle();
	UE_LOG(LogWuwaBuff, Verbose, TEXT("Add buff %lld (%s) to %s"), BuffId, *GetNameSafe(Effect), *GetNameSafe(Target->GetOwner()));
	return Source->ApplyGameplayEffectSpecToTarget(*Spec.Data, Target);
}

int32 UWuwaBuffLibrary::RemoveBuff(UAbilitySystemComponent* Target, int64 BuffId, int32 StackCount)
{
	const TSubclassOf<UGameplayEffect> Effect = FindBuffEffect(BuffId);
	if (!Target || !Target->IsOwnerActorAuthoritative() || !Effect) return 0;
	FGameplayEffectQuery Query;
	Query.EffectDefinition = Effect;
	const TArray<FActiveGameplayEffectHandle> Handles = Target->GetActiveEffects(Query);
	for (const FActiveGameplayEffectHandle& Handle : Handles) Target->RemoveActiveGameplayEffect(Handle, StackCount);
	return Handles.Num();
}

bool UWuwaBuffLibrary::HasBuff(const UAbilitySystemComponent* Target, int64 BuffId)
{
	const TSubclassOf<UGameplayEffect> Effect = FindBuffEffect(BuffId);
	if (!Target || !Effect) return false;
	FGameplayEffectQuery Query;
	Query.EffectDefinition = Effect;
	return Target->GetActiveEffects(Query).Num() > 0;
}

const UWuwaBuffInfoComponent* UWuwaBuffLibrary::GetBuffInfo(const UGameplayEffect* Effect)
{
	return Effect ? Effect->FindComponent<UWuwaBuffInfoComponent>() : nullptr;
}

int64 UWuwaBuffLibrary::GetBuffId(const FActiveGameplayEffect& Effect)
{
	const UWuwaBuffInfoComponent* Info = GetBuffInfo(Effect.Spec.Def);
	return Info ? Info->BuffId : 0;
}

const FWuwaBuffCueRow* UWuwaBuffLibrary::FindCue(int64 CueId)
{
	const UDataTable* Cues = WuwaBuff::Table(GetDefault<UWuwaCombatSettings>()->BuffCueTable, FWuwaBuffCueRow::StaticStruct());
	return Cues && CueId != 0 ? Cues->FindRow<FWuwaBuffCueRow>(WuwaBuff::RowName(CueId), TEXT("WuwaBuffCue"), false) : nullptr;
}

bool UWuwaBuffLibrary::K2_AddBuff(UAbilitySystemComponent* Target, int64 BuffId)
{
	return AddBuff(Target, BuffId).IsValid();
}

int32 UWuwaBuffLibrary::K2_RemoveBuff(UAbilitySystemComponent* Target, int64 BuffId, int32 StackCount)
{
	return RemoveBuff(Target, BuffId, StackCount);
}

bool UWuwaBuffLibrary::K2_HasBuff(const UAbilitySystemComponent* Target, int64 BuffId)
{
	return HasBuff(Target, BuffId);
}
