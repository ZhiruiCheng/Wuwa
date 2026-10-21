#pragma once

#include "Tickable.h"
#include "Game/Effect/Specs/Base/WuwaEffectSpec.h"
#include "WuwaEffectNiagaraSpec.generated.h"

class UNiagaraComponent;
class UWuwaEffectModelNiagara;

/**
 * 一次 Niagara 播放；组件、播放时长和停止进度属于 Spec，不写回共享 Model。
 * Model 的变换或用户参数使用曲线时，每帧按播放时长更新；全是常量则不 Tick。
 */
UCLASS()
class WUWA_API UWuwaEffectNiagaraSpec : public UWuwaEffectSpec, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual bool Play_Implementation(const FWuwaEffectSpawnRequest& Request) override;
	virtual void Stop_Implementation(bool bImmediately) override;

	/** Niagara 池归还/淘汰入口；不通知播放结束，也不改变共享 Model。 */
	void ResetForPool();
	void ReleaseNiagaraComponent();

	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsTickableWhenPaused() const override { return false; }

private:
	UFUNCTION()
	void HandleSystemFinished(UNiagaraComponent* Component);

	/** 写入当前播放时长对应的变换和用户参数。 */
	void ApplyStateAtAge();
	bool ShouldAutoStop() const;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> NiagaraComponent;

	UPROPERTY(Transient)
	TObjectPtr<UWuwaEffectModelNiagara> Model;

	// 请求给出的变换：挂接时是相对插槽，否则是世界变换。Model 的局部变换叠加在它之上。
	FTransform BaseTransform = FTransform::Identity;
	bool bAttached = false;
	bool bAnimated = false;
	bool bPlaying = false;
	bool bStopping = false;
	float Age = 0.f;
};
