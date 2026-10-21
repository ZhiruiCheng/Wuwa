#pragma once

#include "Components/AudioComponent.h"
#include "Game/Audio/WuwaWwiseAudio.h"
#include "Game/Effect/Specs/Base/WuwaEffectSpec.h"
#include "Tickable.h"
#include "WuwaEffectAudioSpec.generated.h"

class UWuwaEffectModelAudio;

/**
 * 一次声音播放；组件、停止进度属于 Spec，不写回共享 Model。
 * UnrealSound：本 Spec 自己的 AudioComponent。WwiseEvent：挂点上的 AkComponent 播放事件，每帧查询事件是否播完。
 */
UCLASS()
class WUWA_API UWuwaEffectAudioSpec : public UWuwaEffectSpec, public FTickableGameObject
{
	GENERATED_BODY()

public:
	virtual bool Play_Implementation(const FWuwaEffectSpawnRequest& Request) override;
	virtual void Stop_Implementation(bool bImmediately) override;

	/** Audio 池归还/淘汰入口；不通知播放结束，也不改变共享 Model。 */
	void ResetForPool();
	void ReleaseAudioComponent();

	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual TStatId GetStatId() const override;
	virtual UWorld* GetTickableGameObjectWorld() const override { return GetWorld(); }
	virtual ETickableTickType GetTickableTickType() const override;
	virtual bool IsTickableWhenPaused() const override { return false; }

private:
	bool PlayWwise(const UWuwaEffectModelAudio& Model, const FWuwaEffectSpawnRequest& Request);
	void StopWwise(bool bImmediately);

	void HandlePlayStateChanged(const UAudioComponent* Component, EAudioComponentPlayState PlayState);

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> AudioComponent;

	float FadeOutSeconds = 0.f;
	EAudioFaderCurve FadeOutCurve = EAudioFaderCurve::Linear;
	bool bStopping = false;

	/** Wwise 播放（只在 C++ 里用）：播放 ID、拖尾事件与挂点，停止时用。 */
	FWuwaWwisePlaying WwisePlaying;
	int32 WwiseFadeCurve = 4;
	bool bWwiseKeepAlive = false;
	FSoftObjectPath WwiseTrailingEvent;
	TWeakObjectPtr<USceneComponent> WwiseAttachTo;
	FName WwiseSocket;
	FTransform WwiseTransform;
};
