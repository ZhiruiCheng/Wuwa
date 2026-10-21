// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAbilitySystemComponent.h"

#include "Game/Input/WuwaInputTypes.h"
#include "Core/Utilities/DebugHelper.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"

UWuwaGameplayAbilityBase* UWuwaAbilitySystemComponent::GetAnimatingWuwaAbility() const
{
	return Cast<UWuwaGameplayAbilityBase>(GetAnimatingAbility());
}

//当有 GameplayEffect 被成功应用到自己身上时，就调用 EffectApplied
void UWuwaAbilitySystemComponent::InitAbilitySystemCompoent()
{
	//仅仅在服务器生效
	OnGameplayEffectAppliedDelegateToSelf.AddUObject(this,&UWuwaAbilitySystemComponent::EffectApplied);
}

void UWuwaAbilitySystemComponent::EffectApplied(UAbilitySystemComponent* ASC, const FGameplayEffectSpec& EffectSpec,FActiveGameplayEffectHandle GameplayEffectHandle)
{
	AttributeEffectAppliedDelegate.Broadcast(EffectSpec);
}

#pragma region Skill

FGameplayAbilitySpecHandle UWuwaAbilitySystemComponent::GiveSkill(int32 SkillId, const FWuwaSkillInfo& Info, UObject* SourceObject)
{
	if (SkillId <= 0 || !Info.Ability || GrantedSkills.Contains(SkillId)) return FGameplayAbilitySpecHandle();
	const FGameplayAbilitySpecHandle Handle = GiveAbility(FGameplayAbilitySpec(Info.Ability, 1, INDEX_NONE, SourceObject));
	if (Handle.IsValid()) GrantedSkills.Add(SkillId, { Handle, Info.SkillTags });
	return Handle;
}

TArray<FGameplayAbilitySpecHandle> UWuwaAbilitySystemComponent::FindSkillHandles(int32 SkillId) const
{
	TArray<FGameplayAbilitySpecHandle> Handles;
	const FGrantedSkill* Skill = GrantedSkills.Find(SkillId);
	const FGameplayAbilitySpec* Spec = Skill ? FindAbilitySpecFromHandle(Skill->Handle) : nullptr;
	if (Spec && Spec->Ability && !Spec->PendingRemove && !Spec->RemoveAfterActivation) Handles.Add(Skill->Handle);
	return Handles;
}

int32 UWuwaAbilitySystemComponent::GetSkillIdForHandle(FGameplayAbilitySpecHandle AbilityHandle) const
{
	if (!AbilityHandle.IsValid()) return 0;
	for (const TPair<int32, FGrantedSkill>& Pair : GrantedSkills)
	{
		if (Pair.Value.Handle == AbilityHandle) return Pair.Key;
	}
	return 0;
}

FGameplayTagContainer UWuwaAbilitySystemComponent::GetSkillTags(int32 SkillId) const
{
	const FGrantedSkill* Skill = GrantedSkills.Find(SkillId);
	return Skill ? Skill->Tags : FGameplayTagContainer();
}

bool UWuwaAbilitySystemComponent::SkillMatchesTagQuery(int32 SkillId, const FGameplayTagQuery& Query) const
{
	if (Query.IsEmpty()) return true;
	const FGrantedSkill* Skill = GrantedSkills.Find(SkillId);
	return Skill && Query.Matches(Skill->Tags);
}

void UWuwaAbilitySystemComponent::OnRemoveAbility(FGameplayAbilitySpec& AbilitySpec)
{
	for (auto It = GrantedSkills.CreateIterator(); It; ++It)
	{
		if (It.Value().Handle == AbilitySpec.Handle) It.RemoveCurrent();
	}
	Super::OnRemoveAbility(AbilitySpec);
}

#pragma endregion

#pragma region GA

//判断这个 ASC 当前身上拥有的 GameplayTag，是否满足给定的 Tag 查询条件
bool UWuwaAbilitySystemComponent::MatchesOwnedTagQuery(const FGameplayTagQuery& Query) const
{
	if (Query.IsEmpty()) return true;
	FGameplayTagContainer OwnedTags;
	GetOwnedGameplayTags(OwnedTags);
	return Query.Matches(OwnedTags);
}

//检查当前技能是否激活,是不是可激活的技能
bool UWuwaAbilitySystemComponent::IsSpecAvailableForActivation(FGameplayAbilitySpecHandle AbilityHandle) const
{
	const FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(AbilityHandle);
	const UWuwaGameplayAbilityBase* Ability = GetAbilityForInput(AbilityHandle);
	return Spec && IsValid(Ability) && (!Spec->IsActive()|| (Ability->bIsMainSkill && Ability->GetInstancingPolicy() == EGameplayAbilityInstancingPolicy::InstancedPerExecution));
}

UWuwaGameplayAbilityBase* UWuwaAbilitySystemComponent::GetAbilityForInput(FGameplayAbilitySpecHandle AbilityHandle) const
{
	const FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(AbilityHandle);
	return Spec && !Spec->PendingRemove && !Spec->RemoveAfterActivation ? Cast<UWuwaGameplayAbilityBase>(Spec->Ability) : nullptr;
}


//其实就是返回角色的Avatar
AWuwaCharacter* UWuwaAbilitySystemComponent::GetInputAvatar() const
{
	return Cast<AWuwaCharacter>(GetAvatarActor());
}

//其实就是返回世界时间
double UWuwaAbilitySystemComponent::GetInputTimeSeconds() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : -1.0;
}

//请求激活AbilityHandle的这个GA
EWuwaAbilityRequestResult UWuwaAbilitySystemComponent::RequestAbilityActivation(FGameplayAbilitySpecHandle AbilityHandle)
{
	FScopedAbilityListLock AbilityListLock(*this);
	FGameplayAbilitySpec* Spec =FindAbilitySpecFromHandle(AbilityHandle);

	if (!Spec|| !Spec->Ability|| Spec->PendingRemove|| Spec->RemoveAfterActivation){return EWuwaAbilityRequestResult::InvalidHandle;}
	
	if (Spec->IsActive() && !IsSpecAvailableForActivation(AbilityHandle)){return EWuwaAbilityRequestResult::AlreadyActive;}

	return TryActivateAbility(AbilityHandle)? EWuwaAbilityRequestResult::ActivationRequested: EWuwaAbilityRequestResult::Rejected;
}

#pragma endregion

