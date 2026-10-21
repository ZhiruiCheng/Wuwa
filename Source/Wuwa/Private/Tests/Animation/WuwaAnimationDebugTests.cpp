#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"

#pragma region Debug

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaAnimationDebugMissingMachineTest, "Wuwa.Animation.Debug.MissingMachine",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaAnimationDebugMissingMachineTest::RunTest(const FString& Parameters)
{
	USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>();
	UWuwaAnimInstance* Instance = NewObject<UWuwaAnimInstance>(Mesh);
	const FName MissingName(TEXT("MissingMachine"));
	const FWuwaDebugAnimStateMachine Data = Instance->GetDebugStateMachineData(MissingName);
	TestEqual(TEXT("Requested name is retained"), Data.MachineName, MissingName);
	TestFalse(TEXT("Missing machine is not found"), Data.bFound);
	TestFalse(TEXT("Missing machine is not initialized"), Data.bInitialized);
	TestTrue(TEXT("Missing machine has no active states"), Data.ActiveStates.IsEmpty());
	TestTrue(TEXT("Missing machine has no current state"), Instance->GetDebugStateName(MissingName).IsNone());
	TestEqual(TEXT("Missing state has zero weight"), Instance->GetDebugStateWeight(MissingName, TEXT("Walk")), 0.f);
	TestTrue(TEXT("Missing machine has an explicit diagnostic"),
		Instance->GetDebugStateMachineText(MissingName).Contains(TEXT("not found")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaAnimationDebugUninitializedTest, "Wuwa.Animation.Debug.Uninitialized",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaAnimationDebugUninitializedTest::RunTest(const FString& Parameters)
{
	UClass* AnimClass = LoadClass<UWuwaAnimInstance>(nullptr,
		TEXT("/Game/Characters/Role/changli/ABP_Changli.ABP_Changli_C"));
	if (!TestNotNull(TEXT("Existing Changli AnimBP class loads"), AnimClass))
	{
		return false;
	}
	USkeletalMeshComponent* Mesh = NewObject<USkeletalMeshComponent>();
	UWuwaAnimInstance* Instance = NewObject<UWuwaAnimInstance>(Mesh, AnimClass);
	const FWuwaDebugAnimStateMachine Data = Instance->GetDebugStateMachineData(TEXT("SM_Ground"));
	TestTrue(TEXT("Baked ground state machine is found before animation initialization"), Data.bFound);
	TestFalse(TEXT("Uninitialized state machine is reported safely"), Data.bInitialized);
	TestTrue(TEXT("Uninitialized state machine has no active states"), Data.ActiveStates.IsEmpty());
	TestEqual(TEXT("Uninitialized state weight is safe"),
		Instance->GetDebugStateWeight(TEXT("SM_Ground"), TEXT("Walk")), 0.f);
	TestTrue(TEXT("Uninitialized machine has an explicit diagnostic"),
		Instance->GetDebugStateMachineText(TEXT("SM_Ground")).Contains(TEXT("not initialized")));
	return true;
}

#pragma endregion Debug

#endif
