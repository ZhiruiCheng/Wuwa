#include "Game/Animation/Notifies/WuwaAnimNotify_AddBuff.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/SkeletalMeshComponent.h"
#include "Game/NewWorld/Buff/WuwaBuffLibrary.h"

FString UWuwaAnimNotify_AddBuff::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Add Buff %lld"), BuffId);
}

void UWuwaAnimNotify_AddBuff::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	// 动画编辑器预览没有 ASC，什么也不做
	UAbilitySystemComponent* ASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(MeshComp ? MeshComp->GetOwner() : nullptr);
	if (ASC && BuffId != 0) UWuwaBuffLibrary::AddBuff(ASC, BuffId);
}
