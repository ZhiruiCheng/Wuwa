#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "Animation/AnimNode_AssetPlayerBase.h"
#include "AnimNodes/AnimNode_ApplyAdditive.h"
#include "UObject/UnrealType.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimInstance.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputIntentComponent.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Role/Component/WuwaRoleGaitBridgeComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Engine/Engine.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Formation/WuwaFormationComponent.h"
#include "Game/Common/WuwaGameTags.h"
#include "Game/Input/DataAsset/WuwaInputDataAsset.h"
#include "Game/Input/WuwaInputTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaMoveStartLeanTest, "Wuwa.Animation.RoleSwitch.StartLean",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaMoveStartLeanTest::RunTest(const FString& Parameters)
{
	const TCHAR* Classes[] = { TEXT("/Game/Characters/Role/changli/BP_Role_Changli.BP_Role_Changli_C"),
		TEXT("/Game/Characters/Role/jiabei/BP_Role_jiabei.BP_Role_jiabei_C") };
	FString Csv = TEXT("role,frame,state,state_weight,start_time,alpha,lean_on_degrees,lean_off_degrees,delta_degrees\n");
	for (int32 R = 0; R < 2; ++R)
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		ON_SCOPE_EXIT { World->DestroyWorld(false); };
		World->TimeSeconds = 1.f;
		AWuwaCharacter* Roles[2] = {};
		UWuwaAnimInstance* Anims[2] = {};
		FAnimNode_ApplyAdditive* Adds[2] = {};
		UClass* Class = LoadClass<AWuwaCharacter>(nullptr, Classes[R]);
		if (!TestNotNull(TEXT("Role class"), Class)) return false;
		for (int32 I = 0; I < 2; ++I)
		{
			FActorSpawnParameters Spawn;
			Spawn.ObjectFlags |= RF_Transient;
			Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			Roles[I] = World->SpawnActor<AWuwaCharacter>(Class, Spawn);
			auto* Role = Roles[I];
			Role->GetWuwaMovementComponent()->MovementMode = MOVE_Walking;
			if (!TestTrue(TEXT("Movement initialized"), Role->EnsureMovementStateSystem())) return false;
			Role->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
			Role->GetMesh()->bEnableUpdateRateOptimizations = false;
			Role->GetMesh()->InitAnim(true);
			Anims[I] = CastChecked<UWuwaAnimInstance>(Role->GetMesh()->GetAnimInstance());
			for (TFieldIterator<FStructProperty> It(Anims[I]->GetClass()); It; ++It)
				if (It->Struct == FAnimNode_ApplyAdditive::StaticStruct())
				{
					if (!TestNull(TEXT("One authored additive node"), Adds[I])) return false;
					Adds[I] = It->ContainerPtrToValuePtr<FAnimNode_ApplyAdditive>(Anims[I]);
				}
			if (!TestNotNull(TEXT("Start additive"), Adds[I])) return false;
		}
		float MaxDifference = 0.f;
		bool bObservedStart = false;
		for (int32 Frame = 0; Frame < 90; ++Frame)
		{
			World->TimeSeconds += 1.f / 60.f;
			for (int32 I = 0; I < 2; ++I)
			{
				auto* Role = Roles[I];
				// Same motion/phase on both meshes. Only the transient control instance suppresses additive.
				Adds[I]->AlphaScaleBias.Scale = I == 0 ? 1.f : 0.f;
				const bool Moving = Frame >= 10;
				Role->InputIntent->SetMoveAxis(Moving ? FVector2D(0, 1) : FVector2D::ZeroVector);
				const float Speed = Moving ? FMath::Min((Frame - 9) * 20.f, 500.f) : 0.f;
				Role->GetWuwaMovementComponent()->RestoreSwitchMotion(FVector(Speed, 0, 0), Moving ? FVector::ForwardVector : FVector::ZeroVector, FVector::ZeroVector);
				Role->RoleGaitComponent->RefreshPolicy();
				Role->GetMesh()->TickAnimation(1.f / 60.f, true);
				Role->GetMesh()->RefreshBoneTransforms();
			}
			auto Lean = [](AWuwaCharacter* Role)
			{
				const FVector Torso = Role->GetMesh()->GetBoneLocation(TEXT("Bip001Head")) - Role->GetMesh()->GetBoneLocation(TEXT("Bip001Pelvis"));
				return FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(Torso, Role->GetActorForwardVector()), FVector::DotProduct(Torso, FVector::UpVector)));
			};
			const FName State = Anims[0]->GetDebugStateName(TEXT("SM_Ground"));
			const float Weight = Anims[0]->GetDebugStateWeight(TEXT("SM_Ground"), TEXT("StartMove"));
			float Time = -1.f;
			for (TFieldIterator<FStructProperty> It(Anims[0]->GetClass()); It; ++It)
				if (It->Struct->IsChildOf(FAnimNode_AssetPlayerBase::StaticStruct()))
				{
					const auto* Player = It->ContainerPtrToValuePtr<FAnimNode_AssetPlayerBase>(Anims[0]);
					if (Player->GetGroupName() == TEXT("LocomotionStart")) Time = Player->GetAccumulatedTime();
				}
			const float On = Lean(Roles[0]), Off = Lean(Roles[1]);
			if (State == TEXT("StartMove") && Weight > .95f)
			{
				bObservedStart = true;
				MaxDifference = FMath::Max(MaxDifference, FMath::Abs(On - Off));
			}
			Csv += FString::Printf(TEXT("%d,%d,%s,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f\n"), R, Frame,
				*State.ToString(), Weight, Time, Adds[0]->ActualAlpha, On, Off, On - Off);
		}
		AddInfo(FString::Printf(TEXT("Role %d additive changes evaluated torso lean by up to %.3f degrees"), R, MaxDifference));
		TestTrue(TEXT("Real start state evaluated"), bObservedStart);
		TestTrue(TEXT("Additive visibly changes the final skeletal pose (> 1 degree)"), MaxDifference > 1.f);
	}
	const FString Directory = FPaths::ProjectSavedDir() / TEXT("Diagnostics/MoveStartSplit");
	IFileManager::Get().MakeDirectory(*Directory, true);
	FFileHelper::SaveStringToFile(Csv, *(Directory / TEXT("LeanComparison.csv")));
	return true;
}

