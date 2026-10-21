#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Animation/AnimNode_AssetPlayerBase.h"
#include "UObject/UnrealType.h"
#include "Game/Formation/WuwaRoleSwitchSnapshot.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimInstance.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputIntentComponent.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateBridgeComponent.h"
#include "Game/NewWorld/Character/Role/Component/WuwaRoleGaitBridgeComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"

namespace WuwaSwitchTest
{
const TCHAR* Roles[] = {
	TEXT("/Game/Characters/Role/changli/BP_Role_Changli.BP_Role_Changli_C"),
	TEXT("/Game/Characters/Role/jiabei/BP_Role_jiabei.BP_Role_jiabei_C") };

struct FWorldScope
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	~FWorldScope() { if (World) World->DestroyWorld(false); }
};

void TickPose(AWuwaCharacter* Role, float Delta)
{
	Role->GetWorld()->TimeSeconds += Delta;
	Role->RoleGaitComponent->RefreshPolicy();
	Role->GetMesh()->TickAnimation(Delta, true);
	Role->GetMesh()->RefreshBoneTransforms();
}

const FWuwaAnimPlayerSnapshot* StartPlayer(const FWuwaAnimSyncSnapshot& Snapshot)
{
	return Snapshot.Players.FindByPredicate([](const auto& Player) { return !Player.bLooping; });
}

bool HasActiveStart(UWuwaAnimInstance* Anim)
{
	// Player cached weights are not cleared when their branch stops updating. Check the state contribution too.
	if (Anim->GetDebugStateWeight(TEXT("SM_Ground"), TEXT("StartMove")) <= .001f) return false;
	for (TFieldIterator<FStructProperty> It(Anim->GetClass()); It; ++It)
		if (It->Struct->IsChildOf(FAnimNode_AssetPlayerBase::StaticStruct()))
		{
			const auto* Player = It->ContainerPtrToValuePtr<FAnimNode_AssetPlayerBase>(Anim);
			if (Player->GetGroupName() == TEXT("LocomotionStart") && Player->GetCachedBlendWeight() > .001f) return true;
		}
	return false;
}
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FWuwaRoleSwitchAnimationTest, "Wuwa.Animation.RoleSwitch.Ground",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FWuwaRoleSwitchAnimationTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
	for (int32 Direction = 0; Direction < 2; ++Direction)
		for (int32 Gait = 0; Gait < 3; ++Gait)
		{
			Names.Add(FString::Printf(TEXT("%s.%s"), Direction == 0 ? TEXT("ChangliToJiabei") : TEXT("JiabeiToChangli"),
				Gait == 0 ? TEXT("Walk") : Gait == 1 ? TEXT("Run") : TEXT("Sprint")));
			Commands.Add(FString::Printf(TEXT("%d %d"), Direction, Gait));
		}
}

