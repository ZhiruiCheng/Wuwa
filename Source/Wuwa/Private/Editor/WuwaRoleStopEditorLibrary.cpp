#include "Editor/WuwaRoleStopEditorLibrary.h"

#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "AnimGraphNode_BlendListByBool.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_StateResult.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "EdGraphSchema_K2.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimInstance.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Logging/TokenizedMessage.h"
#include "UObject/UnrealType.h"

namespace WuwaRoleStopEditor
{
const TCHAR* Tags[] = { TEXT("WuwaRoleStop:WalkLeft"), TEXT("WuwaRoleStop:WalkRight"),
	TEXT("WuwaRoleStop:RunLeft"), TEXT("WuwaRoleStop:RunRight"), TEXT("WuwaRoleStop:SprintLeft"), TEXT("WuwaRoleStop:SprintRight") };
const FName Properties[] = { TEXT("RoleStopWalkLeft"), TEXT("RoleStopWalkRight"), TEXT("RoleStopRunLeft"),
	TEXT("RoleStopRunRight"), TEXT("RoleStopSprintLeft"), TEXT("RoleStopSprintRight") };

FString Compile(UAnimBlueprint* Blueprint)
{
	FCompilerResultsLog Results;
	FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipSave, &Results);
	if (Results.NumErrors || Blueprint->Status == BS_Error || !Blueprint->GeneratedClass)
	{
		FString Error = FString::Printf(TEXT("ERROR: %s compile failed (%d errors)."), *Blueprint->GetPathName(), Results.NumErrors);
		for (const TSharedRef<FTokenizedMessage>& Message : Results.Messages) Error += TEXT("\n") + Message->ToText().ToString();
		return Error;
	}
	return FString();
}

UAnimStateNode* FindStop(UAnimBlueprint* Blueprint)
{
	TArray<UEdGraph*> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	for (UEdGraph* Graph : Graphs)
	{
		if (Graph->GetFName() != TEXT("SM_Ground")) continue;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			UAnimStateNode* State = Cast<UAnimStateNode>(Node);
			if (State && State->BoundGraph && State->BoundGraph->GetFName() == TEXT("Stop")) return State;
		}
	}
	return nullptr;
}

bool Link(UEdGraph* Graph, UEdGraphPin* From, UEdGraphPin* To)
{
	return From && To && (From->LinkedTo.Contains(To) || Graph->GetSchema()->TryCreateConnection(From, To));
}

UEdGraphPin* PoseOutput(UEdGraphNode* Node)
{
	for (UEdGraphPin* Pin : Node->Pins)
		if (Pin->Direction == EGPD_Output && Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct) return Pin;
	return nullptr;
}

UK2Node_VariableGet* Getter(UEdGraph* Graph, FName Property, int32 X, int32 Y)
{
	FGraphNodeCreator<UK2Node_VariableGet> Creator(*Graph);
	UK2Node_VariableGet* Node = Creator.CreateNode();
	// 这些是当前 AnimInstance 继承的属性；外部成员引用会产生未连接的 Target。
	Node->VariableReference.SetSelfMember(Property);
	Node->NodePosX = X;
	Node->NodePosY = Y;
	Creator.Finalize();
	return Node;
}

UAnimGraphNode_BlendListByBool* Branch(UEdGraph* Graph, FName Property, UEdGraphNode* TruePose,
	UEdGraphNode* FalsePose, int32 X, int32 Y, bool& bLinked)
{
	FGraphNodeCreator<UAnimGraphNode_BlendListByBool> Creator(*Graph);
	UAnimGraphNode_BlendListByBool* Node = Creator.CreateNode();
	Node->NodePosX = X;
	Node->NodePosY = Y;
	Creator.Finalize();
	UK2Node_VariableGet* Bool = Getter(Graph, Property, X - 210, Y - 80);
	bLinked &= Link(Graph, Bool->FindPin(Property), Node->FindPin(TEXT("bActiveValue")));
	bLinked &= Link(Graph, PoseOutput(TruePose), Node->FindPin(TEXT("BlendPose_0")));
	bLinked &= Link(Graph, PoseOutput(FalsePose), Node->FindPin(TEXT("BlendPose_1")));
	// The selection is frozen at entry; internal gait/foot switching needs no crossfade.
	for (UEdGraphPin* Pin : Node->Pins)
		if (Pin->PinName.ToString().StartsWith(TEXT("BlendTime_"))) Pin->DefaultValue = TEXT("0.0");
	return Node;
}

