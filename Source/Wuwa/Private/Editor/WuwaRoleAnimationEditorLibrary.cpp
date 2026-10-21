#include "Editor/WuwaRoleAnimationEditorLibrary.h"

#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"
#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_SequenceEvaluator.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_BlendSpaceEvaluator.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "EdGraphSchema_K2.h"
#include "GameFramework/Character.h"
#include "K2Node_CallFunction.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaRoleAnimationEditor, Log, All);

namespace WuwaRoleAnimationEditor
{
struct FDefaultSnapshot
{
	FName Name;
	FString CppType;
	FString Value;
};

static bool Fail(const FString& Message)
{
	UE_LOG(LogWuwaRoleAnimationEditor, Error, TEXT("%s"), *Message);
	return false;
}

static bool CompileChecked(UAnimBlueprint* Blueprint)
{
	FCompilerResultsLog Results;
	const EBlueprintCompileOptions Options = EBlueprintCompileOptions::SkipSave
		| EBlueprintCompileOptions::UseDeltaSerializationDuringReinstancing
		| EBlueprintCompileOptions::SkipNewVariableDefaultsDetection;
	FKismetEditorUtilities::CompileBlueprint(Blueprint, Options, &Results);
	if (Results.NumErrors != 0 || Blueprint->Status == BS_Error || !Blueprint->GeneratedClass)
	{
		return Fail(FString::Printf(TEXT("Compile failed for %s (%d errors); no package was saved."),
			*Blueprint->GetPathName(), Results.NumErrors));
	}
	UE_LOG(LogWuwaRoleAnimationEditor, Display, TEXT("Compiled %s: %d errors, %d warnings."),
		*Blueprint->GetPathName(), Results.NumErrors, Results.NumWarnings);
	return true;
}

// Widening an object reference from a role class to its common character base is safe.
static bool CanInheritProperty(const FProperty* OldProperty, const FProperty* ParentProperty)
{
	if (!OldProperty || !ParentProperty) { return false; }
	if (OldProperty->SameType(ParentProperty)) { return true; }
	const FObjectProperty* OldObject = CastField<FObjectProperty>(OldProperty);
	const FObjectProperty* ParentObject = CastField<FObjectProperty>(ParentProperty);
	return OldObject && ParentObject && !CastField<FClassProperty>(OldProperty)
		&& !CastField<FClassProperty>(ParentProperty) && OldProperty->ArrayDim == ParentProperty->ArrayDim
		&& OldObject->PropertyClass->IsChildOf(ParentObject->PropertyClass);
}

static bool CaptureDefaults(UAnimBlueprint* Child, UAnimBlueprint* Parent,
	TArray<FDefaultSnapshot>& OutDefaults)
{
	UObject* OldCDO = Child->GeneratedClass->GetDefaultObject();
	for (TFieldIterator<FProperty> It(Parent->GeneratedClass); It; ++It)
	{
		const FProperty* ParentProperty = *It;
		if (!ParentProperty->HasAnyPropertyFlags(CPF_Edit | CPF_BlueprintVisible)
			|| ParentProperty->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient | CPF_NonPIEDuplicateTransient))
		{
			continue;
		}
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(ParentProperty))
		{
			if (StructProperty->Struct->IsChildOf(FAnimNode_Base::StaticStruct())) { continue; }
		}
		const FProperty* OldProperty = FindFProperty<FProperty>(Child->GeneratedClass, ParentProperty->GetFName());
		if (!CanInheritProperty(OldProperty, ParentProperty)) { continue; }
		if (OldProperty->ArrayDim != 1)
		{
			return Fail(FString::Printf(TEXT("Cannot snapshot fixed-array CDO property %s."), *OldProperty->GetName()));
		}
		FDefaultSnapshot& Snapshot = OutDefaults.AddDefaulted_GetRef();
		Snapshot.Name = OldProperty->GetFName();
		Snapshot.CppType = ParentProperty->GetCPPType();
		OldProperty->ExportTextItem_Direct(Snapshot.Value,
			OldProperty->ContainerPtrToValuePtr<void>(OldCDO), nullptr, OldCDO, PPF_None);
	}
	return true;
}

