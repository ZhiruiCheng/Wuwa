#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game/Common/WuwaGameTags.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Formation/WuwaFormationComponent.h"
#include "Game/Input/WuwaInputTypes.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"

// 只读诊断：比较队员的胶囊体/网格/移动设置，然后在真实地图里切一次人，逐帧记录角色和镜头的位置，找"切人时抖一下"的来源。
namespace FormationSwitchDiag
{
	AWuwaPlayerController* FindController()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if (Context.WorldType == EWorldType::PIE && IsValid(Context.World()))
			{
				return Cast<AWuwaPlayerController>(Context.World()->GetFirstPlayerController());
			}
		}
		return nullptr;
	}

	FString Describe(const AWuwaCharacter* C)
	{
		const UCapsuleComponent* Capsule = C->GetCapsuleComponent();
		const USkeletalMeshComponent* Mesh = C->GetMesh();
		const UCharacterMovementComponent* Move = C->GetCharacterMovement();
		return FString::Printf(TEXT("%s capsule(r=%.1f hh=%.1f) meshRel=%s meshRot=%s mode=%d vel=%s tickOpt=%d"),
			*GetNameSafe(C->GetClass()), Capsule->GetScaledCapsuleRadius(), Capsule->GetScaledCapsuleHalfHeight(),
			*Mesh->GetRelativeLocation().ToString(), *Mesh->GetRelativeRotation().ToString(),
			static_cast<int32>(Move->MovementMode), *Move->Velocity.ToString(), static_cast<int32>(Mesh->VisibilityBasedAnimTickOption));
	}

	class FRecord final : public IAutomationLatentCommand
	{
	public:
		explicit FRecord(FAutomationTestBase* InTest) : Test(InTest) {}

		virtual bool Update() override
		{
			AWuwaPlayerController* PC = FindController();
			UWuwaFormationComponent* Formation = PC ? PC->FormationComponent.Get() : nullptr;
			if (!Formation || Formation->GetMembers().Num() < 2)
			{
				if (++WaitFrames > 600) { Test->AddError(TEXT("No formation.")); return true; }
				return false;
			}
			const TArray<TObjectPtr<AWuwaCharacter>>& Members = Formation->GetMembers();
			if (Frame == 0)
			{
				for (const AWuwaCharacter* Member : Members) Test->AddInfo(Describe(Member));
				const FWuwaGameTags& Tags = FWuwaGameTags::Get();
				FWuwaInputEvent Switch;
				Switch.InputTag = Tags.Formation_SwitchtoTwo;
				Switch.RouteTag = Tags.Input_Route_Formation;
				Switch.Phase = EWuwaInputPhase::Pressed;
				Test->AddInfo(FString::Printf(TEXT("before: pawnLoc=%s cam=%s"), *PC->GetPawn()->GetActorLocation().ToString(),
					*PC->PlayerCameraManager->GetCameraLocation().ToString()));
				PC->RouteInputEvent(Switch);
				Test->AddInfo(TEXT("after switch:"));
				for (const AWuwaCharacter* Member : Members) Test->AddInfo(Describe(Member));
			}
			const APawn* Pawn = PC->GetPawn();
			Test->AddInfo(FString::Printf(TEXT("frame %2d: pawn=%s loc=%s cam=%s camRot=%s"), Frame, *GetNameSafe(Pawn),
				Pawn ? *Pawn->GetActorLocation().ToString() : TEXT("-"),
				*PC->PlayerCameraManager->GetCameraLocation().ToString(), *PC->PlayerCameraManager->GetCameraRotation().ToString()));
			return ++Frame > 12;
		}

	private:
		FAutomationTestBase* Test;
		int32 Frame = 0;
		int32 WaitFrames = 0;
	};
}

namespace FormationSwitchDiag
{
	// 验证"上场那一刻为什么找不到地面"：把离场队员放到在场队员的位置、打开碰撞，分别在在场队员碰撞开/关时做一次 FindFloor。
	class FProbeFloor final : public IAutomationLatentCommand
	{
	public:
		explicit FProbeFloor(FAutomationTestBase* InTest) : Test(InTest) {}

		virtual bool Update() override
		{
			AWuwaPlayerController* PC = FindController();
			UWuwaFormationComponent* Formation = PC ? PC->FormationComponent.Get() : nullptr;
			if (!Formation || Formation->GetMembers().Num() < 2)
			{
				if (++WaitFrames > 600) { Test->AddError(TEXT("No formation.")); return true; }
				return false;
			}
			AWuwaCharacter* OnField = Formation->GetMembers()[0];
			AWuwaCharacter* OffField = Formation->GetMembers()[1];
			const FVector Location = OnField->GetActorLocation();
			OffField->SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
			OffField->SetActorEnableCollision(true);

			auto Probe = [&](const TCHAR* Label)
			{
				FFindFloorResult Floor;
				OffField->GetCharacterMovement()->FindFloor(OffField->GetCapsuleComponent()->GetComponentLocation(), Floor, false);
				Test->AddInfo(FString::Printf(TEXT("%s: blocking=%d walkable=%d startPenetrating=%d hitActor=%s dist=%.2f"), Label,
					Floor.bBlockingHit, Floor.bWalkableFloor, Floor.HitResult.bStartPenetrating,
					*GetNameSafe(Floor.HitResult.GetActor()), Floor.FloorDist));
			};
			Probe(TEXT("on-field role collision ON "));
			OnField->SetActorEnableCollision(false);
			Probe(TEXT("on-field role collision OFF"));

			OnField->SetActorEnableCollision(true);
			OffField->SetActorEnableCollision(false);
			return true;
		}

	private:
		FAutomationTestBase* Test;
		int32 WaitFrames = 0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFormationFloorProbeDiagTest, "Diag.Formation.FloorProbe",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFormationFloorProbeDiagTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/Map/Wuwa"))) return false;
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FormationSwitchDiag::FProbeFloor(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFormationSwitchDiagTest, "Diag.Formation.SwitchFrames",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFormationSwitchDiagTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/Map/Wuwa"))) return false;
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(2.0f));
	ADD_LATENT_AUTOMATION_COMMAND(FormationSwitchDiag::FRecord(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
