#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAttributeSet.h"
#include "WuwaCombatAttributeSet.generated.h"

/**
 * 伤害公式用到的加成 / 抗性属性（≈ 原作 EAttributeType 里 Proto_DamageChange* / Proto_DamageResistance* 等），都是小数（原作万分比 / 10000）。
 * 元素下标就是原作元素 ID：0 物理、1 冷凝、2 热熔、3 导电、4 气动、5 衍射、6 湮灭（EWuwaElement）。
 * 攻击类型下标是原作伤害表 Type：0 普攻、1 共鸣技能、2 共鸣解放、3 变奏、4 普通技能、5 声骸。
 * 只在服务器上算伤害时读，暂不复制。初始值都是 0（没有加成、没有抗性），由属性初始化（角色 / 怪物）写入。
 */
UCLASS()
class WUWA_API UWuwaCombatAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	static constexpr int32 ElementCount = 7;
	static constexpr int32 AttackTypeCount = 6;

	/** 伤害加成（Proto_DamageChange）：所有伤害。 */
	UPROPERTY(BlueprintReadOnly, Category = "Damage") FGameplayAttributeData DamageBonus;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, DamageBonus);
	/** 特殊伤害加成（Proto_SpecialDamageChange），单独一个乘区。 */
	UPROPERTY(BlueprintReadOnly, Category = "Damage") FGameplayAttributeData SpecialDamageBonus;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, SpecialDamageBonus);
	/** 受到伤害减免（Proto_DamageReduce），最多 1。 */
	UPROPERTY(BlueprintReadOnly, Category = "Damage") FGameplayAttributeData DamageReduce;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, DamageReduce);
	/** 无视防御比例（Proto_IgnoreDefRate）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Damage") FGameplayAttributeData IgnoreDefenseRate;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, IgnoreDefenseRate);

	// 元素伤害加成（Proto_DamageChangePhys / Element1~6）
	UPROPERTY(BlueprintReadOnly, Category = "Element") FGameplayAttributeData ElementDamageBonus0;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ElementDamageBonus0);
	UPROPERTY(BlueprintReadOnly, Category = "Element") FGameplayAttributeData ElementDamageBonus1;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ElementDamageBonus1);
	UPROPERTY(BlueprintReadOnly, Category = "Element") FGameplayAttributeData ElementDamageBonus2;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ElementDamageBonus2);
	UPROPERTY(BlueprintReadOnly, Category = "Element") FGameplayAttributeData ElementDamageBonus3;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ElementDamageBonus3);
	UPROPERTY(BlueprintReadOnly, Category = "Element") FGameplayAttributeData ElementDamageBonus4;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ElementDamageBonus4);
	UPROPERTY(BlueprintReadOnly, Category = "Element") FGameplayAttributeData ElementDamageBonus5;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ElementDamageBonus5);
	UPROPERTY(BlueprintReadOnly, Category = "Element") FGameplayAttributeData ElementDamageBonus6;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ElementDamageBonus6);

	// 元素抗性（Proto_DamageResistancePhys / Element1~6）
	UPROPERTY(BlueprintReadOnly, Category = "Element") FGameplayAttributeData ElementResistance0;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ElementResistance0);
	UPROPERTY(BlueprintReadOnly, Category = "Element") FGameplayAttributeData ElementResistance1;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ElementResistance1);
	UPROPERTY(BlueprintReadOnly, Category = "Element") FGameplayAttributeData ElementResistance2;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ElementResistance2);
	UPROPERTY(BlueprintReadOnly, Category = "Element") FGameplayAttributeData ElementResistance3;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ElementResistance3);
	UPROPERTY(BlueprintReadOnly, Category = "Element") FGameplayAttributeData ElementResistance4;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ElementResistance4);
	UPROPERTY(BlueprintReadOnly, Category = "Element") FGameplayAttributeData ElementResistance5;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ElementResistance5);
	UPROPERTY(BlueprintReadOnly, Category = "Element") FGameplayAttributeData ElementResistance6;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ElementResistance6);

	// 无视元素抗性（Proto_IgnoreDamageResistancePhys / Element1~6）
	UPROPERTY(BlueprintReadOnly, Category = "Element") FGameplayAttributeData ElementIgnoreResistance0;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ElementIgnoreResistance0);
	UPROPERTY(BlueprintReadOnly, Category = "Element") FGameplayAttributeData ElementIgnoreResistance1;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ElementIgnoreResistance1);
	UPROPERTY(BlueprintReadOnly, Category = "Element") FGameplayAttributeData ElementIgnoreResistance2;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ElementIgnoreResistance2);
	UPROPERTY(BlueprintReadOnly, Category = "Element") FGameplayAttributeData ElementIgnoreResistance3;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ElementIgnoreResistance3);
	UPROPERTY(BlueprintReadOnly, Category = "Element") FGameplayAttributeData ElementIgnoreResistance4;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ElementIgnoreResistance4);
	UPROPERTY(BlueprintReadOnly, Category = "Element") FGameplayAttributeData ElementIgnoreResistance5;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ElementIgnoreResistance5);
	UPROPERTY(BlueprintReadOnly, Category = "Element") FGameplayAttributeData ElementIgnoreResistance6;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ElementIgnoreResistance6);

	// 元素伤害减免（Proto_DamageReducePhys / Element1~6），最多 1
	UPROPERTY(BlueprintReadOnly, Category = "Element") FGameplayAttributeData ElementDamageReduce0;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ElementDamageReduce0);
	UPROPERTY(BlueprintReadOnly, Category = "Element") FGameplayAttributeData ElementDamageReduce1;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ElementDamageReduce1);
	UPROPERTY(BlueprintReadOnly, Category = "Element") FGameplayAttributeData ElementDamageReduce2;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ElementDamageReduce2);
	UPROPERTY(BlueprintReadOnly, Category = "Element") FGameplayAttributeData ElementDamageReduce3;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ElementDamageReduce3);
	UPROPERTY(BlueprintReadOnly, Category = "Element") FGameplayAttributeData ElementDamageReduce4;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ElementDamageReduce4);
	UPROPERTY(BlueprintReadOnly, Category = "Element") FGameplayAttributeData ElementDamageReduce5;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ElementDamageReduce5);
	UPROPERTY(BlueprintReadOnly, Category = "Element") FGameplayAttributeData ElementDamageReduce6;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ElementDamageReduce6);

	// 攻击类型伤害加成（Proto_DamageChangeAuto / Cast / Ultra / Qte / NormalSkill / Phantom）
	UPROPERTY(BlueprintReadOnly, Category = "AttackType") FGameplayAttributeData AttackTypeBonus0;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, AttackTypeBonus0);
	UPROPERTY(BlueprintReadOnly, Category = "AttackType") FGameplayAttributeData AttackTypeBonus1;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, AttackTypeBonus1);
	UPROPERTY(BlueprintReadOnly, Category = "AttackType") FGameplayAttributeData AttackTypeBonus2;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, AttackTypeBonus2);
	UPROPERTY(BlueprintReadOnly, Category = "AttackType") FGameplayAttributeData AttackTypeBonus3;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, AttackTypeBonus3);
	UPROPERTY(BlueprintReadOnly, Category = "AttackType") FGameplayAttributeData AttackTypeBonus4;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, AttackTypeBonus4);
	UPROPERTY(BlueprintReadOnly, Category = "AttackType") FGameplayAttributeData AttackTypeBonus5;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, AttackTypeBonus5);

	// ---- 韧性（原作 Proto_Tough 等，见 UWuwaHitComponent）：数值单位同原作，比例是小数 ----

	/** 当前韧性 / 上限：命中扣，扣到 0 才会播受击动作。 */
	UPROPERTY(BlueprintReadOnly, Category = "Tough") FGameplayAttributeData Tough;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, Tough);
	UPROPERTY(BlueprintReadOnly, Category = "Tough") FGameplayAttributeData ToughMax;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ToughMax);
	/** 每秒回复多少韧性。 */
	UPROPERTY(BlueprintReadOnly, Category = "Tough") FGameplayAttributeData ToughRecover;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ToughRecover);
	/** 攻击者削韧倍率（Proto_ToughChange，默认 1）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Tough") FGameplayAttributeData ToughChange;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ToughChange);
	/** 受击者被削韧倍率（Proto_ToughReduce，默认 1）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Tough") FGameplayAttributeData ToughReduce;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ToughReduce);
	/** 攻击者技能削韧倍率（Proto_SkillToughRatio，默认 1）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Tough") FGameplayAttributeData SkillToughRatio;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, SkillToughRatio);
	/** 被削韧后多久才开始回复（秒）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Tough") FGameplayAttributeData ToughRecoverDelayTime;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, ToughRecoverDelayTime);
	/** 韧性扣空后的虚弱时间（秒）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Tough") FGameplayAttributeData WeakTime;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, WeakTime);
	/** 质量（Proto_Mass，默认 100）：受击位移按 100 / 质量 缩放。 */
	UPROPERTY(BlueprintReadOnly, Category = "Tough") FGameplayAttributeData Mass;
	ATTRIBUTE_ACCESSORS(UWuwaCombatAttributeSet, Mass);

	UWuwaCombatAttributeSet();
	/** 韧性夹在 [0, 上限]。 */
	virtual void PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	/** 按元素 ID 取对应属性（越界返回无效属性）。 */
	static FGameplayAttribute ElementDamageBonusAttribute(int32 Element);
	static FGameplayAttribute ElementResistanceAttribute(int32 Element);
	static FGameplayAttribute ElementIgnoreResistanceAttribute(int32 Element);
	static FGameplayAttribute ElementDamageReduceAttribute(int32 Element);
	/** 按伤害表 Type 取攻击类型加成属性。 */
	static FGameplayAttribute AttackTypeBonusAttribute(int32 Type);
};
