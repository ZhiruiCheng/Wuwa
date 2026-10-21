#include "Game/Animation/Notifies/WuwaAnimNotify_CameraModify.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Game/Camera/WuwaPlayerCameraManager.h"
#include "GameFramework/Pawn.h"
#include "TimerManager.h"

namespace WuwaCameraModifyNotify
{
	AWuwaPlayerCameraManager* CameraFor(const USkeletalMeshComponent* MeshComp)
	{
		return MeshComp ? AWuwaPlayerCameraManager::GetForPawn(Cast<APawn>(MeshComp->GetOwner())) : nullptr;
	}

	/**
	 * 每 0.05 秒查一次（一次性计时器，没到时间就再排一次）：到了 Duration（> 0 时）就退回；
	 * Duration ≤ 0 或 bStopOnMontageEnd 时，蒙太奇不播了（播完 / 被打断）也退回。
	 */
	void PopWhenDone(UWorld* World, TWeakObjectPtr<AWuwaPlayerCameraManager> Camera, int32 Handle, double EndTime, bool bStopOnMontageEnd,
		TWeakObjectPtr<USkeletalMeshComponent> Mesh, TWeakObjectPtr<const UAnimMontage> Montage)
	{
		if (!World || !Camera.IsValid()) return;
		FTimerHandle Timer;
		World->GetTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(Camera.Get(), [=]()
		{
			const UAnimInstance* Anim = Mesh.IsValid() ? Mesh->GetAnimInstance() : nullptr;
			const bool bMontagePlaying = Anim && Montage.IsValid() && Anim->Montage_IsPlaying(Montage.Get());
			const bool bTimeUp = EndTime > 0.0 && World->GetTimeSeconds() >= EndTime;
			const bool bMontageDone = (EndTime <= 0.0 || bStopOnMontageEnd) && !bMontagePlaying;
			if (!bTimeUp && !bMontageDone)
			{
				PopWhenDone(World, Camera, Handle, EndTime, bStopOnMontageEnd, Mesh, Montage);
				return;
			}
			if (Camera.IsValid()) Camera->PopCameraMode(Handle);
		}), 0.05f, false);
	}
}

void UWuwaAnimNotify_CameraModify::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);
	AWuwaPlayerCameraManager* Camera = WuwaCameraModifyNotify::CameraFor(MeshComp);
	UWorld* World = MeshComp ? MeshComp->GetWorld() : nullptr;
	if (!Camera || !World) return;
	const int32 Handle = Camera->PushCameraModify(MeshComp->GetOwner(), Modify, BlendInTime, BlendOutTime);
	if (Handle == 0) return;
	// 原作 持续时间 从淡入开始算；没有蒙太奇（单独播序列）时只按时间退回
	const UAnimMontage* Montage = Cast<UAnimMontage>(Animation);
	const double EndTime = Duration > 0.f ? World->GetTimeSeconds() + Duration : 0.0;
	if (!Montage && EndTime <= 0.0)
	{
		Camera->PopCameraMode(Handle);
		return;
	}
	WuwaCameraModifyNotify::PopWhenDone(World, Camera, Handle, EndTime, Modify.bStopOnMontageEnd && Montage, MeshComp, Montage);
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
		if (const int32 Handle = Camera->PushCameraModify(MeshComp->GetOwner(), Modify, BlendInTime, BlendOutTime)) Handles.Add(MeshComp, Handle);
	}
}

void UWuwaAnimNotifyState_CameraModify::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	int32 Handle = 0;
	if (Handles.RemoveAndCopyValue(MeshComp, Handle))
	{
		if (AWuwaPlayerCameraManager* Camera = WuwaCameraModifyNotify::CameraFor(MeshComp)) Camera->PopCameraMode(Handle);
	}
	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UWuwaAnimNotifyState_CameraModify::GetNotifyName_Implementation() const
{
	return TEXT("Camera Modify Window");
}
