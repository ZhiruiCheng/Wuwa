#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/Input/WuwaTestMoveInput.h"
#include "Tests/Movement/WuwaTestGait.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNode_AssetPlayerBase.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "AnimNode_ControlRig.h"
#include "ControlRig.h"
#include "Components/SkeletalMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "InputAction.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/UnrealType.h"
#include "Core/Tag/WuwaGameTags.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Input/WuwaInputRouterComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAbilitySystemComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAttributeSet.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimInstance.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaAbilityInputHandlerComponent.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputCommandConfig.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillBridgeComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"

namespace WuwaRoleChildrenRuntime
{
constexpr float StepSeconds = 1.f / 60.f;
const TCHAR* TrackedBones[] = { TEXT("Root"), TEXT("Bip001Pelvis"), TEXT("Bip001Head"),
	TEXT("Bip001LThigh"), TEXT("Bip001LCalf"), TEXT("Bip001LFoot"),
	TEXT("Bip001RThigh"), TEXT("Bip001RCalf"), TEXT("Bip001RFoot") };

// Independently inspect the authored entry interval; later recovery contacts
// must not make a tail start look like a valid stopping entry.
float StopEntryIntervalEnd(const UAnimSequence* Sequence)
{
	if (!Sequence) return -1.f;
	TArray<FAnimSyncMarker> FootMarkers;
	for (const FAnimSyncMarker& Marker : Sequence->AuthoredSyncMarkers)
	{
		if ((Marker.MarkerName == TEXT("Left") || Marker.MarkerName == TEXT("Right"))
			&& FMath::IsFinite(Marker.Time) && Marker.Time >= 0.f && Marker.Time < Sequence->GetPlayLength())
		{
			FootMarkers.Add(Marker);
		}
	}
	FootMarkers.Sort([](const FAnimSyncMarker& A, const FAnimSyncMarker& B) { return A.Time < B.Time; });
	for (int32 Index = 1; Index < FootMarkers.Num(); ++Index)
	{
		if (FootMarkers[Index].MarkerName != FootMarkers[0].MarkerName)
		{
			return FootMarkers[Index].Time > FootMarkers[0].Time ? FootMarkers[Index].Time : -1.f;
		}
	}
	return -1.f;
}

struct FFixture
{
	FAutomationTestBase& Test;
	FString Role;
	UWorld* World = nullptr;
	AWuwaCharacter* Character = nullptr;
	AWuwaPlayerController* Controller = nullptr;
	UWuwaMovementComponent* Movement = nullptr;
	UWuwaAbilitySystemComponent* ASC = nullptr;
	UWuwaAnimInstance* Anim = nullptr;
	USkeletalMeshComponent* Mesh = nullptr;
	UClass* ExpectedRigClass = nullptr;
	FStructProperty* AccelerationProperty = nullptr;
	TArray<int32> BoneIndices;
	TArray<FTransform> ReferenceWorld;
	TArray<FString> ActiveAssets;
	TArray<TSharedPtr<FJsonValue>> Frames;
	TSharedRef<FJsonObject> Report = MakeShared<FJsonObject>();
	TSet<FString> ObservedAssets;
	TSet<FString> FailedPhases;
	FString Phase = TEXT("Initialize");
	FVector Velocity = FVector::ZeroVector;
	FVector Acceleration = FVector::ZeroVector;
	int32 FrameIndex = 0;
	bool bObservedRuntimeRig = false;
	bool bObservedSharedAccel = false;
	bool bCaptureEveryFrame = false;
	FString ReportPrefix = TEXT("Runtime_");
	FString ReportDirectory = TEXT("Diagnostics/RoleChildrenMigration");

	FFixture(FAutomationTestBase& InTest, const TCHAR* InRole) : Test(InTest), Role(InRole)
	{
		Report->SetStringField(TEXT("role"), Role);
		Report->SetStringField(TEXT("scope"), TEXT("Real role Blueprint, mesh, child AnimBP, input router, command table, GA and montage. Transient world; locomotion uses production InputIntent/RoleGait with prescribed CMC velocity/acceleration samples. No world collision simulation or asset/CDO mutation."));
	}

	~FFixture()
	{
		if (ASC) ASC->CancelAllAbilities();
		if (Controller) Controller->SetPawn(nullptr);
		if (Anim) Anim->UninitializeAnimation();
		if (ASC) ASC->ClearActorInfo();
		if (World) World->DestroyWorld(false);
		Report->SetBoolField(TEXT("observedChildRig"), bObservedRuntimeRig);
		Report->SetBoolField(TEXT("observedSharedAccel"), bObservedSharedAccel);
		Report->SetArrayField(TEXT("frames"), Frames);
		TArray<TSharedPtr<FJsonValue>> Assets;
		for (const FString& Path : ObservedAssets) Assets.Add(MakeShared<FJsonValueString>(Path));
		Report->SetArrayField(TEXT("observedAssets"), Assets);
		FString Json;
		const auto Writer = TJsonWriterFactory<>::Create(&Json);
		const FString Directory = FPaths::ProjectSavedDir() / ReportDirectory;
		IFileManager::Get().MakeDirectory(*Directory, true);
		const FString Path = Directory / (ReportPrefix + Role + TEXT(".json"));
		Test.TestTrue(TEXT("Runtime pose report is written"), FJsonSerializer::Serialize(Report, Writer) && FFileHelper::SaveStringToFile(Json, *Path));
		Test.AddInfo(FString::Printf(TEXT("%s runtime report: %s"), *Role, *Path));
	}

