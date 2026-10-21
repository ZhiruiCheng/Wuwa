#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "Game/Input/WuwaInputTypes.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillTypes.h"
#include "WuwaInputCommandConfig.generated.h"

class UWuwaAbilitySystemComponent;
class AWuwaCharacter;

/** 一次选招的只读上下文。Condition 不得缓存角色引用或修改技能/输入状态。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaInputCommandContext
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly) TObjectPtr<UWuwaAbilitySystemComponent> ASC = nullptr;
	UPROPERTY(BlueprintReadOnly) TObjectPtr<AWuwaCharacter> Avatar = nullptr;
	UPROPERTY(BlueprintReadOnly) FWuwaSkillData CurrentSkill;
	UPROPERTY(BlueprintReadOnly) FGameplayTag InputTag;
	/** 这次输入的阶段（按下 / 长按 / 松开）；只有 Phase 相同的行参与选招。 */
	UPROPERTY(BlueprintReadOnly) EWuwaInputPhase Phase = EWuwaInputPhase::Pressed;
};

/** 配置对象可能被多个角色共享；只保存参数，运行数据从 Context 查询。 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class WUWA_API UWuwaInputCondition : public UObject
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintPure, BlueprintNativeEvent, Category = "Wuwa|Combat|Input")
	bool Evaluate(const FWuwaInputCommandContext& Context) const;
};

/**
 * 这一行在哪里能选中（原作 BP_Input_<角色> 攻击按下先分地面 / 空中两条链：地面 = CMC IsMovingOnGround，否则走空中普攻）。
 * 原作技能表没有位置字段，位置只在输入层区分；技能本身不再检查。
 */
UENUM(BlueprintType)
enum class EWuwaInputPosition : uint8
{
	/** 不限制。 */
	Any,
	/** 只在地面（CMC IsMovingOnGround）。 */
	Ground,
	/** 只在空中（下落 / 飞行）。 */
	Air
};

/** 按输入 Tag + 阶段（Phase）选行，按数组顺序首命中，不按 InterruptLevel 排序。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaInputCommandRule
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FName RuleName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTag InputTag;
	/**
	 * 这一行响应输入的哪个阶段（≈ 原作 BP_Input_<角色> 里分开的 攻击按下 / 攻击长按 / 攻击抬起 函数）：
	 * Pressed 按下（默认）；Held 按住到 Hold 触发器的阈值（这个键的 InputAction 要配 Hold 触发器，每次按下只发一次）；Released 松开。
	 * Triggered / Canceled 不用于选招。同一个键的按下和长按可以各配各的行，例如 按下 = 普攻、长按 = 重击。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EWuwaInputPhase Phase = EWuwaInputPhase::Pressed;
	/** 选中这一行时要放的技能（技能表行名 = 技能 ID）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 TargetSkillId = 0;
	/** 角色在地面 / 空中才能选中这一行；位置不符和其他条件不符一样，继续看下一行。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) EWuwaInputPosition Position = EWuwaInputPosition::Any;
	/** 0 = 不限制；非 0 = 当前主技能必须正是这个技能 ID。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 RequiredCurrentSkillId = 0;
	/** 空 = 不限制；非空 = 必须有当前主技能，且它的分类 Tag（技能表 SkillTags）满足查询，例如"当前是普攻"。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTagQuery CurrentSkillTagQuery;
	/** 空 = 不限制角色 Tag。查询 ASC 拥有的 Tag，而不是 GA 身份 Tag。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) FGameplayTagQuery OwnerTagQuery;
	/**
	 * 命中这一行就吞掉这次输入：不放技能、不预输入，也不再看后面的行或走通用动作（原作输入函数返回空命令）。
	 * 例：长离有心眼但当前技能不在接招窗口也没 ReadyEnd 时按跳跃，什么都不做。TargetSkillId 不用填。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bBlockInput = false;
	/** 所有条件都满足才选中此行；空数组 = 无附加限制，空对象 = 配置错误。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced) TArray<TObjectPtr<UWuwaInputCondition>> Conditions;
};

UCLASS(BlueprintType)
class WUWA_API UWuwaInputCommandConfig : public UDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat|Input", meta = (TitleProperty = "RuleName"))
	TArray<FWuwaInputCommandRule> Rules;
};