static bool RestoreDefaults(UAnimBlueprint* Child, const TArray<FDefaultSnapshot>& Defaults)
{
	UObject* NewCDO = Child->GeneratedClass->GetDefaultObject();
	NewCDO->Modify();
	for (const FDefaultSnapshot& Snapshot : Defaults)
	{
		FProperty* Property = FindFProperty<FProperty>(Child->GeneratedClass, Snapshot.Name);
		if (!Property || Property->GetCPPType() != Snapshot.CppType
			|| !Property->ImportText_Direct(*Snapshot.Value, Property->ContainerPtrToValuePtr<void>(NewCDO), NewCDO, PPF_None))
		{
			return Fail(FString::Printf(TEXT("Failed to restore inherited CDO default %s on %s."),
				*Snapshot.Name.ToString(), *Child->GetPathName()));
		}
	}
	return true;
}

static bool AcceptsAsset(const UAnimGraphNode_Base* Node, const UAnimationAsset* Asset)
{
	if (Node->IsA<UAnimGraphNode_SequencePlayer>() || Node->IsA<UAnimGraphNode_SequenceEvaluator>())
	{
		return Asset->IsA<UAnimSequenceBase>();
	}
	if (Node->IsA<UAnimGraphNode_BlendSpacePlayer>() || Node->IsA<UAnimGraphNode_BlendSpaceEvaluator>())
	{
		return Asset->IsA<UBlendSpace>();
	}
	return false;
}

static UAnimationAsset* ReadCompiledAsset(UAnimBlueprintGeneratedClass* Class, UObject* CDO,
	const UAnimGraphNode_Base* GraphNode, const FGuid& Guid)
{
	if (GraphNode->IsA<UAnimGraphNode_SequencePlayer>())
	{
		if (auto* Node = Class->GetPropertyInstance<FAnimNode_SequencePlayer>(CDO, Guid, EPropertySearchMode::Hierarchy))
		{
			return Node->GetSequence();
		}
	}
	else if (GraphNode->IsA<UAnimGraphNode_SequenceEvaluator>())
	{
		if (auto* Node = Class->GetPropertyInstance<FAnimNode_SequenceEvaluator>(CDO, Guid, EPropertySearchMode::Hierarchy))
		{
			return Node->GetSequence();
		}
	}
	else if (GraphNode->IsA<UAnimGraphNode_BlendSpacePlayer>())
	{
		if (auto* Node = Class->GetPropertyInstance<FAnimNode_BlendSpacePlayer>(CDO, Guid, EPropertySearchMode::Hierarchy))
		{
			return Node->GetBlendSpace();
		}
	}
	else if (GraphNode->IsA<UAnimGraphNode_BlendSpaceEvaluator>())
	{
		if (auto* Node = Class->GetPropertyInstance<FAnimNode_BlendSpaceEvaluator>(CDO, Guid, EPropertySearchMode::Hierarchy))
		{
			return Node->GetBlendSpace();
		}
	}
	return nullptr;
}
}
#endif

