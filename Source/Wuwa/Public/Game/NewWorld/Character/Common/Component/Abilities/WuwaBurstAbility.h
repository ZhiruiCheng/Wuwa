#pragma once

#include "CoreMinimal.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaSkillMontageAbility.h"
#include "WuwaBurstAbility.generated.h"

/**
 * 大招（原作 GA_ChangLi_Burst，按蓝图字节码移植）：在空中放时先落到地面再播蒙太奇。
 * - 激活：SetForceSpeed(0)；身上有 位置状态.空中 时从角色位置往下直线检测 GroundTraceDistance（原作 TraceGround 1000），
 *   打到地面就把角色放到 命中点 + 向上半高（原作 traceGroundWithGravity + 设置角色位置），然后播 [0]。
 *   原作 10 米内没有地面时什么都不播、技能挂着不结束；项目改成直接结束，免得技能卡住。
 * - 在地面：直接播 [0]。播完 / 淡出 / 打断 / 取消都结束（同通用技能）。
 * 原作的水面检测（落点在水里不放）项目没有水，不做。
 */
UCLASS()
class WUWA_API UWuwaBurstAbility : public UWuwaSkillMontageAbility
{
	GENERATED_BODY()

public:
	/** 原作 TraceGround：往下找地面的距离。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Skill", meta = (Units = "cm"))
	float GroundTraceDistance = 1000.f;

	/** 从 Start 往下找地面，返回角色应放的位置（地面点 + 向上半高）；没找到返回 false。 */
	static bool TraceGround(const class ACharacter* Character, const FVector& Start, float Distance, FVector& OutLocation);

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;
};
