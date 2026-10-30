#include "Editor/Commandlets/WuwaCameraSetupCommandlet.h"

#if WITH_EDITOR
#include "Game/Camera/CameraData/WuwaFightCameraConfig.h"
#include "Game/Camera/WuwaPlayerCameraManager.h"
#include "Game/Input/DataAsset/WuwaInputDataAsset.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/DataTable.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/World.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "FileHelpers.h"
#include "K2Node_CallFunction.h"
#include "K2Node_InputKey.h"
#include "K2Node_Variable.h"
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
	HelpDescription = TEXT("Reports camera setup; use -Apply to create the fight camera config and migrate only the listed project assets. "
		"-CleanLegacyLevel instead lists (with -Apply removes) the L_DaKa level Blueprint nodes that still call the deleted PushCameraMode / PopCameraMode.");
	HelpUsage = TEXT("-run=WuwaCameraSetup [-CleanLegacyLevel] [-Apply]");
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
	// 战斗镜头配置和配置列表；C# 战斗镜头按这个路径加载
	constexpr const TCHAR* FightConfigPackage = TEXT("/Game/Game/Camera/DA_FightCameraConfig");
	constexpr const TCHAR* FightConfigPath = TEXT("/Game/Game/Camera/DA_FightCameraConfig.DA_FightCameraConfig");
	constexpr const TCHAR* ConfigListPackage = TEXT("/Game/Game/Camera/DT_FightCameraConfigList");
	constexpr const TCHAR* ConfigListPath = TEXT("/Game/Game/Camera/DT_FightCameraConfigList.DT_FightCameraConfigList");
	constexpr const TCHAR* ManagerPackage = TEXT("/Game/Game/Camera/BP_WuwaPlayerCameraManager");
	constexpr const TCHAR* ManagerPath = TEXT("/Game/Game/Camera/BP_WuwaPlayerCameraManager.BP_WuwaPlayerCameraManager");
	// 旧"参数镜头"（UWuwaCameraMode + PCM 的 PushCameraMode / PopCameraMode）已删；这个关卡的关卡蓝图里还留着按 K / L 试它的调试节点
	constexpr const TCHAR* LegacyCameraLevel = TEXT("/Game/Environment/DaKa/Maps/L_DaKa");

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
				continue; // A newly created asset has no previous file to preserve.
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

	/**
	 * 配置列表里的 基础镜头 行：一直生效、从这一行开始叠配置；淡入 0.3 秒、淡出 1 秒；
	 * 没改任何编号（全用默认配置）。
	 */
	FWuwaFightCameraConfigRow MakeBaseRow()
	{
		FWuwaFightCameraConfigRow Row;
		Row.Type = EWuwaFightCameraConfigType::Base;
		Row.Priority = 0;
		Row.FadeInTime = 0.3f;
		Row.FadeOutTime = 1.f;
		Row.bEnableModifyCamera = true;
		Row.bResetDefaultConfig = true;
		Row.bResetCameraLock = false;
		Row.bUniqueFade = false;
		return Row;
	}

	bool IsExecPin(const UEdGraphPin* Pin)
	{
		return Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec;
	}

	bool IsLegacyCameraModeCall(const UEdGraphNode* Node)
	{
		const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
		const FName Function = Call ? Call->FunctionReference.GetMemberName() : NAME_None;
		return Function == TEXT("PushCameraMode") || Function == TEXT("PopCameraMode");
	}

	/** 从一个节点沿执行线往后，收集整条链。 */
	void CollectExecChain(UEdGraphNode* Node, TSet<UEdGraphNode*>& Chain)
	{
		if (!Node || Chain.Contains(Node)) return;
		Chain.Add(Node);
		for (UEdGraphPin* Pin : Node->Pins)
		{
			if (Pin->Direction != EGPD_Output || !IsExecPin(Pin)) continue;
			for (UEdGraphPin* Linked : Pin->LinkedTo) CollectExecChain(Linked->GetOwningNode(), Chain);
		}
	}

	/** 只给要删的节点供数据的纯节点（没有执行引脚、输出全连在要删的节点上）也一起删，直到没有新的。 */
	void AddExclusiveFeeders(TSet<UEdGraphNode*>& Remove)
	{
		for (bool bChanged = true; bChanged;)
		{
			bChanged = false;
			for (UEdGraphNode* Node : Remove.Array())
			{
				for (UEdGraphPin* Pin : Node->Pins)
				{
					if (Pin->Direction != EGPD_Input || IsExecPin(Pin)) continue;
					for (UEdGraphPin* Linked : Pin->LinkedTo)
					{
						UEdGraphNode* Feeder = Linked->GetOwningNode();
						if (Remove.Contains(Feeder)) continue;
						bool bPure = true;
						bool bExclusive = true;
						for (UEdGraphPin* FeederPin : Feeder->Pins)
						{
							bPure &= !IsExecPin(FeederPin);
							if (FeederPin->Direction != EGPD_Output) continue;
							for (UEdGraphPin* Out : FeederPin->LinkedTo) bExclusive &= Remove.Contains(Out->GetOwningNode());
						}
						if (bPure && bExclusive)
						{
							Remove.Add(Feeder);
							bChanged = true;
						}
					}
				}
			}
		}
	}

	void LogNode(const UEdGraphNode* Node, const TCHAR* Prefix)
	{
		UE_LOG(LogWuwaCameraSetup, Display, TEXT("%s%s [%s] \"%s\""), Prefix, *Node->GetName(), *Node->GetClass()->GetName(),
			*Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
		for (const UEdGraphPin* Pin : Node->Pins)
		{
			FString Links;
			for (const UEdGraphPin* Linked : Pin->LinkedTo)
			{
				Links += FString::Printf(TEXT(" %s.%s"), *Linked->GetOwningNode()->GetName(), *Linked->PinName.ToString());
			}
			if (Links.IsEmpty() && Pin->DefaultValue.IsEmpty() && !Pin->DefaultObject) continue;
			UE_LOG(LogWuwaCameraSetup, Display, TEXT("%s    %s %s = %s%s ->%s"), Prefix, Pin->Direction == EGPD_Input ? TEXT("in ") : TEXT("out"),
				*Pin->PinName.ToString(), *Pin->DefaultValue, Pin->DefaultObject ? *Pin->DefaultObject->GetName() : TEXT(""), *Links);
		}
	}

	/**
	 * 删掉关卡蓝图里调用已删函数 PushCameraMode / PopCameraMode 的节点（留着关卡一加载就报蓝图编译错误）：
	 * 按键事件链里有这两个调用 → 整条链（按键事件 + 后面的节点）删掉；别处的调用只删它自己，执行线前后接上；
	 * 只给它们供数据的纯节点、只被它们用到的成员变量一起删。只读时只打印关卡蓝图的全部节点和要删的节点。
	 */
	int32 CleanLegacyCameraLevel(bool bApply)
	{
		const FString LevelFile = FPackageName::LongPackageNameToFilename(LegacyCameraLevel, FPackageName::GetMapPackageExtension());
		if (!IFileManager::Get().FileExists(*LevelFile))
		{
			UE_LOG(LogWuwaCameraSetup, Error, TEXT("Level %s does not exist."), LegacyCameraLevel);
			return 1;
		}
		UWorld* World = UEditorLoadingAndSavingUtils::LoadMap(LegacyCameraLevel);
		ULevelScriptBlueprint* Blueprint = World && World->PersistentLevel ? World->PersistentLevel->GetLevelScriptBlueprint(true) : nullptr;
		if (!Blueprint)
		{
			UE_LOG(LogWuwaCameraSetup, Error, TEXT("%s has no level Blueprint."), LegacyCameraLevel);
			return 1;
		}

		TArray<UEdGraphNode*> Calls;
		TArray<UK2Node_InputKey*> Keys;
		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			UE_LOG(LogWuwaCameraSetup, Display, TEXT("Graph %s (%d nodes)"), *Graph->GetName(), Graph->Nodes.Num());
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				LogNode(Node, TEXT("  "));
				if (IsLegacyCameraModeCall(Node)) Calls.Add(Node);
				if (UK2Node_InputKey* Key = Cast<UK2Node_InputKey>(Node)) Keys.Add(Key);
			}
		}
		TSet<UEdGraphNode*> Remove;
		for (UK2Node_InputKey* Key : Keys)
		{
			TSet<UEdGraphNode*> Chain;
			CollectExecChain(Key, Chain);
			if (Calls.ContainsByPredicate([&Chain](UEdGraphNode* Call) { return Chain.Contains(Call); })) Remove.Append(Chain);
		}
		TArray<UEdGraphNode*> Bypassed;
		for (UEdGraphNode* Call : Calls)
		{
			if (!Remove.Contains(Call)) Bypassed.Add(Call);
			Remove.Add(Call);
		}
		AddExclusiveFeeders(Remove);
		TSet<FName> Variables;
		for (const UEdGraphNode* Node : Remove)
		{
			if (const UK2Node_Variable* Variable = Cast<UK2Node_Variable>(Node)) Variables.Add(Variable->VariableReference.GetMemberName());
			LogNode(Node, Bypassed.Contains(Node) ? TEXT("Remove (bypass exec) ") : TEXT("Remove "));
		}
		UE_LOG(LogWuwaCameraSetup, Display, TEXT("%d legacy camera mode calls, %d nodes to remove."), Calls.Num(), Remove.Num());
		if (!bApply || Remove.IsEmpty())
		{
			if (!bApply) UE_LOG(LogWuwaCameraSetup, Display, TEXT("Read-only; pass -Apply to remove them and save %s."), LegacyCameraLevel);
			return 0;
		}

		const FString BackupDir = FPaths::ProjectSavedDir() / TEXT("Backups/CameraSetup") / FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S-%s"));
		if (IFileManager::Get().Copy(*(BackupDir / FPaths::GetCleanFilename(LevelFile)), *LevelFile, false, true) != COPY_OK)
		{
			UE_LOG(LogWuwaCameraSetup, Error, TEXT("Could not back up %s; nothing changed."), *LevelFile);
			return 1;
		}
		UE_LOG(LogWuwaCameraSetup, Display, TEXT("Backed up %s to %s"), *LevelFile, *BackupDir);
		Blueprint->Modify();
		for (UEdGraphNode* Call : Bypassed)
		{
			UEdGraphPin* In = Call->FindPin(UEdGraphSchema_K2::PN_Execute);
			UEdGraphPin* Out = Call->FindPin(UEdGraphSchema_K2::PN_Then);
			if (!In || !Out) continue;
			for (UEdGraphPin* From : In->LinkedTo)
			{
				for (UEdGraphPin* To : Out->LinkedTo) From->MakeLinkTo(To);
			}
		}
		for (UEdGraphNode* Node : Remove) FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
		for (const FName Variable : Variables)
		{
			if (FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, Variable) != INDEX_NONE && !FBlueprintEditorUtils::IsVariableUsed(Blueprint, Variable))
			{
				FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, Variable);
				UE_LOG(LogWuwaCameraSetup, Display, TEXT("Removed variable %s"), *Variable.ToString());
			}
		}
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		if (!Compile(Blueprint)) return 1;
		if (!UEditorLoadingAndSavingUtils::SaveMap(World, LegacyCameraLevel))
		{
			UE_LOG(LogWuwaCameraSetup, Error, TEXT("Failed to save %s; the original is in %s."), LegacyCameraLevel, *BackupDir);
			return 1;
		}
		UE_LOG(LogWuwaCameraSetup, Display, TEXT("Saved %s"), LegacyCameraLevel);
		return 0;
	}
}
#endif

