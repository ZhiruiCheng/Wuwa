#include "Game/NewWorld/Combat/WuwaDamage.h"
#include "Game/Common/WuwaGameTags.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/NewWorld/Bullet/WuwaBulletSubsystem.h"
#include "Game/NewWorld/Buff/WuwaBuffLibrary.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAttributeSet.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaCombatAttributeSet.h"
#include "Game/NewWorld/Character/Common/WuwaCharactorBase.h"
#include "Game/NewWorld/Combat/WuwaCombatSettings.h"
#include "Game/NewWorld/Combat/WuwaHitComponent.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaDamage, Log, All);

static TAutoConsoleVariable<float> CVarDamageScale(TEXT("wuwa.Damage.Scale"), -1.f,
	TEXT("测试用伤害倍率（不是原作数据）；< 0 用 WuwaCombatSettings.DamageScale。"));
static TAutoConsoleVariable<float> CVarEnergyScale(TEXT("wuwa.Damage.EnergyScale"), -1.f,
	TEXT("测试用命中回能倍率（不是原作数据）；< 0 用 WuwaCombatSettings.EnergyGainScale。"));

const FName UWuwaDamageEffect::SetByCallerName(TEXT("Damage"));

UWuwaDamageEffect::UWuwaDamageEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	FSetByCallerFloat SetByCaller;
	SetByCaller.DataName = SetByCallerName;
	FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
	Modifier.Attribute = UWuwaAttributeSet::GetHealthAttribute();
	Modifier.ModifierOp = EGameplayModOp::Additive;
	Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
}

const FName UWuwaEnergyGainEffect::EnergyName(TEXT("Energy"));
const FName UWuwaEnergyGainEffect::ConcertoName(TEXT("Concerto"));

UWuwaEnergyGainEffect::UWuwaEnergyGainEffect()
{
	DurationPolicy = EGameplayEffectDurationType::Instant;
	for (const TPair<FGameplayAttribute, FName>& Gain : {
		TPair<FGameplayAttribute, FName>(UWuwaAttributeSet::GetEnergyAttribute(), EnergyName),
		TPair<FGameplayAttribute, FName>(UWuwaAttributeSet::GetConcertoEnergyAttribute(), ConcertoName) })
	{
		FSetByCallerFloat SetByCaller;
		SetByCaller.DataName = Gain.Value;
		FGameplayModifierInfo& Modifier = Modifiers.AddDefaulted_GetRef();
		Modifier.Attribute = Gain.Key;
		Modifier.ModifierOp = EGameplayModOp::Additive;
		Modifier.ModifierMagnitude = FGameplayEffectModifierMagnitude(SetByCaller);
	}
}

namespace WuwaDamage
{
	// 原作 CharacterDamageCalculations 的常数
	constexpr double DefenseFactorMax = 2.0;     // DAMAGE_CONSTANT1
	constexpr double DefenseBase = 800.0;        // DAMAGE_CONSTANT2
	constexpr double DefensePerLevel = 8.0;      // DAMAGE_CONSTANT3
	constexpr double NegativeResistance = 2.0;   // DAMAGE_CONSTANT4
	constexpr double ResistanceKnee = 0.8;       // DAMAGE_CONSTANT5
	constexpr double HighResistance = 5.0;       // DAMAGE_CONSTANT6
	constexpr double ElementCounterRate = 1.0;   // ELEMENT_CONTER_RATE（克制目前不加成）

	namespace
	{
		double Value(const UAbilitySystemComponent* ASC, const FGameplayAttribute& Attribute)
		{
			bool bFound = false;
			const double V = ASC && Attribute.IsValid() ? ASC->GetGameplayAttributeValue(Attribute, bFound) : 0.0;
			return bFound ? V : 0.0;
		}

		/** 原作属性 ID → 项目属性（伤害表 RelatedProperty）。 */
		FGameplayAttribute RelatedAttribute(int32 PropertyId)
		{
			switch (PropertyId)
			{
			case 2: return UWuwaAttributeSet::GetMaxHealthAttribute();
			case 7: return UWuwaAttributeSet::GetAttackAttribute();
			case 10: return UWuwaAttributeSet::GetDefenseAttribute();
			default: return FGameplayAttribute();
			}
		}
	}

	float GetLevelValue(const TArray<float>& Values, int32 Level, float Default)
	{
		if (Values.IsEmpty()) return Default;
		if (Level >= 1 && Level - 1 < Values.Num()) return Values[Level - 1];
		return Level == 0 ? Values[0] : Values.Last();
	}

