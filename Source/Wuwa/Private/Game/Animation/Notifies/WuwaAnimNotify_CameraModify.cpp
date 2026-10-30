#include "Game/Animation/Notifies/WuwaAnimNotify_CameraModify.h"

#include "Components/SkeletalMeshComponent.h"
#include "Game/Camera/WuwaPlayerCameraManager.h"
#include "GameFramework/Pawn.h"

namespace WuwaCameraModifyNotify
{
	// 窗口版传的持续时间：足够长，窗口结束时再手动结束
	constexpr float WindowDuration = 100.f;

	AWuwaPlayerCameraManager* CameraFor(const USkeletalMeshComponent* MeshComp)
	{
		return MeshComp ? AWuwaPlayerCameraManager::GetForPawn(Cast<APawn>(MeshComp->GetOwner())) : nullptr;
	}
}

void UWuwaAnimNotify_CameraModify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	// 持续时间、淡入淡出、蒙太奇结束提前退回都由 C# 的 Modify 控制器每帧处理
	if (AWuwaPlayerCameraManager* Camera = WuwaCameraModifyNotify::CameraFor(MeshComp))
	{
		Camera->ApplyCameraModify(Modify, Duration, BlendInTime, BlendOutTime, BreakBlendOutTime, Animation, MeshComp->GetOwner());
	}
}

FString UWuwaAnimNotify_CameraModify::GetNotifyName_Implementation() const
{
	return FString::Printf(TEXT("Camera Modify %.1fs"), Duration);
}

void UWuwaAnimNotifyState_CameraModify::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);
	if (AWuwaPlayerCameraManager* Camera = WuwaCameraModifyNotify::CameraFor(MeshComp))
	{
		const int32 Instance = Camera->ApplyCameraModify(Modify, WuwaCameraModifyNotify::WindowDuration, BlendInTime, BlendOutTime,
			BreakBlendOutTime, Animation, MeshComp->GetOwner());
		if (Instance >= 0) Instances.Add(MeshComp, Instance);
	}
}

void UWuwaAnimNotifyState_CameraModify::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	int32 Instance = -1;
	if (Instances.RemoveAndCopyValue(MeshComp, Instance))
	{
		// 只结束自己开的那一段：中途被别的修改顶掉了，编号对不上就什么都不做
		if (AWuwaPlayerCameraManager* Camera = WuwaCameraModifyNotify::CameraFor(MeshComp)) Camera->StopCameraModify(Animation, Instance);
	}
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UWuwaAnimNotifyState_CameraModify::GetNotifyName_Implementation() const
{
	return TEXT("Camera Modify Window");
}