int32 UWuwaCameraSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaCameraSetup;
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	if (FParse::Param(*Params, TEXT("CleanLegacyLevel")))
	{
		return CleanLegacyCameraLevel(bApply);
	}
	UInputAction* Zoom = LoadObject<UInputAction>(nullptr, ZoomPath, nullptr, LOAD_NoWarn);
	UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, ContextPath);
	UWuwaInputDataAsset* Bindings = LoadObject<UWuwaInputDataAsset>(nullptr, BindingsPath);
	UBlueprint* ControllerBlueprint = LoadObject<UBlueprint>(nullptr, ControllerPath);
	UBlueprint* CharacterBlueprint = LoadObject<UBlueprint>(nullptr, CharacterPath);
	UBlueprint* TestCharacterBlueprint = LoadObject<UBlueprint>(nullptr, TestCharacterPath);
	UWuwaFightCameraConfig* FightConfig = LoadObject<UWuwaFightCameraConfig>(nullptr, FightConfigPath, nullptr, LOAD_NoWarn);
	UDataTable* ConfigList = LoadObject<UDataTable>(nullptr, ConfigListPath, nullptr, LOAD_NoWarn);
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
	UE_LOG(LogWuwaCameraSetup, Display, TEXT("Fight camera config=%s, config list=%s (%d rows), manager Blueprint=%s"),
		FightConfig ? TEXT("exists") : TEXT("missing"), ConfigList ? TEXT("exists") : TEXT("missing"),
		ConfigList ? ConfigList->GetRowMap().Num() : 0, ManagerBlueprint ? TEXT("exists") : TEXT("missing"));
	if (!bApply)
	{
		UE_LOG(LogWuwaCameraSetup, Display, TEXT("Nothing saved. Add -Apply to migrate camera input, create the fight camera config / config list, the manager Blueprint, and the three listed controller/character Blueprints. No maps are edited."));
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
	// 配置资产只在缺的时候新建（默认值来自 UWuwaFightCameraConfig 的构造函数），已有的保留策划改过的值
	if (!FightConfig)
	{
		if (FPackageName::DoesPackageExist(FightConfigPackage))
		{
			UE_LOG(LogWuwaCameraSetup, Error, TEXT("%s exists but is not a loadable fight camera config. No assets saved."), FightConfigPackage);
			return 1;
		}
		FightConfig = NewObject<UWuwaFightCameraConfig>(CreatePackage(FightConfigPackage),
			*FPackageName::GetShortName(FightConfigPackage), RF_Public | RF_Standalone);
	}
	if (!ConfigList)
	{
		if (FPackageName::DoesPackageExist(ConfigListPackage))
		{
			UE_LOG(LogWuwaCameraSetup, Error, TEXT("%s exists but is not a loadable data table. No assets saved."), ConfigListPackage);
			return 1;
		}
		ConfigList = NewObject<UDataTable>(CreatePackage(ConfigListPackage),
			*FPackageName::GetShortName(ConfigListPackage), RF_Public | RF_Standalone);
		ConfigList->RowStruct = FWuwaFightCameraConfigRow::StaticStruct();
		ConfigList->AddRow(TEXT("100001"), MakeBaseRow());
	}
	if (ConfigList->RowStruct != FWuwaFightCameraConfigRow::StaticStruct())
	{
		UE_LOG(LogWuwaCameraSetup, Error, TEXT("%s uses row struct %s, expected FWuwaFightCameraConfigRow. No assets saved."),
			ConfigListPackage, *GetNameSafe(ConfigList->RowStruct));
		return 1;
	}
	if (FightConfig->ConfigList != ConfigList)
	{
		FightConfig->Modify();
		FightConfig->ConfigList = ConfigList;
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
	// Compile all affected Blueprints before saving any asset. The manager Blueprint is recompiled so that
	// properties removed from the native class (the old camera-mode defaults) are dropped when it is saved.
	ManagerBlueprint->Modify();
	FBlueprintEditorUtils::MarkBlueprintAsModified(ManagerBlueprint);
	for (UBlueprint* Blueprint : { ManagerBlueprint, ControllerBlueprint, CharacterBlueprint, TestCharacterBlueprint })
	{
		if (!Compile(Blueprint))
		{
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
	TArray<UObject*> Assets = { FightConfig, ConfigList, ManagerBlueprint, Zoom, Context, Bindings,
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
