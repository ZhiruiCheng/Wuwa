#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaCombatAttributeSet.h"

namespace
{
	template <int32 Count>
	FGameplayAttribute Pick(const FGameplayAttribute (&Attributes)[Count], int32 Index)
	{
		return Index >= 0 && Index < Count ? Attributes[Index] : FGameplayAttribute();
	}
}

UWuwaCombatAttributeSet::UWuwaCombatAttributeSet()
{
	// 原作 flatbuffer 默认值：倍率类 10000（= 1），质量 100（MASS_RATE）
	InitToughChange(1.f);
	InitToughReduce(1.f);
	InitSkillToughRatio(1.f);
	InitMass(100.f);
}

void UWuwaCombatAttributeSet::PreAttributeBaseChange(const FGameplayAttribute& Attribute, float& NewValue) const
{
	Super::PreAttributeBaseChange(Attribute, NewValue);
	if (Attribute == GetToughAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, GetToughMax());
}

void UWuwaCombatAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	if (Attribute == GetToughAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, GetToughMax());
}

FGameplayAttribute UWuwaCombatAttributeSet::ElementDamageBonusAttribute(int32 Element)
{
	const FGameplayAttribute Attributes[ElementCount] = {GetElementDamageBonus0Attribute(), GetElementDamageBonus1Attribute(), GetElementDamageBonus2Attribute(),
		GetElementDamageBonus3Attribute(), GetElementDamageBonus4Attribute(), GetElementDamageBonus5Attribute(), GetElementDamageBonus6Attribute()};
	return Pick(Attributes, Element);
}

FGameplayAttribute UWuwaCombatAttributeSet::ElementResistanceAttribute(int32 Element)
{
	const FGameplayAttribute Attributes[ElementCount] = {GetElementResistance0Attribute(), GetElementResistance1Attribute(), GetElementResistance2Attribute(),
		GetElementResistance3Attribute(), GetElementResistance4Attribute(), GetElementResistance5Attribute(), GetElementResistance6Attribute()};
	return Pick(Attributes, Element);
}

FGameplayAttribute UWuwaCombatAttributeSet::ElementIgnoreResistanceAttribute(int32 Element)
{
	const FGameplayAttribute Attributes[ElementCount] = {GetElementIgnoreResistance0Attribute(), GetElementIgnoreResistance1Attribute(),
		GetElementIgnoreResistance2Attribute(), GetElementIgnoreResistance3Attribute(), GetElementIgnoreResistance4Attribute(),
		GetElementIgnoreResistance5Attribute(), GetElementIgnoreResistance6Attribute()};
	return Pick(Attributes, Element);
}

FGameplayAttribute UWuwaCombatAttributeSet::ElementDamageReduceAttribute(int32 Element)
{
	const FGameplayAttribute Attributes[ElementCount] = {GetElementDamageReduce0Attribute(), GetElementDamageReduce1Attribute(), GetElementDamageReduce2Attribute(),
		GetElementDamageReduce3Attribute(), GetElementDamageReduce4Attribute(), GetElementDamageReduce5Attribute(), GetElementDamageReduce6Attribute()};
	return Pick(Attributes, Element);
}

FGameplayAttribute UWuwaCombatAttributeSet::AttackTypeBonusAttribute(int32 Type)
{
	const FGameplayAttribute Attributes[AttackTypeCount] = {GetAttackTypeBonus0Attribute(), GetAttackTypeBonus1Attribute(), GetAttackTypeBonus2Attribute(),
		GetAttackTypeBonus3Attribute(), GetAttackTypeBonus4Attribute(), GetAttackTypeBonus5Attribute()};
	return Pick(Attributes, Type);
}
