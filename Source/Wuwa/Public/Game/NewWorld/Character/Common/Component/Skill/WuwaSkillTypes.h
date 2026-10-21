#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "WuwaSkillTypes.generated.h"

class UAnimMontage;
class UWuwaGameplayAbilityBase;

/**
 * 技能开始时怎么挑目标（原作 ESkillTargetPriority，数值是原作代码里用的值，DT_SkillInfo 里显示名在括号里）。
 * 挑法见 UWuwaLockOnComponent：先在"与参考方向夹角 < 容忍角"的单位里挑最近的，没有就在全部里挑最近的（精准模式没有这一步）。
 */
UENUM(BlueprintType)
enum class EWuwaSkillTargetPriority : uint8
{
	/** 摇杆方向优先：参考方向 = 输入方向，没有输入时用镜头朝向；输入方向没变时保留当前目标。 */
	InputDirection = 0,
	/** 角色方向优先：参考方向 = 角色朝向。 */
	ActorDirection = 1,
	/** 锁定目标优先：当前目标还有效就继续用；否则参考方向 = 镜头朝向。 */
	KeepLockTarget = 2,
	/** 距离优先：没有参考方向，直接挑最近的。 */
	Distance = 3,
	/** 镜头方向优先：参考方向 = 镜头朝向。 */
	CameraDirection = 4,
	/** 摇杆方向优先（精准模式）：只在容忍角内挑。 */
	InputDirectionPrecise = 5,
	/** 镜头方向优先（精准模式）。 */
	CameraDirectionPrecise = 6,
	/** 角色方向优先（精准模式）。 */
	ActorDirectionPrecise = 7,
	/** 系统设置：跟随游戏设置"技能锁敌模式"（UWuwaCombatSettings::SkillLockEnemyMode）。 */
	GameSetting = 8,
};

/** 技能开始时角色转向哪里（原作 ESkillTargetDirection，数值是原作代码里用的值；原作 SetSkillTargetDirection）。 */
UENUM(BlueprintType)
enum class EWuwaSkillDirection : uint8
{
	/** 技能目标方向：有目标转向目标，没有就转向输入方向（优先级为镜头精准模式时转向镜头方向）。 */
	SkillTarget = 0,
	/** 摇杆方向：转向输入方向（没有输入不转）。 */
	InputDirection = 1,
	/** 角色方向：不转。 */
	ActorDirection = 2,
	/** 相机方向：转向镜头朝向。 */
	CameraDirection = 3,
	/** 技能目标方向（载具专用），项目里没有载具，按 SkillTarget 处理。 */
	VehicleSkillTarget = 4,
};

/**
 * 技能配置表的一行，对应原作 DT_SkillInfo（SSkillInfo）。行名就是技能 ID（沿用原作编号，例如长离普攻1 = 1205001：
 * 1205 是角色 ID，后三位按类别分段）。ID 是技能的身份，SkillTags 只是分类（一个技能可以属于多个类别）。
 * 第一步只把身份和分类挪到表里；打断等级、技能类型等仍在 GA 上。
 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaSkillInfo : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	FText SkillName;

	/** 执行这个技能的 GA。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	TSubclassOf<UWuwaGameplayAbilityBase> Ability;

	/** 分类 Tag（Skill.Type.* / Skill.Role.*），供输入规则等按类别查询；不作为身份。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	FGameplayTagContainer SkillTags;

	/**
	 * 技能期间加在自己身上的动作标识（原作 Skill.BeginSkillBuffAndTag 把技能表 SkillTag 全部加到身上，技能结束移除）。
	 * 项目只映射输入 / buff 要查的那几个（Role.<角色>.Action.*，如空中普攻 1~5、地面跳跃派生），分类 Tag 仍在 SkillTags。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	FGameplayTagContainer ActionTags;

	/** 技能用到的蒙太奇（原作 Animations）。目前只做记录，GA 仍自己引用蒙太奇。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill")
	TArray<TSoftObjectPtr<UAnimMontage>> Montages;

	// ---- 目标（原作 IsLockOn / SkillTarget / SkillDirection） ----

	/** 原作 IsLockOn：技能开始时挑一个技能目标（锁定组件），技能目标供开始时转向、子弹"技能目标"出生基准等使用。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Target")
	bool bIsLockOn = false;

	/** 原作 SkillTarget.LockOnConfigId：挑目标用哪套锁定参数（DT_LockOnConfig 行名）；0 = 角色默认的软锁定参数。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Target")
	int32 LockOnConfigId = 0;

	/** 原作 SkillTarget.SkillTargetPriority。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Target")
	EWuwaSkillTargetPriority SkillTargetPriority = EWuwaSkillTargetPriority::GameSetting;

	/** 原作 SkillDirection：技能开始时转向哪里。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Target")
	EWuwaSkillDirection SkillDirection = EWuwaSkillDirection::SkillTarget;

	/** 技能开始时给自己加的 buff（原作 SkillStartBuff，buff ID = DT_Buff 行名）。例：心眼技 1205005 开始时加 1205000002（移除心眼）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Buff")
	TArray<int64> SkillStartBuffs;

	/**
	 * 技能结束时给自己加的 buff（原作 SkillEndBuff，Skill.EndSkill 里加：正常结束、被打断都加）。
	 * 例：嘉贝每段普攻 / 重击结束时加 0.1 秒的 buff，到期再挂 0.6 秒的"刚打完第几段"连段窗口 Tag。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Buff")
	TArray<int64> SkillEndBuffs;

	/**
	 * 原作 WalkOffLedge（字段名容易读反）：为真 = 技能期间开"边缘保护"，不能走出平台边缘。
	 * 原作 CharacterSkillComponent 技能开始 SetWalkOffLedgeRecord(false)、DoSkillEndMoveAction 恢复。长离地面普攻是真，空中普攻 / E 是假。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Skill|Move")
	bool bWalkOffLedge = false;
};

namespace WuwaSkill
{
	/** 行名 → 技能 ID；不是正整数的行名返回 0（无效）。 */
	WUWA_API int32 ParseSkillId(FName RowName);
}

/** 对应技能配置的 OverrideType，只描述技能能申请的战斗类别。 */
UENUM(BlueprintType)
enum class EWuwaSkillOverrideType : uint8
{
	None,
	Hit,
	Parry,
	WeaknessBreak,
	Special
};

/** C# 当前主技能的只读查询结果；实际运行记录保存在 WuwaSkill 中。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaSkillData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) TObjectPtr<UWuwaGameplayAbilityBase> ActiveAbility = nullptr;
	/** 当前主技能的技能 ID（技能表行名）；没有主技能时为 0。 */
	UPROPERTY(BlueprintReadOnly) int32 SkillId = 0;
	//和下面的Serial有区别,这个是记录当前的技能是谁,而Skill是负责大局也就是当前是第几个主技能切换了
	UPROPERTY(BlueprintReadOnly) int32 FightStateHandle = 0;
	UPROPERTY(BlueprintReadOnly) int32 InterruptLevel = 0;
	UPROPERTY(BlueprintReadOnly) bool bSkillAcceptInput = false;
	UPROPERTY(BlueprintReadOnly) bool bMainSkillReadyEnd = false;
	/** 动画断点/技能结束的递增编号；输入层每个新编号最多检查一批缓存。 */
	UPROPERTY(BlueprintReadOnly) int64 InputOpportunitySerial = 0;
	/** 成功开始主技能的递增编号；即使该技能在一次输入采样前已经结束也保留。 */
	UPROPERTY(BlueprintReadOnly) int64 SkillStartSerial = 0;
};
