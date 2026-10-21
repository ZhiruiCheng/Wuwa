#pragma once

#include "Tickable.h"
#include "Game/Effect/Specs/Base/WuwaEffectSpec.h"
#include "WuwaEffectDecalSpec.generated.h"

class UDecalComponent;
class UMaterialInstanceDynamic;
class UWuwaEffectModelDecal;

/**
 * 一次贴花播放，对应原作 EffectModelDecalSpec：DecalSize 固定 (100,100,100)，再按 Model 的缩放曲线缩放；
 * 有材质参数时建动态材质实例，每帧按播放时长写入曲线值。组件在结束时销毁，不写回共享 Model。
 * 原作的 ZFadingFactor / ZFadingPower 是库洛引擎扩展，这里由重建的贴花材质自己实现（材质参数）。
 */
UCLASS()
class WUWA_API UWuwaEffectDecalSpec : public UWuwaEffectSpec, public FTickableGameObject
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
	/** 写入当前播放时长对应的变换和材质参数。 */
	void ApplyStateAtAge();
	void ReleaseComponent();

	UPROPERTY(Transient)
	TObjectPtr<UDecalComponent> DecalComponent;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> Material;

	UPROPERTY(Transient)
	TObjectPtr<UWuwaEffectModelDecal> Model;

	// 请求给出的变换：挂接时是相对插槽，否则是世界变换。Model 的局部变换叠加在它之上。
	FTransform BaseTransform = FTransform::Identity;
	bool bAttached = false;
	bool bPlaying = false;
	float Age = 0.f;
};
