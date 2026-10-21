#include "Game/Effect/Specs/Decal/WuwaEffectDecalSpec.h"

#include "Components/DecalComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Game/Effect/Types/WuwaEffectSpawnRequest.h"
#include "Game/Effect/Types/WuwaEffectTransform.h"
#include "Game/EffectModel/Models/WuwaEffectModelDecal.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaEffectDecal, Log, All);

namespace WuwaEffectDecal
{
	// 原作 EffectModelDecalSpec：DecalSize = (100,100,100)，大小全部由 Model 的 Scale 决定。
	const FVector DecalSize(100.0, 100.0, 100.0);
	// 重建的贴花材质读取这两个参数来做库洛的 Z 向淡出（原生 UE 贴花组件没有对应字段）。
	const FName ZFadingFactorName(TEXT("ZFadingFactor"));
	const FName ZFadingPowerName(TEXT("ZFadingPower"));
}

bool UWuwaEffectDecalSpec::Play_Implementation(const FWuwaEffectSpawnRequest& Request)
{
	Model = Cast<UWuwaEffectModelDecal>(Request.Model);
	UWorld* World = GetWorld();
	if (!Model || !World)
	{
		return false;
	}
	if (!Model->DecalMaterialRef)
	{
		UE_LOG(LogWuwaEffectDecal, Warning, TEXT("%s: no decal material is configured."), *Model->GetName());
		return false;
	}

	ReleaseComponent();
	DecalComponent = NewObject<UDecalComponent>(this);
	DecalComponent->DecalSize = WuwaEffectDecal::DecalSize;
	// 材质参数一律走动态实例：曲线要逐帧写，ZFading 也是材质参数。
	Material = UMaterialInstanceDynamic::Create(Model->DecalMaterialRef, this);
	Material->SetScalarParameterValue(WuwaEffectDecal::ZFadingFactorName, Model->ZfadingFactor);
	Material->SetScalarParameterValue(WuwaEffectDecal::ZFadingPowerName, Model->ZfadingPower);
	DecalComponent->SetDecalMaterial(Material);
	DecalComponent->RegisterComponentWithWorld(World);

	BaseTransform = Request.Transform;
	bAttached = false;
	if (USceneComponent* AttachTo = Request.AttachTo.Get())
	{
		DecalComponent->AttachToComponent(AttachTo, FAttachmentTransformRules::KeepRelativeTransform, Request.SocketName);
		bAttached = true;
	}

	Age = 0.f;
	bPlaying = true;
	ApplyStateAtAge();
	return true;
}

void UWuwaEffectDecalSpec::Stop_Implementation(bool bImmediately)
{
	if (IsFinished()) return;
	// 贴花没有“停止发射后自然播完”的阶段：原作 OnStop 只关 Tick，OnEnd 销毁组件。
	bPlaying = false;
	ReleaseComponent();
	Finish();
}

void UWuwaEffectDecalSpec::ApplyStateAtAge()
{
	if (!Model || !IsValid(DecalComponent)) return;

	const FTransform Local = WuwaEffectTransform::Evaluate(Model->Location, Model->Rotation, Model->Scale, Age);
	if (bAttached)
	{
		DecalComponent->SetRelativeTransform(Local * BaseTransform);
	}
	else
	{
		DecalComponent->SetWorldTransform(Local * BaseTransform);
	}
	if (!IsValid(Material)) return;
	for (const auto& Parameter : Model->MaterialFloatParameters)
	{
		Material->SetScalarParameterValue(Parameter.Key, Parameter.Value.Evaluate(Age));
	}
	for (const auto& Parameter : Model->MaterialColorParameters)
	{
		Material->SetVectorParameterValue(Parameter.Key, Parameter.Value.Evaluate(Age));
	}
}

void UWuwaEffectDecalSpec::Tick(float DeltaTime)
{
	Age += FMath::Max(0.f, DeltaTime);
	ApplyStateAtAge();
	// 起始段 + 结束段播完即结束；负数 StartTime 保留原作的持续播放语义，需外部停止。
	if (Model && Model->LoopTime <= 0.f && Model->StartTime > 0.f && Age >= Model->StartTime + Model->EndTime)
	{
		Stop_Implementation(true);
	}
}

bool UWuwaEffectDecalSpec::IsTickable() const
{
	return bPlaying && !IsFinished() && IsValid(DecalComponent);
}

ETickableTickType UWuwaEffectDecalSpec::GetTickableTickType() const
{
	// 类默认对象不参与 Tick；实例按 IsTickable 决定。
	return HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Conditional;
}

TStatId UWuwaEffectDecalSpec::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWuwaEffectDecalSpec, STATGROUP_Tickables);
}

void UWuwaEffectDecalSpec::ReleaseComponent()
{
	if (IsValid(DecalComponent))
	{
		DecalComponent->DestroyComponent();
	}
	DecalComponent = nullptr;
	Material = nullptr;
}
