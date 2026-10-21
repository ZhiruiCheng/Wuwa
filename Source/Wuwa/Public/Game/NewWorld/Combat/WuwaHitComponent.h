#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "Game/NewWorld/Combat/WuwaHitTypes.h"
#include "WuwaHitComponent.generated.h"

class AWuwaCharactorBase;
class UAbilitySystemComponent;
class UAnimMontage;
class UCurveFloat;
struct FWuwaBulletHit;
struct FWuwaBulletTimeScale;
struct FWuwaDamageEvent;

/**
 * 受击（≈ 原作 CharacterHitComponent + BaseDamageComponent 的韧性部分 + PawnTimeScaleComponent），挂在每个战斗单位上。
 *
 * 韧性（原作 BaseDamageComponent.mqr / BaseAttributeComponent.AutoRecoverAttr）：
 *   命中扣韧性（UWuwaDamageSubsystem 算好削韧值交给 ApplyToughDecrease）；扣完还有剩时，被削韧后 ToughRecoverDelayTime 秒内不回复；
 *   扣到 0 时进入虚弱 WeakTime 秒（State.Combat.Weak），虚弱期间回复速度清零，结束后恢复。平时每秒回复 ToughRecover，最多到 ToughMax。
 * 顿帧（原作 SetTimeScale）：按优先级取最高的一条，时间缩放写到 Actor 的 CustomTimeDilation；有曲线时按曲线从缩放值过渡回 1。
 * 受击动作（原作 CharacterHitComponent.OnHit / a6r）：这次命中扣了韧性、韧性已经 ≤ 0、没有禁止受击标签时，
 *   按攻击者受击效果表的 被击动作 转向（子弹受击方向 + Z 轴偏转）、播受击蒙太奇（AWuwaCharactorBase::BeHitMontages，原作 AI 状态机的 TaskBeHitMontage）、
 *   地面受击位移（按 100 / 质量）、硬直标签。战斗状态（霸体技能不能被打断）还没接：项目怪物还没有技能 / AI。
 */
UCLASS(ClassGroup = (Wuwa), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaHitComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	/** 重新算一次时间缩放（顿帧 × 大招时停）；时停开始 / 结束时由 UWuwaTimeStopSubsystem 调。 */
	void RefreshTimeScale() { UpdateTimeScale(); }

	UWuwaHitComponent();

	/** 扣韧性（值是已经算好的削韧量）。 */
	void ApplyToughDecrease(float Value);

	/** 子弹命中后（伤害已结算，Damage 为空表示这颗子弹不造成伤害）：顿帧、受击动作。 */
	void OnHit(const FWuwaBulletHit& Hit, const FWuwaDamageEvent* Damage);

	/** 加一条顿帧；返回编号。 */
	int32 SetTimeScale(int32 Priority, float Value, const UCurveFloat* Curve, float Duration);
	void RemoveTimeScale(int32 Id);
	/** 当前生效的时间缩放（1 = 不缩放）。 */
	float GetCurrentTimeScale() const { return CurrentTimeScale; }

	bool IsWeak() const { return WeakRemaining > 0.f; }
	float GetToughRecoverDelayRemaining() const { return RecoverDelayRemaining; }
	/** 最近一次受击动作（测试用；没有时为空）。 */
	TOptional<EWuwaHitAnim> GetLastHitAnim() const { return LastHitAnim; }
	UAnimMontage* GetLastHitMontage() const { return LastHitMontage.Get(); }

	/** 原作 GetHitRotator：按子弹受击方向算受击者的朝向；不转（方向 3）返回 false。 */
	static bool ComputeHitRotation(const FWuwaBulletHit& Hit, const AActor* Victim, FRotator& OutRotation);
	/** 原作 TimeScale.TimeDilation：有曲线时按 已过 / 时长 取曲线值归一，1 - 归一值 × (1 - 缩放值)。 */
	static float EvaluateTimeScale(float Value, const UCurveFloat* Curve, float Elapsed, float Duration);

	/** 推进虚弱 / 硬直 / 回复延迟计时和韧性回复（世界时间的秒数），再刷新时间缩放。Tick 里调；测试可以直接调。 */
	void AdvanceTimers(float WorldDeltaSeconds);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	struct FTimeScaleEntry
	{
		int32 Id = 0;
		int32 Priority = 0;
		float Value = 1.f;
		TWeakObjectPtr<const UCurveFloat> Curve;
		float StartTime = 0.f;
		float Duration = 0.f;
	};

	void ApplyTimeScale(const FWuwaBulletTimeScale& TimeScale);
	void UpdateTimeScale();
	bool CanPlayHitAnim() const;
	void PlayHitAnim(const FWuwaBulletHit& Hit, const FWuwaHitEffectInfo& Effect);
	void EnterWeak(float Duration);
	void ExitWeak();
	void SetTag(const FGameplayTag& Tag, bool bOn);
	UAbilitySystemComponent* GetAbilities() const;
	float GetAttribute(const struct FGameplayAttribute& Attribute) const;
	float WorldTime() const;

	TArray<FTimeScaleEntry> TimeScales;
	int32 NextTimeScaleId = 1;
	float CurrentTimeScale = 1.f;
	float RecoverDelayRemaining = 0.f;
	float WeakRemaining = 0.f;
	/** 虚弱前的回复速度（虚弱结束时恢复）。 */
	float SavedToughRecover = 0.f;
	float StiffRemaining = 0.f;
	TOptional<EWuwaHitAnim> LastHitAnim;
	TWeakObjectPtr<UAnimMontage> LastHitMontage;
};