	bool Initialize(const TCHAR* CharacterPath, const TCHAR* AnimationPath)
	{
		World = UWorld::CreateWorld(EWorldType::Game, false);
		if (!Test.TestNotNull(TEXT("Transient world"), World)) return false;
		World->TimeSeconds = 1.f;
		UClass* CharacterClass = LoadClass<AWuwaCharacter>(nullptr, CharacterPath);
		UClass* ExpectedAnimClass = LoadClass<UWuwaAnimInstance>(nullptr, AnimationPath);
		if (!Test.TestNotNull(CharacterPath, CharacterClass) || !Test.TestNotNull(AnimationPath, ExpectedAnimClass)) return false;
		FActorSpawnParameters Spawn;
		Spawn.ObjectFlags |= RF_Transient;
		Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Character = World->SpawnActor<AWuwaCharacter>(CharacterClass, Spawn);
		if (!Test.TestNotNull(TEXT("Authored role character"), Character)) return false;
		Movement = Character->GetWuwaMovementComponent();
		Mesh = Character->GetMesh();
		if (!Test.TestNotNull(TEXT("Role movement"), Movement) || !Test.TestNotNull(TEXT("Role mesh component"), Mesh)
			|| !Test.TestNotNull(TEXT("Authored skeletal mesh"), Mesh->GetSkeletalMeshAsset())) return false;
		Movement->MovementMode = MOVE_Walking;
		if (!Test.TestTrue(TEXT("Production movement state assembles"), Character->EnsureMovementStateSystem())
			|| !Test.TestTrue(TEXT("Production skill state assembles"), Character->EnsureSkillSystem())) return false;
		WuwaTestInput::SetMoveAxis(Character, FVector2D::ZeroVector);
		Movement->RefreshMovementSettings();
		AccelerationProperty = FindFProperty<FStructProperty>(Movement->GetClass(), TEXT("Acceleration"));
		if (!Test.TestNotNull(TEXT("CMC acceleration sample property"), AccelerationProperty)) return false;
		Mesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
		Mesh->bEnableUpdateRateOptimizations = false;
		Mesh->InitAnim(true);
		Anim = Cast<UWuwaAnimInstance>(Mesh->GetAnimInstance());
		if (!Test.TestNotNull(TEXT("Authored role animation instance"), Anim)
			|| !Test.TestEqual(TEXT("Role uses the expected child AnimBP"), Anim->GetClass(), ExpectedAnimClass)) return false;
		Anim->bUseMultiThreadedAnimationUpdate = false;
		FClassProperty* RigProperty = FindFProperty<FClassProperty>(Anim->GetClass(), TEXT("FootIKRigClass"));
		ExpectedRigClass = RigProperty ? Cast<UClass>(RigProperty->GetObjectPropertyValue_InContainer(Anim)) : nullptr;
		if (!Test.TestNotNull(TEXT("Child declares its FootIKRigClass"), ExpectedRigClass)) return false;
		Test.TestFalse(TEXT("Child rig differs from the obsolete original Changli rig"), ExpectedRigClass->GetPathName() == TEXT("/Game/Characters/Role/changli/Rig/CR_ChangliFootIK.CR_ChangliFootIK_C"));
		Report->SetStringField(TEXT("character"), CharacterClass->GetPathName());
		Report->SetStringField(TEXT("mesh"), Mesh->GetSkeletalMeshAsset()->GetPathName());
		Report->SetStringField(TEXT("animClass"), Anim->GetClass()->GetPathName());
		Report->SetStringField(TEXT("expectedRigClass"), ExpectedRigClass->GetPathName());
		if (!Test.TestNotNull(TEXT("Authored role command table"), Character->InputCommandConfig.Get())) return false;
		ASC = NewObject<UWuwaAbilitySystemComponent>(Character);
		ASC->RegisterComponent();
		Character->AbilitySystemComponent = ASC;
		ASC->InitAbilityActorInfo(Character, Character);
		ASC->AddAttributeSetSubobject(NewObject<UWuwaAttributeSet>(Character));
		// 走角色自己的授予流程：技能表（技能 ID）+ 其他非技能能力
		Character->InitInitialAbilities();
		if (!Test.TestNotNull(TEXT("Authored role skill table"), Character->SkillTable.Get())
			|| !Test.TestTrue(TEXT("Authored role abilities are granted"), ASC->GetActivatableAbilities().Num() > 0)) return false;
		Controller = World->SpawnActor<AWuwaPlayerController>(Spawn);
		if (!Test.TestNotNull(TEXT("Production controller"), Controller)) return false;
		Controller->SetAsLocalPlayerController();
		ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
		LocalPlayer->PlayerController = Controller;
		Controller->Player = LocalPlayer;
		Controller->SetPawn(Character);
		Character->SetController(Controller);
		Controller->AscComponent = ASC;
		ASC->InitAbilityActorInfo(Character, Character);
		Controller->RegisterInputRouteHandlers();
		const FReferenceSkeleton& Ref = Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
		TArray<FTransform> RefComponent = Ref.GetRefBonePose();
		for (int32 Index = 0; Index < RefComponent.Num(); ++Index)
		{
			const int32 Parent = Ref.GetParentIndex(Index);
			if (Parent != INDEX_NONE) RefComponent[Index] = RefComponent[Index] * RefComponent[Parent];
		}
		for (const TCHAR* Name : TrackedBones)
		{
			const int32 Index = Ref.FindBoneIndex(Name);
			if (!Test.TestTrue(FString::Printf(TEXT("Mesh has %s"), Name), Index != INDEX_NONE)) return false;
			BoneIndices.Add(Index);
			ReferenceWorld.Add(RefComponent[Index] * Mesh->GetComponentTransform());
		}
		Advance(0.2f);
		return true;
	}