	float CalculateHurt(const FWuwaDamageInfo& Info, float Rate, const UAbilitySystemComponent* Attacker, const UAbilitySystemComponent* Target, bool bCritical)
	{
		using C = UWuwaCombatAttributeSet;
		using S = UWuwaAttributeSet;
		const int32 Element = Info.Element;
		const FGameplayAttribute Related = RelatedAttribute(Info.RelatedProperty);
		if (!Related.IsValid())
		{
			UE_LOG(LogWuwaDamage, Warning, TEXT("Damage RelatedProperty %d is not mapped; damage is 0."), Info.RelatedProperty);
			return 0.f;
		}
		const double ElementBonus = Value(Attacker, C::ElementDamageBonusAttribute(Element));
		const double ElementReduce = FMath::Min(Value(Target, C::ElementDamageReduceAttribute(Element)), 1.0);
		// 抗性系数（分段）
		const double Resist = Value(Target, C::ElementResistanceAttribute(Element)) - Value(Attacker, C::ElementIgnoreResistanceAttribute(Element));
		const double ResistFactor = Resist <= 0.0 ? 1.0 - Resist / NegativeResistance : (Resist < ResistanceKnee ? 1.0 - Resist : 1.0 / (1.0 + Resist * HighResistance));
		const double TypeBonus = Value(Attacker, C::AttackTypeBonusAttribute(Info.Type));
		const double Base = Rate * Value(Attacker, Related);
		// 防御系数：攻击者等级越高、目标防御越低越接近 1；上限 2
		const double Defense = Value(Target, S::GetDefenseAttribute());
		const double IgnoreDefense = Value(Attacker, C::GetIgnoreDefenseRateAttribute());
		const double Level = Value(Attacker, S::GetLevelAttribute());
		const double DefenseFactor = FMath::Min(DefenseFactorMax, 1.0 / (Defense * (1.0 - IgnoreDefense) / (DefenseBase + Level * DefensePerLevel) + 1.0));
		const double Bonus = 1.0 + Value(Attacker, C::GetDamageBonusAttribute()) + ElementBonus + TypeBonus;
		const double Reduce = FMath::Min(Value(Target, C::GetDamageReduceAttribute()), 1.0);
		const double Special = 1.0 + Value(Attacker, C::GetSpecialDamageBonusAttribute());
		const double Critical = bCritical ? Value(Attacker, S::GetCritDamageAttribute()) : 1.0;
		const double Result = Base * Critical * DefenseFactor * Bonus * ResistFactor * (1.0 - Reduce) * (1.0 - ElementReduce) * Special * ElementCounterRate;
		return static_cast<float>(FMath::Max(0.0, Result));
	}

	float CalculateFinal(const FWuwaDamageInfo& Info, int32 SkillLevel, const UAbilitySystemComponent* Attacker, const UAbilitySystemComponent* Target,
		bool bCritical, float Random01)
	{
		if (Info.FormulaType != 0)
		{
			UE_LOG(LogWuwaDamage, Warning, TEXT("Damage formula type %d is not ported yet; damage is 0."), Info.FormulaType);
			return 0.f;
		}
		if (Info.CalculateType != 0)
		{
			UE_LOG(LogWuwaDamage, Warning, TEXT("Heal damage rows are not supported yet."));
			return 0.f;
		}
		const float Rate = GetLevelValue(Info.RateLv, SkillLevel, 0.f);
		const float Hurt = CalculateHurt(Info, Rate, Attacker, Target, bCritical);
		// 随机浮动：在 [下限, 上限] 之间（原作用随机种子 % 10000 插值）
		const float Upper = GetLevelValue(Info.FluctuationUpper, SkillLevel, 1.f);
		const float Lower = GetLevelValue(Info.FluctuationLower, SkillLevel, 1.f);
		return FMath::CeilToFloat(Hurt * FMath::Lerp(Lower, Upper, FMath::Clamp(Random01, 0.f, 1.f)));
	}

	float CalculateTough(const FWuwaDamageInfo& Info, int32 SkillLevel, const UAbilitySystemComponent* Attacker, const UAbilitySystemComponent* Target)
	{
		using C = UWuwaCombatAttributeSet;
		auto Ratio = [](const UAbilitySystemComponent* ASC, const FGameplayAttribute& Attribute)
		{
			// 没有这个属性集的单位按原作默认值 1
			return ASC && ASC->HasAttributeSetForAttribute(Attribute) ? ASC->GetNumericAttribute(Attribute) : 1.0;
		};
		const double Base = GetLevelValue(Info.ToughLv, SkillLevel, 0.f);
		return static_cast<float>(Base * Ratio(Attacker, C::GetToughChangeAttribute()) * Ratio(Target, C::GetToughReduceAttribute())
			* Ratio(Attacker, C::GetSkillToughRatioAttribute()));
	}

