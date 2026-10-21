#pragma once

#include "Tickable.h"
#include "Game/Effect/Specs/Base/WuwaEffectSpec.h"
#include "WuwaEffectLightSpec.generated.h"

class UPointLightComponent;
class UWuwaEffectModelLight;

/**
 * 一次特效灯播放，对应原作 EffectModelLightSpec：建点光组件，播放时可见，每帧按播放时长写入位置、强度、颜色、半径；
 * 播完（StartTime + EndTime）销毁组件。原作的角色专用光（CharacterLightAlpha / CharacterColor，库洛卡通渲染）没有适配。
 */
UCLASS()
class WUWA_API UWuwaEffectLightSpec : public UWuwaEffectSpec, public FTickableGameObject
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
	void ApplyStateAtAge();
	void ReleaseComponent();

	UPROPERTY(Transient)
	TObjectPtr<UPointLightComponent> LightComponent;

	UPROPERTY(Transient)
	TObjectPtr<UWuwaEffectModelLight> Model;

	// 请求给出的变换：挂接时是相对插槽，否则是世界变换。Model 的 Location 曲线叠加在它之上。
	FTransform BaseTransform = FTransform::Identity;
	bool bAttached = false;
	bool bPlaying = false;
	float Age = 0.f;
};
