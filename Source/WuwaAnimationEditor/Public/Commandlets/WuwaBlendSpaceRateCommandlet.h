#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaBlendSpaceRateCommandlet.generated.h"

/**
 * 设置播放倍率：混合空间采样的 FBlendSample::RateScale，或动画序列自身的 RateScale。
 * 用途：旧 24fps 导出的动画时长是原作的 1.25 倍，当时在动画序列上加了 1.25 倍补偿；按 30fps 重新导出（时长已和原作一致）后要去掉。
 * 默认只报告，-Apply 才修改并保存（先备份到 Saved/Backups/BlendSpaceRate/<时间>/）。
 *
 * -BlendSpace=<路径> -Rate=<倍率> [-Apply]
 *
 * 动画序列自身的 RateScale（混合空间、蒙太奇播放时都会乘上）：
 * -Folder=<目录> [-Contains=<名字片段>] 列出目录下 RateScale 不为 1 的动画序列；加 -From=<旧倍率> -Rate=<新倍率> [-Apply] 把等于旧倍率的改成新倍率。
 */
UCLASS()
class WUWAANIMATIONEDITOR_API UWuwaBlendSpaceRateCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaBlendSpaceRateCommandlet();
	virtual int32 Main(const FString& Params) override;
};
