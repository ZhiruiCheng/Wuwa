#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Game/Animation/Notifies/WuwaAnimNotifyState_MoveToTarget.h"
#include "Game/Animation/Notifies/WuwaAnimNotifyState_RoleRotate.h"
#include "Game/Animation/Notifies/WuwaAnimNotifyState_SkillRotate.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaSkillMotion.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillBridgeComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaSkillMotionMathTest, "Wuwa.Combat.SkillMotion.Math",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaSkillMotionMathTest::RunTest(const FString& Parameters)
{
	using EBranch = FWuwaMoveToTargetResult::EBranch;
	// 三次缓动与进度
	TestEqual(TEXT("cubic(0.5)"), WuwaSkillMotion::GetCubicValue(0.5f), 0.5f);
	TestEqual(TEXT("cubic(1)"), WuwaSkillMotion::GetCubicValue(1.f), 1.f);
	TestEqual(TEXT("rate: last frame covers the rest"), WuwaSkillMotion::GetRate(nullptr, 0.19f, 0.2f, 0.02f), 1.f);
	TestNearlyEqual(TEXT("rate: (C(t1) - C(t0)) / (1 - C(t0))"), WuwaSkillMotion::GetRate(nullptr, 0.05f, 0.2f, 0.05f),
		(WuwaSkillMotion::GetCubicValue(0.5f) - WuwaSkillMotion::GetCubicValue(0.25f)) / (1.f - WuwaSkillMotion::GetCubicValue(0.25f)), 1e-5f);

	// 默认配置：停止距离 30 + 两个半径 34 = 98，水平计算
	FWuwaMoveToTargetConfig Config;
	FWuwaMoveToTargetFrame Frame;
	Frame.RootMotion = FVector(10.f, 0.f, 0.f);
	Frame.TargetLocation = FVector(500.f, 0.f, 50.f);
	Frame.SelfRadius = Frame.TargetRadius = 34.f;
	Frame.TotalTime = 0.2f;
	Frame.DeltaTime = 0.02f;
	{
		// 远：根运动之后离目标 490，往目标推 (490 - 98) × 进度
		const FWuwaMoveToTargetResult Result = WuwaSkillMotion::ComputeMoveToTarget(Config, Frame);
		const float Pull = (490.f - 98.f) * WuwaSkillMotion::GetRate(nullptr, 0.f, 0.2f, 0.02f);
		TestTrue(TEXT("far: pull branch"), Result.Branch == EBranch::Pull);
		TestNearlyEqual(TEXT("far: root motion + eased pull"), Result.Displacement.X, 10.0 + Pull, 1e-3);
		TestNearlyEqual(TEXT("far: ignore Z keeps the pull horizontal"), Result.Displacement.Z, 0.0, 1e-6);
		TestTrue(TEXT("far: faces the target"), Result.bFaceTarget && FMath::Abs(Result.Facing.Yaw) < 1e-3);
	}
	{
		// 远、最大速度封顶：每帧最多 100 × 0.02 = 2
		FWuwaMoveToTargetConfig Slow = Config;
		Slow.MaxSpeed = 100.f;
		FWuwaMoveToTargetFrame Late = Frame;
		Late.NowTime = 0.19f;   // 最后一帧：进度 1，要走完剩余距离
		TestNearlyEqual(TEXT("max speed caps the pull"), WuwaSkillMotion::ComputeMoveToTarget(Slow, Late).Displacement.X, 12.0, 1e-3);
	}
	{
		// 近：根运动之后离目标 90 < 98，抵消水平位移、保留竖直根运动
		FWuwaMoveToTargetFrame Near = Frame;
		Near.RootMotion = FVector(10.f, 3.f, 5.f);
		Near.TargetLocation = FVector(100.f, 3.f, 0.f);
		const FWuwaMoveToTargetResult Result = WuwaSkillMotion::ComputeMoveToTarget(Config, Near);
		TestTrue(TEXT("near: hold branch"), Result.Branch == EBranch::Hold);
		TestTrue(TEXT("near: horizontal root motion cancelled, vertical kept"), Result.Displacement.Equals(FVector(0.f, 0.f, 5.f), 1e-4));
	}
	{
		// 这一帧越过了目标（点积 < 0）：也抵消
		FWuwaMoveToTargetFrame Pass = Frame;
		Pass.RootMotion = FVector(60.f, 0.f, 0.f);
		Pass.TargetLocation = FVector(50.f, 0.f, 0.f);
		TestTrue(TEXT("passed the target: cancelled"), WuwaSkillMotion::ComputeMoveToTarget(Config, Pass).Displacement.IsNearlyZero(1e-4));
	}
	{
		// 最大速度 0：远时不推（根运动原样），近时照样抵消
		FWuwaMoveToTargetConfig Hold = Config;
		Hold.MaxSpeed = 0.f;
		const FWuwaMoveToTargetResult Far = WuwaSkillMotion::ComputeMoveToTarget(Hold, Frame);
		TestTrue(TEXT("max speed 0, far: untouched"), Far.Branch == EBranch::None && Far.Displacement.Equals(Frame.RootMotion));
		TestFalse(TEXT("max speed 0, far: does not turn"), Far.bFaceTarget);
		FWuwaMoveToTargetFrame Near = Frame;
		Near.TargetLocation = FVector(80.f, 0.f, 0.f);
		TestTrue(TEXT("max speed 0, near: cancelled"), WuwaSkillMotion::ComputeMoveToTarget(Hold, Near).Displacement.IsNearlyZero(1e-4));
	}
	{
		// 允许反向：太近时往后退
		FWuwaMoveToTargetConfig Reverse = Config;
		Reverse.bAllowReverse = true;
		FWuwaMoveToTargetFrame Near = Frame;
		Near.RootMotion = FVector::ZeroVector;
		Near.TargetLocation = FVector(50.f, 0.f, 0.f);
		const FWuwaMoveToTargetResult Result = WuwaSkillMotion::ComputeMoveToTarget(Reverse, Near);
		TestTrue(TEXT("reverse: backs away"), Result.Branch == EBranch::Pull && Result.Displacement.X < 0.0);
		// 不允许正向：远时不推，抵消
		FWuwaMoveToTargetConfig NoForward = Config;
		NoForward.bAllowForward = false;
		TestTrue(TEXT("no forward: far is held"), WuwaSkillMotion::ComputeMoveToTarget(NoForward, Frame).Branch == EBranch::Hold);
	}
	{
		// 最小 / 最大吸附距离之外整段不管
		FWuwaMoveToTargetConfig Range = Config;
		Range.MinDistance = 100.f;
		Range.MaxDistance = 400.f;
		TestTrue(TEXT("beyond max distance: untouched"), WuwaSkillMotion::ComputeMoveToTarget(Range, Frame).Branch == EBranch::None);
		FWuwaMoveToTargetFrame Near = Frame;
		Near.TargetLocation = FVector(80.f, 0.f, 0.f);
		TestTrue(TEXT("inside min distance: untouched"), WuwaSkillMotion::ComputeMoveToTarget(Range, Near).Branch == EBranch::None);
	}
	{
		// 不忽略半径：停止距离只有 30
		FWuwaMoveToTargetConfig NoRadius = Config;
		NoRadius.bIgnoreRadius = true;
		FWuwaMoveToTargetFrame Near = Frame;
		Near.TargetLocation = FVector(100.f, 0.f, 0.f);
		TestTrue(TEXT("ignore radius: 90 > 30 still pulls"), WuwaSkillMotion::ComputeMoveToTarget(NoRadius, Near).Branch == EBranch::Pull);
	}

	// 技能转向方向：停转 / 继续阈值
	{
		FWuwaSkillRotateState State;
		State.PauseThreshold = 10.f;
		State.ResumeThreshold = 20.f;
		const FVector Forward = FVector::ForwardVector;
		const FVector At5 = FRotator(0.f, 5.f, 0.f).Vector() * 500.f;
		TestTrue(TEXT("within pause threshold: keeps forward"), WuwaSkillMotion::GetSkillRotateDirection(State, FVector::ZeroVector, Forward, &At5).Equals(Forward, 1e-4) && State.bPaused);
		const FVector At15 = FRotator(0.f, 15.f, 0.f).Vector() * 500.f;
		TestTrue(TEXT("paused, below resume: keeps forward"), WuwaSkillMotion::GetSkillRotateDirection(State, FVector::ZeroVector, Forward, &At15).Equals(Forward, 1e-4));
		const FVector At30 = FRotator(0.f, 30.f, 0.f).Vector() * 500.f;
		const FVector Direction = WuwaSkillMotion::GetSkillRotateDirection(State, FVector::ZeroVector, Forward, &At30);
		TestTrue(TEXT("beyond resume: turns to the target again"), !State.bPaused && FMath::Abs(Direction.Rotation().Yaw - 30.f) < 1e-3);
		FWuwaSkillRotateState Offset;
		Offset.bUseOffset = true;
		Offset.Offset = 45.f;
		TestNearlyEqual(TEXT("offset rotates the target direction"), WuwaSkillMotion::GetSkillRotateDirection(Offset, FVector::ZeroVector, Forward, &At30).Rotation().Yaw, 75.0, 1e-3);
		TestTrue(TEXT("no target: no direction"), WuwaSkillMotion::GetSkillRotateDirection(Offset, FVector::ZeroVector, Forward, nullptr).IsZero());
	}
	TestNearlyEqual(TEXT("yaw interp: capped by speed x dt"), WuwaSkillMotion::InterpYawConstantTo(0.f, 90.f, 0.1f, 600.f), 60.f, 1e-3f);
	TestNearlyEqual(TEXT("yaw interp: reaches the target"), WuwaSkillMotion::InterpYawConstantTo(0.f, 30.f, 0.1f, 600.f), 30.f, 1e-3f);
	TestNearlyEqual(TEXT("yaw interp: shortest way across 180"), WuwaSkillMotion::InterpYawConstantTo(170.f, -170.f, 0.01f, 600.f), 176.f, 1e-3f);
	return true;
}

