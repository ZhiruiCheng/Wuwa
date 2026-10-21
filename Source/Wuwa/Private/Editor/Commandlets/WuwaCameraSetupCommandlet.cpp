#include "Editor/Commandlets/WuwaCameraSetupCommandlet.h"

#if WITH_EDITOR
#include "Game/Camera/WuwaCameraMode.h"
#include "Game/Camera/WuwaPlayerCameraManager.h"
#include "Game/Input/DataAsset/WuwaInputDataAsset.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/PlayerController.h"
#include "HAL/FileManager.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputCoreTypes.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/DateTime.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaCameraSetup, Log, All);

UWuwaCameraSetupCommandlet::UWuwaCameraSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpDescription = TEXT("Reports camera input setup; use -Apply to migrate only the listed project assets.");
	HelpUsage = TEXT("-run=WuwaCameraSetup [-Apply]");
}

#if WITH_EDITOR
namespace WuwaCameraSetup
{
	constexpr const TCHAR* ZoomPackage = TEXT("/Game/CoreInput/Actions/IA_CameraZoom");
	constexpr const TCHAR* ZoomPath = TEXT("/Game/CoreInput/Actions/IA_CameraZoom.IA_CameraZoom");
	constexpr const TCHAR* ContextPath = TEXT("/Game/CoreInput/Contexts/IMC_Character.IMC_Character");
	constexpr const TCHAR* BindingsPath = TEXT("/Game/CoreInput/DataAsset/DA_IARouterMap.DA_IARouterMap");
	constexpr const TCHAR* ControllerPath = TEXT("/Game/Core/BP_WuwaPlayerController.BP_WuwaPlayerController");
	constexpr const TCHAR* CharacterPath = TEXT("/Game/Characters/Role/changli/BP_Role_Changli.BP_Role_Changli");
	constexpr const TCHAR* TestCharacterPath = TEXT("/Game/Characters/Test/Test.Test");
	constexpr const TCHAR* GameplayModePackage = TEXT("/Game/Game/Camera/DA_Camera_Gameplay");
	constexpr const TCHAR* GameplayModePath = TEXT("/Game/Game/Camera/DA_Camera_Gameplay.DA_Camera_Gameplay");
	constexpr const TCHAR* SkillModePackage = TEXT("/Game/Game/Camera/DA_Camera_SkillExample");
	constexpr const TCHAR* SkillModePath = TEXT("/Game/Game/Camera/DA_Camera_SkillExample.DA_Camera_SkillExample");
	constexpr const TCHAR* ManagerPackage = TEXT("/Game/Game/Camera/BP_WuwaPlayerCameraManager");
	constexpr const TCHAR* ManagerPath = TEXT("/Game/Game/Camera/BP_WuwaPlayerCameraManager.BP_WuwaPlayerCameraManager");

	UWuwaCameraMode* CreateModeIfMissing(UWuwaCameraMode* Existing, const TCHAR* PackageName, bool& bCreated)
	{
		bCreated = false;
		if (Existing)
		{
			return Existing;
		}
		if (FPackageName::DoesPackageExist(PackageName))
		{
			UE_LOG(LogWuwaCameraSetup, Error, TEXT("%s exists but is not a loadable camera mode. No assets saved."), PackageName);
			return nullptr;
		}
		bCreated = true;
		return NewObject<UWuwaCameraMode>(CreatePackage(PackageName),
			*FPackageName::GetShortName(PackageName), RF_Public | RF_Standalone);
	}

