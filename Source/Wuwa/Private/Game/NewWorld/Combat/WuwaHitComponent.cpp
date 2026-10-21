#include "Game/NewWorld/Combat/WuwaHitComponent.h"
#include "Game/NewWorld/Combat/WuwaTimeStopSubsystem.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Curves/CurveFloat.h"
#include "Engine/DataTable.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/RootMotionSource.h"
#include "Game/NewWorld/Bullet/WuwaBulletSubsystem.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaCombatAttributeSet.h"
#include "Game/NewWorld/Character/Common/WuwaCharactorBase.h"
#include "Game/NewWorld/Combat/WuwaDamage.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaHit, Log, All);

namespace WuwaHit
{
	/** 原作 MASS_RATE：受击位移 × 100 / 质量。 */
	constexpr float MassRate = 100.f;

	const FGameplayTag& WeakTag() { static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("State.Combat.Weak")); return Tag; }
	const FGameplayTag& StiffTag() { static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("State.Combat.Stiff")); return Tag; }
	const FGameplayTag& HitImmuneTag() { static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("State.Combat.HitImmune")); return Tag; }
}

UWuwaHitComponent::UWuwaHitComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

UAbilitySystemComponent* UWuwaHitComponent::GetAbilities() const
{
	const AWuwaCharactorBase* Owner = Cast<AWuwaCharactorBase>(GetOwner());
	return Owner ? Owner->GetAbilitySystemComponent() : nullptr;
}

float UWuwaHitComponent::GetAttribute(const FGameplayAttribute& Attribute) const
{
	const UAbilitySystemComponent* Abilities = GetAbilities();
	return Abilities && Abilities->HasAttributeSetForAttribute(Attribute) ? Abilities->GetNumericAttribute(Attribute) : 0.f;
}

float UWuwaHitComponent::WorldTime() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}

void UWuwaHitComponent::SetTag(const FGameplayTag& Tag, bool bOn)
{
	UAbilitySystemComponent* Abilities = GetAbilities();
	if (!Abilities || !Tag.IsValid()) return;
	if (bOn && !Abilities->HasMatchingGameplayTag(Tag)) Abilities->AddLooseGameplayTag(Tag);
	else if (!bOn && Abilities->HasMatchingGameplayTag(Tag)) Abilities->RemoveLooseGameplayTag(Tag);
}

// ---------------------------------------------------------------- 韧性

void UWuwaHitComponent::ApplyToughDecrease(float Value)
{
	UAbilitySystemComponent* Abilities = GetAbilities();
	if (!Abilities || !Abilities->HasAttributeSetForAttribute(UWuwaCombatAttributeSet::GetToughAttribute())) return;
	// 原作 mqr：先扣，扣完还有剩 → 回复延迟；扣到 0 → 虚弱（已经虚弱就不再进）
	if (Value != 0.f)
	{
		Abilities->SetNumericAttributeBase(UWuwaCombatAttributeSet::GetToughAttribute(),
			Abilities->GetNumericAttributeBase(UWuwaCombatAttributeSet::GetToughAttribute()) - Value);
	}
	if (Abilities->GetNumericAttribute(UWuwaCombatAttributeSet::GetToughAttribute()) > 0.f)
	{
		const float Delay = GetAttribute(UWuwaCombatAttributeSet::GetToughRecoverDelayTimeAttribute());
		if (Delay > 0.f && Value != 0.f) RecoverDelayRemaining = Delay;
	}
	else if (!IsWeak())
	{
		const float Weak = GetAttribute(UWuwaCombatAttributeSet::GetWeakTimeAttribute());
		if (Weak > 0.f) EnterWeak(Weak);
	}
}

void UWuwaHitComponent::EnterWeak(float Duration)
{
	UAbilitySystemComponent* Abilities = GetAbilities();
	WeakRemaining = Duration;
	SetTag(WuwaHit::WeakTag(), true);
	// 原作：虚弱期间回复速度清零（结束后由服务器恢复，这里自己恢复）
	SavedToughRecover = Abilities->GetNumericAttributeBase(UWuwaCombatAttributeSet::GetToughRecoverAttribute());
	Abilities->SetNumericAttributeBase(UWuwaCombatAttributeSet::GetToughRecoverAttribute(), 0.f);
}

void UWuwaHitComponent::ExitWeak()
{
	WeakRemaining = 0.f;
	SetTag(WuwaHit::WeakTag(), false);
	if (UAbilitySystemComponent* Abilities = GetAbilities())
	{
		Abilities->SetNumericAttributeBase(UWuwaCombatAttributeSet::GetToughRecoverAttribute(), SavedToughRecover);
	}
}

void UWuwaHitComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// 自身的时间缩放不影响这些计时（原作按世界时间 / 世界流速）
	AdvanceTimers(GetWorld() ? GetWorld()->GetDeltaSeconds() : DeltaTime);
}

