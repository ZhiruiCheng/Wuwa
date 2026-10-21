#include "Editor/WuwaRoleRigEditorLibrary.h"

#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "AnimExecutionContextLibrary.h"
#include "AnimGraphNode_ControlRig.h"
#include "AnimNode_ControlRig_Library.h"
#include "ControlRig.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Logging/TokenizedMessage.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "UObject/UnrealType.h"

namespace WuwaRoleRigMigration
{
const FName VariableName(TEXT("FootIKRigClass"));
const FName FunctionName(TEXT("InitializeRoleFootIK"));

FString Compile(UAnimBlueprint* Blueprint)
{
	FCompilerResultsLog Results;
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipSave, &Results);
	if (Results.NumErrors || Blueprint->Status == BS_Error || !Blueprint->GeneratedClass)
	{
		FString Error = FString::Printf(TEXT("ERROR: %s failed to compile (%d errors)."), *Blueprint->GetPathName(), Results.NumErrors);
		for (const TSharedRef<FTokenizedMessage>& Message : Results.Messages)
		{
			Error += TEXT("\n") + Message->ToText().ToString();
		}
		return Error;
	}
	return FString();
}

bool Save(UAnimBlueprint* Blueprint)
{
	UEditorAssetSubsystem* Assets = GEditor ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
	return Assets && Assets->SaveLoadedAsset(Blueprint, false);
}

UK2Node_CallFunction* AddCall(UEdGraph* Graph, UFunction* Function, int32 X, int32 Y)
{
	FGraphNodeCreator<UK2Node_CallFunction> Creator(*Graph);
	UK2Node_CallFunction* Node = Creator.CreateNode();
	Node->SetFromFunction(Function);
	Node->NodePosX = X;
	Node->NodePosY = Y;
	Creator.Finalize();
	return Node;
}

bool Link(UEdGraph* Graph, UEdGraphPin* From, UEdGraphPin* To)
{
	return From && To && (From->LinkedTo.Contains(To) || Graph->GetSchema()->TryCreateConnection(From, To));
}

bool VerifyFunctionGraph(UEdGraph* Graph, UFunction* ConvertFunction, UFunction* SetFunction)
{
	UK2Node_FunctionEntry* Entry = nullptr;
	UK2Node_CallFunction* Convert = nullptr;
	UK2Node_CallFunction* Setter = nullptr;
	UK2Node_VariableGet* Getter = nullptr;
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (UK2Node_FunctionEntry* Candidate = Cast<UK2Node_FunctionEntry>(Node)) Entry = Candidate;
		if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
		{
			if (Call->GetTargetFunction() == ConvertFunction) Convert = Call;
			if (Call->GetTargetFunction() == SetFunction) Setter = Call;
		}
		if (UK2Node_VariableGet* Candidate = Cast<UK2Node_VariableGet>(Node))
		{
			if (Candidate->VariableReference.GetMemberName() == VariableName) Getter = Candidate;
		}
	}
	if (!Entry || !Convert || !Setter || !Getter || !Entry->MetaData.bThreadSafe) return false;
	auto Connected = [](UEdGraphPin* A, UEdGraphPin* B)
	{
		return A && B && A->LinkedTo.Contains(B);
	};
	return Connected(Entry->FindPin(UEdGraphSchema_K2::PN_Then), Setter->FindPin(UEdGraphSchema_K2::PN_Execute))
		&& Connected(Entry->FindPin(TEXT("Node")), Convert->FindPin(TEXT("Node")))
		&& Connected(Convert->FindPin(TEXT("ControlRig")), Setter->FindPin(TEXT("Node")))
		&& Connected(Getter->FindPin(VariableName), Setter->FindPin(TEXT("ControlRigClass")));
}
}
#endif