void RemoveObsoletePhaseUpdates(UAnimBlueprint* Blueprint)
{
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		const TArray<TObjectPtr<UEdGraphNode>> Nodes = Graph->Nodes;
		for (UEdGraphNode* Node : Nodes)
		{
			UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
			if (!Call) continue;
			const FName Name = Call->FunctionReference.GetMemberName();
			if (Name != TEXT("获取左右脚进度参数") && Name != TEXT("获取左右脚进度BS空间参数")) continue;
			UEdGraphPin* In = Call->FindPin(UEdGraphSchema_K2::PN_Execute);
			UEdGraphPin* Out = Call->FindPin(UEdGraphSchema_K2::PN_Then);
			const TArray<UEdGraphPin*> Before = In ? In->LinkedTo : TArray<UEdGraphPin*>();
			const TArray<UEdGraphPin*> After = Out ? Out->LinkedTo : TArray<UEdGraphPin*>();
			Call->BreakAllNodeLinks();
			for (UEdGraphPin* Source : Before) for (UEdGraphPin* Target : After) Link(Graph, Source, Target);
			FBlueprintEditorUtils::RemoveNode(Blueprint, Call, true);
		}
	}
}

UWuwaAnimInstance* Defaults(UAnimBlueprint* Blueprint)
{
	return Blueprint && Blueprint->GeneratedClass ? Cast<UWuwaAnimInstance>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
}
}
#endif