void UWuwaHitComponent::AdvanceTimers(float Delta)
{
	if (WeakRemaining > 0.f && (WeakRemaining -= Delta) <= 0.f) ExitWeak();
	if (StiffRemaining > 0.f && (StiffRemaining -= Delta) <= 0.f) SetTag(WuwaHit::StiffTag(), false);
	if (RecoverDelayRemaining > 0.f)
	{
		RecoverDelayRemaining -= Delta;
	}
	else if (UAbilitySystemComponent* Abilities = GetAbilities())
	{
		// 原作 AutoRecoverAttr：Tough += ToughRecover × 秒
		const float Recover = GetAttribute(UWuwaCombatAttributeSet::GetToughRecoverAttribute());
		const float Tough = GetAttribute(UWuwaCombatAttributeSet::GetToughAttribute());
		if (Recover != 0.f && Tough < GetAttribute(UWuwaCombatAttributeSet::GetToughMaxAttribute()))
		{
			Abilities->SetNumericAttributeBase(UWuwaCombatAttributeSet::GetToughAttribute(), Tough + Recover * Delta);
		}
	}
	UpdateTimeScale();
}

// ---------------------------------------------------------------- 顿帧

float UWuwaHitComponent::EvaluateTimeScale(float Value, const UCurveFloat* Curve, float Elapsed, float Duration)
{
	if (!Curve || Duration <= 0.f) return Value;
	float TimeMin = 0.f, TimeMax = 1.f, ValueMin = 0.f, ValueMax = 1.f;
	Curve->GetTimeRange(TimeMin, TimeMax);
	Curve->GetValueRange(ValueMin, ValueMax);
	const float Progress = FMath::Clamp(Elapsed / Duration, 0.f, 1.f);
	const float CurveValue = Curve->GetFloatValue(FMath::Lerp(TimeMin, TimeMax, Progress));
	const float Normalized = ValueMax > ValueMin ? FMath::Clamp((CurveValue - ValueMin) / (ValueMax - ValueMin), 0.f, 1.f) : 1.f;
	return 1.f - Normalized * (1.f - Value);
}

int32 UWuwaHitComponent::SetTimeScale(int32 Priority, float Value, const UCurveFloat* Curve, float Duration)
{
	if (Duration <= 0.f) return -1;
	FTimeScaleEntry& Entry = TimeScales.AddDefaulted_GetRef();
	Entry.Id = NextTimeScaleId++;
	Entry.Priority = Priority;
	Entry.Value = FMath::Max(Value, 0.f);
	Entry.Curve = Curve;
	Entry.StartTime = WorldTime();
	Entry.Duration = Duration;
	UpdateTimeScale();
	return Entry.Id;
}

void UWuwaHitComponent::RemoveTimeScale(int32 Id)
{
	TimeScales.RemoveAll([Id](const FTimeScaleEntry& Entry) { return Entry.Id == Id; });
	UpdateTimeScale();
}

void UWuwaHitComponent::UpdateTimeScale()
{
	const float Now = WorldTime();
	TimeScales.RemoveAll([Now](const FTimeScaleEntry& Entry) { return Now >= Entry.StartTime + Entry.Duration; });
	// 原作 CompareScalePriority：优先级高的在前，同优先级取更慢的
	const FTimeScaleEntry* Top = nullptr;
	float TopValue = 1.f;
	for (const FTimeScaleEntry& Entry : TimeScales)
	{
		const float Value = EvaluateTimeScale(Entry.Value, Entry.Curve.Get(), Now - Entry.StartTime, Entry.Duration);
		if (!Top || Entry.Priority > Top->Priority || (Entry.Priority == Top->Priority && Value < TopValue))
		{
			Top = &Entry;
			TopValue = Value;
		}
	}
	CurrentTimeScale = Top ? TopValue : 1.f;
	// 大招时停冻住（停移动）时整体为 0：顿帧每帧都会重算，不能把时停覆盖掉
	if (AActor* Owner = GetOwner())
	{
		const UWuwaTimeStopSubsystem* TimeStop = UWuwaTimeStopSubsystem::Get(Owner);
		Owner->CustomTimeDilation = TimeStop && TimeStop->IsMovementFrozen(Owner) ? 0.f : CurrentTimeScale;
	}
}

void UWuwaHitComponent::ApplyTimeScale(const FWuwaBulletTimeScale& TimeScale)
{
	if (TimeScale.Duration > 0.f) SetTimeScale(TimeScale.Priority, TimeScale.Value, TimeScale.Curve.LoadSynchronous(), TimeScale.Duration);
}

// ---------------------------------------------------------------- 受击

bool UWuwaHitComponent::ComputeHitRotation(const FWuwaBulletHit& Hit, const AActor* Victim, FRotator& OutRotation)
{
	if (!Victim) return false;
	OutRotation = Victim->GetActorRotation();
	FVector Direction = FVector::ZeroVector;
	const FVector VictimLocation = Victim->GetActorLocation();
	switch (Hit.Info.HitDirection)
	{
	case 0:   // 面向攻击者
		if (const AActor* Attacker = Hit.Attacker.Get()) Direction = Attacker->GetActorLocation() - VictimLocation;
		break;
	case 1:   // 面向子弹
		Direction = Hit.BulletTransform.GetLocation() - VictimLocation;
		break;
	case 2:   // 迎着子弹运动方向（项目子弹不移动：取子弹朝向的反方向）
		Direction = -Hit.BulletTransform.GetRotation().GetForwardVector();
		break;
	case 4:   // 原作：在子弹里背向子弹中心，否则面向子弹（项目按面向子弹）
		Direction = Hit.BulletTransform.GetLocation() - VictimLocation;
		break;
	default:  // 3：不转
		return false;
	}
	Direction.Z = 0.0;
	if (Direction.IsNearlyZero()) return true;
	OutRotation = FRotator(0.0, Direction.Rotation().Yaw, 0.0);
	return true;
}

