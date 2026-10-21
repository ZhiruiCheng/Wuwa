#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "UObject/UnrealType.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimInstance.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputIntentComponent.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateBridgeComponent.h"
#include "Game/NewWorld/Character/Role/Component/WuwaRoleGaitBridgeComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaMoveResumeTest, "Wuwa.Animation.MoveResume.RapidTaps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaMoveResumeTest::RunTest(const FString& Parameters)
{
	const TCHAR* Roles[] = { TEXT("/Game/Characters/Role/changli/BP_Role_Changli.BP_Role_Changli_C"),
		TEXT("/Game/Characters/Role/jiabei/BP_Role_jiabei.BP_Role_jiabei_C") };
	FString Csv = TEXT("role,walk,warmup_frames,tap,tail_contact,before_state,after_state,previous,next,before_phase,after_phase,left_jump_cm,right_jump_cm\n");
	int32 TailCases = 0;
	for (int32 R = 0; R < 2; ++R) for (int32 Walk = 0; Walk < 2; ++Walk)
	for (const int32 WarmupFrames : {3, 120})
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		ON_SCOPE_EXIT { World->DestroyWorld(false); };
		World->TimeSeconds = 1.f;
		UClass* Class = LoadClass<AWuwaCharacter>(nullptr, Roles[R]);
		if (!TestNotNull(TEXT("Role class"), Class)) return false;
		FActorSpawnParameters Spawn;
		Spawn.ObjectFlags |= RF_Transient;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Role = World->SpawnActor<AWuwaCharacter>(Class, Spawn);
		UWuwaMovementComponent* Movement = Role->GetWuwaMovementComponent();
		Movement->MovementMode = MOVE_Walking;
		if (!TestTrue(TEXT("Movement ready"), Role->EnsureMovementStateSystem())) return false;
		auto* Mesh = Role->GetMesh();
		Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Mesh->bEnableUpdateRateOptimizations = false;
		Mesh->InitAnim(true);
		auto* Anim = CastChecked<UWuwaAnimInstance>(Mesh->GetAnimInstance());
		Role->UnifiedStateComponent->SetWalkPreference(Walk != 0);
		bool bInputHeld = false;
		auto Tick = [&](float Delta)
		{
			World->TimeSeconds += Delta;
			if (!bInputHeld)
			{
				const FVector Velocity = Movement->Velocity;
				const FVector BrakingVelocity = Velocity.GetSafeNormal() * FMath::Max(0.f, Velocity.Size() - 1800.f * Delta);
				Movement->RestoreSwitchMotion(BrakingVelocity, FVector::ZeroVector, FVector::ZeroVector);
			}
			Role->RoleGaitComponent->RefreshPolicy();
			Mesh->TickAnimation(Delta, true);
			Mesh->RefreshBoneTransforms();
		};
		auto Input = [&](bool Move, FVector2D Axis)
		{
			bInputHeld = Move;
			Role->InputIntent->SetMoveAxis(Move ? Axis : FVector2D::ZeroVector);
			const FVector Direction(Axis.Y, Axis.X, 0.f);
			Movement->RestoreSwitchMotion(Move ? Direction * (Walk ? 180.f : 420.f) : Movement->Velocity,
				Move ? Direction : FVector::ZeroVector, Move ? Direction * 500.f : FVector::ZeroVector);
		};
		for (int32 I = 0; I < 5; ++I) Tick(1.f / 60.f);
		Input(true, FVector2D(0, 1));
		for (int32 I = 0; I < WarmupFrames; ++I) Tick(1.f / 60.f);
		const FVector2D Directions[] = { {0, 1}, {-1, 0}, {0, -1}, {1, 0} };
		for (int32 Tap = 0; Tap < 8; ++Tap)
		{
			Input(false, Directions[Tap % 4]);
			// Include interruptions during blend-out and after Stop has full weight.
			for (int32 I = 0; I < (Tap % 2 ? 12 : 3); ++I) Tick(1.f / 60.f);
			const FName BeforeState = Anim->GetDebugStateName(TEXT("SM_Ground"));
			TestEqual(TEXT("Release still enters Stop"), BeforeState, FName(TEXT("Stop")));
			auto Before = Anim->GetSyncGroupPosition(TEXT("Locomotion"));
			const bool bTailContact = !Before.IsValid();
			if (bTailContact)
			{
				// Past the final authored contact, require the resume to start on that foot.
				const UAnimSequence* Stop = Anim->bRoleStopWalk
					? (Anim->bRoleStopLeft ? Anim->RoleStopWalkLeft.Get() : Anim->RoleStopWalkRight.Get())
					: (Anim->bRoleStopLeft ? Anim->RoleStopRunLeft.Get() : Anim->RoleStopRunRight.Get());
				if (!TestTrue(TEXT("Stop tail has authored contacts"), Stop && Stop->AuthoredSyncMarkers.Num() >= 2)) return false;
				const auto& Last = Stop->AuthoredSyncMarkers.Last();
				bool bPastContact = false;
				for (TFieldIterator<FStructProperty> It(Anim->GetClass()); It; ++It)
					if (It->Struct->IsChildOf(FAnimNode_SequencePlayer::StaticStruct()))
					{
						const auto* Player = It->ContainerPtrToValuePtr<FAnimNode_SequencePlayer>(Anim);
						bPastContact |= Player->GetSequence() == Stop && Player->GetCachedBlendWeight() > .001f && Player->GetAccumulatedTime() >= Last.Time;
					}
				TestTrue(TEXT("Missing group phase is limited to the actual stop tail"), bPastContact);
				Before = FMarkerSyncAnimPosition(Last.MarkerName, Last.MarkerName == TEXT("Left") ? FName(TEXT("Right")) : FName(TEXT("Left")), 0.f);
				++TailCases;
			}
			const FVector LeftBefore = Mesh->GetSocketLocation(TEXT("Bip001LFoot"));
			const FVector RightBefore = Mesh->GetSocketLocation(TEXT("Bip001RFoot"));
			Input(true, Directions[Tap % 4]);
			// A tiny positive frame exercises the real update path without advancing a visible step.
			Tick(.0001f);
			const FName AfterState = Anim->GetDebugStateName(TEXT("SM_Ground"));
			TestEqual(TEXT("Repress retains the existing Stop to StartMove transition"), AfterState, FName(TEXT("StartMove")));
			const auto After = Anim->GetSyncGroupPosition(TEXT("Locomotion"));
			TestTrue(TEXT("Resumed movement has a valid foot-contact sync position"), After.IsValid());
			TestEqual(TEXT("Repress preserves previous contact"), After.PreviousMarkerName, Before.PreviousMarkerName);
			TestEqual(TEXT("Repress preserves next contact"), After.NextMarkerName, Before.NextMarkerName);
			TestTrue(TEXT("Repress preserves progress between contacts"), FMath::IsNearlyEqual(After.PositionBetweenMarkers, Before.PositionBetweenMarkers, .025f));
			Csv += FString::Printf(TEXT("%d,%d,%d,%d,%d,%s,%s,%s,%s,%.5f,%.5f,%.4f,%.4f\n"), R, Walk, WarmupFrames, Tap, bTailContact,
				*BeforeState.ToString(), *AfterState.ToString(), *Before.PreviousMarkerName.ToString(),
				*Before.NextMarkerName.ToString(), Before.PositionBetweenMarkers, After.PositionBetweenMarkers,
				FVector::Distance(LeftBefore, Mesh->GetSocketLocation(TEXT("Bip001LFoot"))),
				FVector::Distance(RightBefore, Mesh->GetSocketLocation(TEXT("Bip001RFoot"))));
			for (int32 I = 0; I < 10 + Tap; ++I) Tick(1.f / 60.f);
		}
		Input(false, FVector2D(0, 1));
		for (int32 I = 0; I < 360; ++I) Tick(1.f / 60.f);
		TestEqual(TEXT("Full stop still finishes into Idle"), Anim->GetDebugStateName(TEXT("SM_Ground")), FName(TEXT("Idle")));
		Input(true, FVector2D(0, 1));
		for (int32 I = 0; I < 3; ++I) Tick(1.f / 60.f);
		TestEqual(TEXT("Fresh idle start still enters StartMove"), Anim->GetDebugStateName(TEXT("SM_Ground")), FName(TEXT("StartMove")));
	}
	TestTrue(TEXT("Rapid taps exercise recovery after the final stop contact"), TailCases > 0);
	FFileHelper::SaveStringToFile(Csv, *(FPaths::ProjectSavedDir() / TEXT("Diagnostics/MoveResume/RapidTaps.csv")));
	return true;
}
#endif
