#include "Editor/Commandlets/WuwaDebugPrintSetupCommandlet.h"

#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Game/Common/Debug/WuwaDebug.h"
#include "HAL/FileManager.h"
#include "K2Node_CallFunction.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/DateTime.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaDebugPrintSetup, Log, All);

UWuwaDebugPrintSetupCommandlet::UWuwaDebugPrintSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpUsage = TEXT("-run=WuwaDebugPrintSetup [-Apply]");
}

int32 UWuwaDebugPrintSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	const TPair<const TCHAR*, EWuwaDebugChannel> Targets[] = {
		{TEXT("/Game/Characters/Role/Common/BP_RoleBase.BP_RoleBase"), EWuwaDebugChannel::MoveState},
		{TEXT("/Game/Characters/Role/Common/ABP_RoleBase.ABP_RoleBase"), EWuwaDebugChannel::AnimParams},
	};
	UFunction* PrintString = UKismetSystemLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString));
	UFunction* DebugPrint = UWuwaDebugLibrary::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UWuwaDebugLibrary, DebugPrint));
	check(PrintString && DebugPrint);
	const FString BackupRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/DebugPrintSetup") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	const FString ChannelName = TEXT("Channel");

	for (const TPair<const TCHAR*, EWuwaDebugChannel>& Target : Targets)
	{
		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, Target.Key);
		if (!Blueprint)
		{
			UE_LOG(LogWuwaDebugPrintSetup, Error, TEXT("Missing %s"), Target.Key);
			return 1;
		}
		const FString ChannelValue = StaticEnum<EWuwaDebugChannel>()->GetNameStringByValue(static_cast<int64>(Target.Value));
		TArray<UK2Node_CallFunction*> Nodes;
		TArray<UEdGraph*> Graphs;
		Blueprint->GetAllGraphs(Graphs);
		for (UEdGraph* Graph : Graphs)
		{
			for (UEdGraphNode* GraphNode : Graph->Nodes)
			{
				UK2Node_CallFunction* Node = Cast<UK2Node_CallFunction>(GraphNode);
				if (Node && Node->GetTargetFunction() == PrintString) Nodes.Add(Node);
			}
		}
		UE_LOG(LogWuwaDebugPrintSetup, Display, TEXT("%s: %d Print String -> Wuwa Debug Print (%s)"), *Blueprint->GetName(), Nodes.Num(), *ChannelValue);
		if (!bApply || Nodes.IsEmpty()) continue;

		// 备份
		const FString PackageName = Blueprint->GetOutermost()->GetName();
		FString Relative = PackageName;
		Relative.RemoveFromStart(TEXT("/Game/"));
		const FString Source = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
		const FString Destination = BackupRoot / (Relative + FPackageName::GetAssetPackageExtension());
		if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Destination), true) || IFileManager::Get().Copy(*Destination, *Source, false, true) != COPY_OK)
		{
			UE_LOG(LogWuwaDebugPrintSetup, Error, TEXT("Could not back up %s"), *Source);
			return 1;
		}

		// 原地换函数：ReconstructNode 按引脚名重新接线（InString / bPrintToScreen / ... 同名），只多出 Channel
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
		for (UK2Node_CallFunction* Node : Nodes)
		{
			Node->Modify();
			const int32 LinksBefore = Node->Pins.FilterByPredicate([](const UEdGraphPin* Pin) { return Pin->LinkedTo.Num() > 0; }).Num();
			Node->SetFromFunction(DebugPrint);
			Node->ReconstructNode();
			UEdGraphPin* Channel = Node->FindPin(*ChannelName);
			if (!Channel)
			{
				UE_LOG(LogWuwaDebugPrintSetup, Error, TEXT("%s: replaced node has no Channel pin."), *Blueprint->GetName());
				return 1;
			}
			Schema->TrySetDefaultValue(*Channel, ChannelValue);
			const int32 LinksAfter = Node->Pins.FilterByPredicate([](const UEdGraphPin* Pin) { return Pin->LinkedTo.Num() > 0; }).Num();
			if (LinksAfter != LinksBefore)
			{
				UE_LOG(LogWuwaDebugPrintSetup, Error, TEXT("%s: node %s lost links (%d -> %d); nothing saved."), *Blueprint->GetName(), *Node->NodeGuid.ToString(), LinksBefore, LinksAfter);
				return 1;
			}
		}
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FCompilerResultsLog Results;
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::None, &Results);
		if (Results.NumErrors > 0 || Blueprint->Status == BS_Error)
		{
			UE_LOG(LogWuwaDebugPrintSetup, Error, TEXT("%s failed to compile; nothing saved (backup in %s)."), *Blueprint->GetName(), *BackupRoot);
			return 1;
		}
		UPackage* Package = Blueprint->GetOutermost();
		Package->MarkPackageDirty();
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		Args.Error = GWarn;
		if (!UPackage::SavePackage(Package, Blueprint, *Source, Args))
		{
			UE_LOG(LogWuwaDebugPrintSetup, Error, TEXT("Could not save %s (is the editor open?)"), *Source);
			return 1;
		}
		UE_LOG(LogWuwaDebugPrintSetup, Display, TEXT("Saved %s (backup in %s)"), *PackageName, *BackupRoot);
	}
	return 0;
#else
	return 1;
#endif
}
