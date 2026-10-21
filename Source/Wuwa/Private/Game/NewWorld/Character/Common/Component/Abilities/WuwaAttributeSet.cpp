// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffectExtension.h"
#include "Net/UnrealNetwork.h"




UWuwaAttributeSet::UWuwaAttributeSet()
{
	InitHealth(100);
	InitMaxHealth(100);
	InitLevel(1);
	InitCritDamage(1.f);
	InitEnergyEfficiency(1.f);
}

//这里面的参数是写死的,不要随便改名字
void UWuwaAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	//注册才能被网络同步
	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaAttributeSet, Level, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaAttributeSet, ElementType, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaAttributeSet, Attack, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaAttributeSet, Defense, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaAttributeSet, CritRate, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaAttributeSet, CritDamage, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaAttributeSet, EnergyEfficiency, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaAttributeSet, Energy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaAttributeSet, MaxEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaAttributeSet, ConcertoEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaAttributeSet, MaxConcertoEnergy, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaAttributeSet, SpecialEnergy1, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaAttributeSet, MaxSpecialEnergy1, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaAttributeSet, SpecialEnergy2, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaAttributeSet, MaxSpecialEnergy2, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaAttributeSet, SpecialEnergy3, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaAttributeSet, MaxSpecialEnergy3, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaAttributeSet, SpecialEnergy4, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaAttributeSet, MaxSpecialEnergy4, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaAttributeSet, SpecialEnergy5, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UWuwaAttributeSet, MaxSpecialEnergy5, COND_None, REPNOTIFY_Always);
	

}
//客户端上的 GAS 系统正确感知并处理这个值
void UWuwaAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaAttributeSet, Health, OldHealth);
}

void UWuwaAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaAttributeSet, MaxHealth, OldMaxHealth);
}

void UWuwaAttributeSet::OnRep_Level(const FGameplayAttributeData& OldLevel) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaAttributeSet, Level, OldLevel);
}

void UWuwaAttributeSet::OnRep_ElementType(const FGameplayAttributeData& OldElementType) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaAttributeSet, ElementType, OldElementType);
}

void UWuwaAttributeSet::OnRep_Attack(const FGameplayAttributeData& OldAttack) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaAttributeSet, Attack, OldAttack);
}

void UWuwaAttributeSet::OnRep_Defense(const FGameplayAttributeData& OldDefense) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaAttributeSet, Defense, OldDefense);
}

void UWuwaAttributeSet::OnRep_CritRate(const FGameplayAttributeData& OldCritRate) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaAttributeSet, CritRate, OldCritRate);
}

void UWuwaAttributeSet::OnRep_CritDamage(const FGameplayAttributeData& OldCritDamage) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaAttributeSet, CritDamage, OldCritDamage);
}

void UWuwaAttributeSet::OnRep_EnergyEfficiency(const FGameplayAttributeData& OldEnergyEfficiency) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaAttributeSet, EnergyEfficiency, OldEnergyEfficiency);
}

void UWuwaAttributeSet::OnRep_Energy(const FGameplayAttributeData& OldEnergy) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaAttributeSet, Energy, OldEnergy);
}

void UWuwaAttributeSet::OnRep_MaxEnergy(const FGameplayAttributeData& OldMaxEnergy) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaAttributeSet, MaxEnergy, OldMaxEnergy);
}

void UWuwaAttributeSet::OnRep_ConcertoEnergy(const FGameplayAttributeData& OldConcertoEnergy) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaAttributeSet, ConcertoEnergy, OldConcertoEnergy);
}

void UWuwaAttributeSet::OnRep_MaxConcertoEnergy(const FGameplayAttributeData& OldMaxConcertoEnergy) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaAttributeSet, MaxConcertoEnergy, OldMaxConcertoEnergy);
}

void UWuwaAttributeSet::OnRep_SpecialEnergy1(const FGameplayAttributeData& OldSpecialEnergy1) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaAttributeSet, SpecialEnergy1, OldSpecialEnergy1);
}

void UWuwaAttributeSet::OnRep_MaxSpecialEnergy1(const FGameplayAttributeData& OldMaxSpecialEnergy1) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaAttributeSet, MaxSpecialEnergy1, OldMaxSpecialEnergy1);
}

void UWuwaAttributeSet::OnRep_SpecialEnergy2(const FGameplayAttributeData& OldSpecialEnergy2) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaAttributeSet, SpecialEnergy2, OldSpecialEnergy2);
}

void UWuwaAttributeSet::OnRep_MaxSpecialEnergy2(const FGameplayAttributeData& OldMaxSpecialEnergy2) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaAttributeSet, MaxSpecialEnergy2, OldMaxSpecialEnergy2);
}

