#pragma once

#include "CoreMinimal.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaSkillMontageAbility.h"
#include "WuwaAirDodgeAbility.generated.h"

class UAbilityTask_WaitDelay;

/**
 * 空中闪避（原作通用技能 100003 空中前闪 / 100004 空中后闪的 GA_Role_LandEnd_Net，按蓝图字节码移植）：
 * - 激活：加 buff 1101004005（闪避禁止，落地且在前台时移除 = 一次滞空只能空中闪避一次），播技能蒙太奇 [0]（AM_Jump_Second_F / B）。
 *   （原作今汐神子 / 大招状态时改为放子弹 100121、不加 buff，项目没有今汐，不做。）
 * - 0.5 秒后：已经在地面就结束；否则等 位置状态.地面（是当前操作的角色就再等 0.1 秒结束，否则 0.1 秒淡出停蒙太奇）或 位置状态.攀爬（立刻结束）。
 * - 蒙太奇播完 / 淡出 / 被打断 / 取消都结束（默认行为）。
 * - 结束时有移动输入就按一次冲刺（原作 SprintPress）。
 */
UCLASS()
class WUWA_API UWuwaAirDodgeAbility : public UWuwaSkillMontageAbility
{
	GENERATED_BODY()

public:
	/** 原作 AddBuffFromGA 1101004005。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Skill")
	int64 StartBuffId = 1101004005;

	/** 原作 Delay(0.5)：至少这么久之后才开始看落地。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Skill", meta = (Units = "s"))
	float MinAirTime = 0.5f;

	/** 原作落地后 WaitDelay(0.1) 再结束 / MontageStop(0.1)。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Skill", meta = (Units = "s"))
	float LandEndDelay = 0.1f;

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;
	/** 原作 GA_Role_LandEnd_Net 只在落地 / 攀爬时结束：蒙太奇在空中就播完（高处闪避）时不结束技能，继续下落等落地。 */
	virtual void OnSkillMontageCompleted(int32 Index) override;
	virtual void OnSkillMontageBlendOut(int32 Index) override;

private:
	bool IsAirborne() const;
	UFUNCTION() void HandleMinAirTimeElapsed();
	UFUNCTION() void HandleGroundAdded();
	UFUNCTION() void HandleClimbAdded();
	UFUNCTION() void HandleLandEndDelay();
};
