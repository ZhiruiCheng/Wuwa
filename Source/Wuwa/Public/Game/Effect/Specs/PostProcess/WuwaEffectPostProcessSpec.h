#pragma once

#include "Tickable.h"
#include "Game/Effect/Specs/Base/WuwaEffectSpec.h"
#include "WuwaEffectPostProcessSpec.generated.h"

class UMaterialInstanceDynamic;
class UPostProcessComponent;
class UWuwaEffectModelPostProcess;

/**
 * 一次后处理播放，对应原作 EffectModelPostProcessSpec：建一个后处理组件，优先级取 WeatherPriority，
 * 混合权重 = 体积硬度（未开体积时），每帧按播放时长写入曲线；播完（StartTime + EndTime）销毁组件。
 * 色差、暗角写 UE 原生后处理字段；库洛的径向模糊由 Model 指定的后处理材质实现（见 Model 注释）。
 * 原作 EnableVolume 的相机距离衰减、OverrideTOD 还没有适配。
 */
UCLASS()
class WUWA_API UWuwaEffectPostProcessSpec : public UWuwaEffectSpec, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual bool Play_Implementation(const FWuwaEffectSpawnRequest& Request) override;
	virtual void Stop_Implementation(bool bImmediately) override;

	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsTickableWhenPaused() const override { return false; }

private:
	/** 写入当前播放时长对应的混合权重和后处理参数。 */
	void ApplyStateAtAge();
	FVector2D RadialBlurCenter() const;
	void ReleaseComponent();

	UPROPERTY(Transient)
	TObjectPtr<UPostProcessComponent> PostProcessComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> RadialBlurMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UWuwaEffectModelPostProcess> Model;

	TWeakObjectPtr<USceneComponent> AttachTo;
	bool bPlaying = false;
	float Age = 0.f;
};