void UWuwaAttributeSet::OnRep_SpecialEnergy3(const FGameplayAttributeData& OldSpecialEnergy3) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaAttributeSet, SpecialEnergy3, OldSpecialEnergy3);
}

void UWuwaAttributeSet::OnRep_MaxSpecialEnergy3(const FGameplayAttributeData& OldMaxSpecialEnergy3) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaAttributeSet, MaxSpecialEnergy3, OldMaxSpecialEnergy3);
}

void UWuwaAttributeSet::OnRep_SpecialEnergy4(const FGameplayAttributeData& OldSpecialEnergy4) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaAttributeSet, SpecialEnergy4, OldSpecialEnergy4);
}

void UWuwaAttributeSet::OnRep_MaxSpecialEnergy4(const FGameplayAttributeData& OldMaxSpecialEnergy4) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaAttributeSet, MaxSpecialEnergy4, OldMaxSpecialEnergy4);
}

void UWuwaAttributeSet::OnRep_SpecialEnergy5(const FGameplayAttributeData& OldSpecialEnergy5) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaAttributeSet, SpecialEnergy5, OldSpecialEnergy5);
}

void UWuwaAttributeSet::OnRep_MaxSpecialEnergy5(const FGameplayAttributeData& OldMaxSpecialEnergy5) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UWuwaAttributeSet, MaxSpecialEnergy5, OldMaxSpecialEnergy5);
}

const TArray<TPair<FGameplayAttribute, FGameplayAttribute>>& UWuwaAttributeSet::GetClampedAttributes()
{
	static const TArray<TPair<FGameplayAttribute, FGameplayAttribute>> Pairs = {
		{GetHealthAttribute(), GetMaxHealthAttribute()},
		{GetEnergyAttribute(), GetMaxEnergyAttribute()},
		{GetConcertoEnergyAttribute(), GetMaxConcertoEnergyAttribute()},
		{GetSpecialEnergy1Attribute(), GetMaxSpecialEnergy1Attribute()},
		{GetSpecialEnergy2Attribute(), GetMaxSpecialEnergy2Attribute()},
		{GetSpecialEnergy3Attribute(), GetMaxSpecialEnergy3Attribute()},
		{GetSpecialEnergy4Attribute(), GetMaxSpecialEnergy4Attribute()},
		{GetSpecialEnergy5Attribute(), GetMaxSpecialEnergy5Attribute()},
	};
	return Pairs;
}

/*举个例子，MaxHealth = 100，Health = 100：

受到 120 点伤害（Instant GE，Health −120）
BaseValue = −20，CurrentValue 被 Clamp 为 0。看起来一切正常。
治疗 50 点（Instant GE，Health +50）
BaseValue = −20 + 50 = 30，CurrentValue = 30。*/
//在真正设定数值之前会执行这个函数,上面的例子说明了可能会失效所以要在下面的函数改
void UWuwaAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);
	
	// 血量和各种能量都夹在 [0, 上限] 之间
	for (const TPair<FGameplayAttribute, FGameplayAttribute>& Pair : GetClampedAttributes())
	{
		if (Attribute == Pair.Key)
		{
			NewValue = FMath::Clamp(NewValue, 0.f, Pair.Value.GetNumericValue(this));
			break;
		}
	}
}

/*SetHealth 是由 ATTRIBUTE_ACCESSORS 生成的，它会通过 ASC 去设置 BaseValue，所以这样就把根源修正了。PreAttributeChange 里的 Clamp 可以保留，用来处理 Duration/Infinite GE 通过 Modifier 影响 CurrentValue 的情况。*/
void UWuwaAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);
	// 上面例子说的问题：Instant GE 改的是 BaseValue，超出范围要在这里把 BaseValue 修回来
	for (const TPair<FGameplayAttribute, FGameplayAttribute>& Pair : GetClampedAttributes())
	{
		if (Data.EvaluatedData.Attribute == Pair.Key)
		{
			UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
			const float Max = Pair.Value.GetNumericValue(this);
			// 要看 BaseValue：CurrentValue 已经被 PreAttributeChange 夹过了（例：嘉贝强化 E 先清空弹药 1208001005，
			// BaseValue 变成 -100、CurrentValue 0，不修的话接着 1208003313 加 100 只回到 0）
			const float Base = ASC ? ASC->GetNumericAttributeBase(Pair.Key) : Pair.Key.GetNumericValue(this);
			// 和 SetHealth 一样经过 ASC 设 BaseValue
			if ((Base < 0.f || Base > Max) && ASC)
			{
				ASC->SetNumericAttributeBase(Pair.Key, FMath::Clamp(Base, 0.f, Max));
			}
			break;
		}
	}
}
