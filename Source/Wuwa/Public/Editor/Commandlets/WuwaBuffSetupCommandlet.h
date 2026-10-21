#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaBuffSetupCommandlet.generated.h"

/**
 * 按 ArtSource/Combat/prepare_buffs.py 生成的配方建 buff：
 *   1. 每个 buff 一个 GE 蓝图（GE_Buff_<ID>）：持续方式、时长；授予 Tag（Target Tags 组件）；buff 信息（UWuwaBuffInfoComponent：BuffId、CueIds）；
 *      原作额外效果"移除 buff"→ Remove Other Gameplay Effect 组件（按 GE 类匹配）。组件每次整体重建。
 *   2. DT_Buff（buff ID → GE）、DT_BuffCue（cue ID → 图标等）整表重建；图标 PNG 导入成贴图。
 *   3. 动画通知：蒙太奇上的 Add Buff 通知整体换成配方里的（时间按 项目长度 / 原作长度 缩放）。
 * 默认只读，打印要做的事；-Apply 才写入，已有资产先备份到 Saved/Backups/BuffSetup/<时间>/。
 *   -run=WuwaBuffSetup -Recipe=ArtSource/Combat/buffs.recipe.json [-Apply]
 */
UCLASS()
class WUWA_API UWuwaBuffSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaBuffSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
