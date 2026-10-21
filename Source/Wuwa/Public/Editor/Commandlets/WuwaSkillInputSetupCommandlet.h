#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaSkillInputSetupCommandlet.generated.h"

/**
 * 技能按键（E 共鸣技能 / R 共鸣解放 / Q 声骸）的输入配置。IMC_Character 里这三个键已经映射到 IA_Skill1 / IA_Ultimate / IA_Echo1，
 * 但 DA_IARouterMap 里没有它们，PC 不绑定、按了没有任何输入事件。-Apply 给 DA_IARouterMap 加三行：
 *   IA_Skill1 → Input.Combat.Skill、IA_Ultimate → Input.Combat.Ultimate、IA_Echo1 → Input.Combat.Echo，路由都是 Input.Route.Ability。
 * 战斗输入运行时对没配置指令的 InputTag 直接忽略（不进预输入缓存），等以后有了这些技能再配指令。
 *   -run=WuwaSkillInputSetup [-Apply]   默认只读审计；-Apply 先备份到 Saved/Backups/SkillInputSetup/<时间>/。
 */
UCLASS()
class WUWA_API UWuwaSkillInputSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaSkillInputSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