bool FWuwaRoleSwitchAnimationTest::RunTest(const FString& Parameters)
{
	using namespace WuwaSwitchTest;
	TArray<FString> Args;
	Parameters.ParseIntoArrayWS(Args);
	const int32 Direction = FCString::Atoi(*Args[0]), GaitIndex = FCString::Atoi(*Args[1]);
	const EWuwaGait Gait = GaitIndex == 0 ? EWuwaGait::Walk : GaitIndex == 1 ? EWuwaGait::Run : EWuwaGait::Sprint;
	FWorldScope Scope;
	if (!TestNotNull(TEXT("World"), Scope.World)) return false;
	Scope.World->TimeSeconds = 1.f;
	AWuwaCharacter* Characters[2] = {};
	for (int32 Index = 0; Index < 2; ++Index)
	{
		UClass* Class = LoadClass<AWuwaCharacter>(nullptr, Roles[Index]);
		if (!TestNotNull(TEXT("Authored role class"), Class)) return false;
		FActorSpawnParameters Spawn;
		Spawn.ObjectFlags |= RF_Transient;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AWuwaCharacter* Role = Scope.World->SpawnActor<AWuwaCharacter>(Class, Spawn);
		Characters[Index] = Role;
		if (!TestNotNull(TEXT("Role"), Role)) return false;
		Role->GetWuwaMovementComponent()->MovementMode = MOVE_Walking;
		if (!TestTrue(TEXT("Managed state ready"), Role->EnsureMovementStateSystem())) return false;
		Role->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Role->GetMesh()->bEnableUpdateRateOptimizations = false;
		Role->GetMesh()->InitAnim(true);
	}
	AWuwaCharacter* Old = Characters[Direction];
	AWuwaCharacter* New = Characters[1 - Direction];
	New->SetOnField(false);
	Old->InputIntent->SetMoveAxis(FVector2D(0, 1));
	Old->UnifiedStateComponent->SetWalkPreference(Gait == EWuwaGait::Walk);
	if (Gait == EWuwaGait::Sprint) Old->RoleGaitComponent->RequestSprint();
	const FVector Velocity(GaitIndex == 0 ? 200.f : GaitIndex == 1 ? 500.f : 900.f, 0, 0);
	Old->GetWuwaMovementComponent()->RestoreSwitchMotion(Velocity, FVector::ForwardVector, FVector(100, 0, 0));
	for (int32 Frame = 0; Frame < 120; ++Frame) TickPose(Old, 1.f / 60.f);

	const FWuwaRoleSwitchSnapshot Snapshot = Old->CaptureRoleSwitch();
	if (!Snapshot.Animation.IsValid())
	{
		auto* SourceAnim = CastChecked<UWuwaAnimInstance>(Old->GetMesh()->GetAnimInstance());
		AddInfo(FString::Printf(TEXT("Source ground=%d active=%d state=%s outer=%s"), SourceAnim->LocomotionData.bStateGround,
			SourceAnim->LocomotionData.bGroundMoveActive, *SourceAnim->GetDebugStateName(TEXT("SM_Ground")).ToString(),
			*SourceAnim->GetDebugStateName(TEXT("Locmotion")).ToString()));
		for (TFieldIterator<FStructProperty> It(SourceAnim->GetClass()); It; ++It)
			if (It->Struct->IsChildOf(FAnimNode_AssetPlayerBase::StaticStruct()))
			{
				const auto* Player = It->ContainerPtrToValuePtr<FAnimNode_AssetPlayerBase>(SourceAnim);
				if (Player->GetCachedBlendWeight() > .01f)
					AddInfo(FString::Printf(TEXT("Player %s %s asset=%s weight=%f"), *It->GetName(), *It->Struct->GetName(),
						*GetNameSafe(Player->GetAnimAsset()), Player->GetCachedBlendWeight()));
			}
	}
	if (!TestTrue(TEXT("Real moving role produces an animation snapshot"), Snapshot.Animation.IsValid())) return false;
	AddInfo(FString::Printf(TEXT("Captured %d players; phase %.4f"), Snapshot.Animation.Players.Num(), Snapshot.Animation.Players[0].NormalizedTime));
	Old->ResetPlayerInputState();
	Old->SetOnField(false);
	New->SetOnField(true);
	New->RestoreRoleSwitch(Snapshot);
	auto* Anim = Cast<UWuwaAnimInstance>(New->GetMesh()->GetAnimInstance());
	if (!TestNotNull(TEXT("Destination animation"), Anim)) return false;
	TestTrue(TEXT("Velocity immediately inherited"), New->GetVelocity().Equals(Velocity, 0.1f));
	TestTrue(TEXT("Continuous input inherited"), New->InputIntent->GetMoveIntent().Axis.Equals(FVector2D(0, 1)));
	TestEqual(TEXT("Gait inherited"), New->GetWuwaMovementComponent()->GetAllowedGait(), Gait);
	TestEqual(TEXT("First pose is Move, without waiting for Idle transition"), Anim->GetDebugStateName(TEXT("SM_Ground")), FName(TEXT("Move")));
	TestFalse(TEXT("Switching sustained movement never reactivates the start additive"), HasActiveStart(Anim));
	FWuwaAnimSyncSnapshot Restored = Anim->CaptureRoleAnimation();
	if (!TestTrue(TEXT("Destination has a running player"), Restored.IsValid())) return false;
	for (const FWuwaAnimPlayerSnapshot& Player : Snapshot.Animation.Players)
	{
		const auto* Other = Restored.Players.FindByPredicate([&](const auto& P) { return P.NodeName == Player.NodeName; });
		if (!TestNotNull(TEXT("Destination uses corresponding node"), Other)) continue;
		AddInfo(FString::Printf(TEXT("Phase old=%.5f %s->%s %.5f; new=%.5f %s->%s %.5f"),
			Player.NormalizedTime, *Player.MarkerPosition.PreviousMarkerName.ToString(), *Player.MarkerPosition.NextMarkerName.ToString(), Player.MarkerPosition.PositionBetweenMarkers,
			Other->NormalizedTime, *Other->MarkerPosition.PreviousMarkerName.ToString(), *Other->MarkerPosition.NextMarkerName.ToString(), Other->MarkerPosition.PositionBetweenMarkers));
		if (!Player.MarkerPosition.PreviousMarkerName.IsNone() && !Other->MarkerPosition.PreviousMarkerName.IsNone())
		{
			TestEqual(TEXT("Same preceding foot contact"), Other->MarkerPosition.PreviousMarkerName, Player.MarkerPosition.PreviousMarkerName);
			TestEqual(TEXT("Same next foot contact"), Other->MarkerPosition.NextMarkerName, Player.MarkerPosition.NextMarkerName);
			TestTrue(TEXT("Same position between foot contacts"), FMath::IsNearlyEqual(Other->MarkerPosition.PositionBetweenMarkers, Player.MarkerPosition.PositionBetweenMarkers, .025f));
		}
		else TestTrue(TEXT("Normalized phase fallback"), FMath::IsNearlyEqual(Other->NormalizedTime, Player.NormalizedTime, .025f));
	}
	TickPose(New, .05f);
	const FWuwaAnimSyncSnapshot Advanced = Anim->CaptureRoleAnimation();
	TestTrue(TEXT("Animation continues after one-shot restore"), Advanced.IsValid()
		&& !FMath::IsNearlyEqual(Advanced.Players[0].NormalizedTime, Restored.Players[0].NormalizedTime, .001f));
	TestFalse(TEXT("No montage transferred"), Anim->IsAnyMontagePlaying());
	// Neither a reused instance nor a normal entry may reapply the last handoff.
	New->SetOnField(false);
	New->ResetPlayerInputState();
	New->SetOnField(true);
	TestFalse(TEXT("A subsequent normal entry has no stale moving snapshot"), Anim->CaptureRoleAnimation().IsValid());
	if (Gait == EWuwaGait::Sprint)
	{
		New->RoleGaitComponent->SetGaitBlocked(New, EWuwaGait::Sprint, true);
		New->RestoreRoleSwitch(Snapshot);
		TestTrue(TEXT("Destination restriction wins over inherited sprint"), New->GetWuwaMovementComponent()->GetAllowedGait() != EWuwaGait::Sprint);
		TestEqual(TEXT("Blocked sprint desire is not retained"), New->RoleGaitComponent->ReadSprintDesire(), EWuwaSprintDesire::None);
		New->RoleGaitComponent->SetGaitBlocked(New, EWuwaGait::Sprint, false);
		New->RoleGaitComponent->RestoreSwitchGait(EWuwaSprintDesire::Temporary, .2f, EWuwaGait::Sprint);
		Scope.World->TimeSeconds += .075;
		const float Remaining = New->RoleGaitComponent->ReadSprintRemainingSeconds();
		TestTrue(TEXT("Temporary sprint retains only its remaining duration"), FMath::IsNearlyEqual(Remaining, .125f, .002f));
		Scope.World->TimeSeconds += .15;
		New->RoleGaitComponent->RefreshPolicy();
		TestEqual(TEXT("Temporary sprint expires after handoff"), New->RoleGaitComponent->ReadSprintDesire(), EWuwaSprintDesire::None);
	}
	FWuwaAnimSyncSnapshot Incompatible = Snapshot.Animation;
	Incompatible.GraphOwner = UWuwaAnimInstance::StaticClass();
	TestFalse(TEXT("A different graph owner cannot restore matching-looking node names"), Anim->QueueRoleAnimation(Incompatible));
	return true;
}