FString UWuwaRoleStopEditorLibrary::ConfigureTemplateStops(UAnimBlueprint* Blueprint)
{
#if WITH_EDITOR
	using namespace WuwaRoleStopEditor;
	if (!Defaults(Blueprint)) return TEXT("ERROR: Expected an animation Blueprint derived from WuwaAnimInstance.");
	UAnimStateNode* Stop = FindStop(Blueprint);
	if (!Stop) return TEXT("ERROR: Authored SM_Ground.Stop not found.");
	UEdGraph* Graph = Stop->BoundGraph;
	UAnimGraphNode_StateResult* Result = nullptr;
	UAnimStateTransitionNode* Exit = nullptr;
	TArray<UAnimGraphNode_SequencePlayer*> Players;
	Players.SetNumZeroed(6);
	for (UEdGraphNode* Node : Graph->Nodes)
	{
		if (UAnimGraphNode_StateResult* Candidate = Cast<UAnimGraphNode_StateResult>(Node))
		{
			if (Result) return TEXT("ERROR: Stop has more than one state result.");
			Result = Candidate;
		}
		if (UAnimGraphNode_SequencePlayer* Player = Cast<UAnimGraphNode_SequencePlayer>(Node))
		{
			for (int32 Index = 0; Index < 6; ++Index) if (Player->NodeComment == Tags[Index]) Players[Index] = Player;
		// 保留原四个节点的 GUID，精确移除子蓝图中的旧停步覆盖。
			if (Player->NodeComment.StartsWith(TEXT("WuwaRoleStop:"))) continue;
			const FString Name = Player->GetName();
			if (Name == TEXT("AnimGraphNode_SequencePlayer_0")) Players[2] = Player;
			else if (Name == TEXT("AnimGraphNode_SequencePlayer_2")) Players[0] = Player;
			else if (Name == TEXT("AnimGraphNode_SequencePlayer_3")) Players[4] = Player;
			else if (Name == TEXT("AnimGraphNode_SequencePlayer_4")) Players[5] = Player;
			else return TEXT("ERROR: Unexpected stop sequence player; refusing to discard an unknown player.");
		}
	}
	for (UEdGraphNode* Node : Stop->GetGraph()->Nodes)
	{
		UAnimStateTransitionNode* Transition = Cast<UAnimStateTransitionNode>(Node);
		if (Transition && Transition->GetPreviousState() == Stop && Transition->GetNextState()
			&& Transition->GetNextState()->GetStateName() == TEXT("Idle")) Exit = Transition;
	}
	if (!Result || !Exit || !Players[0] || !Players[2] || !Players[4] || !Players[5])
		return TEXT("ERROR: Expected original four stop players, state result, and Stop -> Idle transition.");
	const FFloatProperty* PlayRateProperty = FindFProperty<FFloatProperty>(FAnimNode_SequencePlayer::StaticStruct(), TEXT("PlayRate"));
	const FFloatProperty* StartPositionProperty = FindFProperty<FFloatProperty>(FAnimNode_SequencePlayer::StaticStruct(), TEXT("StartPosition"));
	if (!PlayRateProperty || !StartPositionProperty) return TEXT("ERROR: Sequence player timing properties not found.");
	Blueprint->Modify();
	Graph->Modify();
	Stop->Modify();
	Exit->Modify();
	Result->Modify();
	const TArray<TObjectPtr<UEdGraphNode>> OldNodes = Graph->Nodes;
	for (UEdGraphNode* Node : OldNodes)
	{
		Node->BreakAllNodeLinks();
		UAnimGraphNode_SequencePlayer* Player = Cast<UAnimGraphNode_SequencePlayer>(Node);
		if (Node != Result && (!Player || !Players.Contains(Player)))
			FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
	}
	bool bLinked = true;
	for (int32 Index = 0; Index < 6; ++Index)
	{
		if (!Players[Index])
		{
			FGraphNodeCreator<UAnimGraphNode_SequencePlayer> Creator(*Graph);
			Players[Index] = Creator.CreateNode();
			Creator.Finalize();
		}
		UAnimGraphNode_SequencePlayer* Player = Players[Index];
		Player->Modify();
		Player->NodeComment = Tags[Index];
		Player->NodePosX = -900;
		Player->NodePosY = Index * 210;
		Player->Node.SetSequence(nullptr);
		// Sprint stops have no contact markers; keep their existing independent clock.
		const bool bContactSync = Index < 4;
		Player->Node.SetGroupName(bContactSync ? FName(TEXT("Locomotion")) : NAME_None);
		Player->Node.SetGroupMethod(bContactSync ? EAnimSyncMethod::SyncGroup : EAnimSyncMethod::DoNotSync);
		// Stop owns the deceleration clock. It must not be pulled backwards by an outgoing loop.
		Player->Node.SetGroupRole(bContactSync ? EAnimGroupRole::AlwaysLeader : EAnimGroupRole::CanBeLeader);
		Player->Node.SetOverridePositionWhenJoiningSyncGroupAsLeader(bContactSync);
		Player->Node.SetLoopAnimation(false);
		Player->Node.SetIgnoreForRelevancyTest(false);
		// UE 5.7 的这两个运行时 setter 不写编辑器数据，需直接设置反射属性。
		PlayRateProperty->SetPropertyValue_InContainer(&Player->Node, 1.f);
		StartPositionProperty->SetPropertyValue_InContainer(&Player->Node, 0.f);
		Player->InitialUpdateFunction = FMemberReference();
		Player->BecomeRelevantFunction = FMemberReference();
		Player->UpdateFunction.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UWuwaAnimInstance, UpdateRoleStopPlayer), UWuwaAnimInstance::StaticClass());
		for (int32 PinIndex = 0; PinIndex < Player->ShowPinForProperties.Num(); ++PinIndex)
		{
			const FName Name = Player->ShowPinForProperties[PinIndex].PropertyName;
			if (Name == TEXT("Sequence")) Player->SetPinVisibility(true, PinIndex);
			else if (Name == TEXT("StartPosition") || Name == TEXT("PlayRate")) Player->SetPinVisibility(false, PinIndex);
		}
		Player->ReconstructNode();
		UK2Node_VariableGet* Asset = Getter(Graph, Properties[Index], -1230, Index * 210);
		bLinked &= Link(Graph, Asset->FindPin(Properties[Index]), Player->FindPin(TEXT("Sequence")));
	}
	UAnimGraphNode_BlendListByBool* Walk = Branch(Graph, TEXT("bRoleStopLeft"), Players[0], Players[1], -500, 120, bLinked);
	UAnimGraphNode_BlendListByBool* Run = Branch(Graph, TEXT("bRoleStopLeft"), Players[2], Players[3], -500, 540, bLinked);
	UAnimGraphNode_BlendListByBool* Sprint = Branch(Graph, TEXT("bRoleStopLeft"), Players[4], Players[5], -500, 960, bLinked);
	UAnimGraphNode_BlendListByBool* WalkRun = Branch(Graph, TEXT("bRoleStopWalk"), Walk, Run, -80, 330, bLinked);
	UAnimGraphNode_BlendListByBool* Gait = Branch(Graph, TEXT("bRoleStopSprint"), Sprint, WalkRun, 350, 580, bLinked);
	Result->NodePosX = 780;
	Result->NodePosY = 580;
	bLinked &= Link(Graph, PoseOutput(Gait), Result->FindPin(TEXT("Result")));
	Result->StateEntryFunction.SetExternalMember(GET_FUNCTION_NAME_CHECKED(UWuwaAnimInstance, EnterRoleStop), UWuwaAnimInstance::StaticClass());
	Result->StateExitFunction = FMemberReference();
	Stop->bAlwaysResetOnEntry = true;
	Exit->bAutomaticRuleBasedOnSequencePlayerInState = true;
	Exit->AutomaticRuleTriggerTime = 0.05f;
	Exit->CrossfadeDuration = 0.2f;
	Exit->LogicType = ETransitionLogicType::TLT_StandardBlend;
	Exit->SyncGroupNameToRequireValidMarkersRule = NAME_None;
	RemoveObsoletePhaseUpdates(Blueprint);
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	if (!bLinked) return TEXT("ERROR: Stop graph pin connection failed; discard unsaved asset and inspect pin names.");
	const FString Error = Compile(Blueprint);
	return Error.IsEmpty() ? TEXT("OK: Six independent stop players, frozen entry selection, 0.05s remaining exit / 0.2s blend; unsaved.") : Error;