namespace WuwaSkillMotionTest
{
	/** 测试世界：地板 + 真实长离（真实动画蓝图、蒙太奇、根运动），按 CMC → 网格的真实顺序逐帧推进。 */
	struct FArena
	{
		FAutomationTestBase& Test;
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		AWuwaCharacter* Character = nullptr;
		UWuwaMovementComponent* Movement = nullptr;
		UAnimInstance* Anim = nullptr;

		explicit FArena(FAutomationTestBase& InTest) : Test(InTest) {}
		~FArena() { if (World) World->DestroyWorld(false); }

		bool Initialize()
		{
			World->InitializeActorsForPlay(FURL());
			FActorSpawnParameters Spawn;
			Spawn.ObjectFlags |= RF_Transient;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AStaticMeshActor* Floor = World->SpawnActor<AStaticMeshActor>(FVector(0.f, 0.f, -50.f), FRotator::ZeroRotator, Spawn);
			Floor->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
			Floor->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
			Floor->GetStaticMeshComponent()->SetWorldScale3D(FVector(100.f, 100.f, 1.f));
			Floor->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));

			UClass* Class = LoadClass<AWuwaCharacter>(nullptr, TEXT("/Game/Characters/Role/changli/BP_Role_Changli.BP_Role_Changli_C"));
			if (!Test.TestNotNull(TEXT("Changli blueprint loads"), Class)) return false;
			Character = World->SpawnActor<AWuwaCharacter>(Class, FTransform(FVector(0.f, 0.f, 100.f)), Spawn);
			if (!Test.TestNotNull(TEXT("Changli spawned"), Character)) return false;
			if (!Test.TestTrue(TEXT("movement state assembles"), Character->EnsureMovementStateSystem())
				|| !Test.TestTrue(TEXT("skill state assembles"), Character->EnsureSkillSystem())) return false;
			Movement = Character->GetWuwaMovementComponent();
			Movement->SetMovementMode(MOVE_Walking);
			USkeletalMeshComponent* Mesh = Character->GetMesh();
			Mesh->SetSkeletalMeshAsset(LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Role/changli/NewModel/changli2_0.changli2_0")));
			Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			Mesh->SetAnimInstanceClass(LoadClass<UAnimInstance>(nullptr, TEXT("/Game/Characters/Role/changli/ABP_Changli.ABP_Changli_C")));
			Mesh->InitAnim(true);
			Anim = Mesh->GetAnimInstance();
			if (!Test.TestNotNull(TEXT("anim instance"), Anim)) return false;
			// 落地
			for (int32 Frame = 0; Frame < 30; ++Frame) Step(1.f / 60.f);
			return Test.TestTrue(TEXT("Changli stands on the floor"), Movement->IsMovingOnGround());
		}

