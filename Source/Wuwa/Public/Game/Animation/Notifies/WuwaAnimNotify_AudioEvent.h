#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "WuwaAnimNotify_AudioEvent.generated.h"

/**
 * 播放一个 Wwise 事件，对应原作 TsAnimNotifyAudioEvent（喊招语音等）：
 * Follow（默认）时在网格插槽上的 AkComponent 播放、声音跟着角色；否则在插槽当前位置一次性播放。
 * 原作的替换特效表、按标签概率（TagProbabilityInfo）不在本项目范围；事件里自带的播放概率由 Wwise 处理。
 */
UCLASS(meta = (DisplayName = "Wwise Audio Event"))
class WUWA_API UWuwaAnimNotify_AudioEvent : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Audio", meta = (AllowedClasses = "/Script/AkAudio.AkAudioEvent"))
	FSoftObjectPath AudioEvent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Audio")
	FName SocketName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Audio")
	bool bFollow = true;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