	bool Compile(UBlueprint* Blueprint)
	{
		FCompilerResultsLog Results;
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Results);
		if (Results.NumErrors || Blueprint->Status == BS_Error || !Blueprint->GeneratedClass)
		{
			UE_LOG(LogWuwaCameraSetup, Error, TEXT("Compilation failed for %s; no migration assets saved."), *Blueprint->GetPathName());
			return false;
		}
		return true;
	}

	bool Backup(const TArray<UObject*>& Assets, const FString& BackupRoot)
	{
		for (const UObject* Asset : Assets)
		{
			const FString PackageName = Asset->GetOutermost()->GetName();
			const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
			if (!IFileManager::Get().FileExists(*Filename))
			{
				continue; // A newly created input action has no previous file to preserve.
			}
			FString RelativeName = PackageName;
			RelativeName.RemoveFromStart(TEXT("/Game/"));
			const FString BackupFilename = BackupRoot / (RelativeName + FPackageName::GetAssetPackageExtension());
			if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(BackupFilename), true)
				|| IFileManager::Get().Copy(*BackupFilename, *Filename, false, true) != COPY_OK)
			{
				UE_LOG(LogWuwaCameraSetup, Error, TEXT("Could not back up %s; no migration assets saved."), *Filename);
				return false;
			}
		}
		UE_LOG(LogWuwaCameraSetup, Display, TEXT("Original assets backed up to %s"), *BackupRoot);
		return true;
	}

	bool Save(UObject* Asset)
	{
		UPackage* Package = Asset->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		Args.Error = GWarn;
		if (!UPackage::SavePackage(Package, Asset, *Filename, Args))
		{
			UE_LOG(LogWuwaCameraSetup, Error, TEXT("Failed to save %s. Earlier saves may have succeeded; original assets are in the printed backup directory."), *Filename);
			return false;
		}
		UE_LOG(LogWuwaCameraSetup, Display, TEXT("Saved %s"), *Package->GetName());
		return true;
	}
}
#endif