namespace WuwaStartLeanPIE
{
class FObserveStart final : public IAutomationLatentCommand
{
public:
	explicit FObserveStart(FAutomationTestBase* InTest) : Test(InTest) {}
	bool Update() override
	{
		if (StartedAt == 0.) StartedAt = FPlatformTime::Seconds();
		if (FPlatformTime::Seconds() - StartedAt > 30.) { Test->AddError(TEXT("PIE start observation timed out")); return true; }
		UWorld* World = nullptr;
		for (const auto& Context : GEngine->GetWorldContexts())
			if (Context.WorldType == EWorldType::PIE) World = Context.World();
		auto* PC = World ? Cast<AWuwaPlayerController>(World->GetFirstPlayerController()) : nullptr;
		auto* Role = PC ? Cast<AWuwaCharacter>(PC->GetPawn()) : nullptr;
		if (!Role || !PC->FormationComponent || PC->FormationComponent->GetMembers().Num() < 2) return false;
		auto* Anim = Cast<UWuwaAnimInstance>(Role->GetMesh()->GetAnimInstance());
		if (!Anim) return false;
		Role->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Role->GetMesh()->bEnableUpdateRateOptimizations = false;
		const auto& Tags = FWuwaGameTags::Get();
		const auto* Entry = PC->InputTagMap ? PC->InputTagMap->InputDataAssetMap.FindByPredicate(
			[&Tags](const auto& Row) { return Row.InputTag == Tags.Player_Common_Movement_Move; }) : nullptr;
		if (!Test->TestNotNull(TEXT("Authored movement binding"), Entry)) return true;
		FWuwaInputEvent Input;
		Input.InputTag = Entry->InputTag; Input.RouteTag = Entry->RouteTag; Input.SourceAction = Entry->InputAction;
		if (StageTime < 0.) StageTime = World->GetTimeSeconds();
		const double Elapsed = World->GetTimeSeconds() - StageTime;
		const bool bMove = Elapsed >= 1. && Elapsed < 3.;
		Input.Phase = bMove ? EWuwaInputPhase::Triggered : EWuwaInputPhase::Released;
		Input.Value = FInputActionValue(bMove ? FVector2D(0, 1) : FVector2D::ZeroVector);
		PC->RouteInputEvent(Input);
		const FName State = Anim->GetDebugStateName(TEXT("SM_Ground"));
		const float Weight = Anim->GetDebugStateWeight(TEXT("SM_Ground"), TEXT("StartMove"));
		float Alpha = 0.f, Time = -1.f;
		for (TFieldIterator<FStructProperty> It(Anim->GetClass()); It; ++It)
		{
			if (It->Struct == FAnimNode_ApplyAdditive::StaticStruct()) Alpha = It->ContainerPtrToValuePtr<FAnimNode_ApplyAdditive>(Anim)->ActualAlpha;
			if (It->Struct->IsChildOf(FAnimNode_AssetPlayerBase::StaticStruct()))
			{
				const auto* Player = It->ContainerPtrToValuePtr<FAnimNode_AssetPlayerBase>(Anim);
				if (Player->GetGroupName() == TEXT("LocomotionStart")) Time = Player->GetAccumulatedTime();
			}
		}
		const FVector Torso = Role->GetMesh()->GetBoneLocation(TEXT("Bip001Head")) - Role->GetMesh()->GetBoneLocation(TEXT("Bip001Pelvis"));
		const float Lean = FMath::RadiansToDegrees(FMath::Atan2(FVector::DotProduct(Torso, Role->GetActorForwardVector()), Torso.Z));
		Csv += FString::Printf(TEXT("%d,%.4f,%s,%.4f,%.4f,%.4f,%.4f,%.4f,%d\n"), RoleIndex, Elapsed, *State.ToString(),
			Role->GetVelocity().Size2D(), Weight, Time, Alpha, Lean, int32(Role->GetWuwaMovementComponent()->MovementMode));
		if (bMove && State == TEXT("StartMove") && Weight > .5f && Alpha > .5f) Observed[RoleIndex] = true;
		if (Elapsed < 4.) return false;
		if (RoleIndex == 0)
		{
			PC->FormationComponent->RequestSwitch(1);
			RoleIndex = 1; StageTime = -1.;
			return false;
		}
		Test->TestTrue(TEXT("Changli real input/physics enters weighted StartMove"), Observed[0]);
		Test->TestTrue(TEXT("Jiabei real input/physics enters weighted StartMove"), Observed[1]);
		FFileHelper::SaveStringToFile(Csv, *(FPaths::ProjectSavedDir() / TEXT("Diagnostics/MoveStartSplit/LeanPIE.csv")));
		return true;
	}
private:
	FAutomationTestBase* Test;
	double StartedAt = 0., StageTime = -1.;
	int32 RoleIndex = 0;
	bool Observed[2] = {};
	FString Csv = TEXT("role,elapsed,state,speed,state_weight,start_time,alpha,lean_degrees,movement_mode\n");
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaMoveStartLeanPIETest, "Wuwa.Animation.RoleSwitch.StartLeanPIE",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWuwaMoveStartLeanPIETest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/Map/Wuwa"))) return false;
	ADD_LATENT_AUTOMATION_COMMAND(WuwaStartLeanPIE::FObserveStart(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}
#endif
