#include "Game/Animation/Notifies/WuwaAnimNotifyState_AudioEvent.h"

#include "Components/SkeletalMeshComponent.h"

FString UWuwaAnimNotifyState_AudioEvent::GetNotifyName_Implementation() const
{
	return AudioEvent.IsNull() ? TEXT("Wwise Audio Event Window") : FString::Printf(TEXT("Audio: %s"), *AudioEvent.GetAssetName());
}

void UWuwaAnimNotifyState_AudioEvent::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (!MeshComp) return;
	WuwaWwiseAudio::SetSwitch(MeshComp, SocketName, SwitchGroup, SwitchValue);
	const FWuwaWwisePlaying Playing = WuwaWwiseAudio::Post(AudioEvent, MeshComp, SocketName, bFollow, MeshComp->GetSocketTransform(SocketName));
	if (Playing.IsValid()) Playings.Add(MeshComp, Playing);
}

void UWuwaAnimNotifyState_AudioEvent::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);
	FWuwaWwisePlaying Playing;
	if (!Playings.RemoveAndCopyValue(MeshComp, Playing) || bKeepAlive) return;
	WuwaWwiseAudio::Stop(Playing, FadeDuration, FadeCurve);
	if (MeshComp && !TrailingAudioEvent.IsNull())
	{
		WuwaWwiseAudio::Post(TrailingAudioEvent, MeshComp, SocketName, bFollow, MeshComp->GetSocketTransform(SocketName));
	}
}
