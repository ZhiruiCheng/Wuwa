#include "Game/Effect/Specs/SkeletalMesh/WuwaEffectSkeletalMeshSpec.h"

#include "Animation/AnimationAsset.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Game/Effect/Types/WuwaEffectSpawnRequest.h"
#include "Game/Effect/Types/WuwaEffectTransform.h"
#include "Game/EffectModel/Models/WuwaEffectModelSkeletalMesh.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaEffectSkeletalMesh, Log, All);

bool UWuwaEffectSkeletalMeshSpec::Play_Implementation(const FWuwaEffectSpawnRequest& Request)
{
	Model = Cast<UWuwaEffectModelSkeletalMesh>(Request.Model);
	UWorld* World = GetWorld();
	if (!Model || !World)
	{
		return false;
	}
	// 原作 OnInit：网格或动画缺一个就初始化失败
	if (!Model->SkeletalMeshRef || !Model->AnimationRef)
	{
		UE_LOG(LogWuwaEffectSkeletalMesh, Warning, TEXT("%s: skeletal mesh or animation is not configured."), *Model->GetName());
		return false;
	}

	ReleaseComponent();
	MeshComponent = NewObject<USkeletalMeshComponent>(this);
	MeshComponent->SetSkeletalMesh(Model->SkeletalMeshRef);
	MeshComponent->SetCollisionEnabled(Model->EnableCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
	MeshComponent->SetCastShadow(Model->CastShadow);
	MeshComponent->SetAnimationMode(EAnimationMode::AnimationSingleNode);
	MeshComponent->RegisterComponentWithWorld(World);

	BaseTransform = Request.Transform;
	bAttached = false;
	if (USceneComponent* AttachTo = Request.AttachTo.Get())
	{
		MeshComponent->AttachToComponent(AttachTo, FAttachmentTransformRules::KeepRelativeTransform, Request.SocketName);
		bAttached = true;
	}

	// 原作 OnPlay：先隐藏 HideFrames + 1 帧，从头播放动画，播放速度 1
	HideCounter = FMath::Max(0, Model->HideFrames) + 1;
	MeshComponent->SetVisibility(false);
	MeshComponent->SetPlayRate(1.f);
	MeshComponent->PlayAnimation(Model->AnimationRef, Model->Looping);

	Age = 0.f;
	bPlaying = true;
	ApplyStateAtAge();
	return true;
}

void UWuwaEffectSkeletalMeshSpec::Stop_Implementation(bool bImmediately)
{
	if (IsFinished()) return;
	// 原作 OnStop：隐藏、停 Tick、停动画；OnEnd 停动画。组件在这里一并销毁。
	bPlaying = false;
	ReleaseComponent();
	Finish();
}

void UWuwaEffectSkeletalMeshSpec::ApplyStateAtAge()
{
	if (!Model || !IsValid(MeshComponent)) return;
	const FTransform Local = WuwaEffectTransform::Evaluate(Model->Location, Model->Rotation, Model->Scale, Age);
	if (bAttached)
	{
		MeshComponent->SetRelativeTransform(Local * BaseTransform);
	}
	else
	{
		MeshComponent->SetWorldTransform(Local * BaseTransform);
	}
}

void UWuwaEffectSkeletalMeshSpec::Tick(float DeltaTime)
{
	Age += FMath::Max(0.f, DeltaTime);
	ApplyStateAtAge();
	if (HideCounter > 0 && --HideCounter == 0 && IsValid(MeshComponent))
	{
		MeshComponent->SetVisibility(true);
	}
	// 与其他组件类特效相同：起始段 + 结束段播完即结束；负数 StartTime 表示持续播放，需外部停止。
	if (Model && Model->LoopTime <= 0.f && Model->StartTime > 0.f && Age >= Model->StartTime + Model->EndTime)
	{
		Stop_Implementation(true);
	}
}

bool UWuwaEffectSkeletalMeshSpec::IsTickable() const
{
	return bPlaying && !IsFinished() && IsValid(MeshComponent);
}

ETickableTickType UWuwaEffectSkeletalMeshSpec::GetTickableTickType() const
{
	return HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Conditional;
}

TStatId UWuwaEffectSkeletalMeshSpec::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWuwaEffectSkeletalMeshSpec, STATGROUP_Tickables);
}

void UWuwaEffectSkeletalMeshSpec::ReleaseComponent()
{
	if (IsValid(MeshComponent))
	{
		MeshComponent->Stop();
		MeshComponent->SetVisibility(false);
		MeshComponent->DestroyComponent();
	}
	MeshComponent = nullptr;
}
