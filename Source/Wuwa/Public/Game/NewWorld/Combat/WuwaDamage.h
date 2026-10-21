#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayEffect.h"
#include "Subsystems/WorldSubsystem.h"
#include "WuwaDamage.generated.h"

class UAbilitySystemComponent;
struct FWuwaBulletHit;

/**
 * 伤害表的一行，对应原作 db_damage Damage（策划表"j.结算"）。行名 = 伤害 ID（子弹表 DamageId）。
 * 这不是最终数值：最终伤害 = 倍率 × 攻击者属性 × 一串乘区（见 WuwaDamage::CalculateHurt）。
 * 按技能等级取值的数组（倍率、浮动等）用原作 GetLevelValue 规则：等级在范围内取第 等级-1 项，0 取第一项，超出取最后一项。
 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaDamageInfo : public FTableRowBase
{
	GENERATED_BODY()

	/** 元素（原作元素 ID：0 物理、1 冷凝、2 热熔、3 导电、4 气动、5 衍射、6 湮灭）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	int32 Element = 0;

	/**
	 * 攻击类型（原作 Type），决定吃哪个攻击类型加成：0 普攻、1 共鸣技能、2 共鸣解放、3 变奏、4 普通技能、5 声骸（UWuwaCombatAttributeSet::AttackTypeBonus*）。
	 * 原作表里还有其他值（如 7、11），原作 GetAttackTypeDamageBonus 对它们不加成，这里一样。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	int32 Type = 0;

	/** 倍率乘的是攻击者哪个属性（原作属性 ID：2 生命上限、7 攻击、10 防御；表里不填时默认 7）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	int32 RelatedProperty = 7;

	/** 0 = 伤害，1 = 治疗（原作 CalculateType）。治疗还没接。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	int32 CalculateType = 0;

	/** 伤害飘字类型（原作 DamageTextType）：0 按元素选飘字配置，-1 不飘，其他值直接是飘字配置 Id。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	int32 DamageTextType = 0;

	/** 0 = 普通公式；非 0 = 原作特殊公式（还没移植，算 0 并警告）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	int32 FormulaType = 0;

	/** 按技能等级的倍率（小数：原作 1484 → 0.1484）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	TArray<float> RateLv;

	/** 按技能等级的随机浮动下限 / 上限（小数，1 = 不浮动）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	TArray<float> FluctuationLower;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	TArray<float> FluctuationUpper;

	// ---- 以下原样保存，后面的步骤（削韧、霸体、能量回复）再用；单位是原作的 ----

	/** 削韧（原作 ToughLv）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage|Later")
	TArray<float> ToughLv;
	/** 破霸体（原作 HardnessLv）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage|Later")
	TArray<float> HardnessLv;
	/** 共鸣能量回复（原作 Energy）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage|Later")
	TArray<float> Energy;
	/** 协奏能量回复（原作 ElementPower）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage|Later")
	TArray<float> ElementPower;
};

/** 扣血用的 GE：Instant，Health 加 SetByCaller "Damage"（传负数）。 */
UCLASS()
class WUWA_API UWuwaDamageEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UWuwaDamageEffect();
	static const FName SetByCallerName;
};

/**
 * 命中回能用的 GE：Instant，给攻击者自己加共鸣能量（Energy += SetByCaller "Energy"）和协奏能量（ConcertoEnergy += SetByCaller "Concerto"）。
 * 上限由属性集钳制（Energy ≤ MaxEnergy、ConcertoEnergy ≤ MaxConcertoEnergy）。
 */
UCLASS()
class WUWA_API UWuwaEnergyGainEffect : public UGameplayEffect
{
	GENERATED_BODY()

public:
	UWuwaEnergyGainEffect();
	static const FName EnergyName;
	static const FName ConcertoName;
};

