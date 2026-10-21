#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaRoleAttributeInit.h"

#include "AbilitySystemComponent.h"
#include "Game/Common/Resource/WuwaResourceSubsystem.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAttributeSet.h"
#include "Game/NewWorld/Character/Role/DataAsset/WuwaRoleDataAsset.h"

namespace WuwaRoleAttributeInit
{
	/** GE 里修改器的顺序：上限在当前值前面，否则当前值会被旧上限夹住（PreAttributeChange）。 */
	const TArray<FGameplayAttribute>& Attributes()
	{
		using S = UWuwaAttributeSet;
		static const TArray<FGameplayAttribute> List = {
			S::GetLevelAttribute(), S::GetElementTypeAttribute(),
			S::GetMaxHealthAttribute(), S::GetHealthAttribute(),
			S::GetAttackAttribute(), S::GetDefenseAttribute(), S::GetCritRateAttribute(), S::GetCritDamageAttribute(), S::GetEnergyEfficiencyAttribute(),
			S::GetMaxEnergyAttribute(), S::GetEnergyAttribute(),
			S::GetMaxConcertoEnergyAttribute(), S::GetConcertoEnergyAttribute(),
			S::GetMaxSpecialEnergy1Attribute(), S::GetSpecialEnergy1Attribute(),
			S::GetMaxSpecialEnergy2Attribute(), S::GetSpecialEnergy2Attribute(),
			S::GetMaxSpecialEnergy3Attribute(), S::GetSpecialEnergy3Attribute(),
			S::GetMaxSpecialEnergy4Attribute(), S::GetSpecialEnergy4Attribute(),
			S::GetMaxSpecialEnergy5Attribute(), S::GetSpecialEnergy5Attribute(),
		};
		return List;
	}

	/** SetByCaller 用属性名当键（不需要额外注册标签）。 */
	FName SetByCallerName(const FGameplayAttribute& Attribute)
	{
		return FName(Attribute.GetName());
	}

	UDataTable* LoadTable(const UObject* WorldContext, const TSoftObjectPtr<UDataTable>& Table)
	{
		if (Table.IsNull()) return nullptr;
		UWuwaResourceSubsystem* Resources = WorldContext ? UWuwaResourceSubsystem::Get(WorldContext) : nullptr;
		return Resources ? Resources->LoadSync(Table, TEXT("RoleAttributes")) : Table.LoadSynchronous();
	}
}

UWuwaInitRoleAttributesEffect::UWuwaInitRoleAttributesEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	for (const FGameplayAttribute& Attribute : WuwaRoleAttributeInit::Attributes())
	{
		FSetByCallerFloat SetByCaller;
		SetByCaller.DataName = WuwaRoleAttributeInit::SetByCallerName(Attribute);
		FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Attribute;
		Modifier.ModifierOp = EGameplayModOp::Override;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	}
}

