#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "AnimNode_ControlRig.h"
#include "ControlRig.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "UObject/UnrealType.h"
#include "Game/Animation/IK/WuwaFootIK.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimInstance.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputIntentComponent.h"
#include "Game/NewWorld/Character/Role/Component/WuwaRoleGaitBridgeComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaFootIKTest, "Wuwa.Animation.FootIK.TerrainAndGating",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaFootIKTest::RunTest(const FString& Parameters)
{
	FString Csv = TEXT("role,case,left_weight,right_weight,left_offset,right_offset,pelvis_offset\n");
	for (const TCHAR* RolePath : { TEXT("/Game/Characters/Role/changli/BP_Role_Changli.BP_Role_Changli_C"),
		TEXT("/Game/Characters/Role/jiabei/BP_Role_jiabei.BP_Role_jiabei_C") })
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
		ON_SCOPE_EXIT { World->DestroyWorld(false); };
		World->TimeSeconds = 1.f;
		FActorSpawnParameters Spawn;
		Spawn.ObjectFlags |= RF_Transient;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Role = World->SpawnActor<AWuwaCharacter>(LoadClass<AWuwaCharacter>(nullptr, RolePath), Spawn);
		if (!TestNotNull(TEXT("Role"), Role)) return false;
		UWuwaMovementComponent* Movement = Role->GetWuwaMovementComponent();
		Movement->MovementMode = MOVE_Walking;
		if (!TestTrue(TEXT("Production movement initialized"), Role->EnsureMovementStateSystem())) return false;
		auto* Mesh = Role->GetMesh();
		Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Mesh->bEnableUpdateRateOptimizations = false;
		Mesh->InitAnim(true);
		auto* Anim = CastChecked<UWuwaAnimInstance>(Mesh->GetAnimInstance());
		auto Tick = [&]()
		{
			World->TimeSeconds += 1.f/60.f;
			Role->RoleGaitComponent->RefreshPolicy();
			Mesh->TickAnimation(1.f/60.f, true);
			Mesh->RefreshBoneTransforms();
		};
		for (int I=0; I<90; ++I) Tick();
		TestTrue(TEXT("IK enabled by default on the real role"), Anim->FootIKSettings.bEnabled);
		TestTrue(TEXT("Standing enables global IK"), Anim->FootIKFrame.bGrounded && Anim->FootIKFrame.Alpha > .99f);
		for (int I=0; I<12; ++I)
		{
			const bool Move = I%2 == 0;
			Role->InputIntent->SetMoveAxis(Move ? FVector2D(0,1) : FVector2D::ZeroVector);
			Movement->RestoreSwitchMotion(Move ? FVector(350,0,0) : FVector::ZeroVector,
				Move ? FVector::ForwardVector : FVector::ZeroVector, FVector::ZeroVector);
			for (int F=0; F<4; ++F) Tick();
			TestTrue(TEXT("Start/Stop taps do not disable IK"), Anim->FootIKFrame.bGrounded && Anim->FootIKFrame.Alpha > .99f);
		}
		Movement->MovementMode = MOVE_Falling;
		Tick();
		TestFalse(TEXT("Airborne disables IK"), Anim->FootIKFrame.bGrounded);
		Movement->MovementMode = MOVE_Walking;
		Anim->bForceDisableFootIK = true;
		Tick();
		TestFalse(TEXT("Explicit override disables IK"), Anim->FootIKFrame.bGrounded);
		Anim->bForceDisableFootIK = false;
		Role->InputIntent->SetMoveAxis(FVector2D::ZeroVector);
		Movement->RestoreSwitchMotion(FVector::ZeroVector, FVector::ZeroVector, FVector::ZeroVector);
		Anim->FootIKSettings.bEnabled = false;
		for (int I=0; I<180; ++I) Tick();
		UControlRig* Rig = nullptr;
		for (TFieldIterator<FStructProperty> It(Anim->GetClass()); It; ++It)
			if (It->Struct == FAnimNode_ControlRig::StaticStruct()) Rig = It->ContainerPtrToValuePtr<FAnimNode_ControlRig>(Anim)->GetControlRig();
		if (!TestNotNull(TEXT("Live per-role Control Rig"), Rig)) return false;
		URigHierarchy* H = Rig->GetHierarchy();
		TMap<FRigElementKey,FTransform> Raw;
		for (const auto& Key : H->GetAllKeys()) if (Key.Type == ERigElementType::Bone) Raw.Add(Key, H->GetGlobalTransform(Key));
		auto Bone = [](const TCHAR* Name) { return FRigElementKey(Name,ERigElementType::Bone); };
		const auto Left=Bone(TEXT("Bip001LFoot")), Right=Bone(TEXT("Bip001RFoot"));
		FControlRigExecuteContext Context;
		Context.Hierarchy = H;
		Context.SetWorld(World);
		Context.SetOwningComponent(Mesh);
		Context.SetOwningActor(Role);
		Context.SetToWorldSpaceTransform(Mesh->GetComponentTransform());
		Context.SetDeltaTime(1.f/60.f);
		FWuwaFootIKFrame Frame;
		Frame.bGrounded = true;
		Frame.Alpha = 1.f;
		Frame.GroundZ = Anim->FootIKFrame.GroundZ;
		FWuwaFootIKSettings Settings;
		FWuwaFootIKWork Work;
		TArray<UBoxComponent*> Floors;
		auto Floor = [&](FVector Top, FVector Extent, FRotator Rotation)
		{
			auto* Actor=World->SpawnActor<AActor>(Spawn);
			auto* Box=NewObject<UBoxComponent>(Actor);
			Actor->SetRootComponent(Box);
			Box->SetBoxExtent(Extent);
			Box->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			Box->SetCollisionResponseToAllChannels(ECR_Block);
			Box->SetWorldLocationAndRotation(Top-Rotation.RotateVector(FVector(0,0,Extent.Z)),Rotation);
			Box->RegisterComponent();
			Floors.Add(Box);
			return Box;
		};
		auto Clear = [&]() { for (auto* Box : Floors) Box->DestroyComponent(); Floors.Reset(); };
		auto Restore = [&]() { for (const auto& Pair : Raw) H->SetGlobalTransform(Pair.Key,Pair.Value,false,false); };
		auto Validate = [&](const TCHAR* Case)
		{
			for (const auto& Pair : Raw)
			{
				const FTransform Current = H->GetGlobalTransform(Pair.Key);
				if (!TestFalse(FString(Case)+TEXT(" finite pose"), Current.ContainsNaN())) return;
				if (!TestTrue(FString(Case)+TEXT(" preserves bone scale"), Current.GetScale3D().Equals(Pair.Value.GetScale3D(),.01))) return;
			}
			for (const TCHAR* Side : {TEXT("L"),TEXT("R")}) for (int Segment=0;Segment<2;++Segment)
			{
				const auto A=Bone(*(FString(TEXT("Bip001"))+Side+(Segment?TEXT("Calf"):TEXT("Thigh"))));
				const auto B=Bone(*(FString(TEXT("Bip001"))+Side+(Segment?TEXT("Foot"):TEXT("Calf"))));
				const float Before=FVector::Distance(Raw[A].GetLocation(),Raw[B].GetLocation());
				const float After=FVector::Distance(H->GetGlobalTransform(A).GetLocation(),H->GetGlobalTransform(B).GetLocation());
				TestTrue(FString(Case)+TEXT(" preserves leg length"),FMath::IsNearlyEqual(Before,After,.05f));
			}
		};
		auto Solve = [&](const TCHAR* Case, int Frames=120)
		{
			FWuwaFootIKResult Result;
			for (int I=0; I<Frames; ++I) { Restore(); Result=WuwaFootIK::Solve(Context,Frame,Settings,Work); }
			Validate(Case);
			Csv+=FString::Printf(TEXT("%s,%s,%.4f,%.4f,%.4f,%.4f,%.4f\n"),*Role->GetName(),Case,
				Result.LeftWeight,Result.RightWeight,Result.Offsets.X,Result.Offsets.Y,Result.Offsets.Z);
			return Result;
		};
		const FVector LW=Context.ToWorldSpace(Raw[Left].GetLocation()),RW=Context.ToWorldSpace(Raw[Right].GetLocation());
		Floor(FVector(0,0,Frame.GroundZ),FVector(500,500,10),FRotator::ZeroRotator);
		auto Result=Solve(TEXT("flat"));
		TestTrue(TEXT("Both feet detect floor"),Result.bLeftHit&&Result.bRightHit);
		TestTrue(TEXT("Flat floor leaves authored pose alone"),Result.Offsets.IsNearlyZero(.05));
		TestTrue(TEXT("Flat ankle preserved"),H->GetGlobalTransform(Left).Equals(Raw[Left],.01));
		Clear();
		Floor(FVector(0,0,Frame.GroundZ),FVector(500,500,10),FRotator(0,0,20));
		Result=Solve(TEXT("slope20"));
		TestTrue(TEXT("Slope has both supports"),Result.bLeftHit&&Result.bRightHit);
		TestTrue(TEXT("Slope changes feet independently"),FMath::Abs(Result.Offsets.X-Result.Offsets.Y)>1.f);
		TestTrue(TEXT("Slope tilts ankle"),Raw[Left].GetRotation().AngularDistance(H->GetGlobalTransform(Left).GetRotation())>.05);
		Clear();
		Floor(FVector(LW.X,LW.Y,Frame.GroundZ+12),FVector(4,4,4),FRotator::ZeroRotator);
		Floor(FVector(RW.X,RW.Y,Frame.GroundZ-10),FVector(4,4,4),FRotator::ZeroRotator);
		Result=Solve(TEXT("split_step"));
		TestTrue(TEXT("Raised foot follows +12cm step"),FMath::IsNearlyEqual(float(Result.Offsets.X),12.f,.1f));
		TestTrue(TEXT("Lower foot follows -10cm step"),FMath::IsNearlyEqual(float(Result.Offsets.Y),-10.f,.1f));
		TestTrue(TEXT("Pelvis lowers for supported lower foot"),Result.Offsets.Z < -5.f && Result.Offsets.Z>=-Settings.MaxPelvisDrop-.01f);
		TestTrue(TEXT("Solved left ankle reaches raised target"),FMath::IsNearlyEqual(float(Context.ToWorldSpace(H->GetGlobalTransform(Left).GetLocation()).Z-LW.Z),12.f,.2f));
		TestTrue(TEXT("Solved right ankle reaches lowered target"),FMath::IsNearlyEqual(float(Context.ToWorldSpace(H->GetGlobalTransform(Right).GetLocation()).Z-RW.Z),-10.f,.2f));
		const auto CurveL=FRigElementKey(TEXT("Foot_IK_l"),ERigElementType::Curve);
		if (!TestTrue(TEXT("Rig accepts authored foot curves"),H->GetIndex(CurveL)!=INDEX_NONE)) return false;
		H->SetCurveValue(CurveL,0.f);
		Result=Solve(TEXT("left_curve_zero"));
		TestTrue(TEXT("Authored zero is respected"),Result.LeftWeight==0.f&&FMath::Abs(Result.Offsets.X)<.01f);
		H->UnsetCurveValue(CurveL);
		Clear();
		Result=Solve(TEXT("no_ground"));
		TestFalse(TEXT("Miss is not treated as ground"),Result.bLeftHit||Result.bRightHit);
		TestTrue(TEXT("Miss returns to animation"),Result.Offsets.IsNearlyZero(.01));
		Frame.bGrounded=false;
		Result=Solve(TEXT("airborne"),1);
		TestTrue(TEXT("Airborne pose unchanged"),H->GetGlobalTransform(Left).Equals(Raw[Left],.001));
		Frame.bGrounded=true;
		Settings.bEnabled=false;
		Result=Solve(TEXT("disabled"),1);
		TestTrue(TEXT("Switch off is a bypass"),H->GetGlobalTransform(Left).Equals(Raw[Left],.001));
		Clear();
		// Exercise the serialized RigVM node through the production AnimBP, not just the shared solver.
		const FVector LiveBefore=Mesh->GetSocketLocation(TEXT("Bip001LFoot"));
		auto* Platform=Floor(FVector(0,0,Frame.GroundZ+10),FVector(500,500,10),FRotator::ZeroRotator);
		Anim->FootIKSettings.bEnabled=true;
		for (int I=0;I<120;++I) Tick();
		const FVector LiveAfter=Mesh->GetSocketLocation(TEXT("Bip001LFoot"));
		TestTrue(TEXT("Serialized live rig actually raises feet onto terrain"),LiveAfter.Z-LiveBefore.Z>5.f);
		Role->SetBase(Platform);
		Platform->ComponentVelocity=FVector(100,0,0);
		for (int I=0;I<180;++I) Tick();
		TestTrue(TEXT("Moving platform weakens IK"),Anim->FootIKFrame.Alpha<.02f);
		Platform->ComponentVelocity=FVector::ZeroVector;
		for (int I=0;I<180;++I) Tick();
		TestTrue(TEXT("Stopped platform restores IK"),Anim->FootIKFrame.Alpha>.98f);
		Role->SetBase(nullptr);
		Clear();
	}
	FFileHelper::SaveStringToFile(Csv,*(FPaths::ProjectSavedDir()/TEXT("Diagnostics/FootIK/Terrain.csv")));
	return true;
}
#endif
