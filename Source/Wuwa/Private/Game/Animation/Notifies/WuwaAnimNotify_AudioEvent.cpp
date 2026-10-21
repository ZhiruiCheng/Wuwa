#include "Game/Animation/Notifies/WuwaAnimNotify_AudioEvent.h"

#include "Components/SkeletalMeshComponent.h"
#include "Game/Audio/WuwaWwiseAudio.h"

FString UWuwaAnimNotify_AudioEvent::GetNotifyName_Implementation() const
{
	return AudioEvent.IsNull() ? TEXT("Wwise Audio Event") : FString::Printf(TEXT("Audio: %s"), *AudioEvent.GetAssetName());
}

void UWuwaAnimNotify_AudioEvent::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	if (!MeshComp) return;
	WuwaWwiseAudio::Post(AudioEvent, MeshComp, SocketName, bFollow, MeshComp->GetSocketTransform(SocketName));
}
