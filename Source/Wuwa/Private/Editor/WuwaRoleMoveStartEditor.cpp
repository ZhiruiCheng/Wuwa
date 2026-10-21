#include "Editor/WuwaRoleStopEditorLibrary.h"

#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "AnimGraphNode_ApplyAdditive.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_StateResult.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimGraphNode_UseCachedPose.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "EdGraphSchema_K2.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaLocomotionTypes.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"

namespace WuwaMoveStartEditor
{
template<class T> T* Find(UEdGraph* Graph)
{
	for (UEdGraphNode* Node : Graph->Nodes) if (T* Match = Cast<T>(Node)) return Match;
	return nullptr;
}
UAnimStateNode* State(UEdGraph* Graph, FName Name)
{
	for (UEdGraphNode* Node : Graph->Nodes)
		if (auto* S = Cast<UAnimStateNode>(Node); S && S->BoundGraph->GetFName() == Name) return S;
	return nullptr;
}
UEdGraphPin* Pose(UEdGraphNode* Node)
{
	for (UEdGraphPin* Pin : Node->Pins)
		if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct) return Pin;
	return nullptr;
}
bool Link(UEdGraph* Graph, UEdGraphPin* From, UEdGraphPin* To)
{
	return From && To && Graph->GetSchema()->TryCreateConnection(From, To);
}
UAnimStateTransitionNode* Transition(UEdGraph* Graph, UAnimStateNode* From, UAnimStateNode* To, int32 Priority)
{
	FGraphNodeCreator<UAnimStateTransitionNode> Creator(*Graph);
	auto* Node = Creator.CreateNode();
	Node->CrossfadeDuration = .1f;
	Node->PriorityOrder = Priority;
	Creator.Finalize();
	Node->CreateConnections(From, To);
	return Node;
}
// Same interruption rules as the existing Move state; StartMove must never trap movement/skills.
bool InterruptionRule(UAnimStateTransitionNode* Transition, bool bStop)
{
	UEdGraph* Graph = Transition->GetBoundGraph();
	FGraphNodeCreator<UK2Node_VariableGet> GetCreator(*Graph);
	auto* Get = GetCreator.CreateNode();
	Get->VariableReference.SetSelfMember(TEXT("LocomotionData"));
	Get->NodePosX = -900;
	GetCreator.Finalize();
	FGraphNodeCreator<UK2Node_BreakStruct> BreakCreator(*Graph);
	auto* Break = BreakCreator.CreateNode();
	Break->StructType = FWuwaLocomotionAnimData::StaticStruct();
	Break->NodePosX = -650;
	BreakCreator.Finalize();
	for (auto& Pin : Break->ShowPinForProperties)
		if (Pin.PropertyName == TEXT("bGroundMoveActive") || Pin.PropertyName == TEXT("bWantsToStop")) Pin.bShowPin = true;
	Break->ReconstructNode();
	UEdGraphPin* StructInput = nullptr;
	for (auto* Pin : Break->Pins)
		if (Pin->Direction == EGPD_Input && Pin->PinType.PinSubCategoryObject == FWuwaLocomotionAnimData::StaticStruct()) StructInput = Pin;
	bool bOK = Link(Graph, Get->FindPin(TEXT("LocomotionData")), StructInput);
	auto* Result = Find<UAnimGraphNode_TransitionResult>(Graph);
	if (bStop) return bOK && Link(Graph, Break->FindPin(TEXT("bWantsToStop")), Result->FindPin(TEXT("bCanEnterTransition")));
	auto Call = [Graph](const TCHAR* Function, int32 X, int32 Y)
	{
		FGraphNodeCreator<UK2Node_CallFunction> Creator(*Graph);
		auto* Node = Creator.CreateNode();
		Node->SetFromFunction(UKismetMathLibrary::StaticClass()->FindFunctionByName(Function));
		Node->NodePosX = X; Node->NodePosY = Y;
		Creator.Finalize();
		return Node;
	};
	auto* NotMove = Call(TEXT("Not_PreBool"), -400, 0);
	auto* NotStop = Call(TEXT("Not_PreBool"), -400, 150);
	auto* And = Call(TEXT("BooleanAND"), -200, 0);
	bOK &= Link(Graph, Break->FindPin(TEXT("bGroundMoveActive")), NotMove->FindPin(TEXT("A")));
	bOK &= Link(Graph, Break->FindPin(TEXT("bWantsToStop")), NotStop->FindPin(TEXT("A")));
	bOK &= Link(Graph, NotMove->FindPin(TEXT("ReturnValue")), And->FindPin(TEXT("A")));
	bOK &= Link(Graph, NotStop->FindPin(TEXT("ReturnValue")), And->FindPin(TEXT("B")));
	bOK &= Link(Graph, And->FindPin(TEXT("ReturnValue")), Result->FindPin(TEXT("bCanEnterTransition")));
	return bOK;
}
}
#endif