bool UWuwaHitComponent::CanPlayHitAnim() const
{
	const UAbilitySystemComponent* Abilities = GetAbilities();
	return !(Abilities && Abilities->HasMatchingGameplayTag(WuwaHit::HitImmuneTag()));
}

void UWuwaHitComponent::OnHit(const FWuwaBulletHit& Hit, const FWuwaDamageEvent* Damage)
{
	// 顿帧（原作 iwr）：攻击者攻击顿帧；受击者受击顿帧（持续碰撞的子弹没有）
	AWuwaCharactorBase* Attacker = Cast<AWuwaCharactorBase>(Hit.Attacker.Get());
	if (Attacker && !Hit.Info.bTimeScaleIgnoreAttacker)
	{
		if (UWuwaHitComponent* AttackerHit = Attacker->HitComponent) AttackerHit->ApplyTimeScale(Hit.Info.AttackerTimeScale);
	}
	if (!Hit.Info.bContinuousCollision) ApplyTimeScale(Hit.Info.VictimTimeScale);

	// 受击动作（原作 a6r）：要有受击效果；这次扣了韧性且韧性已经 ≤ 0；没有禁止受击
	const UDataTable* Effects = Attacker ? Attacker->HitEffectTable.Get() : nullptr;
	const FWuwaHitEffectInfo* Effect = Effects && !Hit.Info.BeHitEffect.IsNone()
		? Effects->FindRow<FWuwaHitEffectInfo>(Hit.Info.BeHitEffect, TEXT("WuwaHit"), false) : nullptr;
	if (!Effect || !Damage || Damage->ToughDecrease <= 0.f || GetAttribute(UWuwaCombatAttributeSet::GetToughAttribute()) > 0.f || !CanPlayHitAnim()) return;
	PlayHitAnim(Hit, *Effect);
}

void UWuwaHitComponent::PlayHitAnim(const FWuwaBulletHit& Hit, const FWuwaHitEffectInfo& Effect)
{
	AWuwaCharactorBase* Owner = Cast<AWuwaCharactorBase>(GetOwner());
	if (!Owner) return;
	LastHitAnim = Effect.BeHitAnim;
	// 转向（原作 SetHitRotator：受击方向 + Z 轴偏转）
	FRotator Rotation;
	if (ComputeHitRotation(Hit, Owner, Rotation))
	{
		Rotation.Yaw += Effect.RotationOffsetZ;
		Owner->SetActorRotation(Rotation);
	}
	// 受击蒙太奇（原作 AI 状态机 TaskBeHitMontage 的 MontageMap）
	UAnimMontage* Montage = Owner->BeHitMontages.FindRef(static_cast<int32>(Effect.BeHitAnim)).LoadSynchronous();
	UAnimInstance* Anim = Owner->GetMesh() ? Owner->GetMesh()->GetAnimInstance() : nullptr;
	LastHitMontage = Montage;
	if (Montage && Anim) Anim->Montage_Play(Montage);
	// 地面受击位移（原作 A6r：局部速度 × 100 / 质量，持续 地面受击移动时间）
	UCharacterMovementComponent* Movement = Owner->GetCharacterMovement();
	const float Mass = FMath::Max(GetAttribute(UWuwaCombatAttributeSet::GetMassAttribute()), 1.f);
	if (Movement && Effect.GroundMoveTime > 0.f && !Effect.GroundSpeed.IsNearlyZero())
	{
		TSharedPtr<FRootMotionSource_ConstantForce> Push = MakeShared<FRootMotionSource_ConstantForce>();
		Push->InstanceName = TEXT("WuwaHitPush");
		Push->AccumulateMode = ERootMotionAccumulateMode::Additive;
		Push->Priority = 5;
		Push->Force = Owner->GetActorRotation().RotateVector(FVector(Effect.GroundSpeed.X, Effect.GroundSpeed.Y, 0.0)) * (WuwaHit::MassRate / Mass);
		Push->Duration = Effect.GroundMoveTime;
		Movement->ApplyRootMotionSource(Push);
	}
	// 硬直（原作 ActiveStiff）
	if (Effect.StiffTime > 0.f)
	{
		StiffRemaining = Effect.StiffTime;
		SetTag(WuwaHit::StiffTag(), true);
	}
	UE_LOG(LogWuwaHit, Verbose, TEXT("%s hit anim %d -> %s"), *Owner->GetName(), static_cast<int32>(Effect.BeHitAnim), *GetNameSafe(Montage));
}