IMPLEMENT_COMPLEX_AUTOMATION_TEST(FWuwaRoleSwitchStartTest, "Wuwa.Animation.RoleSwitch.StartMove",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

void FWuwaRoleSwitchStartTest::GetTests(TArray<FString>& Names, TArray<FString>& Commands) const
{
	for (int32 Direction = 0; Direction < 2; ++Direction)
	{
		Names.Add(Direction == 0 ? TEXT("ChangliToJiabei") : TEXT("JiabeiToChangli"));
		Commands.Add(FString::FromInt(Direction));
	}
}

bool FWuwaRoleSwitchStartTest::RunTest(const FString& Parameters)
{
	using namespace WuwaSwitchTest;
	FWorldScope Scope;
	if (!TestNotNull(TEXT("World"), Scope.World)) return false;
	Scope.World->TimeSeconds = 1.f;
	AWuwaCharacter* RolesInWorld[2] = {};
	for (int32 I = 0; I < 2; ++I)
	{
		UClass* Class = LoadClass<AWuwaCharacter>(nullptr, Roles[I]);
		if (!TestNotNull(TEXT("Role class"), Class)) return false;
		FActorSpawnParameters Spawn;
		Spawn.ObjectFlags |= RF_Transient;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Role = Scope.World->SpawnActor<AWuwaCharacter>(Class, Spawn);
		if (!TestNotNull(TEXT("Role"), Role)) return false;
		RolesInWorld[I] = Role;
		Role->GetWuwaMovementComponent()->MovementMode = MOVE_Walking;
		if (!TestTrue(TEXT("Movement ready"), Role->EnsureMovementStateSystem())) return false;
		Role->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Role->GetMesh()->bEnableUpdateRateOptimizations = false;
		Role->GetMesh()->InitAnim(true);
		for (int32 Frame = 0; Frame < 5; ++Frame) TickPose(Role, 1.f / 60.f);
	}
	const int32 Direction = FCString::Atoi(*Parameters);
	auto* Old = RolesInWorld[Direction];
	auto* New = RolesInWorld[1 - Direction];
	New->SetOnField(false);
	Old->InputIntent->SetMoveAxis(FVector2D(0, 1));
	Old->GetWuwaMovementComponent()->RestoreSwitchMotion(FVector(300, 0, 0), FVector::ForwardVector, FVector(500, 0, 0));
	for (int32 Frame = 0; Frame < 10; ++Frame) TickPose(Old, 1.f / 60.f);
	const FWuwaRoleSwitchSnapshot Snapshot = Old->CaptureRoleSwitch();
	TestEqual(TEXT("Ordinary movement entry plays StartMove"), Snapshot.Animation.GroundState, FName(TEXT("StartMove")));
	const auto* SourceStart = StartPlayer(Snapshot.Animation);
	if (!TestNotNull(TEXT("One-shot start progress captured"), SourceStart)) return false;
	TestTrue(TEXT("Source start is in progress"), SourceStart->NormalizedTime > 0.f && SourceStart->NormalizedTime < .95f);
	Old->SetOnField(false);
	New->SetOnField(true);
	New->RestoreRoleSwitch(Snapshot);
	auto* Anim = CastChecked<UWuwaAnimInstance>(New->GetMesh()->GetAnimInstance());
	TestEqual(TEXT("Mid-start switch preserves StartMove"), Anim->GetDebugStateName(TEXT("SM_Ground")), FName(TEXT("StartMove")));
	const auto Restored = Anim->CaptureRoleAnimation();
	const auto* TargetStart = StartPlayer(Restored);
	if (!TestNotNull(TEXT("Target additive is active"), TargetStart)) return false;
	TestTrue(TEXT("Additive restores its own progress, independent of foot phase"), FMath::IsNearlyEqual(SourceStart->NormalizedTime, TargetStart->NormalizedTime, .002f));
	TickPose(New, .03f);
	const auto Next = Anim->CaptureRoleAnimation();
	const auto* Advanced = StartPlayer(Next);
	TestTrue(TEXT("One-shot advances after restoration"), Advanced && Advanced->NormalizedTime > TargetStart->NormalizedTime);
	for (int32 Frame = 0; Frame < 120; ++Frame) TickPose(New, 1.f / 60.f);
	TestEqual(TEXT("Start finishes into sustained Move"), Anim->GetDebugStateName(TEXT("SM_Ground")), FName(TEXT("Move")));
	TestFalse(TEXT("Finished start no longer contributes"), HasActiveStart(Anim));
	// Return to idle and start again: a real new start must still replay its own additive.
	New->InputIntent->SetMoveAxis(FVector2D::ZeroVector);
	New->GetWuwaMovementComponent()->RestoreSwitchMotion(FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector);
	for (int32 Frame = 0; Frame < 45; ++Frame) TickPose(New, 1.f / 60.f);
	New->InputIntent->SetMoveAxis(FVector2D(0, 1));
	New->GetWuwaMovementComponent()->RestoreSwitchMotion(FVector(300, 0, 0), FVector::ForwardVector, FVector(500, 0, 0));
	for (int32 Frame = 0; Frame < 3; ++Frame) TickPose(New, 1.f / 60.f);
	TestEqual(TEXT("A later real start enters StartMove again"), Anim->GetDebugStateName(TEXT("SM_Ground")), FName(TEXT("StartMove")));
	TestTrue(TEXT("A later real start replays its additive"), HasActiveStart(Anim));
	New->InputIntent->SetMoveAxis(FVector2D::ZeroVector);
	for (int32 Frame = 0; Frame < 3; ++Frame) TickPose(New, 1.f / 60.f);
	TestEqual(TEXT("Releasing during start reaches Stop immediately"), Anim->GetDebugStateName(TEXT("SM_Ground")), FName(TEXT("Stop")));
	// Action override uses the existing direct-idle policy rather than waiting for start completion.
	New->RestoreRoleSwitch(Snapshot);
	TestTrue(TEXT("Action state accepted"), New->UnifiedStateComponent->SetMoveState(EWuwaMoveState::Dodge, EWuwaGait::Run));
	// Evaluate the action-state sample itself; ordinary gait writes are intentionally allowed to replace it.
	for (int32 Frame = 0; Frame < 3; ++Frame)
	{
		New->GetMesh()->TickAnimation(1.f / 60.f, true);
		New->GetMesh()->RefreshBoneTransforms();
	}
	TestEqual(TEXT("Action override can interrupt StartMove"), Anim->GetDebugStateName(TEXT("SM_Ground")), FName(TEXT("Idle")));
	return true;
}

#endif