bool UWuwaRoleAnimationEditorLibrary::ConvertToTemplateChild(UAnimBlueprint* Child,
	UAnimBlueprint* Parent, USkeleton* Target)
{
#if WITH_EDITOR
	using namespace WuwaRoleAnimationEditor;
	if (!Child || !Parent || Child == Parent || !Target || !Child->GeneratedClass || !Parent->GeneratedClass)
	{
		return Fail(TEXT("Conversion requires distinct compiled child/parent animation blueprints and a target skeleton."));
	}
	if (!Parent->bIsTemplate || Parent->TargetSkeleton != nullptr
		|| Parent->Status == BS_Error || Parent->Status == BS_Dirty)
	{
		return Fail(TEXT("The parent must be a successfully compiled, skeleton-free animation template."));
	}
	if (Parent->GeneratedClass->IsChildOf(Child->GeneratedClass))
	{
		return Fail(TEXT("Conversion would introduce a blueprint inheritance cycle."));
	}
	if (!Child->Timelines.IsEmpty() || !Child->ComponentTemplates.IsEmpty())
	{
		return Fail(TEXT("Unexpected timeline/component ownership: refusing to discard non-animation content."));
	}
	for (const FBPVariableDescription& Variable : Child->NewVariables)
	{
		const FProperty* OldProperty = FindFProperty<FProperty>(Child->GeneratedClass, Variable.VarName);
		const FProperty* ParentProperty = FindFProperty<FProperty>(Parent->GeneratedClass, Variable.VarName);
		if (!CanInheritProperty(OldProperty, ParentProperty))
		{
			return Fail(FString::Printf(TEXT("Local variable %s is not an inherited compatible variable; migrate it explicitly first."),
				*Variable.VarName.ToString()));
		}
	}
	for (const FBPInterfaceDescription& Interface : Child->ImplementedInterfaces)
	{
		if (Interface.Interface && !Parent->GeneratedClass->ImplementsInterface(Interface.Interface))
		{
			return Fail(TEXT("Child owns an interface absent from the template; migrate its behavior before conversion."));
		}
	}
	TArray<FDefaultSnapshot> Defaults;
	if (!CaptureDefaults(Child, Parent, Defaults)) { return false; }
	const FString OriginalAssetPath = Child->GetPathName();
	const FString OriginalClassPath = Child->GeneratedClass->GetPathName();

	Child->Modify();
	// Snapshot only the top-level graphs; RemoveGraph handles their nested state/function graphs.
	TSet<UEdGraph*> Graphs;
	for (UEdGraph* Graph : Child->UbergraphPages) { if (Graph) { Graphs.Add(Graph); } }
	for (UEdGraph* Graph : Child->FunctionGraphs) { if (Graph) { Graphs.Add(Graph); } }
	for (UEdGraph* Graph : Child->MacroGraphs) { if (Graph) { Graphs.Add(Graph); } }
	for (UEdGraph* Graph : Child->DelegateSignatureGraphs) { if (Graph) { Graphs.Add(Graph); } }
	for (const FBPInterfaceDescription& Interface : Child->ImplementedInterfaces)
	{
		for (UEdGraph* Graph : Interface.Graphs) { if (Graph) { Graphs.Add(Graph); } }
	}
	for (UEdGraph* Graph : Graphs)
	{
		FBlueprintEditorUtils::RemoveGraph(Child, Graph, EGraphRemoveFlags::MarkTransient);
	}
	// Graph references are gone, so batch removal avoids compiling a partially converted class.
	Child->NewVariables.Reset();
	Child->ImplementedInterfaces.Reset();
	Child->IntermediateGeneratedGraphs.Reset();
	Child->EventGraphs.Reset();
	Child->LastEditedDocuments.Reset();
	Child->ParentAssetOverrides.Reset();
	Child->ParentClass = Parent->GeneratedClass;
	Child->bIsTemplate = false;
	Child->TargetSkeleton = Target;
	if (UBlueprintGeneratedClass* OldClass = Cast<UBlueprintGeneratedClass>(Child->GeneratedClass))
	{
		OldClass->PrepareToConformSparseClassData(Parent->GeneratedClass->GetSparseClassDataStruct());
	}
	FBlueprintEditorUtils::RefreshAllNodes(Child);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Child);
	if (!CompileChecked(Child)) { return false; }
	if (Child->GetPathName() != OriginalAssetPath || Child->GeneratedClass->GetPathName() != OriginalClassPath
		|| Child->GeneratedClass->GetSuperClass() != Parent->GeneratedClass
		|| !Child->FunctionGraphs.IsEmpty() || !Child->UbergraphPages.IsEmpty()
		|| !Child->MacroGraphs.IsEmpty() || !Child->DelegateSignatureGraphs.IsEmpty() || !Child->NewVariables.IsEmpty())
	{
		return Fail(TEXT("Converted blueprint did not retain its identity or still owns graphs/variables."));
	}
	UAnimBlueprintGeneratedClass* AnimClass = Cast<UAnimBlueprintGeneratedClass>(Child->GeneratedClass);
	if (!AnimClass || AnimClass->TargetSkeleton != Target)
	{
		return Fail(TEXT("Converted generated class did not bind the requested skeleton."));
	}
	if (!RestoreDefaults(Child, Defaults)) { return false; }
	Child->MarkPackageDirty();
	UE_LOG(LogWuwaRoleAnimationEditor, Display, TEXT("Converted %s in place to child of %s; restored %d defaults; no save."),
		*Child->GetPathName(), *Parent->GetPathName(), Defaults.Num());
	return true;