	const FWuwaDamageInfo* FindDamage(int64 DamageId)
	{
		const UDataTable* Table = GetDefault<UWuwaCombatSettings>()->DamageTable.LoadSynchronous();
		if (!Table || Table->GetRowStruct() != FWuwaDamageInfo::StaticStruct() || DamageId <= 0) return nullptr;
		return Table->FindRow<FWuwaDamageInfo>(FName(*LexToString(DamageId)), TEXT("WuwaDamage"), false);
	}
}

UWuwaDamageSubsystem* UWuwaDamageSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UWuwaDamageSubsystem>() : nullptr;
}

void UWuwaDamageSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	// 子弹命中 → 结算
	if (UWuwaBulletSubsystem* Bullets = Collection.InitializeDependency<UWuwaBulletSubsystem>())
	{
		BulletHitHandle = Bullets->OnBulletHit.AddUObject(this, &UWuwaDamageSubsystem::HandleBulletHit);
	}
}

void UWuwaDamageSubsystem::Deinitialize()
{
	if (UWuwaBulletSubsystem* Bullets = GetWorld() ? GetWorld()->GetSubsystem<UWuwaBulletSubsystem>() : nullptr)
	{
		Bullets->OnBulletHit.Remove(BulletHitHandle);
	}
	Super::Deinitialize();
}

bool UWuwaDamageSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UWuwaDamageSubsystem::HandleBulletHit(const FWuwaBulletHit& Hit)
{
	// 无敌（大招等）：不受伤害、不受击（原作客户端只换命中特效，伤害在服务端被免掉）
	if (const UAbilitySystemComponent* VictimASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Hit.Victim.Get());
		VictimASC && VictimASC->HasMatchingGameplayTag(FWuwaGameTags::Get().State_Combat_Invincible)) return;
	// 原作 OnHit：先结算伤害（含韧性），再做顿帧和受击动作；不造成伤害的子弹也有顿帧
	// 原作 能量恢复类GE：子弹第一次命中时给发射者加（例：心眼技命中 +1 心火）
	if (Hit.bFirstBulletHit && !Hit.Info.EnergyBuffIds.IsEmpty() && Hit.Attacker.IsValid() && Hit.Attacker->HasAuthority())
	{
		if (UAbilitySystemComponent* AttackerASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Hit.Attacker.Get()))
		{
			for (const int64 BuffId : Hit.Info.EnergyBuffIds) UWuwaBuffLibrary::AddBuff(AttackerASC, BuffId, AttackerASC);
		}
	}
	// 原作 命中后对攻击者发射GameplayEvent标签
	if (Hit.Info.HitEventToAttacker.IsValid() && Hit.Attacker.IsValid())
	{
		FGameplayEventData Payload;
		Payload.EventTag = Hit.Info.HitEventToAttacker;
		Payload.Instigator = Hit.Attacker.Get();
		Payload.Target = Hit.Victim.Get();
		UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Hit.Attacker.Get(), Hit.Info.HitEventToAttacker, Payload);
	}
	FWuwaDamageEvent Event;
	const bool bDamaged = Hit.DamageId != 0 && ApplyDamage(Hit.Attacker.Get(), Hit.Victim.Get(), Hit.DamageId, Hit.HitLocation, &Event);
	const AWuwaCharactorBase* Victim = Cast<AWuwaCharactorBase>(Hit.Victim.Get());
	if (Victim && Victim->HitComponent) Victim->HitComponent->OnHit(Hit, bDamaged ? &Event : nullptr);
}