	void RecordAndValidate()
	{
		if (!Anim || BoneIndices.Num() != UE_ARRAY_COUNT(TrackedBones)) return;
		TSharedRef<FJsonObject> Frame = MakeShared<FJsonObject>();
		Frame->SetStringField(TEXT("phase"), Phase);
		Frame->SetNumberField(TEXT("frame"), FrameIndex);
		Frame->SetNumberField(TEXT("speed"), Anim->LocomotionData.GroundSpeed);
		Frame->SetStringField(TEXT("groundState"), Anim->GetDebugStateName(TEXT("SM_Ground")).ToString());
		Frame->SetBoolField(TEXT("sprint"), Anim->LocomotionData.bStateGroundSprint);
		Frame->SetBoolField(TEXT("groundMoveActive"), Anim->LocomotionData.bGroundMoveActive);
		Frame->SetStringField(TEXT("montage"), GetPathNameSafe(Anim->GetCurrentActiveMontage()));
		bool bPoseValid = true;
		TArray<FTransform> Actual;
		TSharedRef<FJsonObject> BoneData = MakeShared<FJsonObject>();
		for (int32 I = 0; I < BoneIndices.Num(); ++I)
		{
			const FTransform Transform = Mesh->GetBoneTransform(BoneIndices[I]);
			Actual.Add(Transform);
			const FVector RelativeScale = Transform.GetScale3D() / ReferenceWorld[I].GetScale3D();
			bPoseValid &= !Transform.ContainsNaN() && RelativeScale.GetMin() > 0.5 && RelativeScale.GetMax() < 2.0;
			TSharedRef<FJsonObject> Bone = MakeShared<FJsonObject>();
			Bone->SetStringField(TEXT("worldTranslation"), Transform.GetTranslation().ToString());
			Bone->SetStringField(TEXT("worldScale"), Transform.GetScale3D().ToString());
			BoneData->SetObjectField(TrackedBones[I], Bone);
		}
		TArray<TSharedPtr<FJsonValue>> LengthRatios;
		for (const TPair<int32,int32>& Pair : { TPair<int32,int32>(3,4), {4,5}, {6,7}, {7,8} })
		{
			const double RefLength = FVector::Distance(ReferenceWorld[Pair.Key].GetTranslation(), ReferenceWorld[Pair.Value].GetTranslation());
			const double Length = FVector::Distance(Actual[Pair.Key].GetTranslation(), Actual[Pair.Value].GetTranslation());
			const double Ratio = Length / FMath::Max(RefLength, 0.001);
			LengthRatios.Add(MakeShared<FJsonValueNumber>(Ratio));
			bPoseValid &= FMath::IsFinite(Ratio) && Ratio > 0.5 && Ratio < 2.0;
		}
		const double TorsoSpan = FVector::Distance(Actual[1].GetTranslation(), Actual[2].GetTranslation());
		bPoseValid &= TorsoSpan > 15.0 && TorsoSpan < 150.0;
		Frame->SetNumberField(TEXT("pelvisToHeadCm"), TorsoSpan);
		Frame->SetArrayField(TEXT("legLengthRatios"), LengthRatios);
		Frame->SetObjectField(TEXT("bones"), BoneData);
		Frame->SetBoolField(TEXT("poseSizeValid"), bPoseValid);
		UControlRig* RuntimeRig = nullptr;
		TArray<TSharedPtr<FJsonValue>> RigNodes;
		TArray<TSharedPtr<FJsonValue>> PlayerData;
		const FMarkerSyncAnimPosition Sync = Anim->GetSyncGroupPosition(TEXT("Locomotion"));
		Frame->SetStringField(TEXT("previousMarker"), Sync.PreviousMarkerName.ToString());
		Frame->SetStringField(TEXT("nextMarker"), Sync.NextMarkerName.ToString());
		Frame->SetNumberField(TEXT("markerAlpha"), Sync.PositionBetweenMarkers);
		Frame->SetNumberField(TEXT("stopEntryTime"), Anim->RoleStopStartTime);
		Frame->SetBoolField(TEXT("stopEntryMapped"), Anim->bRoleStopEntryMapped);
		Frame->SetBoolField(TEXT("stopLeft"), Anim->bRoleStopLeft);
		Frame->SetNumberField(TEXT("stopEntrySerial"), Anim->RoleStopEntrySerial);
		const float StopStateWeight = Anim->GetDebugStateWeight(TEXT("SM_Ground"), TEXT("Stop"));
		Frame->SetNumberField(TEXT("stopStateWeight"), StopStateWeight);
		const IAnimClassInterface* AnimClass = IAnimClassInterface::GetFromClass(Anim->GetClass());
		ActiveAssets.Reset();
		for (TFieldIterator<FStructProperty> It(Anim->GetClass()); It; ++It)
		{
			FStructProperty* Property = *It;
			if (Property->Struct->IsChildOf(FAnimNode_ControlRig::StaticStruct()))
			{
				const FAnimNode_ControlRig* Node = Property->ContainerPtrToValuePtr<FAnimNode_ControlRig>(Anim);
				RuntimeRig = Node->GetControlRig();
				TSharedRef<FJsonObject> RigInfo = MakeShared<FJsonObject>();
				RigInfo->SetStringField(TEXT("property"), Property->GetPathName());
				RigInfo->SetBoolField(TEXT("inGeneratedNodeList"), AnimClass && AnimClass->GetAnimNodeProperties().Contains(Property));
				RigInfo->SetStringField(TEXT("requestedClass"), GetPathNameSafe(Node->GetControlRigClass().Get()));
				RigInfo->SetStringField(TEXT("actualClass"), RuntimeRig ? RuntimeRig->GetClass()->GetPathName() : TEXT("None"));
				RigNodes.Add(MakeShared<FJsonValueObject>(RigInfo));
			}
			if (Property->Struct->IsChildOf(FAnimNode_AssetPlayerBase::StaticStruct()))
			{
				const FAnimNode_AssetPlayerBase* Player = Property->ContainerPtrToValuePtr<FAnimNode_AssetPlayerBase>(Anim);
				if (UAnimationAsset* Asset = Player->GetAnimAsset(); Asset && Player->GetCachedBlendWeight() > 0.01f)
				{
					// Inactive states can retain their last asset-player weight.
					if (Asset->GetName().Contains(TEXT("Stop_")) && StopStateWeight <= UE_SMALL_NUMBER) continue;
					ObservedAssets.Add(Asset->GetPathName());
					ActiveAssets.Add(Asset->GetPathName());
					bObservedSharedAccel |= Asset->GetName().Contains(TEXT("Accel"));
					TSharedRef<FJsonObject> PlayerInfo = MakeShared<FJsonObject>();
					PlayerInfo->SetStringField(TEXT("asset"), Asset->GetPathName());
					PlayerInfo->SetNumberField(TEXT("time"), Player->GetAccumulatedTime());
					PlayerInfo->SetNumberField(TEXT("weight"), Player->GetCachedBlendWeight());
					if (const UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(Asset))
					{
						PlayerInfo->SetNumberField(TEXT("length"), Sequence->GetPlayLength());
						PlayerInfo->SetNumberField(TEXT("rateScale"), Sequence->RateScale);
					}
					PlayerData.Add(MakeShared<FJsonValueObject>(PlayerInfo));
				}
			}
		}
		const bool bRigValid = RuntimeRig && RuntimeRig->GetClass() == ExpectedRigClass;
		bObservedRuntimeRig |= bRigValid;
		Frame->SetStringField(TEXT("runtimeRigClass"), RuntimeRig ? RuntimeRig->GetClass()->GetPathName() : TEXT("None"));
		Frame->SetArrayField(TEXT("runtimeRigNodes"), RigNodes);
		TArray<TSharedPtr<FJsonValue>> Assets;
		for (const FString& Path : ActiveAssets) Assets.Add(MakeShared<FJsonValueString>(Path));
		Frame->SetArrayField(TEXT("activeAssets"), Assets);
		Frame->SetArrayField(TEXT("players"), PlayerData);
		if (FrameIndex > 4 && (!bPoseValid || !bRigValid) && !FailedPhases.Contains(Phase))
		{
			FailedPhases.Add(Phase);
			Test.TestTrue(FString::Printf(TEXT("%s %s: evaluated pose retains normal bone lengths and scale"), *Role, *Phase), bPoseValid);
			Test.TestTrue(FString::Printf(TEXT("%s %s: runtime Control Rig is child-configured class"), *Role, *Phase), bRigValid);
		}
		if (bCaptureEveryFrame || FrameIndex % 6 == 0 || !bPoseValid || !bRigValid) Frames.Add(MakeShared<FJsonValueObject>(Frame));
	}