#else
	return false;
#endif
}

bool UWuwaRoleAnimationEditorLibrary::SetParentAssetOverrides(UAnimBlueprint* Child,
	const TArray<FGuid>& ParentNodeGuids, const TArray<UAnimationAsset*>& Assets)
{
#if WITH_EDITOR
	using namespace WuwaRoleAnimationEditor;
	if (!Child || !Child->ParentClass || Child->bIsTemplate || !Child->TargetSkeleton
		|| ParentNodeGuids.Num() != Assets.Num() || ParentNodeGuids.IsEmpty())
	{
		return Fail(TEXT("Overrides require a skeleton-bound child and equally sized, nonempty GUID/asset arrays."));
	}
	UAnimBlueprint* Parent = Cast<UAnimBlueprint>(Child->ParentClass->ClassGeneratedBy);
	UAnimBlueprintGeneratedClass* ParentClass = Cast<UAnimBlueprintGeneratedClass>(Child->ParentClass);
	if (!Parent || !ParentClass || Parent->Status == BS_Error || Parent->Status == BS_Dirty)
	{
		return Fail(TEXT("The animation parent must be compiled before resolving override GUIDs."));
	}
	TMap<FGuid, UAnimGraphNode_Base*> ParentNodes;
	TArray<UEdGraph*> Graphs;
	Parent->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UAnimGraphNode_Base* AnimNode = Cast<UAnimGraphNode_Base>(Node))
			{
				ParentNodes.Add(AnimNode->NodeGuid, AnimNode);
			}
		}
	}
	TSet<FGuid> Seen;
	TArray<FAnimParentNodeAssetOverride> Overrides;
	for (int32 Index = 0; Index < ParentNodeGuids.Num(); ++Index)
	{
		const FGuid& Guid = ParentNodeGuids[Index];
		UAnimationAsset* Asset = Assets[Index];
		UAnimGraphNode_Base* Node = ParentNodes.FindRef(Guid);
		if (!Guid.IsValid() || Seen.Contains(Guid) || !Asset || !Node
			|| !AcceptsAsset(Node, Asset) || Asset->GetSkeleton() != Child->TargetSkeleton
			|| ParentClass->GetNodeIndexFromGuid(Guid, EPropertySearchMode::Hierarchy) == INDEX_NONE)
		{
			return Fail(FString::Printf(TEXT("Invalid override %d: GUID=%s asset=%s (duplicate, incompatible, or uncompiled node)."),
				Index, *Guid.ToString(), *GetPathNameSafe(Asset)));
		}
		const FName AssetPinName = Asset->IsA<UBlendSpace>() ? FName(TEXT("BlendSpace")) : FName(TEXT("Sequence"));
		if (const UEdGraphPin* Pin = Node->FindPin(AssetPinName))
		{
			if (!Pin->LinkedTo.IsEmpty())
			{
				return Fail(FString::Printf(TEXT("Node %s receives its asset from a connected pin; a CDO asset override would be overwritten at runtime."), *Guid.ToString()));
			}
		}
		Seen.Add(Guid);
		Overrides.Emplace(Guid, Asset);
	}
	Child->Modify();
	Child->ParentAssetOverrides = MoveTemp(Overrides);
	FBlueprintEditorUtils::MarkBlueprintAsModified(Child);
	if (!CompileChecked(Child)) { return false; }
	UAnimBlueprintGeneratedClass* ChildClass = Cast<UAnimBlueprintGeneratedClass>(Child->GeneratedClass);
	if (!ChildClass) { return Fail(TEXT("Override compilation did not produce an animation generated class.")); }
	UObject* CDO = ChildClass->GetDefaultObject();
	for (int32 Index = 0; Index < ParentNodeGuids.Num(); ++Index)
	{
		const FGuid& Guid = ParentNodeGuids[Index];
		if (ReadCompiledAsset(ChildClass, CDO, ParentNodes.FindRef(Guid), Guid) != Assets[Index])
		{
			return Fail(FString::Printf(TEXT("Compiled override verification failed for node %s; no save."), *Guid.ToString()));
		}
	}
	Child->MarkPackageDirty();
	UE_LOG(LogWuwaRoleAnimationEditor, Display, TEXT("Verified %d parent-node overrides on %s; no save."),
		Assets.Num(), *Child->GetPathName());
	return true;