FString UWuwaRoleStopEditorLibrary::SplitTemplateMoveStart(UAnimBlueprint* Blueprint)
{
#if WITH_EDITOR
	using namespace WuwaMoveStartEditor;
	if (!Blueprint) return TEXT("ERROR: Missing blueprint.");
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	UEdGraph* Ground = nullptr;
	for (auto* Graph : Graphs) if (Graph->GetFName() == TEXT("SM_Ground")) Ground = Graph;
	if (!Ground) return TEXT("ERROR: Missing SM_Ground.");
	if (State(Ground, TEXT("StartMove"))) return TEXT("OK: StartMove already exists; no edits.");
	auto* Move = State(Ground, TEXT("Move"));
	auto* Idle = State(Ground, TEXT("Idle"));
	auto* Stop = State(Ground, TEXT("Stop"));
	if (!Move || !Idle || !Stop) return TEXT("ERROR: Expected Move/Idle/Stop states.");
	UEdGraph* OldGraph = Move->BoundGraph;
	auto* Additive = Find<UAnimGraphNode_ApplyAdditive>(OldGraph);
	auto* Player = Find<UAnimGraphNode_SequencePlayer>(OldGraph);
	auto* Cache = Find<UAnimGraphNode_UseCachedPose>(OldGraph);
	auto* Result = Find<UAnimGraphNode_StateResult>(OldGraph);
	auto* Rate = Find<UK2Node_VariableGet>(OldGraph);
	UAnimStateTransitionNode* Entry = nullptr;
	for (UEdGraphNode* Node : Ground->Nodes)
		if (auto* T = Cast<UAnimStateTransitionNode>(Node); T && T->GetPreviousState() == Idle && T->GetNextState() == Move) Entry = T;
	if (!Additive || !Player || !Cache || !Result || !Rate || OldGraph->Nodes.Num() != 5 || !Entry
		|| Player->Node.IsLooping() || Rate->VariableReference.GetMemberName() != TEXT("TargetPlayRate"))
		return TEXT("ERROR: Move no longer matches the audited five-node additive graph. No edits.");
	if (!Pose(Cache)->LinkedTo.Contains(Additive->FindPin(TEXT("Base")))
		|| !Pose(Additive)->LinkedTo.Contains(Result->FindPin(TEXT("Result")))
		|| !Pose(Player)->LinkedTo.Contains(Additive->FindPin(TEXT("Additive"))))
		return TEXT("ERROR: Unexpected Move pose connections. No edits.");
	Blueprint->Modify();
	FGraphNodeCreator<UAnimStateNode> StateCreator(*Ground);
	auto* Start = StateCreator.CreateNode();
	Start->NodePosX = Move->NodePosX - 220;
	Start->NodePosY = Move->NodePosY - 240;
	Start->bAlwaysResetOnEntry = true;
	StateCreator.Finalize();
	FBlueprintEditorUtils::RenameGraph(Start->BoundGraph, TEXT("StartMove"));
	UEdGraph* StartGraph = Start->BoundGraph;
	auto* StartResult = Find<UAnimGraphNode_StateResult>(StartGraph);
	// Move the original nodes rather than duplicate their GUIDs: child asset overrides remain valid.
	Additive->FindPin(TEXT("Base"))->BreakAllPinLinks();
	Pose(Additive)->BreakAllPinLinks();
	const TArray<UEdGraphNode*> MovedNodes = { Additive, Player, Rate };
	for (UEdGraphNode* Node : MovedNodes)
	{
		OldGraph->Nodes.Remove(Node);
		Node->Rename(nullptr, StartGraph, REN_DontCreateRedirectors);
		StartGraph->Nodes.Add(Node);
	}
	FGraphNodeCreator<UAnimGraphNode_UseCachedPose> CacheCreator(*StartGraph);
	auto* StartCache = CacheCreator.CreateNode();
	StartCache->SaveCachedPoseNode = Cache->SaveCachedPoseNode;
	StartCache->NodePosX = Cache->NodePosX; StartCache->NodePosY = Cache->NodePosY;
	CacheCreator.Finalize();
	// A dedicated group identifies only the explicitly inheritable start player.
	Player->Node.SetGroupName(TEXT("LocomotionStart"));
	Player->Node.SetGroupMethod(EAnimSyncMethod::SyncGroup);
	Player->Node.SetIgnoreForRelevancyTest(false);
	Player->NodeComment = TEXT("WuwaMoveStart: inherited one-shot acceleration");
	bool bOK = Link(OldGraph, Pose(Cache), Result->FindPin(TEXT("Result")));
	bOK &= Link(StartGraph, Pose(StartCache), Additive->FindPin(TEXT("Base")));
	bOK &= Link(StartGraph, Pose(Additive), StartResult->FindPin(TEXT("Result")));
	Entry->RelinkHead(Start);
	bOK &= InterruptionRule(Transition(Ground, Start, Stop, 1), true);
	bOK &= InterruptionRule(Transition(Ground, Start, Idle, 2), false);
	auto* Finish = Transition(Ground, Start, Move, 3);
	Finish->bAutomaticRuleBasedOnSequencePlayerInState = true;
	Finish->AutomaticRuleTriggerTime = .05f;
	Finish->SyncGroupNameToRequireValidMarkersRule = NAME_None;
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	if (!bOK) return TEXT("ERROR: Could not wire StartMove; discard unsaved changes.");
	FCompilerResultsLog Log;
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipSave, &Log);
	if (Log.NumErrors || Blueprint->Status == BS_Error) return TEXT("ERROR: Split graph failed compilation; nothing saved.");
	return TEXT("OK: Idle -> StartMove -> Move; start interruption rules and original player GUID preserved. Unsaved.");
#else
	return TEXT("ERROR: Editor only.");
#endif
}