#else
	return TEXT("ERROR: Editor only.");
#endif
}

FString UWuwaRoleStopEditorLibrary::SetChildStopAnimations(UAnimBlueprint* Blueprint, const TArray<UAnimSequence*>& Sequences)
{
#if WITH_EDITOR
	using namespace WuwaRoleStopEditor;
	UWuwaAnimInstance* CDO = Defaults(Blueprint);
	if (!CDO || Sequences.Num() != 6) return TEXT("ERROR: Expected Wuwa animation Blueprint and six stop sequences.");
	for (const UAnimSequence* Sequence : Sequences)
		if (!Sequence || Sequence->GetPlayLength() <= 0.05f || (Blueprint->TargetSkeleton && Sequence->GetSkeleton() != Blueprint->TargetSkeleton))
			return TEXT("ERROR: Missing, too short, or wrong-skeleton stop sequence.");
	Blueprint->Modify();
	CDO->Modify();
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(CDO->GetClass(), Properties[Index]);
		if (!Property) return TEXT("ERROR: Native stop sequence property not found.");
		Property->SetObjectPropertyValue_InContainer(CDO, Sequences[Index]);
	}
	TSet<FGuid> StopGuids;
	TSet<FGuid> ParentGuids;
	for (UClass* Class = Blueprint->ParentClass; Class; Class = Class->GetSuperClass())
	{
		UAnimBlueprint* Parent = Cast<UAnimBlueprint>(Class->ClassGeneratedBy);
		if (Parent)
		{
			TArray<UEdGraph*> Graphs;
			Parent->GetAllGraphs(Graphs);
			for (UEdGraph* ParentGraph : Graphs)
				for (UEdGraphNode* ParentNode : ParentGraph->Nodes) ParentGuids.Add(ParentNode->NodeGuid);
		}
		UAnimStateNode* Stop = Parent ? FindStop(Parent) : nullptr;
		if (!Stop) continue;
		for (UEdGraphNode* Node : Stop->BoundGraph->Nodes)
			if (Cast<UAnimGraphNode_SequencePlayer>(Node)) StopGuids.Add(Node->NodeGuid);
	}
	// 只删除停步覆盖和父图中已不存在的 GUID；保留 BS、Stand 等其他资产覆盖。
	Blueprint->ParentAssetOverrides.RemoveAll([&StopGuids, &ParentGuids](const FAnimParentNodeAssetOverride& Override)
		{ return StopGuids.Contains(Override.ParentNodeGuid) || !ParentGuids.Contains(Override.ParentNodeGuid); });
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	const FString Error = Compile(Blueprint);
	if (!Error.IsEmpty()) return Error;
	CDO = Defaults(Blueprint);
	if (!CDO) return TEXT("ERROR: Compiled child has no Wuwa animation default object.");
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(CDO->GetClass(), Properties[Index]);
		if (!Property || Property->GetObjectPropertyValue_InContainer(CDO) != Sequences[Index])
			return TEXT("ERROR: Compiled child did not retain a stop sequence default.");
	}
	return TEXT("OK: Six role stop defaults installed and obsolete stop overrides removed; unsaved.");
