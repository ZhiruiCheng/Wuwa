#pragma once

#include "CoreMinimal.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaSkillMontageAbility.h"
#include "WuwaAirAttackAbility.generated.h"

/**
 * 空中 / 下落攻击（按原作蓝图字节码移植）：
 *   GA_Common_AirAttack：长离 1205106 心眼·跃、1205201 E；
 *   GA_ChangLi_AirAttack：长离 1205105 空中下落攻击，只多一条：激活时在地面就结束（原作去播技能表没有的 Animations[3]，失败后结束）。
 * 技能表蒙太奇：[0] 起手、[1] 下落循环、[2] 落地。
 * - [0] 播完 / 淡出：有 [1] 就播 [1]（BlendOut 后仍可打断）并 SetForceSpeed(向下 5000)；没有 [1]（1205106 只有一段）就结束。
 * - [1] 每帧：还在普通空中状态（原作 位置状态.空中 + 动作状态.其他）时，被静态网格挡住累计超过 1 秒就结束；
 *   离开这个状态时（只判断一次）：在地面 → 落地攻击，播 [2]；否则结束。
 * - [1] 播完结束；淡出 / 打断 / 取消：已经落地攻击就不管（是被 [2] 顶掉的），否则结束。
 * - [2] 播完 / 淡出 / 打断 / 取消都结束。
 * - 结束时 SetForceSpeed(0)，并把胶囊对 Pawn 通道设回阻挡（原作 SetCollisionResponse(2, 2)）。
 *   原作只写了"设回"：下落期间胶囊不挡 Pawn，下砸才能穿过敌人的胶囊（否则顶在 Boss 头上被滑开、速度被卸掉）。
 *   项目在 [1] 下落循环开始时把 Pawn 通道改成忽略（推断的那一半），结束时设回。
 * 原作被打断时若在受击里只打印不结束（受击由受击流程结束技能），项目受击同样会结束技能，统一按"结束"处理。
 */
UCLASS()
class WUWA_API UWuwaAirAttackAbility : public UWuwaSkillMontageAbility
{
	GENERATED_BODY()

public:
	/** GA_ChangLi_AirAttack：激活时在地面直接结束（不出招）。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Skill")
	bool bEndIfActivatedOnGround = false;

	/** 原作 SetForceSpeed(ActorUp × -5000)。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Skill", meta = (Units = "cm/s"))
	float FallForceSpeed = 5000.f;

	/** 原作 BlockTime > 1：下落时被静态网格挡住超过这么久就结束。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Skill", meta = (Units = "s"))
	float MaxBlockTime = 1.f;

	/** 已经落地、在播落地段（原作变量 落地攻击）。 */
	bool IsLandingAttack() const { return bLandingAttack; }

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void OnSkillMontageCompleted(int32 Index) override;
	virtual void OnSkillMontageBlendOut(int32 Index) override;
	virtual void OnSkillMontageInterrupted(int32 Index) override;
	virtual void OnSkillMontageCancelled(int32 Index) override;
	virtual void OnSkillMontageTick(int32 Index, float DeltaTime) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

private:
	/** [0] 结束后：播下落循环并往下冲；没有循环段就结束。 */
	void PlayFallLoop();
	/** 下落中落地：播落地段；没有就结束。 */
	void PlayLanding();
	/** 原作「位置状态.空中 + 动作状态.其他」：项目空中的普通动作状态是 Other / Jump / Fall（Jump / Fall 是项目扩展）。 */
	bool IsInPlainAirState() const;
	bool IsOnGround() const;
	class UWuwaMovementComponent* GetWuwaMovement() const;

	bool bLandingAttack = false;
	/** 下落循环把胶囊对 Pawn 通道改成了忽略，结束时要设回阻挡。 */
	bool bIgnoringPawns = false;
	/** 原作 DoOnce：离开空中状态只判断一次。 */
	bool bLeftAirChecked = false;
	float BlockTime = 0.f;
};