	void Advance(float Seconds)
	{
		for (int32 I = 0, Count = FMath::CeilToInt(Seconds / StepSeconds); I < Count; ++I)
		{
			World->TimeSeconds += StepSeconds;
			Movement->Velocity = Velocity;
			AccelerationProperty->CopyCompleteValue(AccelerationProperty->ContainerPtrToValuePtr<void>(Movement), &Acceleration);
			Character->RoleGaitComponent->RefreshPolicy();
			Mesh->TickAnimation(StepSeconds, true);
			Mesh->RefreshBoneTransforms(nullptr);
			if (Controller && Controller->AbilityInputHandler) Controller->AbilityInputHandler->TickComponent(StepSeconds, LEVELTICK_All, &Controller->AbilityInputHandler->PrimaryComponentTick);
			++FrameIndex;
			RecordAndValidate();
		}
	}

	void SetMotion(const TCHAR* Name, bool bInput, float Speed, const FVector& InAcceleration = FVector::ZeroVector)
	{
		Phase = Name;
		Velocity = FVector(Speed, 0.f, 0.f);
		Acceleration = InAcceleration;
		WuwaTestInput::SetMoveAxis(Character, bInput ? FVector2D(0.f, 1.f) : FVector2D::ZeroVector);
		Character->AddMovementInput(FVector::ForwardVector, bInput ? 1.f : 0.f, true);
		Movement->ConsumeInputVector();
	}

