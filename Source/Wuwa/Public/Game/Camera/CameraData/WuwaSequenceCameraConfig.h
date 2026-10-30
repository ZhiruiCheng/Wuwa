#pragma once

#include "CoreMinimal.h"
#include "WuwaSequenceCameraConfig.generated.h"

class ULevelSequence;

/** 特写镜头配置（技能动画通知里填）。由 C# 序列镜头的播放组件使用。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaSequenceCameraConfig
{
	GENERATED_BODY()

	/** 要播的关卡序列（由 WuwaSequenceCameraSetup 生成）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera")
	TSoftObjectPtr<ULevelSequence> CameraSequence;

	/** 切进特写镜头的混合时间。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera", meta = (Units = "s"))
	float BlendInTime = 0.f;

	/** 结束时混回战斗镜头的时间。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera", meta = (Units = "s"))
	float BlendOutTime = 0.f;

	/** 隐藏战斗 UI，藏到序列的第 HideHudTime 秒（为 0 时藏到结束）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera")
	bool bHideHud = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera", meta = (Units = "s"))
	float HideHudTime = 0.f;

	/** 大于 0 时：最多播这么久（序列更长也在这里结束）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera", meta = (Units = "s"))
	float SpecificSequenceTime = 0.f;
};