#else
	return false;
#endif
}


int32 UWuwaRoleAnimationEditorLibrary::ParameterizeAnimationLiterals(UAnimBlueprint* Blueprint,
	const TArray<UAnimationAsset*>& Sources, const TArray<FName>& VariableNames)
{
#if WITH_EDITOR
	using namespace WuwaRoleAnimationEditor;
	if (!Blueprint || Sources.IsEmpty() || Sources.Num() != VariableNames.Num())
	{
		Fail(TEXT("Animation parameterization requires matching source/variable arrays."));
		return -1;
	}
	TSet<UAnimationAsset*> SeenAssets;
	TSet<FName> SeenNames;
	for (int32 Index = 0; Index < Sources.Num(); ++Index)
	{
		if (!Sources[Index] || VariableNames[Index].IsNone() || SeenAssets.Contains(Sources[Index])
			|| SeenNames.Contains(VariableNames[Index]))
		{
			Fail(TEXT("Animation source and variable names must be unique and nonempty."));
			return -1;
		}
		SeenAssets.Add(Sources[Index]);
		SeenNames.Add(VariableNames[Index]);
	}
	Blueprint->Modify();
	for (int32 Index = 0; Index < Sources.Num(); ++Index)
	{
		const int32 VariableIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VariableNames[Index]);
		if (VariableIndex == INDEX_NONE)
		{
			FEdGraphPinType Type;
			Type.PinCategory = UEdGraphSchema_K2::PC_Object;
			Type.PinSubCategoryObject = Sources[Index]->GetClass();
			if (!FBlueprintEditorUtils::AddMemberVariable(Blueprint, VariableNames[Index], Type, TEXT("None")))
			{
				Fail(TEXT("Failed to add an animation query variable."));
				return -1;
			}
			FBlueprintEditorUtils::SetBlueprintVariableCategory(Blueprint, VariableNames[Index], nullptr,
				FText::FromString(TEXT("Role Animation|Stop Queries")), true);
		}
		else
		{
			const FEdGraphPinType& Type = Blueprint->NewVariables[VariableIndex].VarType;
			UClass* PropertyClass = Cast<UClass>(Type.PinSubCategoryObject.Get());
			if (Type.PinCategory != UEdGraphSchema_K2::PC_Object || !PropertyClass || !Sources[Index]->IsA(PropertyClass))
			{
				Fail(TEXT("Existing animation query variable has an incompatible type."));
				return -1;
			}
		}
	}
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	int32 Replaced = 0;
	for (UEdGraph* Graph : Graphs)
	{
		const TArray<TObjectPtr<UEdGraphNode>> ExistingNodes = Graph->Nodes;
		for (UEdGraphNode* Node : ExistingNodes)
		{
			if (!Node->IsA<UK2Node_CallFunction>()) { continue; }
			for (UEdGraphPin* Pin : Node->Pins)
			{
				const int32 SourceIndex = Sources.IndexOfByKey(Cast<UAnimationAsset>(Pin->DefaultObject));
				if (SourceIndex == INDEX_NONE || Pin->Direction != EGPD_Input || !Pin->LinkedTo.IsEmpty()) { continue; }
				Graph->Modify();
				Node->Modify();
				FGraphNodeCreator<UK2Node_VariableGet> Creator(*Graph);
				UK2Node_VariableGet* Getter = Creator.CreateNode();
				Getter->VariableReference.SetSelfMember(VariableNames[SourceIndex]);
				Getter->NodePosX = Node->NodePosX - 256;
				Getter->NodePosY = Node->NodePosY + 96;
				Creator.Finalize();
				if (!Getter->GetValuePin() || !Graph->GetSchema()->TryCreateConnection(Getter->GetValuePin(), Pin))
				{
					Fail(FString::Printf(TEXT("Could not connect animation getter %s to %s.%s."),
						*VariableNames[SourceIndex].ToString(), *Node->GetName(), *Pin->PinName.ToString()));
					return -1;
				}
				Pin->DefaultObject = nullptr;
				Pin->DefaultValue.Reset();
				++Replaced;
			}
		}
	}
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	if (!CompileChecked(Blueprint)) { return -1; }
	Blueprint->MarkPackageDirty();
	UE_LOG(LogWuwaRoleAnimationEditor, Display, TEXT("Parameterized %d animation input literals in %s; no save."),
		Replaced, *Blueprint->GetPathName());
	return Replaced;