FString UWuwaRoleRigEditorLibrary::ConfigureTerrainFootIK(UAnimBlueprint* Blueprint)
{
#if WITH_EDITOR
	if (!Blueprint) return TEXT("ERROR: Missing blueprint.");
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	int32 RigCount = 0, TraceCount = 0;
	Blueprint->Modify();
	for (UEdGraph* Graph : Graphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (auto* Call = Cast<UK2Node_CallFunction>(Node); Call && Graph->GetFName() == TEXT("EventGraph")
				&& Call->FunctionReference.GetMemberName() == TEXT("左右脚IK射线检测"))
			{
				UEdGraphPin* In = Call->FindPin(UEdGraphSchema_K2::PN_Execute);
				UEdGraphPin* Out = Call->FindPin(UEdGraphSchema_K2::PN_Then);
				if (!In || !Out) return TEXT("ERROR: Unexpected legacy trace call pins.");
				const auto Before = In->LinkedTo;
				const auto After = Out->LinkedTo;
				In->BreakAllPinLinks(); Out->BreakAllPinLinks();
				for (auto* A : Before) for (auto* B : After)
					if (!Graph->GetSchema()->TryCreateConnection(A, B)) return TEXT("ERROR: Cannot bypass legacy trace call.");
				Call->NodeComment = TEXT("Deprecated: terrain traces now run on the current input pose inside Wuwa Terrain Foot IK.");
				++TraceCount;
			}
			if (auto* Rig = Cast<UAnimGraphNode_ControlRig>(Node))
			{
				++RigCount;
				for (const TCHAR* Name : {TEXT("Left_Foot_Offset"), TEXT("Right_Foot_Offset"), TEXT("HipOffset"),
					TEXT("LFootDistoGround"), TEXT("RFootDistoGround"), TEXT("EnableFootIK")})
				{
					if (auto* Pin = Rig->FindPin(Name)) Pin->BreakAllPinLinks();
				}
				if (auto* Pin = Rig->FindPin(TEXT("EnableFootIK"))) Pin->DefaultValue = TEXT("true");
				// Hide obsolete inputs so the editor does not advertise a second, ineffective IK switch.
				if (const FArrayProperty* Property = FindFProperty<FArrayProperty>(Rig->GetClass(), TEXT("CustomPinProperties")))
				{
					auto& Pins = *Property->ContainerPtrToValuePtr<TArray<FOptionalPinFromProperty>>(Rig);
					for (auto& Pin : Pins) { Pin.bShowPin = false; Pin.bIsOverrideEnabled = false; }
					Rig->ReconstructNode();
				}
				Rig->NodeComment = TEXT("Terrain IK: tune FootIKSettings in Class Defaults > Wuwa > Animation > Foot IK.");
			}
		}
	}
	if (RigCount != 1) return TEXT("ERROR: Expected one role Control Rig node.");
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	const FString Error = WuwaRoleRigMigration::Compile(Blueprint);
	return Error.IsEmpty() ? FString::Printf(TEXT("OK: Preserved pose graph; bypassed %d legacy trace calls."), TraceCount) : Error;
#else
	return TEXT("ERROR: Editor only.");
#endif
}

