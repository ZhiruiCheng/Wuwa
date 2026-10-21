#include "Game/NewWorld/Combat/WuwaLockOnComponent.h"

#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Game/Common/Debug/WuwaDebug.h"
#include "Game/NewWorld/Character/Common/WuwaCharactorBase.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Game/NewWorld/Combat/WuwaCamp.h"
#include "Game/NewWorld/Combat/WuwaCombatSettings.h"
#include "Game/NewWorld/Combat/WuwaCreatureSubsystem.h"

FVector FWuwaLockOnTarget::GetLocation() const
{
	const AWuwaCharactorBase* Target = Actor.Get();
	if (!Target) return FVector::ZeroVector;
	const USkeletalMeshComponent* Mesh = Target->GetMesh();
	return !Socket.IsNone() && Mesh && Mesh->DoesSocketExist(Socket) ? Mesh->GetSocketLocation(Socket) : Target->GetActorLocation();
}

UWuwaLockOnComponent::UWuwaLockOnComponent()
{
	// 只在调试显示时有事可做；Tick 里先看开关
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

const FWuwaLockOnConfig* UWuwaLockOnComponent::FindConfig(int32 ConfigId)
{
	const UDataTable* Table = GetDefault<UWuwaCombatSettings>()->LockOnConfigTable.LoadSynchronous();
	if (!Table || Table->GetRowStruct() != FWuwaLockOnConfig::StaticStruct()) return nullptr;
	return Table->FindRow<FWuwaLockOnConfig>(FName(*FString::FromInt(ConfigId)), TEXT("WuwaLockOn"), false);
}

EWuwaSkillTargetPriority UWuwaLockOnComponent::ResolvePriority(EWuwaSkillTargetPriority Priority)
{
	return Priority == EWuwaSkillTargetPriority::GameSetting ? GetDefault<UWuwaCombatSettings>()->SkillLockEnemyMode : Priority;
}

void UWuwaLockOnComponent::ClearTarget()
{
	CurrentTarget = FWuwaLockOnTarget();
}

AWuwaCharactorBase* UWuwaLockOnComponent::GetOwnerCharacter() const
{
	return Cast<AWuwaCharactorBase>(GetOwner());
}

bool UWuwaLockOnComponent::DetectSoftLockTarget(int32 ConfigId, EWuwaSkillTargetPriority Priority)
{
	AWuwaCharactorBase* Self = GetOwnerCharacter();
	UWuwaCreatureSubsystem* Creatures = UWuwaCreatureSubsystem::Get(this);
	const int32 ResolvedConfigId = ConfigId != 0 ? ConfigId : (Self ? Self->SoftLockConfigId : 0);
	const FWuwaLockOnConfig* Config = FindConfig(ResolvedConfigId);
	if (!Self || !Creatures || !Config)
	{
		ClearTarget();
		return false;
	}
	LastConfigId = ResolvedConfigId;
	const FVector From = Self->GetActorLocation();

	// 1、2：候选 + 过滤（原作 DetectAlternativeTargets）
	TArray<AWuwaCharactorBase*> Nearby, Candidates;
	Creatures->GetCreaturesInRange(From, FMath::Max(Config->Distance, Config->SectorRadius), Nearby);
	for (AWuwaCharactorBase* Candidate : Nearby)
	{
		if (Candidate == Self || Candidate->IsHidden() || !UWuwaCampLibrary::IsHostile(Self, Candidate)) continue;
		const FVector Location = Candidate->GetActorLocation();
		if (!IsInArea(*Config, From, Location) || IsBlocked(Candidate, Location)) continue;
		Candidates.Add(Candidate);
	}

	// 3：参考方向（原作 FindTheBest）
	const EWuwaSkillTargetPriority Mode = ResolvePriority(Priority);
	const FVector Input = GetInputDirection();
	bool bKeepCurrent = false;
	FVector Reference = FVector::ZeroVector;
	switch (Mode)
	{
	case EWuwaSkillTargetPriority::InputDirection:
	case EWuwaSkillTargetPriority::InputDirectionPrecise:
		// 输入方向没变（包括一直没输入）时保留当前目标
		bKeepCurrent = Input.Equals(LastInputDirection, 0.01);
		Reference = Input.IsNearlyZero() ? GetCameraForward() : Input;
		break;
	case EWuwaSkillTargetPriority::ActorDirection:
	case EWuwaSkillTargetPriority::ActorDirectionPrecise:
		Reference = Self->GetActorForwardVector();
		break;
	case EWuwaSkillTargetPriority::KeepLockTarget:
		bKeepCurrent = true;
		Reference = GetCameraForward();
		break;
	case EWuwaSkillTargetPriority::CameraDirection:
	case EWuwaSkillTargetPriority::CameraDirectionPrecise:
		Reference = GetCameraForward();
		break;
	default:   // 距离优先：没有参考方向
		break;
	}
	LastInputDirection = Input;
	const bool bPrecise = Mode == EWuwaSkillTargetPriority::InputDirectionPrecise || Mode == EWuwaSkillTargetPriority::CameraDirectionPrecise
		|| Mode == EWuwaSkillTargetPriority::ActorDirectionPrecise;
	const FVector Direction2D = FVector(Reference.X, Reference.Y, 0.0).GetSafeNormal();
	// 起点沿参考方向往回退 LockOnOffset（精准模式不退），贴身的目标夹角不会被放大
	const FVector Origin = From - Direction2D * (bPrecise ? 0.0 : GetDefault<UWuwaCombatSettings>()->LockOnOffset);

	AWuwaCharactorBase* InCone = nullptr;
	AWuwaCharactorBase* Outside = nullptr;
	double InConeDistance = TNumericLimits<double>::Max(), OutsideDistance = TNumericLimits<double>::Max();
	AWuwaCharactorBase* Chosen = nullptr;
	for (AWuwaCharactorBase* Candidate : Candidates)
	{
		if (bKeepCurrent && CurrentTarget.Actor.Get() == Candidate)
		{
			Chosen = Candidate;   // 原作 S$r 返回 2：当前目标仍在候选里，直接用
			break;
		}
		const FVector Location = Candidate->GetActorLocation();
		const double Distance = FVector::Dist(From, Location);
		double Angle = 0.0;
		if (!Direction2D.IsNearlyZero())
		{
			const FVector ToTarget = FVector(Location.X - Origin.X, Location.Y - Origin.Y, 0.0).GetSafeNormal();
			Angle = FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(FVector::DotProduct(Direction2D, ToTarget), -1.0, 1.0)));
		}
		if (Angle < Config->ToleranceAngle)
		{
			if (Distance < InConeDistance) { InCone = Candidate; InConeDistance = Distance; }
		}
		else if (!bPrecise && Distance < OutsideDistance)
		{
			Outside = Candidate;
			OutsideDistance = Distance;
		}
	}
	if (!Chosen) Chosen = InCone ? InCone : Outside;
	CurrentTarget = FWuwaLockOnTarget();
	CurrentTarget.Actor = Chosen;
	if (UWuwaDebugLibrary::IsDebugChannelEnabled(EWuwaDebugChannel::LockOn)) DrawDebug(*Config, Candidates);
	return CurrentTarget.IsValid();
}

