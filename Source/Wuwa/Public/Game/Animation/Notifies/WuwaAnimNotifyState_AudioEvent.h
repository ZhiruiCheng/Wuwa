#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Game/Audio/WuwaWwiseAudio.h"
#include "WuwaAnimNotifyState_AudioEvent.generated.h"

/**
 * 窗口内播放一个 Wwise 事件，对应原作 TsAnimNotifyStateAudioEvent（整段攻击的动作音效）：
 * 开始时播放；结束时淡出停止这次播放（KeepAlive 时不停），再播放拖尾事件。
 * 默认值照原作 TsAnimNotifyStateAudioEvent.js：FadeDuration 500ms、FadeCurve 4（线性）。
 * 填了 SwitchGroup / SwitchValue 时先在同一个 AkComponent 上设开关再播放：原作衣服拟音 TsAnimNotifyStateFoleyAudioEvent
 * （foley_variant = run / sprint / weak …，播角色的 FoleyEvent）就是这样；SwitchValue 为空则不设开关（沿用上次的值）。
 */
UCLASS(meta = (DisplayName = "Wwise Audio Event Window"))
class WUWA_API UWuwaAnimNotifyState_AudioEvent : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Audio", meta = (AllowedClasses = "/Script/AkAudio.AkAudioEvent"))
	FSoftObjectPath AudioEvent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Audio")
	FName SocketName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Audio")
	bool bFollow = true;

	/** 窗口结束时不停止，让事件自己播完。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Audio")
	bool bKeepAlive = false;

	/** 窗口结束时的淡出时长（原作毫秒值换算成秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Audio", meta = (ClampMin = "0", Units = "s"))
	float FadeDuration = 0.5f;

	/** Wwise 淡出曲线（AkCurveInterpolation 原值：4 = 线性）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Audio", meta = (ClampMin = "0", ClampMax = "8"))
	int32 FadeCurve = 4;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Audio", meta = (AllowedClasses = "/Script/AkAudio.AkAudioEvent"))
	FSoftObjectPath TrailingAudioEvent;

	/** 播放前要设置的开关组（例如 foley_variant）；为空不设。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Audio")
	FName SwitchGroup;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Audio")
	FName SwitchValue;

	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;
	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;

private:
	/** 通知对象被所有播放这段动画的网格共享，按网格记播放（原作按 Owner 记 HandleMap）。 */
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FWuwaWwisePlaying> Playings;
};
