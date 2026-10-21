#include "Game/Effect/Specs/PostProcess/WuwaEffectPostProcessSpec.h"

#include "Components/PostProcessComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Game/Effect/Types/WuwaEffectSpawnRequest.h"
#include "Game/EffectModel/Models/WuwaEffectModelPostProcess.h"

namespace WuwaEffectPostProcess
{
	// 重建的径向模糊材质参数名（与 WuwaReferenceAssetImport::BuildRadialBlurMaterial 一致）。
	const FName IntensityName(TEXT("Intensity"));
	const FName CenterName(TEXT("Center"));
	const FName RadiusName(TEXT("Radius"));
	const FName HardnessName(TEXT("Hardness"));
}

bool UWuwaEffectPostProcessSpec::Play_Implementation(const FWuwaEffectSpawnRequest& Request)
{
	Model = Cast<UWuwaEffectModelPostProcess>(Request.Model);
	UWorld* World = GetWorld();
	if (!Model || !World)
	{
		return false;
	}

	ReleaseComponent();
	PostProcessComponent = NewObject<UPostProcessComponent>(this);
	// 未开体积时原作组件对全屏生效，混合权重由体积硬度决定。
	PostProcessComponent->bUnbound = true;
	PostProcessComponent->Priority = Model->WeatherPriority;
	PostProcessComponent->Settings = Model->Settings;
	if (Model->BlurIntensityOverride && Model->RadialBlurMaterial)
	{
		RadialBlurMaterial = UMaterialInstanceDynamic::Create(Model->RadialBlurMaterial, this);
		PostProcessComponent->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, RadialBlurMaterial));
	}
	PostProcessComponent->RegisterComponentWithWorld(World);
	AttachTo = Request.AttachTo;
	if (USceneComponent* Parent = AttachTo.Get())
	{
		PostProcessComponent->AttachToComponent(Parent, FAttachmentTransformRules::KeepRelativeTransform, Request.SocketName);
	}

	Age = 0.f;
	bPlaying = true;
	ApplyStateAtAge();
	return true;
}

void UWuwaEffectPostProcessSpec::Stop_Implementation(bool bImmediately)
{
	if (IsFinished()) return;
	// 原作 OnStop 直接关掉组件，没有淡出阶段。
	bPlaying = false;
	ReleaseComponent();
	Finish();
}

void UWuwaEffectPostProcessSpec::ApplyStateAtAge()
{
	if (!Model || !IsValid(PostProcessComponent)) return;

	const float Hardness = Model->UseVolumeHardnessCurve ? Model->VolumeHardnessCurve.Evaluate(Age) : Model->VolumeHardness;
	PostProcessComponent->BlendWeight = FMath::Max(Hardness, 1e-4f);
	PostProcessComponent->bEnabled = PostProcessComponent->BlendWeight > 0.f;

	FPostProcessSettings& Settings = PostProcessComponent->Settings;
	if (Model->SceneFringeIntensityOverride)
	{
		Settings.bOverride_SceneFringeIntensity = true;
		Settings.SceneFringeIntensity = Model->SceneFringeIntensity.Evaluate(Age);
	}
	if (Model->VignetteIntensityOverride)
	{
		Settings.bOverride_VignetteIntensity = true;
		Settings.VignetteIntensity = Model->VignetteIntensity.Evaluate(Age);
	}
	if (IsValid(RadialBlurMaterial))
	{
		const FVector2D Center = RadialBlurCenter();
		RadialBlurMaterial->SetScalarParameterValue(WuwaEffectPostProcess::IntensityName, Model->BlurIntensity.Evaluate(Age));
		RadialBlurMaterial->SetVectorParameterValue(WuwaEffectPostProcess::CenterName, FLinearColor(Center.X, Center.Y, 0.f, 0.f));
		RadialBlurMaterial->SetScalarParameterValue(WuwaEffectPostProcess::RadiusName, Model->RadialBlurRadius.Evaluate(Age));
		RadialBlurMaterial->SetScalarParameterValue(WuwaEffectPostProcess::HardnessName, Model->RadialBlurHardness.Evaluate(Age));
	}
}

FVector2D UWuwaEffectPostProcessSpec::RadialBlurCenter() const
{
	if (!Model->UseWorldPosition) return Model->ScreenPosition;
	// 原作把本地角色的位置投影到屏幕；这里用特效挂接对象的拥有者，投影失败时退回屏幕中心。
	const USceneComponent* Parent = AttachTo.Get();
	const AActor* Owner = Parent ? Parent->GetOwner() : nullptr;
	APlayerController* Controller = GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr;
	FVector2D Screen;
	int32 Width = 0, Height = 0;
	if (Owner && Controller && Controller->ProjectWorldLocationToScreen(Owner->GetActorLocation(), Screen))
	{
		Controller->GetViewportSize(Width, Height);
		if (Width > 0 && Height > 0) return Screen / FVector2D(Width, Height);
	}
	return FVector2D(0.5, 0.5);
}

void UWuwaEffectPostProcessSpec::Tick(float DeltaTime)
{
	Age += FMath::Max(0.f, DeltaTime);
	ApplyStateAtAge();
	// 与贴花相同：起始段 + 结束段播完即结束；负数 StartTime 表示持续播放，需外部停止。
	if (Model && Model->LoopTime <= 0.f && Model->StartTime > 0.f && Age >= Model->StartTime + Model->EndTime)
	{
		Stop_Implementation(true);
	}
}

bool UWuwaEffectPostProcessSpec::IsTickable() const
{
	return bPlaying && !IsFinished() && IsValid(PostProcessComponent);
}

ETickableTickType UWuwaEffectPostProcessSpec::GetTickableTickType() const
{
	return HasAnyFlags(RF_ClassDefaultObject) ? ETickableTickType::Never : ETickableTickType::Conditional;
}

TStatId UWuwaEffectPostProcessSpec::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWuwaEffectPostProcessSpec, STATGROUP_Tickables);
}

void UWuwaEffectPostProcessSpec::ReleaseComponent()
{
	if (IsValid(PostProcessComponent))
	{
		PostProcessComponent->DestroyComponent();
	}
	PostProcessComponent = nullptr;
	RadialBlurMaterial = nullptr;
}