bool UWuwaLockOnComponent::IsInArea(const FWuwaLockOnConfig& Config, const FVector& From, const FVector& Target) const
{
	// 原作 IsInAreaRole：s = 自己.Z - 目标.Z；s < -上距离（目标太高）或 s > 下距离（目标太低）都不行
	const double Height = From.Z - Target.Z;
	if (Height < -Config.UpDistance || Height > Config.DownDistance) return false;
	const double DistanceSquared = FVector::DistSquared(From, Target);
	if (DistanceSquared <= FMath::Square(Config.Distance)) return true;
	if (Config.SectorRadius <= 0.f || DistanceSquared > FMath::Square(Config.SectorRadius)) return false;
	// 扇形以控制器（镜头）朝向为中心
	const AController* Controller = GetOwnerCharacter() ? GetOwnerCharacter()->GetController() : nullptr;
	const double Yaw = Controller ? Controller->GetControlRotation().Yaw : GetOwner()->GetActorRotation().Yaw;
	const double TargetYaw = FMath::RadiansToDegrees(FMath::Atan2(Target.Y - From.Y, Target.X - From.X));
	return FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, TargetYaw)) <= Config.SectorAngle * 0.5;
}

bool UWuwaLockOnComponent::IsBlocked(const AWuwaCharactorBase* Target, const FVector& TargetLocation) const
{
	const UWorld* World = GetWorld();
	if (!World) return false;
	FCollisionQueryParams Params(TEXT("WuwaLockOnBlock"), false, GetOwner());
	Params.AddIgnoredActor(Target);
	FHitResult Hit;
	return World->LineTraceSingleByChannel(Hit, GetCameraLocation(), TargetLocation, ECC_Visibility, Params);
}

