#include "Game/Animation/Notifies/WuwaAnimNotify_SequenceCamera.h"

#include "Components/SkeletalMeshComponent.h"
#include "Game/Camera/WuwaPlayerCameraManager.h"
#include "GameFramework/Character.h"

void UWuwaAnimNotify_SequenceCamera::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	ACharacter* Character = MeshComp ? Cast<ACharacter>(MeshComp->GetOwner()) : nullptr;
	// 只有本地玩家控制的角色切镜头
	if (AWuwaPlayerCameraManager* Camera = AWuwaPlayerCameraManager::GetForPawn(Character))
	{
		Camera->PlayCameraSequence(Config, Character, CameraAttachSocket);
	}
}

FString UWuwaAnimNotify_SequenceCamera::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Sequence Camera %s"), *Config.CameraSequence.GetAssetName());
}
