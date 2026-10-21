#pragma once

#include "Tickable.h"
#include "Game/Effect/Specs/Base/WuwaEffectSpec.h"
#include "Game/Effect/Types/WuwaEffectHandle.h"
#include "WuwaEffectGroupSpec.generated.h"

class UWuwaEffectModelBase;
class UWuwaEffectModelGroup;

/**
 * 一次 Group 播放（原作 EffectModelGroupSpec）：按各自延迟经 EffectSystem 生成子 Model，
 * 子项全部结束后本组结束。整组的局部变换只在生成子项时计算一次，生成后不再跟随组曲线变化。
 */
UCLASS()
class WUWA_API UWuwaEffectGroupSpec : public UWuwaEffectSpec, public FTickableGameObject
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
	struct FPendingChild
	{
		TWeakObjectPtr<UWuwaEffectModelBase> Model;
		float Delay = 0.f;
	};

	void SpawnChild(UWuwaEffectModelBase* ChildModel);
	void FinishIfDone();

	UPROPERTY(Transient)
	TObjectPtr<UWuwaEffectModelGroup> Model;

	FWuwaEffectSpawnRequest ChildRequest;
	TArray<FPendingChild> PendingChildren;
	TArray<FWuwaEffectHandle> ActiveChildren;
	bool bPlaying = false;
	float Age = 0.f;
};