int32 UWuwaCameraSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaCameraSetup;
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	UInputAction* Zoom = LoadObject<UInputAction>(nullptr, ZoomPath, nullptr, LOAD_NoWarn);
	UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, ContextPath);
	UWuwaInputDataAsset* Bindings = LoadObject<UWuwaInputDataAsset>(nullptr, BindingsPath);
	UBlueprint* ControllerBlueprint = LoadObject<UBlueprint>(nullptr, ControllerPath);
	UBlueprint* CharacterBlueprint = LoadObject<UBlueprint>(nullptr, CharacterPath);
	UBlueprint* TestCharacterBlueprint = LoadObject<UBlueprint>(nullptr, TestCharacterPath);
	UWuwaCameraMode* GameplayMode = LoadObject<UWuwaCameraMode>(nullptr, GameplayModePath, nullptr, LOAD_NoWarn);
	UWuwaCameraMode* SkillMode = LoadObject<UWuwaCameraMode>(nullptr, SkillModePath, nullptr, LOAD_NoWarn);
	UBlueprint* ManagerBlueprint = LoadObject<UBlueprint>(nullptr, ManagerPath, nullptr, LOAD_NoWarn);
	if (!Context || !Bindings || !ControllerBlueprint || !CharacterBlueprint || !TestCharacterBlueprint)
	{
		UE_LOG(LogWuwaCameraSetup, Error, TEXT("A required existing asset is missing. No assets saved."));
		return 1;
	}
	const FGameplayTag RotateTag = FGameplayTag::RequestGameplayTag(TEXT("Player.Common.Camera.Rotate"), false);
	const FGameplayTag ZoomTag = FGameplayTag::RequestGameplayTag(TEXT("Player.Common.Camera.Zoom"), false);
	const FGameplayTag CameraRoute = FGameplayTag::RequestGameplayTag(TEXT("Input.Route.Camera"), false);
	if (!RotateTag.IsValid() || !ZoomTag.IsValid() || !CameraRoute.IsValid())
	{
		UE_LOG(LogWuwaCameraSetup, Error, TEXT("Required native camera tags are unavailable. Build the camera code first. No assets saved."));
		return 1;
	}
	int32 RotateRows = 0;
	int32 RotateCameraRows = 0;
	int32 ZoomRows = 0;
	int32 WheelMappings = 0;
	for (const FInputDataAsset& Row : Bindings->InputDataAssetMap)
	{
		if (Row.InputTag == RotateTag)
		{
			++RotateRows;
			RotateCameraRows += Row.RouteTag == CameraRoute;
		}
		ZoomRows += Row.InputTag == ZoomTag;
	}
	for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
	{
		WheelMappings += Zoom && Mapping.Action == Zoom && Mapping.Key == EKeys::MouseWheelAxis;
	}
	const APlayerController* Defaults = ControllerBlueprint->GeneratedClass
		? Cast<APlayerController>(ControllerBlueprint->GeneratedClass->GetDefaultObject()) : nullptr;
	UE_LOG(LogWuwaCameraSetup, Display, TEXT("%s: Zoom=%s, Axis1D=%d, wheel mappings=%d, zoom rows=%d, rotate camera routes=%d/%d, manager=%s"),
		bApply ? TEXT("Apply") : TEXT("Read-only"), Zoom ? TEXT("exists") : TEXT("missing"),
		Zoom && Zoom->ValueType == EInputActionValueType::Axis1D, WheelMappings, ZoomRows, RotateCameraRows, RotateRows,
		Defaults ? *GetNameSafe(Defaults->PlayerCameraManagerClass.Get()) : TEXT("missing"));
	UE_LOG(LogWuwaCameraSetup, Display, TEXT("Gameplay profile=%s, skill example=%s, manager Blueprint=%s"),
		GameplayMode ? TEXT("exists") : TEXT("missing"), SkillMode ? TEXT("exists") : TEXT("missing"),
		ManagerBlueprint ? TEXT("exists") : TEXT("missing"));
	if (!bApply)
	{
		UE_LOG(LogWuwaCameraSetup, Display, TEXT("Nothing saved. Add -Apply to migrate camera input, the camera profiles/manager Blueprint, and the three listed controller/character Blueprints. No maps are edited."));
		return 0;
	}
	if (RotateRows != 1)
	{
		UE_LOG(LogWuwaCameraSetup, Error, TEXT("Expected exactly one existing rotate binding; found %d. No assets saved."), RotateRows);
		return 1;
	}
	if (!Zoom)
	{
		if (FPackageName::DoesPackageExist(ZoomPackage))
		{
			UE_LOG(LogWuwaCameraSetup, Error, TEXT("The requested zoom package already exists but is not a loadable InputAction. No assets saved."));
			return 1;
		}
		UPackage* Package = CreatePackage(ZoomPackage);
		Zoom = NewObject<UInputAction>(Package, TEXT("IA_CameraZoom"), RF_Public | RF_Standalone);
	}
	bool bCreatedGameplayMode = false;
	bool bCreatedSkillMode = false;
	GameplayMode = CreateModeIfMissing(GameplayMode, GameplayModePackage, bCreatedGameplayMode);
	SkillMode = CreateModeIfMissing(SkillMode, SkillModePackage, bCreatedSkillMode);
	if (!GameplayMode || !SkillMode)
	{
		return 1;
	}
	if (bCreatedSkillMode)
	{
		SkillMode->Settings.ArmLength = 250.f;
		SkillMode->Settings.FieldOfView = 75.f;
		SkillMode->Settings.PivotOffset.Z = 50.;
		SkillMode->Settings.bAllowRotationInput = false;
		SkillMode->Settings.bAllowZoomInput = false;
		SkillMode->Settings.bUseUserZoom = false;
		SkillMode->Settings.BlendTime = .25f;
		SkillMode->Priority = 100;
	}
	if (!ManagerBlueprint)
	{
		if (FPackageName::DoesPackageExist(ManagerPackage))
		{
			UE_LOG(LogWuwaCameraSetup, Error, TEXT("Camera manager package exists with an unexpected asset type. No assets saved."));
			return 1;
		}
		ManagerBlueprint = FKismetEditorUtilities::CreateBlueprint(AWuwaPlayerCameraManager::StaticClass(),
			CreatePackage(ManagerPackage), TEXT("BP_WuwaPlayerCameraManager"), BPTYPE_Normal,
			UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass());
	}
	if (!ManagerBlueprint || !ManagerBlueprint->ParentClass
		|| !ManagerBlueprint->ParentClass->IsChildOf(AWuwaPlayerCameraManager::StaticClass()))
	{
		UE_LOG(LogWuwaCameraSetup, Error, TEXT("Camera manager Blueprint has an unexpected parent. No assets saved."));
		return 1;
	}

	Zoom->Modify();
	Zoom->ValueType = EInputActionValueType::Axis1D;
	// Leave existing controller/gamepad mappings and modifiers intact. Normalize only this wheel binding.
	if (WheelMappings != 1)
	{
		Context->Modify();
		Context->UnmapKey(Zoom, EKeys::MouseWheelAxis);
		Context->MapKey(Zoom, EKeys::MouseWheelAxis);
	}
	Bindings->Modify();
	Bindings->InputDataAssetMap.RemoveAll([Zoom, ZoomTag](const FInputDataAsset& Row)
	{
		return Row.InputTag == ZoomTag || Row.InputAction == Zoom;
	});
	FInputDataAsset& ZoomRow = Bindings->InputDataAssetMap.AddDefaulted_GetRef();
	ZoomRow.InputAction = Zoom;
	ZoomRow.InputTag = ZoomTag;
	ZoomRow.RouteTag = CameraRoute;
	for (FInputDataAsset& Row : Bindings->InputDataAssetMap)
	{
		if (Row.InputTag == RotateTag)
		{
			Row.RouteTag = CameraRoute;
		}
	}

	// A native default-subobject change needs the inherited character Blueprints to be recompiled.
	// Compile all affected Blueprints before saving any asset.
	for (UBlueprint* Blueprint : { ManagerBlueprint, ControllerBlueprint, CharacterBlueprint, TestCharacterBlueprint })
	{
		if (!Compile(Blueprint))
		{
			return 1;
		}
	}
	AWuwaPlayerCameraManager* ManagerDefaults = Cast<AWuwaPlayerCameraManager>(ManagerBlueprint->GeneratedClass->GetDefaultObject());
	if (!ManagerDefaults)
	{
		UE_LOG(LogWuwaCameraSetup, Error, TEXT("Compiled camera manager defaults are invalid. No assets saved."));
		return 1;
	}
	// Preserve a user-selected profile on later runs; initialize the default only when unset.
	if (!ManagerDefaults->DefaultMode)
	{
		ManagerBlueprint->Modify();
		ManagerDefaults->Modify();
		ManagerDefaults->DefaultMode = GameplayMode;
		FBlueprintEditorUtils::MarkBlueprintAsModified(ManagerBlueprint);
		if (!Compile(ManagerBlueprint))
		{
			return 1;
		}
		ManagerDefaults = Cast<AWuwaPlayerCameraManager>(ManagerBlueprint->GeneratedClass->GetDefaultObject());
		if (!ManagerDefaults || ManagerDefaults->DefaultMode != GameplayMode)
		{
			UE_LOG(LogWuwaCameraSetup, Error, TEXT("Camera manager compilation lost its gameplay profile. No assets saved."));
			return 1;
		}
	}
	APlayerController* ControllerDefaults = Cast<APlayerController>(ControllerBlueprint->GeneratedClass->GetDefaultObject());
	if (!ControllerDefaults)
	{
		UE_LOG(LogWuwaCameraSetup, Error, TEXT("Controller Blueprint has an unexpected parent. No assets saved."));
		return 1;
	}
	if (ControllerDefaults->PlayerCameraManagerClass.Get() != ManagerBlueprint->GeneratedClass.Get())
	{
		ControllerBlueprint->Modify();
		ControllerDefaults->Modify();
		ControllerDefaults->PlayerCameraManagerClass = ManagerBlueprint->GeneratedClass.Get();
		FBlueprintEditorUtils::MarkBlueprintAsModified(ControllerBlueprint);
		if (!Compile(ControllerBlueprint))
		{
			return 1;
		}
		ControllerDefaults = Cast<APlayerController>(ControllerBlueprint->GeneratedClass->GetDefaultObject());
		if (!ControllerDefaults || ControllerDefaults->PlayerCameraManagerClass.Get() != ManagerBlueprint->GeneratedClass.Get())
		{
			UE_LOG(LogWuwaCameraSetup, Error, TEXT("Controller compilation did not preserve the camera manager class. No assets saved."));
			return 1;
		}
	}
	TArray<UObject*> Assets = { GameplayMode, SkillMode, ManagerBlueprint, Zoom, Context, Bindings,
		ControllerBlueprint, CharacterBlueprint, TestCharacterBlueprint };
	const FString BackupRoot = FPaths::ProjectSavedDir() / TEXT("Backups/CameraSetup") / FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S-%s"));
	if (!Backup(Assets, BackupRoot))
	{
		return 1;
	}
	for (UObject* Asset : Assets)
	{
		Asset->MarkPackageDirty();
		if (!Save(Asset))
		{
			return 1;
		}
	}
	UE_LOG(LogWuwaCameraSetup, Display, TEXT("Camera setup completed. Existing movement actions, other mappings and maps were preserved."));
	return 0;
#else
	UE_LOG(LogWuwaCameraSetup, Error, TEXT("WuwaCameraSetup requires an editor build."));
	return 1;
#endif
}
