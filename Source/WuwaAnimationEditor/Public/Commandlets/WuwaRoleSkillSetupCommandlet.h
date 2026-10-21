#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaRoleSkillSetupCommandlet.generated.h"

/**
 * 角色技能批量建立，按 ArtSource/Combat/prepare_role_skills.py 的配方（原作技能表 + 蒙太奇）：
 * - 动画序列：原作开了根运动的打开 bEnableRootMotion（+ ForceRootLock），挂 UWuwaPromoteRootMotionModifier；
 * - 蒙太奇：按原作重建插槽 / 片段 / 分段 / 混合，并放技能流程通知（轨道 SkillFlow）：ReadyEnd、同级接招窗口、改打断等级、移动取消窗口。
 *   已有的蒙太奇默认不动（上面可能有别的流水线放的子弹 / 特效 / 音效），-RebuildMontages 才整份重建；
 * - GA：每个技能一个蓝图（父类 = 配方里的原生 GA），CDO 填蒙太奇列表、打断等级、主技能；
 * - 技能表：按行名（技能 ID）新增或更新 SkillName / Ability / SkillTags / ActionTags / Montages / bWalkOffLedge，其余字段不动；GA 的 CDO 另按配方 ga.properties 设父类特有的属性。
 * 默认只读，-Apply 才写入；改动的资产先备份到 Saved/Backups/RoleSkillSetup/<时间>/。
 *   -run=WuwaRoleSkillSetup -Recipe=ArtSource/Combat/changli.skills.json [-RebuildMontages] [-Apply]
 */
UCLASS()
class WUWAANIMATIONEDITOR_API UWuwaRoleSkillSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaRoleSkillSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