	bool HasActiveAsset(const TCHAR* Fragment) const
	{
		return ActiveAssets.ContainsByPredicate([Fragment](const FString& Path) { return Path.Contains(Fragment); });
	}

	bool RouteAction(bool bDash, EWuwaInputPhase InputPhase)
	{
		FWuwaInputEvent Event;
		Event.InputTag = bDash ? FWuwaGameTags::Get().Input_Combat_Dash : FWuwaGameTags::Get().Input_Combat_Attack;
		Event.RouteTag = FWuwaGameTags::Get().Input_Route_Ability;
		Event.Phase = InputPhase;
		Event.Timestamp = World->GetTimeSeconds();
		Event.SourceAction = LoadObject<UInputAction>(nullptr, bDash ? TEXT("/Game/CoreInput/Actions/IA_Dodge.IA_Dodge") : TEXT("/Game/CoreInput/Actions/IA_Attack.IA_Attack"));
		return Controller->GetInputRouter()->DispatchInput(Event);
	}

	bool Run()
	{
		SetMotion(TEXT("Idle"), false, 0.f);
		Advance(0.5f);
		Test.TestTrue(TEXT("Idle state evaluates a role animation"), HasActiveAsset(TEXT("Idle")) || HasActiveAsset(TEXT("Stand")));
		SetMotion(TEXT("RunAccelerate"), true, 250.f, FVector(1400.f, 0.f, 0.f));
		Advance(0.6f);
		SetMotion(TEXT("Run"), true, 350.f);
		Advance(0.6f);
		Test.TestTrue(TEXT("Run state evaluates role Run content"), HasActiveAsset(TEXT("Run")) || HasActiveAsset(TEXT("GroumdMove")) || HasActiveAsset(TEXT("GroundMove")));
		for (const FVector& Sample : { FVector(1400,0,0), FVector(-1400,0,0), FVector(0,1400,0), FVector(0,-1400,0) })
		{
			SetMotion(TEXT("SharedAcceleration"), true, 350.f, Sample);
			Advance(0.2f);
		}
		Test.TestTrue(TEXT("Shared Accel animation player contributed during evaluated locomotion"), bObservedSharedAccel);
		Test.TestTrue(TEXT("Production gait accepts Sprint request"), Character->RoleGaitComponent->RequestSprint());
		SetMotion(TEXT("Sprint"), true, 650.f);
		Advance(0.6f);
		Test.TestTrue(TEXT("Sprint is accepted by managed gait and animation snapshot"), Anim->LocomotionData.bStateGroundSprint);
		Test.TestTrue(TEXT("Sprint state evaluates role Sprint content"), HasActiveAsset(TEXT("Sprint")) || HasActiveAsset(TEXT("GroumdMove")) || HasActiveAsset(TEXT("GroundMove")));
		Character->RoleGaitComponent->ResetSprintRequest();
		SetMotion(TEXT("ReleaseToIdle"), false, 0.f);
		Advance(1.3f);
		Test.TestFalse(TEXT("Release clears move intent"), Anim->LocomotionData.bHasMoveInput);
		Phase = TEXT("AttackThroughInput");
		if (!Test.TestTrue(TEXT("Attack enters the production input router"), RouteAction(false, EWuwaInputPhase::Pressed))) return false;
		Advance(StepSeconds);
		RouteAction(false, EWuwaInputPhase::Released);
		UAnimMontage* Attack = Anim->GetCurrentActiveMontage();
		if (!Test.TestNotNull(TEXT("Authored attack GA started a montage"), Attack)) return false;
		Test.TestTrue(TEXT("Attack montage belongs to the role"), Attack->GetPathName().Contains(TEXT("/") + Role + TEXT("/")));
		Advance(FMath::Min(Attack->GetPlayLength() / FMath::Max(FMath::Abs(Attack->RateScale), 0.01f) + 0.6f, 8.f));
		Test.TestFalse(TEXT("Attack montage naturally stopped"), Anim->Montage_IsPlaying(Attack));
		SetMotion(TEXT("AfterAttackRun"), true, 350.f);
		Advance(0.6f);
		Test.TestTrue(TEXT("Locomotion resumes after attack ends"), Anim->LocomotionData.bGroundMoveActive);
		Phase = TEXT("DashThroughInput");
		if (!Test.TestTrue(TEXT("Dodge enters the production input router"), RouteAction(true, EWuwaInputPhase::Pressed))) return false;
		Advance(0.15f);
		RouteAction(true, EWuwaInputPhase::Released);
		UAnimMontage* Dash = Anim->GetCurrentActiveMontage();
		if (!Test.TestNotNull(TEXT("Authored dash GA started a montage"), Dash)) return false;
		Test.TestTrue(TEXT("Dash montage belongs to the role"), Dash->GetPathName().Contains(TEXT("/") + Role + TEXT("/")));
		// Explicit ability cancellation exercises production task cleanup and slot
		// blend-out while the existing movement intent remains held.
		ASC->CancelAllAbilities();
		SetMotion(TEXT("AfterDashCancelRun"), true, 350.f);
		Advance(0.8f);
		Test.TestFalse(TEXT("Cancelled dash montage no longer plays"), Anim->Montage_IsPlaying(Dash));
		Test.TestTrue(TEXT("Locomotion resumes after montage cancellation"), Anim->LocomotionData.bGroundMoveActive);
		Test.TestTrue(TEXT("Runtime instantiated the configured child rig"), bObservedRuntimeRig);
		return FailedPhases.IsEmpty();
	}