/** 一次结算出来的伤害。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaDamageEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Damage") TWeakObjectPtr<AActor> Attacker;
	UPROPERTY(BlueprintReadOnly, Category = "Damage") TWeakObjectPtr<AActor> Victim;
	UPROPERTY(BlueprintReadOnly, Category = "Damage") int64 DamageId = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Damage") float Damage = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Damage") bool bCritical = false;
	UPROPERTY(BlueprintReadOnly, Category = "Damage") int32 Element = 0;
	/** 这次命中给攻击者回的共鸣能量 / 协奏能量（原作伤害表 Energy / ElementPower，按技能等级取值）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Damage") float EnergyGain = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Damage") float ConcertoGain = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Damage") FVector HitLocation = FVector::ZeroVector;
	/** 伤害表的飘字类型（伤害数字用）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Damage") int32 DamageTextType = 0;
	/** 这次扣的韧性（原作 ToughCalculation 的结果）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Damage") float ToughDecrease = 0.f;
	/** 结算后受击者的血量。 */
	UPROPERTY(BlueprintReadOnly, Category = "Damage") float VictimHealth = 0.f;
};

namespace WuwaDamage
{
	/** 原作 AbilityUtils.GetLevelValue。 */
	WUWA_API float GetLevelValue(const TArray<float>& Values, int32 Level, float Default);

	/**
	 * 原作 CharacterDamageCalculations.CalculateHurt（不含随机浮动和取整）：
	 *   (倍率 × 攻击者相关属性) × (暴击 ? 暴击伤害 : 1) × 防御系数 × (1 + 伤害加成 + 元素伤害加成 + 攻击类型加成)
	 *   × 抗性系数 × (1 - 受伤减免) × (1 - 元素伤害减免) × (1 + 特殊伤害加成) × 元素克制（原作常数为 1）
	 * 防御系数 = min(2, 1 / (目标防御 × (1 - 无视防御) / (800 + 攻击者等级 × 8) + 1))；
	 * 抗性系数（抗性 - 无视抗性 = d）：d ≤ 0 → 1 - d / 2；d < 0.8 → 1 - d；否则 1 / (1 + 5d)。
	 */
	WUWA_API float CalculateHurt(const FWuwaDamageInfo& Info, float Rate, const UAbilitySystemComponent* Attacker,
		const UAbilitySystemComponent* Target, bool bCritical);

	/** 原作 CalculateFormula 的收尾：× 随机浮动（Random01 在 [下限, 上限] 之间插值）后向上取整；治疗为负。 */
	WUWA_API float CalculateFinal(const FWuwaDamageInfo& Info, int32 SkillLevel, const UAbilitySystemComponent* Attacker,
		const UAbilitySystemComponent* Target, bool bCritical, float Random01);

	/** 原作 ToughCalculation：削韧值（按技能等级） × 攻击者 ToughChange × 受击者 ToughReduce × 攻击者 SkillToughRatio。 */
	WUWA_API float CalculateTough(const FWuwaDamageInfo& Info, int32 SkillLevel, const UAbilitySystemComponent* Attacker, const UAbilitySystemComponent* Target);

	/** 伤害表里的一行；没有时为空。 */
	WUWA_API const FWuwaDamageInfo* FindDamage(int64 DamageId);
}

/**
 * 伤害结算（≈ 原作命中后的 ExecuteBulletDamage / ProcessDamage），每个游戏世界一个。
 * 监听子弹命中：有伤害 ID 的命中在有权威的一端按公式算出伤害（掷暴击、随机浮动），用 UWuwaDamageEffect 扣受击者血量，再广播 OnDamage。
 * 削韧、霸体、能量回复、受击表现在后面的步骤里接。
 */
UCLASS()
class WUWA_API UWuwaDamageSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UWuwaDamageSubsystem* Get(const UObject* WorldContext);

	/** 结算一次伤害并扣血；没结算（伤害 ID 无效、没有 ASC、不是权威端等）返回 false。 */
	bool ApplyDamage(AActor* Attacker, AActor* Victim, int64 DamageId, const FVector& HitLocation, FWuwaDamageEvent* OutEvent = nullptr);

	DECLARE_MULTICAST_DELEGATE_OneParam(FWuwaOnDamage, const FWuwaDamageEvent&);
	/** 每次结算后广播（伤害数字、受击表现等以后从这里接）。 */
	FWuwaOnDamage OnDamage;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void HandleBulletHit(const FWuwaBulletHit& Hit);
	FDelegateHandle BulletHitHandle;
};
