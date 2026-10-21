// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "AbilitySystemComponent.h" 
#include "WuwaAttributeSet.generated.h"


#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
    GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)
/**
 * 
 */
UCLASS()
class WUWA_API UWuwaAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UWuwaAttributeSet();
	
	//注册可被网路同步事件
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	
	//属性
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Vital Attributes")
	FGameplayAttributeData Health;
	ATTRIBUTE_ACCESSORS(UWuwaAttributeSet, Health);
	
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Vital Attributes")
	FGameplayAttributeData MaxHealth;
	ATTRIBUTE_ACCESSORS(UWuwaAttributeSet, MaxHealth);

	UFUNCTION()
	void OnRep_Health(const FGameplayAttributeData& OldHealth) const;
	
	UFUNCTION()
	void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth) const;

	// —— 以下属性对应原作 EAttributeType（CharacterAttributeTypes），初始值由 UWuwaRoleAttributeInit 按原作基础属性表写入 ——

	/** 等级（原作 Proto_Lv）。初始值来自 UWuwaRoleDataAsset::Level，升级时改这里。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Level, Category = "Role Attributes")
	FGameplayAttributeData Level;
	ATTRIBUTE_ACCESSORS(UWuwaAttributeSet, Level);

	/** 元素（原作 Proto_ElementPropertyType），值是 EWuwaElement。伤害按"伤害元素 vs 目标元素"算克制。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ElementType, Category = "Role Attributes")
	FGameplayAttributeData ElementType;
	ATTRIBUTE_ACCESSORS(UWuwaAttributeSet, ElementType);

	/** 攻击（原作 Proto_Atk）。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Attack, Category = "Combat Attributes")
	FGameplayAttributeData Attack;
	ATTRIBUTE_ACCESSORS(UWuwaAttributeSet, Attack);

	/** 防御（原作 Proto_Def）。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Defense, Category = "Combat Attributes")
	FGameplayAttributeData Defense;
	ATTRIBUTE_ACCESSORS(UWuwaAttributeSet, Defense);

	/** 暴击率，小数（原作 Proto_Crit 是万分比：500 → 0.05）。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CritRate, Category = "Combat Attributes")
	FGameplayAttributeData CritRate;
	ATTRIBUTE_ACCESSORS(UWuwaAttributeSet, CritRate);

	/** 暴击伤害倍率，小数（原作 Proto_CritDamage：15000 → 1.5）。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CritDamage, Category = "Combat Attributes")
	FGameplayAttributeData CritDamage;
	ATTRIBUTE_ACCESSORS(UWuwaAttributeSet, CritDamage);

	/**
	 * 共鸣效率，小数（原作 Proto_EnergyEfficiency 是万分比：10000 → 1.0）。原作 AddEnergyExecution：回能 = 表值 × 共鸣效率。
	 * 客户端角色基础表里这一栏是空的（基础 100% 推断由服务端给），项目默认 1.0；武器 / 声骸加成以后加在这上面。
	 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_EnergyEfficiency, Category = "Energy Attributes")
	FGameplayAttributeData EnergyEfficiency;
	ATTRIBUTE_ACCESSORS(UWuwaAttributeSet, EnergyEfficiency);

	/** 共鸣能量，满了才能放共鸣解放 R（原作 Proto_Energy）。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Energy, Category = "Energy Attributes")
	FGameplayAttributeData Energy;
	ATTRIBUTE_ACCESSORS(UWuwaAttributeSet, Energy);

	/** 共鸣能量上限（原作 Proto_EnergyMax，原作单位）。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxEnergy, Category = "Energy Attributes")
	FGameplayAttributeData MaxEnergy;
	ATTRIBUTE_ACCESSORS(UWuwaAttributeSet, MaxEnergy);

	/** 协奏能量：元素图标外面那一圈（原作 Proto_ElementEnergy）。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ConcertoEnergy, Category = "Energy Attributes")
	FGameplayAttributeData ConcertoEnergy;
	ATTRIBUTE_ACCESSORS(UWuwaAttributeSet, ConcertoEnergy);

	/** 协奏能量上限（原作 Proto_ElementEnergyMax）。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxConcertoEnergy, Category = "Energy Attributes")
	FGameplayAttributeData MaxConcertoEnergy;
	ATTRIBUTE_ACCESSORS(UWuwaAttributeSet, MaxConcertoEnergy);

	/** 特殊能量 1（原作 Proto_SpecialEnergy1）：角色自己的能量条用哪一个由 DT_SpecialEnergyBarInfo 决定。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpecialEnergy1, Category = "SpecialEnergy Attributes")
	FGameplayAttributeData SpecialEnergy1;
	ATTRIBUTE_ACCESSORS(UWuwaAttributeSet, SpecialEnergy1);

	/** 特殊能量 1 的上限（原作 Proto_SpecialEnergy1Max，原作单位：长离的 1 号是 4 格）。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxSpecialEnergy1, Category = "SpecialEnergy Attributes")
	FGameplayAttributeData MaxSpecialEnergy1;
	ATTRIBUTE_ACCESSORS(UWuwaAttributeSet, MaxSpecialEnergy1);

	/** 特殊能量 2（原作 Proto_SpecialEnergy2）：角色自己的能量条用哪一个由 DT_SpecialEnergyBarInfo 决定。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpecialEnergy2, Category = "SpecialEnergy Attributes")
	FGameplayAttributeData SpecialEnergy2;
	ATTRIBUTE_ACCESSORS(UWuwaAttributeSet, SpecialEnergy2);

	/** 特殊能量 2 的上限（原作 Proto_SpecialEnergy2Max，原作单位：长离的 1 号是 4 格）。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxSpecialEnergy2, Category = "SpecialEnergy Attributes")
	FGameplayAttributeData MaxSpecialEnergy2;
	ATTRIBUTE_ACCESSORS(UWuwaAttributeSet, MaxSpecialEnergy2);

	/** 特殊能量 3（原作 Proto_SpecialEnergy3）：角色自己的能量条用哪一个由 DT_SpecialEnergyBarInfo 决定。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpecialEnergy3, Category = "SpecialEnergy Attributes")
	FGameplayAttributeData SpecialEnergy3;
	ATTRIBUTE_ACCESSORS(UWuwaAttributeSet, SpecialEnergy3);

	/** 特殊能量 3 的上限（原作 Proto_SpecialEnergy3Max，原作单位：长离的 1 号是 4 格）。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxSpecialEnergy3, Category = "SpecialEnergy Attributes")
	FGameplayAttributeData MaxSpecialEnergy3;
	ATTRIBUTE_ACCESSORS(UWuwaAttributeSet, MaxSpecialEnergy3);

	/** 特殊能量 4（原作 Proto_SpecialEnergy4）：角色自己的能量条用哪一个由 DT_SpecialEnergyBarInfo 决定。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpecialEnergy4, Category = "SpecialEnergy Attributes")
	FGameplayAttributeData SpecialEnergy4;
	ATTRIBUTE_ACCESSORS(UWuwaAttributeSet, SpecialEnergy4);

	/** 特殊能量 4 的上限（原作 Proto_SpecialEnergy4Max，原作单位：长离的 1 号是 4 格）。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxSpecialEnergy4, Category = "SpecialEnergy Attributes")
	FGameplayAttributeData MaxSpecialEnergy4;
	ATTRIBUTE_ACCESSORS(UWuwaAttributeSet, MaxSpecialEnergy4);

	/** 特殊能量 5（原作 Proto_SpecialEnergy5）：角色自己的能量条用哪一个由 DT_SpecialEnergyBarInfo 决定。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_SpecialEnergy5, Category = "SpecialEnergy Attributes")
	FGameplayAttributeData SpecialEnergy5;
	ATTRIBUTE_ACCESSORS(UWuwaAttributeSet, SpecialEnergy5);

	/** 特殊能量 5 的上限（原作 Proto_SpecialEnergy5Max，原作单位：长离的 1 号是 4 格）。 */
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxSpecialEnergy5, Category = "SpecialEnergy Attributes")
	FGameplayAttributeData MaxSpecialEnergy5;
	ATTRIBUTE_ACCESSORS(UWuwaAttributeSet, MaxSpecialEnergy5);

	UFUNCTION()
	void OnRep_Level(const FGameplayAttributeData& OldLevel) const;

	UFUNCTION()
	void OnRep_ElementType(const FGameplayAttributeData& OldElementType) const;

	UFUNCTION()
	void OnRep_Attack(const FGameplayAttributeData& OldAttack) const;

	UFUNCTION()
	void OnRep_Defense(const FGameplayAttributeData& OldDefense) const;

	UFUNCTION()
	void OnRep_CritRate(const FGameplayAttributeData& OldCritRate) const;

	UFUNCTION()
	void OnRep_CritDamage(const FGameplayAttributeData& OldCritDamage) const;
	UFUNCTION()
	void OnRep_EnergyEfficiency(const FGameplayAttributeData& OldEnergyEfficiency) const;

	UFUNCTION()
	void OnRep_Energy(const FGameplayAttributeData& OldEnergy) const;

	UFUNCTION()
	void OnRep_MaxEnergy(const FGameplayAttributeData& OldMaxEnergy) const;

	UFUNCTION()
	void OnRep_ConcertoEnergy(const FGameplayAttributeData& OldConcertoEnergy) const;

	UFUNCTION()
	void OnRep_MaxConcertoEnergy(const FGameplayAttributeData& OldMaxConcertoEnergy) const;

	UFUNCTION()
	void OnRep_SpecialEnergy1(const FGameplayAttributeData& OldSpecialEnergy1) const;

	UFUNCTION()
	void OnRep_MaxSpecialEnergy1(const FGameplayAttributeData& OldMaxSpecialEnergy1) const;

	UFUNCTION()
	void OnRep_SpecialEnergy2(const FGameplayAttributeData& OldSpecialEnergy2) const;

	UFUNCTION()
	void OnRep_MaxSpecialEnergy2(const FGameplayAttributeData& OldMaxSpecialEnergy2) const;

	UFUNCTION()
	void OnRep_SpecialEnergy3(const FGameplayAttributeData& OldSpecialEnergy3) const;

	UFUNCTION()
	void OnRep_MaxSpecialEnergy3(const FGameplayAttributeData& OldMaxSpecialEnergy3) const;

	UFUNCTION()
	void OnRep_SpecialEnergy4(const FGameplayAttributeData& OldSpecialEnergy4) const;

	UFUNCTION()
	void OnRep_MaxSpecialEnergy4(const FGameplayAttributeData& OldMaxSpecialEnergy4) const;

	UFUNCTION()
	void OnRep_SpecialEnergy5(const FGameplayAttributeData& OldSpecialEnergy5) const;

	UFUNCTION()
	void OnRep_MaxSpecialEnergy5(const FGameplayAttributeData& OldMaxSpecialEnergy5) const;
	
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;
	
	virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data) override;

private:
	/** 有上限的属性（当前值 → 上限）：血量、共鸣能量、协奏能量、特殊能量 1~5。 */
	static const TArray<TPair<FGameplayAttribute, FGameplayAttribute>>& GetClampedAttributes();

};