#else
	return TEXT("ERROR: Editor only.");
#endif
}

FString UWuwaRoleStopEditorLibrary::SetSprintStopEntryFootRelations(UAnimBlueprint* Blueprint, FVector2D Left, FVector2D Right)
{
#if WITH_EDITOR
	using namespace WuwaRoleStopEditor;
	UWuwaAnimInstance* CDO = Defaults(Blueprint);
	if (!CDO || Left.ContainsNaN() || Right.ContainsNaN() || (Left - Right).SizeSquared() < KINDA_SMALL_NUMBER)
		return TEXT("ERROR: Invalid or indistinguishable sprint entry foot prototypes.");
	Blueprint->Modify();
	CDO->Modify();
	CDO->RoleSprintLeftEntryFeet = Left;
	CDO->RoleSprintRightEntryFeet = Right;
	CDO->bRoleSprintEntryFeetConfigured = true;
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	const FString Error = Compile(Blueprint);
	if (!Error.IsEmpty()) return Error;
	CDO = Defaults(Blueprint);
	if (!CDO || !CDO->bRoleSprintEntryFeetConfigured || CDO->RoleSprintLeftEntryFeet != Left || CDO->RoleSprintRightEntryFeet != Right)
		return TEXT("ERROR: Compiled child did not retain sprint entry prototypes.");
	return TEXT("OK: Sprint entry foot prototypes installed; unsaved.");
#else
	return TEXT("ERROR: Editor only.");
#endif
}

FString UWuwaRoleStopEditorLibrary::ConfigureStopLocomotionSync(UAnimBlueprint* Blueprint)
{
#if WITH_EDITOR
	using namespace WuwaRoleStopEditor;
	UAnimStateNode* Stop = Blueprint ? FindStop(Blueprint) : nullptr;
	if (!Stop) return TEXT("ERROR: Missing SM_Ground.Stop.");
	TArray<UAnimGraphNode_SequencePlayer*> Players;
	for (UEdGraphNode* Node : Stop->BoundGraph->Nodes)
		if (auto* Player = Cast<UAnimGraphNode_SequencePlayer>(Node)) Players.Add(Player);
	if (Players.Num() != 6) return TEXT("ERROR: Expected six existing stop players.");
	for (int32 Index = 0; Index < 6; ++Index)
		if (Players.FilterByPredicate([&](const auto* Player) { return Player->NodeComment == Tags[Index]; }).Num() != 1)
			return TEXT("ERROR: Stop player identities are missing or duplicated.");
	Blueprint->Modify();
	for (auto* Player : Players)
	{
		// Without authored contact markers, adding Sprint would disable marker sync for the group.
		if (Player->NodeComment == Tags[4] || Player->NodeComment == Tags[5]) continue;
		Player->Modify();
		Player->Node.SetGroupName(TEXT("Locomotion"));
		Player->Node.SetGroupMethod(EAnimSyncMethod::SyncGroup);
		Player->Node.SetGroupRole(EAnimGroupRole::AlwaysLeader);
		Player->Node.SetOverridePositionWhenJoiningSyncGroupAsLeader(true);
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	const FString Error = Compile(Blueprint);
	return Error.IsEmpty() ? TEXT("OK: Walk/Run stops join Locomotion; unmarked Sprint stops, transitions and entry timing unchanged. Unsaved.") : Error;
#else
	return TEXT("ERROR: Editor only.");
#endif
}
