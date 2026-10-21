#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaBossSetupCommandlet.generated.h"

/**
 * Boss 的动画、蒙太奇和技能表，按 ArtSource/Boss/prepare_boss.py 生成的配方（原作数据）建：
 * - 动画序列：原作开了根运动的打开 bEnableRootMotion，并挂上 UWuwaPromoteRootMotionModifier
 *   （Blender 导出的 FBX 外面多一层骨架骨骼，位移在子骨骼 root 上，要提到第 0 根骨骼）；
 * - 蒙太奇：按原作重建插槽、片段（序列 / 起止时间 / 速率）、分段、混合时间（原作的通知先不搬）；
 * - 技能表（FWuwaSkillInfo）：行名 = 原作技能 ID，填名字和蒙太奇；GA 留空（怪物 AI 那一步再做）。
 * 默认只检查并报告，-Apply 才写入（已有的资产先备份到 Saved/Backups/BossSetup/<时间>/，再整份重建）。
 *
 * -run=WuwaBossSetup -Recipe=ArtSource/Boss/fuludelisi.boss.json [-Apply]
 */
UCLASS()
class WUWAANIMATIONEDITOR_API UWuwaBossSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaBossSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
