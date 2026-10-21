#include "Game/Effect/Specs/Light/WuwaEffectLightSpec.h"

#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "Game/Effect/Types/WuwaEffectSpawnRequest.h"
#include "Game/EffectModel/Models/WuwaEffectModelLight.h"

bool UWuwaEffectLightSpec::Play_Implementation(const FWuwaEffectSpawnRequest& Request)
{
	Model = Cast<UWuwaEffectModelLight>(Request.Model);
	UWorld* World = GetWorld();
	if (!Model || !World)
	{
		return false;
	}

	ReleaseComponent();
	LightComponent = NewObject<UPointLightComponent>(this);
	LightComponent->SetMobility(EComponentMobility::Movable);
	LightComponent->SetIntensityUnits(Model->IntensityUnits);
	LightComponent->SetCastShadows(Model->CastShadows);
	LightComponent->RegisterComponentWithWorld(World);

	BaseTransform = Request.Transform;
	bAttached = false;
	if (USceneComponent* AttachTo = Request.AttachTo.Get())
	{
		LightComponent->AttachToComponent(AttachTo, FAttachmentTransformRules::KeepRelativeTransform, Request.SocketName);
		bAttached = true;
	}

	Age = 0.f;
	bPlaying = true;
	ApplyStateAtAge();
	return true;
}

void UWuwaEffectLightSpec::Stop_Implementation(bool bImmediately)
{
	if (IsFinished()) return;
	// 原作 OnStop 隐藏灯光，OnEnd 销毁组件；没有淡出阶段。
	bPlaying = false;
	ReleaseComponent();
	Finish();
}

void UWuwaEffectLightSpec::ApplyStateAtAge()
{
	if (!Model || !IsValid(LightComponent)) return;

	const FTransform Local(Model->Location.Evaluate(Age));
	if (bAttached)
	{
		LightComponent->SetRelativeTransform(Local * BaseTransform);
	}
	else
	{
		LightComponent->SetWorldTransform(Local * BaseTransform);
	}
	LightComponent->SetIntensity(Model->Intensity.Evaluate(Age));
	LightComponent->SetLightColor(Model->LightColor.Evaluate(Age));
	LightComponent->SetAttenuationRadius(Model->AttenuationRadius.Evaluate(Age));
}

void UWuwaEffectLightSpec::Tick(float DeltaTime)
{
	Age += FMath::Max(0.f, DeltaTime);
	ApplyStateAtAge();
	// 与贴花相同：起始段 + 结束段播完即结束；负数 StartTime 表示持续播放，需外部停止。
	if (Model && Model->LoopTime <= 0.f && Model->StartTime > 0.f && Age >= Model->StartTime + Model->EndTime)
	{
		Stop_Implementation(true);
	}
}

bool UWuwaEffectLightSpec::IsTickable() const
{
	return bPlaying && !IsFinished() && IsValid(LightComponent);
}

ETickableTickType UWuwaEffectLightSpec::GetTickableTickType() const
{
	return HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Conditional;
}

TStatId UWuwaEffectLightSpec::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWuwaEffectLightSpec, STATGROUP_Tickables);
}

void UWuwaEffectLightSpec::ReleaseComponent()
{
	if (IsValid(LightComponent))
	{
		LightComponent->DestroyComponent();
	}
	LightComponent = nullptr;
}