FString UWuwaRoleRigEditorLibrary::ConfigureRoleFootIK(UAnimBlueprint* Blueprint, bool bSave)
{
#if WITH_EDITOR
	using namespace WuwaRoleRigMigration;
	if (!Blueprint) return TEXT("ERROR: Animation Blueprint is required.");
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	TArray<UAnimGraphNode_ControlRig*> Rigs;
	UEdGraph* FunctionGraph = nullptr;
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph->GetFName() == FunctionName) FunctionGraph = Graph;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UAnimGraphNode_ControlRig* RigNode = Cast<UAnimGraphNode_ControlRig>(Node)) Rigs.Add(RigNode);
		}
	}
	if (Rigs.Num() != 1) return FString::Printf(TEXT("ERROR: Expected one directly owned Control Rig node, found %d."), Rigs.Num());
	UAnimGraphNode_ControlRig* RigNode = Rigs[0];
	UClass* DefaultRigClass = RigNode->Node.GetControlRigClass();
	if (!DefaultRigClass || !DefaultRigClass->IsChildOf(UControlRig::StaticClass())) return TEXT("ERROR: The Control Rig node has no valid default rig class.");
	const FName ExistingBinding = RigNode->InitialUpdateFunction.GetMemberName();
	if (ExistingBinding != NAME_None && ExistingBinding != FunctionName) return TEXT("ERROR: The Control Rig node already has a different On Initial Update binding.");
	const FName ExistingUpdateBinding = RigNode->UpdateFunction.GetMemberName();
	if (ExistingUpdateBinding != NAME_None && ExistingUpdateBinding != FunctionName) return TEXT("ERROR: The Control Rig node already has a different On Update binding.");
	UFunction* Prototype = UAnimExecutionContextLibrary::StaticClass()->FindFunctionByName(TEXT("Prototype_ThreadSafeAnimUpdateCall"));
	UFunction* ConvertFunction = UAnimNodeControlRigLibrary::StaticClass()->FindFunctionByName(TEXT("ConvertToControlRigPure"));
	UFunction* SetFunction = UAnimNodeControlRigLibrary::StaticClass()->FindFunctionByName(TEXT("SetControlRigClass"));
	if (!Prototype || !ConvertFunction || !SetFunction) return TEXT("ERROR: Required engine animation node functions are unavailable.");
	if (FunctionGraph && !VerifyFunctionGraph(FunctionGraph, ConvertFunction, SetFunction)) return TEXT("ERROR: Existing InitializeRoleFootIK graph differs from the expected migration graph.");

	FEdGraphPinType ClassType;
	ClassType.PinCategory = UEdGraphSchema_K2::PC_Class;
	ClassType.PinSubCategoryObject = UControlRig::StaticClass();
	const int32 VariableIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VariableName);
	if (VariableIndex != INDEX_NONE && Blueprint->NewVariables[VariableIndex].VarType != ClassType) return TEXT("ERROR: FootIKRigClass exists with an incompatible type.");
	if (VariableIndex == INDEX_NONE && Blueprint->SkeletonGeneratedClass && Blueprint->SkeletonGeneratedClass->FindPropertyByName(VariableName)) return TEXT("ERROR: FootIKRigClass is inherited; run this migration on the animation template that owns the graph.");
	Blueprint->Modify();
	RigNode->Modify();
	if (VariableIndex == INDEX_NONE)
	{
		if (!FBlueprintEditorUtils::AddMemberVariable(Blueprint, VariableName, ClassType, DefaultRigClass->GetPathName())) return TEXT("ERROR: Cannot add FootIKRigClass.");
		FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, VariableName, nullptr, FText::FromString(TEXT("Role|Foot IK")));
		FBlueprintEditorUtils::SetBlueprintVariableMetaData(Blueprint, VariableName, nullptr, TEXT("Tooltip"), TEXT("Control Rig configured by each role's child Animation Blueprint. The rig must retain the template rig's bone and curve keys."));
	}

	if (!FunctionGraph)
	{
		FunctionGraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, FunctionName, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
		FBlueprintEditorUtils::AddFunctionGraph(Blueprint, FunctionGraph, true, Prototype);
		TArray<UK2Node_FunctionEntry*> Entries;
		FunctionGraph->GetNodesOfClass(Entries);
		if (Entries.Num() != 1) return TEXT("ERROR: Cannot create animation node callback entry.");
		UK2Node_FunctionEntry* Entry = Entries[0];
		Entry->MetaData.bThreadSafe = true;
		Entry->MetaData.Category = FText::FromString(TEXT("Role|Foot IK"));
		Entry->NodePosX = 0;
		Entry->NodePosY = 0;
		UK2Node_CallFunction* Convert = AddCall(FunctionGraph, ConvertFunction, 300, 160);
		UK2Node_CallFunction* Setter = AddCall(FunctionGraph, SetFunction, 660, 0);
		FGraphNodeCreator<UK2Node_VariableGet> GetterCreator(*FunctionGraph);
		UK2Node_VariableGet* Getter = GetterCreator.CreateNode();
		Getter->VariableReference.SetSelfMember(VariableName);
		Getter->NodePosX = 310;
		Getter->NodePosY = 390;
		GetterCreator.Finalize();
		if (!Link(FunctionGraph, Entry->FindPin(UEdGraphSchema_K2::PN_Then), Setter->FindPin(UEdGraphSchema_K2::PN_Execute))
			|| !Link(FunctionGraph, Entry->FindPin(TEXT("Node")), Convert->FindPin(TEXT("Node")))
			|| !Link(FunctionGraph, Convert->FindPin(TEXT("ControlRig")), Setter->FindPin(TEXT("Node")))
			|| !Link(FunctionGraph, Getter->FindPin(VariableName), Setter->FindPin(TEXT("ControlRigClass"))))
		{
			return TEXT("ERROR: Cannot connect the role rig callback pins; Blueprint was not saved.");
		}
	}
	// OnInitializeAnimInstance can restore the default Control Rig after a mesh
	// reinitialization, while NodeRelevancy retains its initial-update tracker.
	// Reassert the child class on each update; equal classes reuse the same rig.
	RigNode->InitialUpdateFunction = FMemberReference();
	RigNode->UpdateFunction.SetSelfMember(FunctionName);
	Blueprint->RequestRefreshExtensions();
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FString Error = Compile(Blueprint);
	if (!Error.IsEmpty()) return Error;
	if (!VerifyFunctionGraph(FunctionGraph, ConvertFunction, SetFunction)
		|| RigNode->Node.GetControlRigClass() != DefaultRigClass
		|| RigNode->InitialUpdateFunction.GetMemberName() != NAME_None
		|| RigNode->UpdateFunction.ResolveMember<UFunction>(Blueprint->GeneratedClass) == nullptr)
	{
		return TEXT("ERROR: Callback validation failed after compilation; Blueprint was not saved.");
	}
	FClassProperty* ClassProperty = FindFProperty<FClassProperty>(Blueprint->GeneratedClass, VariableName);
	if (!ClassProperty || ClassProperty->MetaClass != UControlRig::StaticClass()) return TEXT("ERROR: Compiled FootIKRigClass property is missing or has the wrong type.");
	UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
	if (!ClassProperty->GetObjectPropertyValue_InContainer(CDO))
	{
		CDO->Modify();
		ClassProperty->SetObjectPropertyValue_InContainer(CDO, DefaultRigClass);
	}
	Blueprint->MarkPackageDirty();
	if (bSave && !Save(Blueprint)) return TEXT("ERROR: Callback compiled successfully but saving the Blueprint failed.");
	return FString::Printf(TEXT("OK: %s has FootIKRigClass and InitializeRoleFootIK bound to Control Rig On Update; default rig and pose links preserved. Saved=%s"), *Blueprint->GetPathName(), bSave ? TEXT("true") : TEXT("false"));