	bool RunStopCoverage()
	{
		bCaptureEveryFrame = true;
		ReportPrefix = TEXT("StopPhases_");
		const UAnimSequence* StopSequences[] = { Anim->RoleStopWalkLeft, Anim->RoleStopWalkRight,
			Anim->RoleStopRunLeft, Anim->RoleStopRunRight, Anim->RoleStopSprintLeft, Anim->RoleStopSprintRight };
		ReportDirectory = TEXT("Diagnostics/RoleStopRegression");
		float LongestStopSeconds = 0.f;
		TArray<TSharedPtr<FJsonValue>> ConfiguredStops;
		for (const UAnimSequence* Sequence : StopSequences)
		{
			if (!Test.TestNotNull(Role + TEXT(" configured stop sequence"), Sequence)) return false;
			if (!Test.TestTrue(Sequence->GetName() + TEXT(" has a finite positive playback rate"),
				FMath::IsFinite(Sequence->RateScale) && Sequence->RateScale > UE_SMALL_NUMBER)) return false;
			const float Duration = Sequence->GetPlayLength() / Sequence->RateScale;
			LongestStopSeconds = FMath::Max(LongestStopSeconds, Duration);
			if (Sequence->GetName().Contains(TEXT("_L3")) || Sequence->GetName().Contains(TEXT("_R3")))
				ReportDirectory = TEXT("Diagnostics/RoleStopV3Migration");
			TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetStringField(TEXT("asset"), Sequence->GetPathName());
			Row->SetNumberField(TEXT("length"), Sequence->GetPlayLength());
			Row->SetNumberField(TEXT("rateScale"), Sequence->RateScale);
			Row->SetNumberField(TEXT("effectiveSeconds"), Duration);
			ConfiguredStops.Add(MakeShared<FJsonValueObject>(Row));
		}
		// Optional saved-directory-relative override keeps a new audit from replacing old evidence.
		FParse::Value(FCommandLine::Get(), TEXT("WuwaStopReportDir="), ReportDirectory);
		const float StopWaitSeconds = LongestStopSeconds + 1.f; // State entry and final blend-out margin.
		const int32 StopWaitFrames = FMath::CeilToInt(StopWaitSeconds / StepSeconds);
		Report->SetArrayField(TEXT("configuredStops"), ConfiguredStops);
		Report->SetNumberField(TEXT("stopWaitSeconds"), StopWaitSeconds);
		Report->SetStringField(TEXT("scope"), TEXT("Production role/AnimBP/gait. Releases movement in both marker intervals at four phase offsets for Walk, Run and Sprint. Prescribed braking velocity; no terrain or rendered visual validation."));
		TArray<TSharedPtr<FJsonValue>> Cases;
		for (const EWuwaGait Gait : {EWuwaGait::Walk, EWuwaGait::Run, EWuwaGait::Sprint})
		{
			const FString GaitName = StaticEnum<EWuwaGait>()->GetNameStringByValue(static_cast<int64>(Gait));
			const float Speed = Gait == EWuwaGait::Walk ? 150.f : Gait == EWuwaGait::Run ? 350.f : 650.f;
			TSet<FString> ChosenSides;
			for (int32 Case = 0; Case < 8; ++Case)
			{
				Character->RoleGaitComponent->ResetSprintRequest();
				SetMotion(TEXT("StopCoverageIdle"), false, 0.f);
				Advance(0.4f);
				SetMotion(TEXT("StopCoverageMove"), true, Speed);
				WuwaTestGait::SetDesiredGait(Movement, Gait);
				Advance(0.8f);
				const FName WantedPrevious = Case % 2 == 0 ? TEXT("Left") : TEXT("Right");
				const float WantedAlpha = 0.12f + (Case / 2) * 0.22f;
				FMarkerSyncAnimPosition Before;
				bool bReachedPhase = false;
				for (int32 Frame = 0; Frame < 240; ++Frame)
				{
					Before = Anim->GetSyncGroupPosition(TEXT("Locomotion"));
					if (Before.PreviousMarkerName == WantedPrevious && FMath::Abs(Before.PositionBetweenMarkers - WantedAlpha) < 0.045f)
					{
						bReachedPhase = true;
						break;
					}
					Advance(StepSeconds);
				}
				const FString CaseName = FString::Printf(TEXT("%s/%s/%d"), *Role, *GaitName, Case);
				Test.TestTrue(CaseName + TEXT(" reaches requested locomotion phase"), bReachedPhase);
				TSharedRef<FJsonObject> CaseData = MakeShared<FJsonObject>();
				CaseData->SetStringField(TEXT("case"), CaseName);
				CaseData->SetStringField(TEXT("previousMarker"), Before.PreviousMarkerName.ToString());
				CaseData->SetStringField(TEXT("nextMarker"), Before.NextMarkerName.ToString());
				CaseData->SetNumberField(TEXT("alpha"), Before.PositionBetweenMarkers);
				CaseData->SetNumberField(TEXT("releaseFrame"), FrameIndex);
				SetMotion(*CaseName, false, Speed);
				FString ChosenAsset;
				const UAnimSequence* ChosenSequence = nullptr;
				float FirstTime = -1.f, LastTime = -1.f;
				float EntryIntervalEnd = -1.f, EntryFrameAdvance = 0.f;
				int32 StopFrames = 0;
				bool bMonotonic = true, bOnlyOneStop = true;
				for (int32 Frame = 0; Frame < StopWaitFrames; ++Frame)
				{
					// Allow the state system to see real residual speed at release,
					// then stop. The old test used 0 immediately and skipped Stop.
					Velocity.X = Speed * FMath::Max(0.f, 1.f - Frame / 18.f);
					Advance(StepSeconds);
					const auto& Players = Frames.Last()->AsObject()->GetArrayField(TEXT("players"));
					for (const TSharedPtr<FJsonValue>& Value : Players)
					{
						const auto Player = Value->AsObject();
						const FString Asset = Player->GetStringField(TEXT("asset"));
						if (!Asset.Contains(TEXT("Stop_"))) continue;
						const float Time = Player->GetNumberField(TEXT("time"));
						if (ChosenAsset.IsEmpty())
						{
							ChosenAsset = Asset;
							FirstTime = Time;
							for (const UAnimSequence* Sequence : StopSequences)
							{
								if (Sequence->GetPathName() == ChosenAsset) { ChosenSequence = Sequence; break; }
							}
							Test.TestNotNull(CaseName + TEXT(" selects one of the six configured stop assets"), ChosenSequence);
							EntryIntervalEnd = StopEntryIntervalEnd(ChosenSequence);
							EntryFrameAdvance = StepSeconds * Player->GetNumberField(TEXT("rateScale"));
							CaseData->SetNumberField(TEXT("stopLength"), Player->GetNumberField(TEXT("length")));
							CaseData->SetNumberField(TEXT("entryIntervalEnd"), EntryIntervalEnd);
							CaseData->SetNumberField(TEXT("entryFrameAdvance"), EntryFrameAdvance);
							CaseData->SetNumberField(TEXT("entryTime"), Anim->RoleStopStartTime);
							CaseData->SetBoolField(TEXT("entryMapped"), Anim->bRoleStopEntryMapped);
							const float EntryAdvance = FirstTime - Anim->RoleStopStartTime;
							Test.TestTrue(CaseName + TEXT(" actually applies the mapped start, then advances one frame"),
								EntryAdvance >= -0.001f && EntryAdvance <= EntryFrameAdvance + 0.001f);
							if (Gait != EWuwaGait::Sprint)
							{
								Test.TestTrue(CaseName + TEXT(" maps the initial contact interval"), Anim->bRoleStopEntryMapped);
							}
							else
							{
								Test.TestTrue(CaseName + TEXT(" starts sprint stop at frame zero"), FirstTime <= EntryFrameAdvance + 0.001f);
							}
						}
						bOnlyOneStop &= Asset == ChosenAsset;
						bMonotonic &= Time + 0.001f >= LastTime;
						LastTime = Time;
						++StopFrames;
					}
				}
				CaseData->SetStringField(TEXT("selectedAsset"), ChosenAsset);
				CaseData->SetNumberField(TEXT("firstTime"), FirstTime);
				CaseData->SetNumberField(TEXT("lastTime"), LastTime);
				CaseData->SetNumberField(TEXT("observedStopFrames"), StopFrames);
				CaseData->SetBoolField(TEXT("monotonic"), bMonotonic);
				CaseData->SetBoolField(TEXT("onlyOneStop"), bOnlyOneStop);
				CaseData->SetStringField(TEXT("finalState"), Anim->GetDebugStateName(TEXT("SM_Ground")).ToString());
				Cases.Add(MakeShared<FJsonValueObject>(CaseData));
				Test.TestFalse(CaseName + TEXT(" evaluates a stop animation"), ChosenAsset.IsEmpty());
				Test.TestTrue(CaseName + TEXT(" retains its selected stop"), bOnlyOneStop);
				Test.TestTrue(CaseName + TEXT(" advances without synchronization rewind"), bMonotonic);
				if (Gait != EWuwaGait::Sprint)
				{
					Test.TestTrue(CaseName + TEXT(" starts in the authored first contact interval, before the recovery tail"),
						EntryIntervalEnd >= 0.f && FirstTime >= 0.f && FirstTime <= EntryIntervalEnd + EntryFrameAdvance + 0.001f);
				}
				Test.TestTrue(CaseName + TEXT(" plays a meaningful stopping interval"), LastTime - FirstTime > 0.3f);
				Test.TestEqual(CaseName + TEXT(" returns to Idle"), Anim->GetDebugStateName(TEXT("SM_Ground")), FName(TEXT("Idle")));
				const UAnimSequence* ExpectedLeft = Gait == EWuwaGait::Walk ? Anim->RoleStopWalkLeft.Get()
					: Gait == EWuwaGait::Run ? Anim->RoleStopRunLeft.Get() : Anim->RoleStopSprintLeft.Get();
				const UAnimSequence* ExpectedRight = Gait == EWuwaGait::Walk ? Anim->RoleStopWalkRight.Get()
					: Gait == EWuwaGait::Run ? Anim->RoleStopRunRight.Get() : Anim->RoleStopSprintRight.Get();
				Test.TestTrue(CaseName + TEXT(" selects a stop for the requested gait"), ChosenSequence == ExpectedLeft || ChosenSequence == ExpectedRight);
				if (ChosenSequence == ExpectedLeft) ChosenSides.Add(TEXT("L"));
				if (ChosenSequence == ExpectedRight) ChosenSides.Add(TEXT("R"));
			}
			Test.TestEqual(Role + TEXT(" ") + GaitName + TEXT(" exercises both stop feet"), ChosenSides.Num(), 2);
		}
		Report->SetArrayField(TEXT("stopCases"), Cases);
		// 最短一次移动后立即再停，验证新入场时间被实际采用。
		// 同时记录旧 Stop 的权重；当前 Stop -> Move 是瞬时切换，不能把此测试
		// 宣称为带残留混出权重的再入验证。序号机制也避免未来延长混出时漏用起播时间。
		Character->RoleGaitComponent->ResetSprintRequest();
		SetMotion(TEXT("ReentryMove"), true, 350.f);
		WuwaTestGait::SetDesiredGait(Movement, EWuwaGait::Run);
		Advance(0.8f);
		SetMotion(TEXT("ReentryFirstStop"), false, 350.f);
		Advance(3.f * StepSeconds);
		TArray<TSharedPtr<FJsonValue>> Reentries;
		for (int32 Cycle = 0; Cycle < 4; ++Cycle)
		{
			const int32 SerialBefore = Anim->RoleStopEntrySerial;
			SetMotion(TEXT("ReentryBriefMove"), true, 350.f);
			Advance(StepSeconds);
			const float PreviousStopWeight = Anim->GetDebugStateWeight(TEXT("SM_Ground"), TEXT("Stop"));
			SetMotion(TEXT("ReentryStop"), false, 350.f);
			Advance(StepSeconds);
			Test.TestTrue(Role + TEXT(" quick release increments stop entry generation"), Anim->RoleStopEntrySerial > SerialBefore);
			bool bApplied = false;
			TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
			Row->SetNumberField(TEXT("previousStopWeight"), PreviousStopWeight);
			Row->SetNumberField(TEXT("serialBefore"), SerialBefore);
			Row->SetNumberField(TEXT("serialAfter"), Anim->RoleStopEntrySerial);
			Row->SetNumberField(TEXT("mappedStart"), Anim->RoleStopStartTime);
			for (const auto& Value : Frames.Last()->AsObject()->GetArrayField(TEXT("players")))
			{
				const auto Player = Value->AsObject();
				const FString Asset = Player->GetStringField(TEXT("asset"));
				if (!Asset.Contains(TEXT("Stop_Run_"))) continue;
				const float Time = Player->GetNumberField(TEXT("time"));
				const float Difference = Time - Anim->RoleStopStartTime;
				bApplied |= Difference >= -0.001f && Difference <= StepSeconds * Player->GetNumberField(TEXT("rateScale")) + 0.001f;
				Row->SetStringField(TEXT("asset"), Asset);
				Row->SetNumberField(TEXT("actualStart"), Time);
			}
			Row->SetBoolField(TEXT("applied"), bApplied);
			Reentries.Add(MakeShared<FJsonValueObject>(Row));
			Test.TestTrue(Role + TEXT(" quick reentry applies this entry's actual start time"), bApplied);
			Advance(2.f * StepSeconds);
		}
		Report->SetArrayField(TEXT("rapidReentries"), Reentries);
		return FailedPhases.IsEmpty();
	}
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaRoleChildrenRuntimeTest, "Wuwa.Animation.RoleChildren.RuntimePose",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaRoleChildrenRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace WuwaRoleChildrenRuntime;
	{
		FFixture Fixture(*this, TEXT("changli"));
		if (Fixture.Initialize(TEXT("/Game/Characters/Role/changli/BP_Role_Changli.BP_Role_Changli_C"), TEXT("/Game/Characters/Role/changli/ABP_Changli.ABP_Changli_C"))) Fixture.Run();
	}
	{
		FFixture Fixture(*this, TEXT("jiabei"));
		if (Fixture.Initialize(TEXT("/Game/Characters/Role/jiabei/BP_Role_jiabei.BP_Role_jiabei_C"), TEXT("/Game/Characters/Role/jiabei/ABP_Role_Jiabei.ABP_Role_Jiabei_C"))) Fixture.Run();
	}
	return !HasAnyErrors();
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaRoleStopPhasesTest, "Wuwa.Animation.RoleChildren.StopPhases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaRoleStopPhasesTest::RunTest(const FString& Parameters)
{
	using namespace WuwaRoleChildrenRuntime;
	{
		FFixture Fixture(*this, TEXT("changli"));
		if (Fixture.Initialize(TEXT("/Game/Characters/Role/changli/BP_Role_Changli.BP_Role_Changli_C"), TEXT("/Game/Characters/Role/changli/ABP_Changli.ABP_Changli_C"))) Fixture.RunStopCoverage();
	}
	{
		FFixture Fixture(*this, TEXT("jiabei"));
		if (Fixture.Initialize(TEXT("/Game/Characters/Role/jiabei/BP_Role_jiabei.BP_Role_jiabei_C"), TEXT("/Game/Characters/Role/jiabei/ABP_Role_Jiabei.ABP_Role_Jiabei_C"))) Fixture.RunStopCoverage();
	}
	return !HasAnyErrors();
}

#endif

