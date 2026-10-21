#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Game/NewWorld/Combat/WuwaLockOnComponent.h"

namespace WuwaLockOnTest
{
	struct FLockOnScene
	{
		UWorld* World = nullptr;
		AWuwaCharacter* Player = nullptr;

		FLockOnScene()
		{
			World = UWorld::CreateWorld(EWorldType::Game, false);
			World->InitializeActorsForPlay(FURL());
			// 玩家在原点朝 +X；没有控制器时参考方向（镜头朝向）退回角色朝向
			Player = Spawn<AWuwaCharacter>(FVector::ZeroVector);
		}
		~FLockOnScene() { World->DestroyWorld(false); }

		template <typename T>
		T* Spawn(const FVector& Location, FName Camp = NAME_None)
		{
			T* Actor = World->SpawnActor<T>(Location, FRotator::ZeroRotator);
			if (!Camp.IsNone()) Actor->Camp = Camp;
			Actor->DispatchBeginPlay();   // 登记到实体表
			return Actor;
		}
		AWuwaCharactorBase* Monster(const FVector& Location) { return Spawn<AWuwaCharactorBase>(Location, TEXT("Monster")); }

		AActor* Detect(int32 ConfigId, EWuwaSkillTargetPriority Priority = EWuwaSkillTargetPriority::CameraDirection)
		{
			Player->LockOnComponent->DetectSoftLockTarget(ConfigId, Priority);
			return Player->LockOnComponent->GetCurrentTarget().Actor.Get();
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaLockOnRulesTest, "Wuwa.Combat.LockOn.Rules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaLockOnRulesTest::RunTest(const FString& Parameters)
{
	using namespace WuwaLockOnTest;
	// 真实锁定参数表（原作导入）：1 = 8 米 / 上下 10 米 / 容忍角 22.5；114 = 14 米
	const FWuwaLockOnConfig* Default = UWuwaLockOnComponent::FindConfig(1);
	if (!TestNotNull(TEXT("lock-on config 1 imported"), Default)) return false;
	TestEqual(TEXT("config 1 distance"), Default->Distance, 800.f);
	TestEqual(TEXT("config 1 tolerance (original default)"), Default->ToleranceAngle, 22.5f);
	TestEqual(TEXT("config 114 distance"), UWuwaLockOnComponent::FindConfig(114) ? UWuwaLockOnComponent::FindConfig(114)->Distance : 0.f, 1400.f);
	TestEqual(TEXT("game setting resolves to input direction (original default)"),
		UWuwaLockOnComponent::ResolvePriority(EWuwaSkillTargetPriority::GameSetting), EWuwaSkillTargetPriority::InputDirection);

	{
		// 夹角优先：A 在参考方向 ±22.5° 内但更远，B 更近但偏 45°；起点后退 200 后 A 的夹角约 20.6° → 选 A
		FLockOnScene Scene;
		AWuwaCharactorBase* A = Scene.Monster(FVector(600.f, 300.f, 0.f));
		AWuwaCharactorBase* B = Scene.Monster(FVector(300.f, -500.f, 0.f));
		TestTrue(TEXT("in-cone target beats a nearer one outside the cone"), Scene.Detect(1) == A);
		TestTrue(TEXT("distance priority picks the nearest"), Scene.Detect(1, EWuwaSkillTargetPriority::Distance) == B);
		// 锁定目标优先：当前目标 B 还有效就保留（即使 A 在锥内）
		TestTrue(TEXT("keep-lock-target keeps the current target"), Scene.Detect(1, EWuwaSkillTargetPriority::KeepLockTarget) == B);
		// 锥内没有就退回最近的；精准模式不退
		A->SetActorHiddenInGame(true);
		TestTrue(TEXT("hidden (off-field) units are skipped; falls back to nearest"), Scene.Detect(1) == B);
		TestNull(TEXT("precise mode has no fallback outside the cone"), Scene.Detect(1, EWuwaSkillTargetPriority::CameraDirectionPrecise));
	}
	{
		// 范围：8 米外选不到，换 14 米的 114 号就能选到；高度差超过上距离选不到
		FLockOnScene Scene;
		AWuwaCharactorBase* Far = Scene.Monster(FVector(1000.f, 0.f, 0.f));
		TestNull(TEXT("beyond config 1 distance"), Scene.Detect(1));
		TestTrue(TEXT("within config 114 distance"), Scene.Detect(114) == Far);
		Far->SetActorLocation(FVector(500.f, 0.f, 1200.f));
		TestNull(TEXT("too far above (up distance 10 m)"), Scene.Detect(114));
	}
	{
		// 阵营：队友（Player）选不到
		FLockOnScene Scene;
		Scene.Spawn<AWuwaCharactorBase>(FVector(300.f, 0.f, 0.f), TEXT("Player"));
		TestNull(TEXT("friendly units are not targets"), Scene.Detect(1));
	}
	{
		// 遮挡：镜头（没有控制器时为眼睛位置）到目标之间有墙
		FLockOnScene Scene;
		AWuwaCharactorBase* Behind = Scene.Monster(FVector(600.f, 0.f, 0.f));
		TestTrue(TEXT("visible target is found"), Scene.Detect(1) == Behind);
		AStaticMeshActor* Wall = Scene.World->SpawnActor<AStaticMeshActor>(FVector(300.f, 0.f, 50.f), FRotator::ZeroRotator);
		Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable);
		Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")));
		Wall->GetStaticMeshComponent()->SetWorldScale3D(FVector(0.2f, 5.f, 5.f));
		Wall->GetStaticMeshComponent()->SetCollisionProfileName(TEXT("BlockAll"));
		TestNull(TEXT("blocked target is not found"), Scene.Detect(1));
	}
	return true;
}

#endif