FVector UWuwaLockOnComponent::GetCameraLocation() const
{
	const AWuwaCharactorBase* Self = GetOwnerCharacter();
	const APlayerController* Player = Self ? Cast<APlayerController>(Self->GetController()) : nullptr;
	if (Player && Player->PlayerCameraManager) return Player->PlayerCameraManager->GetCameraLocation();
	FVector Location;
	FRotator Rotation;
	GetOwner()->GetActorEyesViewPoint(Location, Rotation);
	return Location;
}

FVector UWuwaLockOnComponent::GetCameraForward() const
{
	const AWuwaCharactorBase* Self = GetOwnerCharacter();
	const APlayerController* Player = Self ? Cast<APlayerController>(Self->GetController()) : nullptr;
	if (Player && Player->PlayerCameraManager) return Player->PlayerCameraManager->GetCameraRotation().Vector();
	if (const AController* Controller = Self ? Self->GetController() : nullptr) return Controller->GetControlRotation().Vector();
	return GetOwner()->GetActorForwardVector();
}

FVector UWuwaLockOnComponent::GetInputDirection() const
{
	const AWuwaCharacter* Role = Cast<AWuwaCharacter>(GetOwner());
	if (!Role) return FVector::ZeroVector;
	const FWuwaPlayerInputState Input = Role->GetPlayerInputState();
	return Input.bHasMoveInput ? Input.MoveWorldDirection : FVector::ZeroVector;
}

void UWuwaLockOnComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!UWuwaDebugLibrary::IsDebugChannelEnabled(EWuwaDebugChannel::LockOn) || !CurrentTarget.IsValid()) return;
	// 当前目标：自己到目标一条红线
	DrawDebugLine(GetWorld(), GetOwner()->GetActorLocation(), CurrentTarget.GetLocation(), FColor::Red, false, -1.f, 0, 2.f);
	DrawDebugSphere(GetWorld(), CurrentTarget.GetLocation(), 30.f, 8, FColor::Red);
}

void UWuwaLockOnComponent::DrawDebug(const FWuwaLockOnConfig& Config, const TArray<AWuwaCharactorBase*>& Candidates) const
{
	const UWorld* World = GetWorld();
	const FVector From = GetOwner()->GetActorLocation();
	// 探测距离（黄圈，2 秒）、候选（黄线）
	DrawDebugCircle(World, From, Config.Distance, 48, FColor::Yellow, false, 2.f, 0, 2.f, FVector(1, 0, 0), FVector(0, 1, 0), false);
	for (const AWuwaCharactorBase* Candidate : Candidates)
	{
		DrawDebugLine(World, From, Candidate->GetActorLocation(), FColor::Yellow, false, 2.f);
	}
}
