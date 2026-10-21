#pragma once

#include "Components/AudioComponent.h"
#include "Game/EffectModel/Base/WuwaEffectModelBase.h"
#include "UObject/SoftObjectPath.h"
#include "WuwaEffectModelAudio.generated.h"

class USoundBase;

/** 本项目的音频资源适配方式，不是鸣潮原生枚举。 */
UENUM(BlueprintType)
enum class EWuwaEffectAudioBackend : uint8
{
	UnrealSound,
	WwiseEvent
};

/**
 * 对应 EffectModelAudio 的声音内容配置。
 * 原作 AudioEvent / TrailingAudioEvent 是 AkAudioEvent 对象。WwiseEvent 后端播放项目 Wwise 工程里同名重建的事件
 * （ArtSource/Audio），UnrealSound 后端用独立的 Sound 字段（DefaultGame.ini 当前是 EnableWwiseOnly，UE 音频不出声）。
 * 只保存数据，不创建 AudioComponent，也不保存声音播放句柄。
 * 原作原生字段的默认值未导出，本类默认值由本项目选择。
 */
UCLASS(BlueprintType, Blueprintable)
class WUWA_API UWuwaEffectModelAudio : public UWuwaEffectModelBase
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	EWuwaEffectAudioBackend AudioBackend = EWuwaEffectAudioBackend::UnrealSound;

	/** 项目 Wwise 事件资产（/Game/WwiseAudio/Events/...，由 Wwise Reconcile 生成）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Wwise Event", meta = (EditCondition = "AudioBackend == EWuwaEffectAudioBackend::WwiseEvent", EditConditionHides, AllowedClasses = "/Script/AkAudio.AkAudioEvent"))
	FSoftObjectPath AudioEvent;

	/** 停止时触发的拖尾事件；原作要求拖尾不能是无限循环音频。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Wwise Event", meta = (EditCondition = "AudioBackend == EWuwaEffectAudioBackend::WwiseEvent", EditConditionHides, AllowedClasses = "/Script/AkAudio.AkAudioEvent"))
	FSoftObjectPath TrailingAudioEvent;

	/** 原作将此值传给音频系统的 TransitionFadeCurve；保留原始数值，不冒充 UE 的曲线枚举。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Wwise Event", meta = (EditCondition = "AudioBackend == EWuwaEffectAudioBackend::WwiseEvent", EditConditionHides, ClampMin = "0"))
	int32 FadeOutCurve = 0;

	/** 本项目 UE 音频入口，支持 SoundWave、SoundCue 等 USoundBase 资源。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Unreal Adapter", meta = (EditCondition = "AudioBackend == EWuwaEffectAudioBackend::UnrealSound", EditConditionHides))
	TObjectPtr<USoundBase> Sound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Unreal Adapter", meta = (EditCondition = "AudioBackend == EWuwaEffectAudioBackend::UnrealSound", EditConditionHides))
	TObjectPtr<USoundBase> TrailingSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Unreal Adapter", meta = (EditCondition = "AudioBackend == EWuwaEffectAudioBackend::UnrealSound", EditConditionHides))
	EAudioFaderCurve UnrealFadeOutCurve = EAudioFaderCurve::Linear;

	/** 原作是多声源位置偏移数组；Spec/音频适配层负责解释，不在 Model 中生成多个声音。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Placement")
	TArray<FVector> LocationOffsets;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Playback")
	bool EnableOcclusion = false;

	/** 特效停止后允许主音频继续播放，不等于无限循环。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Playback")
	bool KeepAlive = false;

	/** 本项目统一使用秒；将来对接原作音频后端时由适配层换算后端需要的时间单位。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio|Playback", meta = (ClampMin = "0", Units = "s"))
	float FadeOutTime = 0.f;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