#else
	return -1;
#endif
}

int32 UWuwaRoleAnimationEditorLibrary::ReplaceCharacterReferenceWithOwner(UAnimBlueprint* Blueprint,
	FName VariableName)
{
#if WITH_EDITOR
	using namespace WuwaRoleAnimationEditor;
	if (!Blueprint || VariableName.IsNone()) { return -1; }
	const int32 VariableIndex = FBlueprintEditorUtils::FindNewVariableIndex(Blueprint, VariableName);
	if (VariableIndex == INDEX_NONE)
	{
		Fail(TEXT("Character-reference repair requires an existing local character variable."));
		return -1;
	}
	FBPVariableDescription& Variable = Blueprint->NewVariables[VariableIndex];
	UClass* OldCharacterClass = Cast<UClass>(Variable.VarType.PinSubCategoryObject.Get());
	if (Variable.VarType.PinCategory != UEdGraphSchema_K2::PC_Object || !OldCharacterClass
		|| !OldCharacterClass->IsChildOf(ACharacter::StaticClass()))
	{
		Fail(TEXT("Character-reference variable does not hold a Character subclass."));
		return -1;
	}
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	TArray<UK2Node_VariableGet*> Getters;
	for (UEdGraph* Graph : Graphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_VariableSet* Setter = Cast<UK2Node_VariableSet>(Node))
			{
				if (Setter->GetVarName() == VariableName && Setter->VariableReference.IsSelfContext())
				{
					Fail(TEXT("Character cache has an assignment; inspect that initialization instead of replacing its reads."));
					return -1;
				}
			}
			if (UK2Node_VariableGet* Getter = Cast<UK2Node_VariableGet>(Node))
			{
				if (Getter->GetVarName() == VariableName && Getter->VariableReference.IsSelfContext())
				{
					if (!Getter->IsNodePure()) { Fail(TEXT("Validated character getters require explicit execution-flow migration.")); return -1; }
					Getters.Add(Getter);
				}
			}
		}
	}
	UFunction* OwnerFunction = UAnimInstance::StaticClass()->FindFunctionByName(GET_FUNCTION_NAME_CHECKED(UAnimInstance, TryGetPawnOwner));
	if (!OwnerFunction) { Fail(TEXT("TryGetPawnOwner function is unavailable.")); return -1; }
	Blueprint->Modify();
	// Preserve the user's variable and metadata, but remove its role-specific class dependency.
	Variable.VarType.PinSubCategoryObject = ACharacter::StaticClass();
	Variable.DefaultValue = TEXT("None");
	int32 Replaced = 0;
	for (UK2Node_VariableGet* Getter : Getters)
	{
		UEdGraph* Graph = Getter->GetGraph();
		Graph->Modify();
		UK2Node_CallFunction* OwnerCall = nullptr;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node))
			{
				if (Call->FunctionReference.GetMemberName() == OwnerFunction->GetFName()
					&& Call->FunctionReference.IsSelfContext()) { OwnerCall = Call; break; }
			}
		}
		if (!OwnerCall)
		{
			FGraphNodeCreator<UK2Node_CallFunction> OwnerCreator(*Graph);
			OwnerCall = OwnerCreator.CreateNode();
			OwnerCall->SetFromFunction(OwnerFunction);
			OwnerCall->NodePosX = Getter->NodePosX - 224;
			OwnerCall->NodePosY = Getter->NodePosY;
			OwnerCreator.Finalize();
		}
		UK2Node_DynamicCast* CharacterCast = nullptr;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(Node))
			{
				if (CastNode->TargetType == ACharacter::StaticClass() && CastNode->IsNodePure()
					&& CastNode->GetCastSourcePin()->LinkedTo.Contains(OwnerCall->GetReturnValuePin()))
				{
					CharacterCast = CastNode;
					break;
				}
			}
		}
		if (!CharacterCast)
		{
			FGraphNodeCreator<UK2Node_DynamicCast> CastCreator(*Graph);
			CharacterCast = CastCreator.CreateNode();
			CharacterCast->TargetType = ACharacter::StaticClass();
			CharacterCast->SetPurity(true);
			CharacterCast->NodePosX = Getter->NodePosX;
			CharacterCast->NodePosY = Getter->NodePosY;
			CastCreator.Finalize();
			if (!Graph->GetSchema()->TryCreateConnection(OwnerCall->GetReturnValuePin(), CharacterCast->GetCastSourcePin()))
			{
				Fail(TEXT("Could not connect pawn owner to the generic character cast."));
				return -1;
			}
		}
		UEdGraphPin* OldValue = Getter->GetValuePin();
		if (!OldValue || !CharacterCast->GetCastResultPin()) { Fail(TEXT("Character reference output pin is missing.")); return -1; }
		const TArray<UEdGraphPin*> Consumers = OldValue->LinkedTo;
		for (UEdGraphPin* Consumer : Consumers)
		{
			if (!Graph->GetSchema()->TryCreateConnection(CharacterCast->GetCastResultPin(), Consumer))
			{
				Fail(TEXT("A character-cache consumer requires a role-specific type; refusing to break its link."));
				return -1;
			}
		}
		FBlueprintEditorUtils::RemoveNode(Blueprint, Getter, true);
		++Replaced;
	}
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	if (!CompileChecked(Blueprint)) { return -1; }
	Blueprint->MarkPackageDirty();
	UE_LOG(LogWuwaRoleAnimationEditor, Display, TEXT("Replaced %d uninitialized character-cache getters with pawn owner in %s; no save."),
		Replaced, *Blueprint->GetPathName());
	return Replaced;
#else
	return -1;
#endif
}


TMap<FString, FGuid> UWuwaRoleAnimationEditorLibrary::GetAssetPlayerNodeGuids(UAnimBlueprint* Blueprint)
{
	TMap<FString, FGuid> Result;
#if WITH_EDITOR
	if (!Blueprint) { return Result; }
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			if (Node && (Node->IsA<UAnimGraphNode_SequencePlayer>()
				|| Node->IsA<UAnimGraphNode_SequenceEvaluator>()
				|| Node->IsA<UAnimGraphNode_BlendSpacePlayer>()
				|| Node->IsA<UAnimGraphNode_BlendSpaceEvaluator>()))
			{
				Result.Add(Node->GetPathName(), Node->NodeGuid);
			}
		}
	}
#endif
	return Result;
}