#else
	return TEXT("ERROR: Editor-only operation.");
#endif
}

FString UWuwaRoleRigEditorLibrary::SetRoleFootIKClass(UAnimBlueprint* Blueprint, UClass* RigClass, bool bSave)
{
#if WITH_EDITOR
	using namespace WuwaRoleRigMigration;
	if (!Blueprint || !RigClass || !RigClass->IsChildOf(UControlRig::StaticClass()) || RigClass->IsNative()) return TEXT("ERROR: A child animation Blueprint and generated Control Rig class are required.");
	FString Error = Compile(Blueprint);
	if (!Error.IsEmpty()) return Error;
	FClassProperty* Property = FindFProperty<FClassProperty>(Blueprint->GeneratedClass, VariableName);
	if (!Property || Property->MetaClass != UControlRig::StaticClass()) return TEXT("ERROR: FootIKRigClass is not inherited from the configured template.");
	Blueprint->Modify();
	UObject* CDO = Blueprint->GeneratedClass->GetDefaultObject();
	CDO->Modify();
	Property->SetObjectPropertyValue_InContainer(CDO, RigClass);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	Error = Compile(Blueprint);
	if (!Error.IsEmpty()) return Error;
	Property = FindFProperty<FClassProperty>(Blueprint->GeneratedClass, VariableName);
	if (!Property || Property->GetObjectPropertyValue_InContainer(Blueprint->GeneratedClass->GetDefaultObject()) != RigClass) return TEXT("ERROR: Child rig class default did not survive compilation.");
	Blueprint->MarkPackageDirty();
	if (bSave && !Save(Blueprint)) return TEXT("ERROR: Rig class configured successfully but saving the Blueprint failed.");
	return FString::Printf(TEXT("OK: %s FootIKRigClass=%s. Saved=%s"), *Blueprint->GetPathName(), *RigClass->GetPathName(), bSave ? TEXT("true") : TEXT("false"));
#else
	return TEXT("ERROR: Editor-only operation.");
#endif
}

