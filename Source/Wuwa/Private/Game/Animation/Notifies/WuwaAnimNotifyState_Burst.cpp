#include "Game/Animation/Notifies/WuwaAnimNotifyState_Burst.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/Tag/WuwaGameTags.h"

void UWuwaAnimNotifyState_Burst::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	Change(MeshComp, 1);
}

void UWuwaAnimNotifyState_Burst::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Change(MeshComp, -1);
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

void UWuwaAnimNotifyState_Burst::Change(const USkeletalMeshComponent* MeshComp, int32 Delta) const
{
	UAbilitySystemComponent* ASC = MeshComp ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(MeshComp->GetOwner()) : nullptr;
	if (!ASC) return;
	const FWuwaGameTags& Tags = FWuwaGameTags::Get();
	// 松散 Tag 按计数加减（和 GE 授予的同名 Tag 互不影响）
	auto Apply = [ASC, Delta](const FGameplayTag& Tag)
	{
		if (Delta > 0) ASC->AddLooseGameplayTag(Tag);
		else ASC->RemoveLooseGameplayTag(Tag);
	};
	if (bForbidSwitch) Apply(Tags.State_Formation_SwitchForbid);
	if (bInvincible) Apply(Tags.State_Combat_Invincible);
}
