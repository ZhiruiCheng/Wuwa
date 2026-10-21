#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaComboWindowSetupCommandlet.generated.h"

/**
 * 普攻连段窗口：在角色的普攻蒙太奇上放 Add Loose Tag Window（State.Combat.ComboWindow），
 * 并让输入配置里"接下一段"的规则要求这个 Tag（OwnerTagQuery）——窗口过后再按攻击就回到第一段。
 * 原作的连段重置是技能结束 Buff（时长在没导出的游戏 DB 里），项目用通知窗口代替：0 ~ ReadyEnd + After 秒（默认 0.6）。
 * 接段规则 = 攻击输入、RequiredCurrentSkillId 非 0 的规则；源蒙太奇 = 技能表里 RequiredCurrentSkillId 那一行的蒙太奇。
 * 已经有这个 Tag 窗口的蒙太奇保留原样（手调过的时间不覆盖）；OwnerTagQuery 已经配了别的条件的规则只警告不改。
 * 默认只读，打印输入规则和每段蒙太奇的窗口；-Apply 才写入，先备份到 Saved/Backups/ComboWindowSetup/<时间>/。
 */
UCLASS()
class WUWA_API UWuwaComboWindowSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaComboWindowSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
