#include "Game/Animation/Notifies/WuwaAnimNotifyState_AddLooseTag.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"

void UWuwaAnimNotifyState_AddLooseTag::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	Change(MeshComp, true);
}

void UWuwaAnimNotifyState_AddLooseTag::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Change(MeshComp, false);
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UWuwaAnimNotifyState_AddLooseTag::GetNotifyName_Implementation() const
{
	return Tag.IsValid() ? FString::Printf(TEXT("Tag %s"), *Tag.ToString()) : TEXT("Add Loose Tag");
}

void UWuwaAnimNotifyState_AddLooseTag::Change(const USkeletalMeshComponent* MeshComp, bool bAdd) const
{
	// 编辑器预览网格没有 ASC，直接忽略
	if (!Tag.IsValid() || !MeshComp) return;
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(MeshComp->GetOwner());
	if (!ASC) return;
	if (bAdd) ASC->AddLooseGameplayTag(Tag);
	else if (ASC->GetTagCount(Tag) > 0) ASC->RemoveLooseGameplayTag(Tag);
}