bool UWuwaRoleAttributeInit::Compute(const UObject* WorldContext, const UWuwaRoleDataAsset* Role, TArray<TPair<FGameplayAttribute, float>>& OutValues, FString& OutError)
{
	using S = UWuwaAttributeSet;
	OutValues.Reset();
	if (!Role || Role->BasePropertyId <= 0)
	{
		OutError = TEXT("role has no BasePropertyId");
		return false;
	}
	const UWuwaRoleSettings* Settings = GetDefault<UWuwaRoleSettings>();
	const UDataTable* BaseTable = WuwaRoleAttributeInit::LoadTable(WorldContext, Settings->BasePropertyTable);
	const UDataTable* GrowthTable = WuwaRoleAttributeInit::LoadTable(WorldContext, Settings->PropertyGrowthTable);
	const FWuwaRoleBaseProperty* Base = BaseTable ? BaseTable->FindRow<FWuwaRoleBaseProperty>(*FString::FromInt(Role->BasePropertyId), TEXT("RoleAttributes"), false) : nullptr;
	if (!Base)
	{
		OutError = FString::Printf(TEXT("no row %d in the base property table (Project Settings > Wuwa Role)"), Role->BasePropertyId);
		return false;
	}
	// 同一等级有突破前、突破后两行（比如 80 级突破 5 / 6），取突破后的（推断：角色停在这个等级上限时已经突破）。
	const FWuwaRolePropertyGrowth* Growth = nullptr;
	if (GrowthTable)
	{
		GrowthTable->ForeachRow<FWuwaRolePropertyGrowth>(TEXT("RoleAttributes"), [&](const FName&, const FWuwaRolePropertyGrowth& Row)
		{
			if (Row.Level == Role->Level && (!Growth || Row.BreachLevel > Growth->BreachLevel)) Growth = &Row;
		});
	}
	if (!Growth)
	{
		OutError = FString::Printf(TEXT("no growth row for level %d"), Role->Level);
		return false;
	}

	const float MaxHealth = FMath::FloorToFloat(Base->MaxHealth * Growth->MaxHealthRatio);
	OutValues = {
		{S::GetLevelAttribute(), static_cast<float>(Role->Level)},
		{S::GetElementTypeAttribute(), static_cast<float>(Base->Element)},
		{S::GetMaxHealthAttribute(), MaxHealth},
		{S::GetHealthAttribute(), MaxHealth},
		{S::GetAttackAttribute(), FMath::FloorToFloat(Base->Attack * Growth->AttackRatio)},
		{S::GetDefenseAttribute(), FMath::FloorToFloat(Base->Defense * Growth->DefenseRatio)},
		{S::GetCritRateAttribute(), Base->CritRate},
		{S::GetCritDamageAttribute(), Base->CritDamage},
		{S::GetEnergyEfficiencyAttribute(), Base->EnergyEfficiency},
		{S::GetMaxEnergyAttribute(), Base->MaxEnergy},
		{S::GetEnergyAttribute(), 0.f},
		{S::GetMaxConcertoEnergyAttribute(), Base->MaxConcertoEnergy},
		{S::GetConcertoEnergyAttribute(), 0.f},
		{S::GetMaxSpecialEnergy1Attribute(), Base->MaxSpecialEnergy1},
		{S::GetSpecialEnergy1Attribute(), 0.f},
		{S::GetMaxSpecialEnergy2Attribute(), Base->MaxSpecialEnergy2},
		{S::GetSpecialEnergy2Attribute(), 0.f},
		{S::GetMaxSpecialEnergy3Attribute(), Base->MaxSpecialEnergy3},
		{S::GetSpecialEnergy3Attribute(), 0.f},
		{S::GetMaxSpecialEnergy4Attribute(), Base->MaxSpecialEnergy4},
		{S::GetSpecialEnergy4Attribute(), 0.f},
		{S::GetMaxSpecialEnergy5Attribute(), Base->MaxSpecialEnergy5},
		{S::GetSpecialEnergy5Attribute(), 0.f},
	};
	return true;
}

bool UWuwaRoleAttributeInit::Apply(UAbilitySystemComponent* ASC, const UWuwaRoleDataAsset* Role)
{
	if (!ASC) return false;
	TArray<TPair<FGameplayAttribute, float>> Values;
	FString Error;
	if (!Compute(ASC, Role, Values, Error))
	{
		UE_LOG(LogTemp, Warning, TEXT("%s: role attributes not initialized: %s."), *GetNameSafe(ASC->GetOwner()), *Error);
		return false;
	}
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(UWuwaInitRoleAttributesEffect::StaticClass(), 1.f, ASC->MakeEffectContext());
	if (!Spec.IsValid()) return false;
	for (const TPair<FGameplayAttribute, float>& Value : Values)
	{
		Spec.Data->SetSetByCallerMagnitude(WuwaRoleAttributeInit::SetByCallerName(Value.Key), Value.Value);
	}
	ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());
	return true;
}
