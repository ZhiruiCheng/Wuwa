#include "Game/Animation/Notifies/WuwaAnimNotify_SequenceCamera.h"

#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"

void UWuwaAnimNotify_SequenceCamera::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	ACharacter* Character = MeshComp ? Cast<ACharacter>(MeshComp->GetOwner()) : nullptr;
	if (UWuwaSequenceCameraSubsystem* Cameras = Character ? UWuwaSequenceCameraSubsystem::Get(Character) : nullptr)
	{
		Cameras->PlayCameraSequence(Config, Character, CameraAttachSocket);
	}
}

FString UWuwaAnimNotify_SequenceCamera::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Sequence Camera %s"), *Config.CameraSequence.GetAssetName());
}
