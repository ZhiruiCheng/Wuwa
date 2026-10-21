#include "Game/Animation/Notifies/WuwaAnimNotify_Footstep.h"

#include "Components/SkeletalMeshComponent.h"
#include "Game/Audio/WuwaWwiseAudio.h"

FString UWuwaAnimNotify_Footstep::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Footstep: %s"), *Variant.ToString());
}

void UWuwaAnimNotify_Footstep::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (!MeshComp) return;
	// 开关必须设在播放事件的同一个游戏对象上（插槽上的 AkComponent），并且在播放之前
	WuwaWwiseAudio::SetSwitch(MeshComp, SocketName, SwitchGroup, Variant);
	WuwaWwiseAudio::Post(AudioEvent, MeshComp, SocketName, true, MeshComp->GetSocketTransform(SocketName));
}