bool UWuwaDamageSubsystem::ApplyDamage(AActor* Attacker, AActor* Victim, int64 DamageId, const FVector& HitLocation, FWuwaDamageEvent* OutEvent)
{
	if (!IsValid(Attacker) || !IsValid(Victim) || !Attacker->HasAuthority()) return false;
	const FWuwaDamageInfo* Info = WuwaDamage::FindDamage(DamageId);
	if (!Info)
	{
		UE_LOG(LogWuwaDamage, Warning, TEXT("Damage %lld is not in the damage table."), DamageId);
		return false;
	}
	UAbilitySystemComponent* Source = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Attacker);
	UAbilitySystemComponent* Target = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Victim);
	if (!Source || !Target) return false;

	const AWuwaCharactorBase* Character = Cast<AWuwaCharactorBase>(Attacker);
	const int32 SkillLevel = Character ? Character->SkillLevel : 1;
	bool bFound = false;
	const float CritRate = Source->GetGameplayAttributeValue(UWuwaAttributeSet::GetCritRateAttribute(), bFound);
	const bool bCritical = FMath::FRand() < CritRate;
	const float Scale = CVarDamageScale.GetValueOnGameThread() >= 0.f ? CVarDamageScale.GetValueOnGameThread() : GetDefault<UWuwaCombatSettings>()->DamageScale;
	const float Damage = FMath::CeilToFloat(WuwaDamage::CalculateFinal(*Info, SkillLevel, Source, Target, bCritical, FMath::FRand()) * Scale);

	FGameplayEffectSpecHandle Spec = Source->MakeOutgoingSpec(UWuwaDamageEffect::StaticClass(), 1.f, Source->MakeEffectContext());
	Spec.Data->SetSetByCallerMagnitude(UWuwaDamageEffect::SetByCallerName, -Damage);
	Source->ApplyGameplayEffectSpecToTarget(*Spec.Data, Target);

	// 命中回能（原作伤害表 Energy = 共鸣能量、ElementPower = 协奏能量，按技能等级取值；GAS 调试面板里一起列出这几项）。
	// 共鸣能量按原作 AddEnergyExecution 的写法乘共鸣效率（伤害回能在原作服务端，推断同一公式），再乘测试倍率；协奏不乘。
	const float Efficiency = Source->GetGameplayAttributeValue(UWuwaAttributeSet::GetEnergyEfficiencyAttribute(), bFound);
	const float EnergyScale = CVarEnergyScale.GetValueOnGameThread() >= 0.f ? CVarEnergyScale.GetValueOnGameThread() : GetDefault<UWuwaCombatSettings>()->EnergyGainScale;
	const float EnergyGain = WuwaDamage::GetLevelValue(Info->Energy, SkillLevel, 0.f) * (bFound ? Efficiency : 1.f) * EnergyScale;
	const float ConcertoGain = WuwaDamage::GetLevelValue(Info->ElementPower, SkillLevel, 0.f);
	if (EnergyGain > 0.f || ConcertoGain > 0.f)
	{
		FGameplayEffectSpecHandle Gain = Source->MakeOutgoingSpec(UWuwaEnergyGainEffect::StaticClass(), 1.f, Source->MakeEffectContext());
		Gain.Data->SetSetByCallerMagnitude(UWuwaEnergyGainEffect::EnergyName, EnergyGain);
		Gain.Data->SetSetByCallerMagnitude(UWuwaEnergyGainEffect::ConcertoName, ConcertoGain);
		Source->ApplyGameplayEffectSpecToSelf(*Gain.Data);
	}

	FWuwaDamageEvent Event;
	Event.EnergyGain = EnergyGain;
	Event.ConcertoGain = ConcertoGain;
	Event.Attacker = Attacker;
	Event.Victim = Victim;
	Event.DamageId = DamageId;
	Event.Damage = Damage;
	Event.bCritical = bCritical;
	Event.Element = Info->Element;
	Event.HitLocation = HitLocation;
	Event.DamageTextType = Info->DamageTextType;
	// 韧性（原作 QE1 / KE1）：算削韧值交给受击者
	Event.ToughDecrease = WuwaDamage::CalculateTough(*Info, SkillLevel, Source, Target);
	if (const AWuwaCharactorBase* VictimCharacter = Cast<AWuwaCharactorBase>(Victim))
	{
		if (VictimCharacter->HitComponent) VictimCharacter->HitComponent->ApplyToughDecrease(Event.ToughDecrease);
	}
	Event.VictimHealth = Target->GetGameplayAttributeValue(UWuwaAttributeSet::GetHealthAttribute(), bFound);
	UE_LOG(LogWuwaDamage, Verbose, TEXT("%s -> %s damage %lld: %.0f%s, health %.0f"), *GetNameSafe(Attacker), *GetNameSafe(Victim), DamageId, Damage,
		bCritical ? TEXT(" (crit)") : TEXT(""), Event.VictimHealth);
	if (OutEvent) *OutEvent = Event;
	OnDamage.Broadcast(Event);
	return true;
}
