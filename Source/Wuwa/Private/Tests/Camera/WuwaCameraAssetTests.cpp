#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Game/Camera/WuwaCameraMode.h"
#include "Game/Camera/WuwaPlayerCameraManager.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Input/DataAsset/WuwaInputDataAsset.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Engine/Blueprint.h"
#include "GameFramework/GameModeBase.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaCameraInputAssetTest,
	"Wuwa.Camera.Assets.SemanticInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaCameraInputAssetTest::RunTest(const FString& Parameters)
{
	const UInputAction* Zoom = LoadObject<UInputAction>(nullptr,
		TEXT("/Game/CoreInput/Actions/IA_CameraZoom.IA_CameraZoom"));
	const UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr,
		TEXT("/Game/CoreInput/Contexts/IMC_Character.IMC_Character"));
	const UWuwaInputDataAsset* Bindings = LoadObject<UWuwaInputDataAsset>(nullptr,
		TEXT("/Game/CoreInput/DataAsset/DA_IARouterMap.DA_IARouterMap"));
	const bool bHasZoom = TestNotNull(TEXT("Wheel zoom input action exists"), Zoom);
	const bool bHasContext = TestNotNull(TEXT("Existing character input context loads"), Context);
	const bool bHasBindings = TestNotNull(TEXT("Existing semantic input bindings load"), Bindings);
	if (!bHasZoom || !bHasContext || !bHasBindings)
	{
		return false;
	}
	TestTrue(TEXT("Wheel zoom supplies a scalar delta"), Zoom->ValueType == EInputActionValueType::Axis1D);
	int32 WheelMappings = 0;
	for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
	{
		WheelMappings += Mapping.Action == Zoom && Mapping.Key == EKeys::MouseWheelAxis;
	}
	TestEqual(TEXT("Wheel axis maps to zoom exactly once"), WheelMappings, 1);
	const FGameplayTag ZoomTag = FGameplayTag::RequestGameplayTag(TEXT("Player.Common.Camera.Zoom"));
	const FGameplayTag RotateTag = FGameplayTag::RequestGameplayTag(TEXT("Player.Common.Camera.Rotate"));
	const FGameplayTag CameraRoute = FGameplayTag::RequestGameplayTag(TEXT("Input.Route.Camera"));
	int32 ZoomRows = 0;
	int32 RotateRows = 0;
	for (const FInputDataAsset& Row : Bindings->InputDataAssetMap)
	{
		if (Row.InputAction == Zoom || Row.InputTag == ZoomTag)
		{
			++ZoomRows;
			TestTrue(TEXT("Zoom row uses the correct action"), Row.InputAction == Zoom);
			TestTrue(TEXT("Zoom row uses the semantic zoom tag"), Row.InputTag == ZoomTag);
			TestTrue(TEXT("Zoom is routed to the camera input handler"), Row.RouteTag == CameraRoute);
		}
		if (Row.InputTag == RotateTag)
		{
			++RotateRows;
			TestTrue(TEXT("Existing rotation action is configured"), Row.IsConfigured());
			TestTrue(TEXT("Rotation uses the camera route"), Row.RouteTag == CameraRoute);
		}
	}
	TestEqual(TEXT("Zoom has one unambiguous semantic binding"), ZoomRows, 1);
	TestEqual(TEXT("Rotation has one semantic binding"), RotateRows, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaCameraOwnershipAssetTest,
	"Wuwa.Camera.Assets.PlayerOwnership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaCameraOwnershipAssetTest::RunTest(const FString& Parameters)
{
	UClass* ControllerClass = LoadClass<AWuwaPlayerController>(nullptr,
		TEXT("/Game/Core/BP_WuwaPlayerController.BP_WuwaPlayerController_C"));
	UClass* GameModeClass = LoadClass<AGameModeBase>(nullptr,
		TEXT("/Game/Core/BP_WuwaGameMode.BP_WuwaGameMode_C"));
	if (!TestNotNull(TEXT("Configured player controller loads"), ControllerClass)
		|| !TestNotNull(TEXT("Existing game mode loads"), GameModeClass))
	{
		return false;
	}
	const AWuwaPlayerController* Controller = ControllerClass->GetDefaultObject<AWuwaPlayerController>();
	UClass* ManagerClass = Controller->PlayerCameraManagerClass;
	if (!TestTrue(TEXT("Player owns a camera manager derived from the native adapter"),
		ManagerClass && ManagerClass->IsChildOf(AWuwaPlayerCameraManager::StaticClass())))
	{
		return false;
	}
	const AWuwaPlayerCameraManager* Manager = ManagerClass->GetDefaultObject<AWuwaPlayerCameraManager>();
	TestNotNull(TEXT("Manager has an editable gameplay mode asset"), Manager->DefaultMode.Get());
	TestNotNull(TEXT("A skill mode example is available without changing any abilities"),
		LoadObject<UWuwaCameraMode>(nullptr, TEXT("/Game/Game/Camera/DA_Camera_SkillExample.DA_Camera_SkillExample")));
	TestTrue(TEXT("Existing game mode still selects the project player controller"),
		GameModeClass->GetDefaultObject<AGameModeBase>()->PlayerControllerClass == ControllerClass);
	for (const TCHAR* BlueprintPath : {
		TEXT("/Game/Characters/Role/changli/BP_Role_Changli.BP_Role_Changli"),
		TEXT("/Game/Characters/Test/Test.Test") })
	{
		const UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, BlueprintPath);
		if (!TestNotNull(FString::Printf(TEXT("Character Blueprint loads: %s"), BlueprintPath), Blueprint))
		{
			continue;
		}
		TestTrue(TEXT("Character Blueprint is compiled"),
			Blueprint->Status == BS_UpToDate || Blueprint->Status == BS_UpToDateWithWarnings);
		if (!TestNotNull(TEXT("Character Blueprint has a generated class"), Blueprint->GeneratedClass.Get()))
		{
			continue;
		}
		AWuwaCharacter* Character = Cast<AWuwaCharacter>(Blueprint->GeneratedClass->GetDefaultObject());
		if (!TestNotNull(TEXT("Character still uses the existing native role class"), Character))
		{
			continue;
		}
		TestNull(TEXT("Character no longer owns the old follow camera"), Character->GetDefaultSubobjectByName(TEXT("FollowCamera")));
		TestNull(TEXT("Character no longer owns the old camera boom"), Character->GetDefaultSubobjectByName(TEXT("CameraBoom")));
	}
	return true;
}

#endif