		AWuwaCharactorBase* SpawnMonster(const FVector& Offset)
		{
			FActorSpawnParameters Spawn;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			AWuwaCharactorBase* Monster = World->SpawnActor<AWuwaCharactorBase>(Character->GetActorLocation() + Offset, FRotator::ZeroRotator, Spawn);
			Monster->Camp = TEXT("Monster");
			Monster->GetCharacterMovement()->GravityScale = 0.f;
			return Monster;
		}

		/** 一帧：CMC（推进动画、取根运动、移动）→ 网格求值并派发排队的通知。 */
		void Step(float Dt)
		{
			World->TimeSeconds += Dt;
			const bool bRootMotion = Character->IsPlayingRootMotion();
			Movement->TickComponent(Dt, LEVELTICK_All, &Movement->PrimaryComponentTick);
			if (!bRootMotion) Character->GetMesh()->TickAnimation(Dt, false);
			Character->GetMesh()->RefreshBoneTransforms(nullptr);
		}

		/** 播 AM_Attack01，返回窗口内（不含第一帧）每帧的水平位移和最后的朝向。 */
		void PlayAttack(AActor* Target, float WindowEnd, TArray<double>& OutSteps, FVector& OutTotal, float& OutYaw)
		{
			Character->SkillComponent->SetSkillTarget(Target, NAME_None);
			UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Characters/Role/changli/AnimMontage/AM_Attack01.AM_Attack01"));
			Anim->Montage_Play(Montage);
			const FVector Start = Character->GetActorLocation();
			FVector Last = Start;
			for (int32 Frame = 0; Frame < 600; ++Frame)
			{
				Step(1.f / 60.f);
				const FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(Montage);
				const FVector Now = Character->GetActorLocation();
				if (Frame > 0) OutSteps.Add(FVector::Dist2D(Now, Last));
				Last = Now;
				if (!Instance || Instance->GetPosition() >= WindowEnd) break;
			}
			OutTotal = Character->GetActorLocation() - Start;
			OutYaw = Character->GetActorRotation().Yaw;
			Anim->Montage_Stop(0.f, Montage);
			for (int32 Frame = 0; Frame < 3; ++Frame) Step(1.f / 60.f);
		}
	};

	const UWuwaAnimNotifyState_MoveToTarget* FindMoveToTarget(const UAnimMontage* Montage, float& OutStart, float& OutEnd)
	{
		for (const FAnimNotifyEvent& Event : Montage->Notifies)
		{
			if (const UWuwaAnimNotifyState_MoveToTarget* Move = Cast<UWuwaAnimNotifyState_MoveToTarget>(Event.NotifyStateClass))
			{
				OutStart = Event.GetTime();
				OutEnd = Event.GetTime() + Event.GetDuration();
				return Move;
			}
		}
		return nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaSkillMotionRealAttackTest, "Wuwa.Combat.SkillMotion.RealAssets.ChangliAttack01",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaSkillMotionRealAttackTest::RunTest(const FString& Parameters)
{
	using namespace WuwaSkillMotionTest;
	// 装好的原作通知：长离普攻1 0~0.124 秒，停止距离 200，最大速度 800
	const UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, TEXT("/Game/Characters/Role/changli/AnimMontage/AM_Attack01.AM_Attack01"));
	float WindowStart = 0.f, WindowEnd = 0.f;
	const UWuwaAnimNotifyState_MoveToTarget* Notify = Montage ? FindMoveToTarget(Montage, WindowStart, WindowEnd) : nullptr;
	if (!TestNotNull(TEXT("AM_Attack01 has the Move To Target window (run WuwaMotionNotifySetup)"), Notify)) return false;
	TestEqual(TEXT("distance"), Notify->Config.Distance, 200.f);
	TestEqual(TEXT("max speed"), Notify->Config.MaxSpeed, 800.f);
	TestNearlyEqual(TEXT("window end"), WindowEnd, 0.124f, 0.01f);

	FArena Arena(*this);
	if (!Arena.Initialize()) return false;
	const float Dt = 1.f / 60.f;
	const float SelfRadius = Arena.Character->GetCapsuleComponent()->GetScaledCapsuleRadius();

	// 对照：没有技能目标，窗口里根运动照常
	TArray<double> ControlSteps;
	FVector Control;
	float Yaw = 0.f;
	Arena.PlayAttack(nullptr, WindowEnd, ControlSteps, Control, Yaw);
	AddInfo(FString::Printf(TEXT("no target: moved %.1f cm in the window (%d frames)"), Control.Size2D(), ControlSteps.Num() + 1));
	if (!TestTrue(TEXT("AM_Attack01 moves the character by root motion in the window"), Control.Size2D() > 2.0)) return false;

	{
		// 近：怪物在停止距离以内，窗口里（第一帧之后）每帧都被钉住
		AWuwaCharactorBase* Monster = Arena.SpawnMonster(Arena.Character->GetActorForwardVector() * 150.f);
		const float Stop = 200.f + SelfRadius + Monster->GetCapsuleComponent()->GetScaledCapsuleRadius();
		TArray<double> Steps;
		FVector Total;
		Arena.PlayAttack(Monster, WindowEnd, Steps, Total, Yaw);
		const double Largest = Steps.Num() ? FMath::Max(Steps) : 0.0;
		AddInfo(FString::Printf(TEXT("near (150 cm < stop %.0f): moved %.1f cm, largest step after the first frame %.2f cm"), Stop, Total.Size2D(), Largest));
		TestTrue(TEXT("near: held in place after the first frame"), Largest < 0.5);
		TestTrue(TEXT("near: moved less than without a target"), Total.Size2D() < Control.Size2D());
		Monster->Destroy();
	}
	{
		// 远、斜前方 45°：往目标推（比对照走得远），朝向转向目标
		const FVector Offset = FRotator(0.f, Arena.Character->GetActorRotation().Yaw + 45.f, 0.f).Vector() * 600.f;
		AWuwaCharactorBase* Monster = Arena.SpawnMonster(Offset);
		const FVector Before = Arena.Character->GetActorLocation();
		TArray<double> Steps;
		FVector Total;
		Arena.PlayAttack(Monster, WindowEnd, Steps, Total, Yaw);
		const double Toward = FVector::DotProduct(Total, Offset.GetSafeNormal());
		const double Largest = Steps.Num() ? FMath::Max(Steps) : 0.0;
		const FVector ToMonster = Monster->GetActorLocation() - Arena.Character->GetActorLocation();
		AddInfo(FString::Printf(TEXT("far (600 cm at 45 deg): moved %.1f cm toward it, largest step %.2f cm, yaw %.1f (target %.1f)"),
			Toward, Largest, Yaw, ToMonster.Rotation().Yaw));
		TestTrue(TEXT("far: pulled toward the target more than root motion alone"), Toward > Control.Size2D() + 5.0);
		TestTrue(TEXT("far: per-frame pull within root motion + max speed"), Largest <= FMath::Max(ControlSteps) + 800.0 * Dt + 0.5);
		TestTrue(TEXT("far: faces the target"), FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, ToMonster.Rotation().Yaw)) < 2.0);
		TestTrue(TEXT("far: still outside the stop distance"), ToMonster.Size2D() > 200.f + SelfRadius);
		(void)Before;
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaSkillMotionRotateTest, "Wuwa.Combat.SkillMotion.RotateWithoutRootMotion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaSkillMotionRotateTest::RunTest(const FString& Parameters)
{
	using namespace WuwaSkillMotionTest;
	// 技能转向（嘉贝普攻3 的参数：600 度 / 秒、转向技能目标）；没有根运动时由移动组件 Tick 末尾补
	FArena Arena(*this);
	if (!Arena.Initialize()) return false;
	AWuwaCharactorBase* Monster = Arena.SpawnMonster(FVector(0.f, 500.f, 0.f));
	const float StartYaw = Arena.Character->GetActorRotation().Yaw;
	const float TargetYaw = (Monster->GetActorLocation() - Arena.Character->GetActorLocation()).Rotation().Yaw;
	Arena.Movement->SetSkillRotateTarget(Monster, NAME_None);
	Arena.Movement->SetSkillCanRotate(true);
	Arena.Movement->SetSkillRotateToTarget(true);
	Arena.Movement->SetSkillRotateSpeed(600.f);
	Arena.Step(0.05f);
	const float AfterOne = Arena.Character->GetActorRotation().Yaw;
	TestNearlyEqual(TEXT("one frame turns speed x dt"), static_cast<double>(FMath::Abs(FMath::FindDeltaAngleDegrees(StartYaw, AfterOne))), 30.0, 0.5);
	for (int32 Frame = 0; Frame < 10; ++Frame) Arena.Step(0.05f);
	TestTrue(TEXT("ends facing the target"), FMath::Abs(FMath::FindDeltaAngleDegrees(Arena.Character->GetActorRotation().Yaw, TargetYaw)) < 0.5);
	TestTrue(TEXT("rotation does not move the character"), FVector::Dist2D(Arena.Character->GetActorLocation(), FVector::ZeroVector) < 1.0);
	// 关掉后不再转
	Arena.Movement->SetSkillCanRotate(false);
	Arena.Character->SetActorRotation(FRotator(0.f, StartYaw, 0.f));
	Arena.Step(0.05f);
	TestNearlyEqual(TEXT("closed window does not turn"), static_cast<double>(Arena.Character->GetActorRotation().Yaw), static_cast<double>(StartYaw), 0.01);
	return true;
}

#endif
