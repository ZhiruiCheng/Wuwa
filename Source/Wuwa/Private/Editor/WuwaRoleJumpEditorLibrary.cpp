#include "Editor/WuwaRoleJumpEditorLibrary.h"

#include "Animation/BlendSpace1D.h"

#if WITH_EDITOR
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimSequence.h"
#include "AnimGraphNode_BlendListByBool.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_StateMachine.h"
#include "AnimGraphNode_StateResult.h"
#include "AnimGraphNode_TransitionResult.h"
#include "AnimStateEntryNode.h"
#include "AnimStateNode.h"
#include "AnimStateTransitionNode.h"
#include "AnimationStateMachineGraph.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNodeUtils.h"
#include "EdGraphSchema_K2.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimInstance.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Logging/TokenizedMessage.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

namespace WuwaRoleJumpEditor
{
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

UWuwaAnimInstance* Defaults(UAnimBlueprint* Blueprint)
{
	return Blueprint && Blueprint->GeneratedClass ? Cast<UWuwaAnimInstance>(Blueprint->GeneratedClass->GetDefaultObject()) : nullptr;
}

/** AnimGraph 里名为 Locmotion 的顶层状态机图（不在事件图里找：那边的同名图是编译中间产物）。 */
UEdGraph* FindLocomotion(UAnimBlueprint* Blueprint)
{
	for (UEdGraph* Graph : Blueprint->FunctionGraphs)
	{
		if (!Graph || Graph->GetFName() != TEXT("AnimGraph")) continue;
		for (UEdGraphNode* Node : Graph->Nodes)
		{
			const UAnimGraphNode_StateMachine* Machine = Cast<UAnimGraphNode_StateMachine>(Node);
			if (Machine && Machine->EditorStateMachineGraph && Machine->EditorStateMachineGraph->GetFName() == WuwaJumpLand::Locomotion)
				return Machine->EditorStateMachineGraph;
		}
	}
	return nullptr;
}

UAnimStateNode* FindState(UEdGraph* Machine, FName Name)
{
	for (UEdGraphNode* Node : Machine->Nodes)
		if (UAnimStateNode* State = Cast<UAnimStateNode>(Node); State && State->BoundGraph && State->BoundGraph->GetFName() == Name) return State;
	return nullptr;
}

template<class T> T* Find(UEdGraph* Graph)
{
	for (UEdGraphNode* Node : Graph->Nodes) if (T* Match = Cast<T>(Node)) return Match;
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
	Node->VariableReference.SetSelfMember(Property);
	Node->NodePosX = X;
	Node->NodePosY = Y;
	Creator.Finalize();
	return Node;
}

/** 新状态（状态图带一个 StateResult）；状态名就是状态图名，改名失败（重名）返回空。 */
UAnimStateNode* AddState(UEdGraph* Machine, FName Name, int32 X, int32 Y, bool bResetOnEntry)
{
	FGraphNodeCreator<UAnimStateNode> Creator(*Machine);
	UAnimStateNode* State = Creator.CreateNode();
	State->NodePosX = X;
	State->NodePosY = Y;
	State->bAlwaysResetOnEntry = bResetOnEntry;
	Creator.Finalize();
	FBlueprintEditorUtils::RenameGraph(State->BoundGraph, Name.ToString());
	return State->BoundGraph && State->BoundGraph->GetFName() == Name ? State : nullptr;
}

/** 过渡：Rule 为空 = 按状态里播放器的剩余时间自动过渡（AutoTriggerTime 秒）；否则读一个布尔成员。 */
bool AddTransition(UEdGraph* Machine, UAnimStateNode* From, UAnimStateNode* To, int32 Priority, float Crossfade, FName Rule, float AutoTriggerTime = 0.f)
{
	FGraphNodeCreator<UAnimStateTransitionNode> Creator(*Machine);
	UAnimStateTransitionNode* Transition = Creator.CreateNode();
	Transition->CrossfadeDuration = Crossfade;
	Transition->PriorityOrder = Priority;
	Transition->LogicType = ETransitionLogicType::TLT_StandardBlend;
	Creator.Finalize();
	Transition->CreateConnections(From, To);
	if (Rule.IsNone())
	{
		Transition->bAutomaticRuleBasedOnSequencePlayerInState = true;
		Transition->AutomaticRuleTriggerTime = AutoTriggerTime;
		Transition->SyncGroupNameToRequireValidMarkersRule = NAME_None;
		return true;
	}
	UEdGraph* Graph = Transition->GetBoundGraph();
	UAnimGraphNode_TransitionResult* Result = Graph ? Find<UAnimGraphNode_TransitionResult>(Graph) : nullptr;
	UK2Node_VariableGet* Bool = Graph ? Getter(Graph, Rule, -300, 0) : nullptr;
	return Result && Bool && Link(Graph, Bool->FindPin(Rule), Result->FindPin(TEXT("bCanEnterTransition")));
}

void ShowPin(UAnimGraphNode_Base* Node, FName Property)
{
	for (int32 Index = 0; Index < Node->ShowPinForProperties.Num(); ++Index)
		if (Node->ShowPinForProperties[Index].PropertyName == Property) Node->SetPinVisibility(true, Index);
}

/** 资产绑定到 AnimInstance 属性的序列播放器。 */
UAnimGraphNode_SequencePlayer* AddSequencePlayer(UEdGraph* Graph, FName Property, bool bLoop, int32 X, int32 Y, bool& bLinked)
{
	FGraphNodeCreator<UAnimGraphNode_SequencePlayer> Creator(*Graph);
	UAnimGraphNode_SequencePlayer* Player = Creator.CreateNode();
	Player->NodePosX = X;
	Player->NodePosY = Y;
	Creator.Finalize();
	Player->Node.SetSequence(nullptr);
	Player->Node.SetLoopAnimation(bLoop);
	Player->NodeComment = FString::Printf(TEXT("WuwaJumpLand:%s"), *Property.ToString());
	ShowPin(Player, TEXT("Sequence"));
	Player->ReconstructNode();
	UK2Node_VariableGet* Asset = Getter(Graph, Property, X - 330, Y);
	bLinked &= Link(Graph, Asset->FindPin(Property), Player->FindPin(TEXT("Sequence")));
	return Player;
}

/** 资产绑定到 AnimInstance 属性、横轴读 JumpBlend 的混合空间播放器（不循环）。 */
UAnimGraphNode_BlendSpacePlayer* AddJumpBlendSpacePlayer(UEdGraph* Graph, FName Property, int32 X, int32 Y, bool& bLinked)
{
	FGraphNodeCreator<UAnimGraphNode_BlendSpacePlayer> Creator(*Graph);
	UAnimGraphNode_BlendSpacePlayer* Player = Creator.CreateNode();
	Player->NodePosX = X;
	Player->NodePosY = Y;
	Creator.Finalize();
	Player->Node.SetBlendSpace(nullptr);
	// UE 5.7 的运行时 setter 不写编辑器数据，直接设反射属性。
	if (const FBoolProperty* Loop = FindFProperty<FBoolProperty>(FAnimNode_BlendSpacePlayer::StaticStruct(), TEXT("bLoop")))
		Loop->SetPropertyValue_InContainer(&Player->Node, false);
	else bLinked = false;
	Player->NodeComment = FString::Printf(TEXT("WuwaJumpLand:%s"), *Property.ToString());
	ShowPin(Player, TEXT("BlendSpace"));
	Player->ReconstructNode();
	UK2Node_VariableGet* Asset = Getter(Graph, Property, X - 330, Y);
	UK2Node_VariableGet* Blend = Getter(Graph, TEXT("JumpBlend"), X - 330, Y + 80);
	bLinked &= Link(Graph, Asset->FindPin(Property), Player->FindPin(TEXT("BlendSpace")));
	bLinked &= Link(Graph, Blend->FindPin(TEXT("JumpBlend")), Player->FindPin(TEXT("X")));
	return Player;
}

UAnimGraphNode_BlendListByBool* Branch(UEdGraph* Graph, FName Property, UEdGraphNode* TruePose, UEdGraphNode* FalsePose, int32 X, int32 Y, bool& bLinked)
{
	FGraphNodeCreator<UAnimGraphNode_BlendListByBool> Creator(*Graph);
	UAnimGraphNode_BlendListByBool* Node = Creator.CreateNode();
	Node->NodePosX = X;
	Node->NodePosY = Y;
	Creator.Finalize();
	UK2Node_VariableGet* Bool = Getter(Graph, Property, X - 220, Y - 90);
	bLinked &= Link(Graph, Bool->FindPin(Property), Node->FindPin(TEXT("bActiveValue")));
	bLinked &= Link(Graph, PoseOutput(TruePose), Node->FindPin(TEXT("BlendPose_0")));
	bLinked &= Link(Graph, PoseOutput(FalsePose), Node->FindPin(TEXT("BlendPose_1")));
	// 选择在状态入场时已经锁定，不需要淡入淡出。
	for (UEdGraphPin* Pin : Node->Pins)
		if (Pin->PinName.ToString().StartsWith(TEXT("BlendTime_"))) Pin->DefaultValue = TEXT("0.0");
	return Node;
}

bool LinkResult(UAnimStateNode* State, UEdGraphNode* Pose)
{
	UEdGraph* Graph = State->BoundGraph;
	UAnimGraphNode_StateResult* Result = Find<UAnimGraphNode_StateResult>(Graph);
	return Result && Link(Graph, PoseOutput(Pose), Result->FindPin(TEXT("Result")));
}
}
#endif

FString UWuwaRoleJumpEditorLibrary::AddTemplateJumpLand(UAnimBlueprint* Blueprint)
{
#if WITH_EDITOR
	using namespace WuwaRoleJumpEditor;
	if (!Defaults(Blueprint)) return TEXT("ERROR: Expected an animation Blueprint derived from WuwaAnimInstance.");
	UEdGraph* Machine = FindLocomotion(Blueprint);
	if (!Machine) return TEXT("ERROR: AnimGraph state machine Locmotion not found.");
	if (FindState(Machine, WuwaJumpLand::JumpStart)) return TEXT("OK: JumpStart already exists; no edits.");
	UAnimStateNode* Ground = FindState(Machine, WuwaJumpLand::Ground);
	if (!Ground) return TEXT("ERROR: Locmotion.Ground not found.");

	Blueprint->Modify();
	Machine->Modify();
	const int32 X = Ground->NodePosX, Y = Ground->NodePosY;
	UAnimStateNode* JumpStart = AddState(Machine, WuwaJumpLand::JumpStart, X + 420, Y - 260, true);
	UAnimStateNode* Fall = AddState(Machine, WuwaJumpLand::Fall, X + 420, Y + 260, true);
	UAnimStateNode* Land = AddState(Machine, WuwaJumpLand::Land, X + 840, Y, true);
	if (!JumpStart || !Fall || !Land) return TEXT("ERROR: Could not name a new state (name clash); discard unsaved changes.");
	bool bLinked = true;

	// 起跳：原作 跳跃状态机（状态选择 → 左 / 右脚起跳 → 跳跃空中循环 → Fail）。选脚在请求时锁定，用一个分支代替 状态选择。
	{
		UEdGraph* Graph = JumpStart->BoundGraph;
		FGraphNodeCreator<UAnimGraphNode_StateMachine> Creator(*Graph);
		UAnimGraphNode_StateMachine* Inner = Creator.CreateNode();
		Inner->NodePosX = -300;
		Creator.Finalize();
		UEdGraph* JumpMachine = Inner->EditorStateMachineGraph;
		FBlueprintEditorUtils::RenameGraph(JumpMachine, WuwaJumpLand::JumpMachine.ToString());
		if (JumpMachine->GetFName() != WuwaJumpLand::JumpMachine) return TEXT("ERROR: Could not name SM_Jump; discard unsaved changes.");
		bLinked &= LinkResult(JumpStart, Inner);

		UAnimStateNode* Takeoff = AddState(JumpMachine, WuwaJumpLand::JumpTakeoff, 200, 0, true);
		UAnimStateNode* Loop = AddState(JumpMachine, WuwaJumpLand::JumpLoop, 600, 0, true);
		UAnimStateNode* Fail = AddState(JumpMachine, WuwaJumpLand::JumpFail, 1000, 0, true);
		if (!Takeoff || !Loop || !Fail) return TEXT("ERROR: Could not name a SM_Jump state; discard unsaved changes.");
		UAnimStateEntryNode* Entry = Find<UAnimStateEntryNode>(JumpMachine);
		bLinked &= Entry && Link(JumpMachine, Entry->GetOutputPin(), Takeoff->GetInputPin());

		UEdGraph* TakeoffGraph = Takeoff->BoundGraph;
		UAnimGraphNode_BlendSpacePlayer* Right = AddJumpBlendSpacePlayer(TakeoffGraph, TEXT("JumpRightBlendSpace"), -700, -150, bLinked);
		UAnimGraphNode_BlendSpacePlayer* Left = AddJumpBlendSpacePlayer(TakeoffGraph, TEXT("JumpLeftBlendSpace"), -700, 150, bLinked);
		bLinked &= LinkResult(Takeoff, Branch(TakeoffGraph, TEXT("bJumpRightFoot"), Right, Left, -250, 0, bLinked));
		bLinked &= LinkResult(Loop, AddSequencePlayer(Loop->BoundGraph, TEXT("JumpLoopSequence"), false, -400, 0, bLinked));
		bLinked &= LinkResult(Fail, AddSequencePlayer(Fail->BoundGraph, TEXT("JumpFailSequence"), true, -400, 0, bLinked));
		// 原作：起跳剩余 < 0.05 → 空中循环（混合 1.0）；空中循环播完 → Fail（混合 1.0）。
		bLinked &= AddTransition(JumpMachine, Takeoff, Loop, 1, 1.f, NAME_None, 0.05f);
		bLinked &= AddTransition(JumpMachine, Loop, Fail, 1, 1.f, NAME_None, 0.f);
	}
	bLinked &= LinkResult(Fall, AddSequencePlayer(Fall->BoundGraph, TEXT("FallSequence"), true, -400, 0, bLinked));
	{
		UEdGraph* Graph = Land->BoundGraph;
		UAnimGraphNode_SequencePlayer* Heavy = AddSequencePlayer(Graph, TEXT("LandHeavySequence"), false, -700, -150, bLinked);
		UAnimGraphNode_SequencePlayer* Light = AddSequencePlayer(Graph, TEXT("LandLightSequence"), false, -700, 150, bLinked);
		bLinked &= LinkResult(Land, Branch(Graph, TEXT("bLandHeavy"), Heavy, Light, -250, 0, bLinked));
	}

	// 普通运动的过渡；经过原作 Conduit 的路径用最后一段的混合时间（地面 → 跳跃 → 起跳 = 0，落地中转 → 落地 = 0.1 / → 地面 = 0.2）。
	bLinked &= AddTransition(Machine, Ground, JumpStart, 0, 0.f, TEXT("bJumpRequested"));
	bLinked &= AddTransition(Machine, Ground, Fall, 1, 0.2f, TEXT("bAirFall"));
	bLinked &= AddTransition(Machine, JumpStart, Ground, 0, 0.2f, TEXT("bJumpToGround"));
	bLinked &= AddTransition(Machine, JumpStart, Land, 1, 0.1f, TEXT("bJumpToLand"));
	bLinked &= AddTransition(Machine, Fall, Ground, 0, 0.2f, TEXT("bFallToGround"));
	bLinked &= AddTransition(Machine, Fall, Land, 1, 0.1f, TEXT("bFallToLand"));
	bLinked &= AddTransition(Machine, Land, JumpStart, 0, 0.f, TEXT("bJumpRequested"));
	bLinked &= AddTransition(Machine, Land, Ground, 1, 0.2f, TEXT("bLandFinish"));
	bLinked &= AddTransition(Machine, Land, Ground, 2, 0.2f, TEXT("bLandExitHigh"));
	bLinked &= AddTransition(Machine, Land, Ground, 3, 0.25f, TEXT("bLandExitMid"));
	bLinked &= AddTransition(Machine, Land, Ground, 4, 0.3f, TEXT("bLandExitWalk"));
	bLinked &= AddTransition(Machine, Land, Ground, 5, 0.2f, TEXT("bLandExitLow"));

	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	if (!bLinked) return TEXT("ERROR: Jump/land pin connection failed; discard unsaved changes and inspect pin names.");
	const FString Error = Compile(Blueprint);
	return Error.IsEmpty() ? TEXT("OK: Locmotion + JumpStart(SM_Jump) / Fall / Land with 12 transitions; unsaved.") : Error;
#else
	return TEXT("ERROR: Editor only.");
#endif
}

FString UWuwaRoleJumpEditorLibrary::SetChildJumpLandAnimations(UAnimBlueprint* Blueprint, UBlendSpace* JumpLeft, UBlendSpace* JumpRight,
	UAnimSequence* JumpLoop, UAnimSequence* JumpFail, UAnimSequence* Fall, UAnimSequence* LandLight, UAnimSequence* LandHeavy)
{
#if WITH_EDITOR
	using namespace WuwaRoleJumpEditor;
	UWuwaAnimInstance* CDO = Defaults(Blueprint);
	if (!CDO) return TEXT("ERROR: Expected a Wuwa animation Blueprint.");
	const TPair<FName, UAnimationAsset*> Assets[] = {
		{ TEXT("JumpLeftBlendSpace"), JumpLeft }, { TEXT("JumpRightBlendSpace"), JumpRight }, { TEXT("JumpLoopSequence"), JumpLoop },
		{ TEXT("JumpFailSequence"), JumpFail }, { TEXT("FallSequence"), Fall }, { TEXT("LandLightSequence"), LandLight }, { TEXT("LandHeavySequence"), LandHeavy } };
	for (const auto& [Name, Asset] : Assets)
		if (!Asset || (Blueprint->TargetSkeleton && !Blueprint->TargetSkeleton->IsCompatibleForEditor(Asset->GetSkeleton())))
			return FString::Printf(TEXT("ERROR: %s missing or on an incompatible skeleton."), *Name.ToString());
	Blueprint->Modify();
	CDO->Modify();
	for (const auto& [Name, Asset] : Assets)
	{
		const FObjectPropertyBase* Property = FindFProperty<FObjectPropertyBase>(CDO->GetClass(), Name);
		if (!Property) return TEXT("ERROR: Native jump/land property not found.");
		Property->SetObjectPropertyValue_InContainer(CDO, Asset);
	}
	FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
	const FString Error = Compile(Blueprint);
	if (!Error.IsEmpty()) return Error;
	CDO = Defaults(Blueprint);
	for (const auto& [Name, Asset] : Assets)
	{
		const FObjectPropertyBase* Property = CDO ? FindFProperty<FObjectPropertyBase>(CDO->GetClass(), Name) : nullptr;
		if (!Property || Property->GetObjectPropertyValue_InContainer(CDO) != Asset)
			return TEXT("ERROR: Compiled child did not retain a jump/land default.");
	}
	return TEXT("OK: Seven jump/land defaults installed; unsaved.");
#else
	return TEXT("ERROR: Editor only.");
#endif
}

FString UWuwaRoleJumpEditorLibrary::AddGroundLandIntoMove(UAnimBlueprint* Blueprint)
{
#if WITH_EDITOR
	using namespace WuwaRoleJumpEditor;
	UEdGraph* Locomotion = Blueprint ? FindLocomotion(Blueprint) : nullptr;
	UAnimStateNode* Ground = Locomotion ? FindState(Locomotion, WuwaJumpLand::Ground) : nullptr;
	UEdGraph* GroundMachine = nullptr;
	if (Ground && Ground->BoundGraph)
		for (UEdGraphNode* Node : Ground->BoundGraph->Nodes)
			if (const UAnimGraphNode_StateMachine* Machine = Cast<UAnimGraphNode_StateMachine>(Node); Machine && Machine->EditorStateMachineGraph
				&& Machine->EditorStateMachineGraph->GetFName() == TEXT("SM_Ground"))
				GroundMachine = Machine->EditorStateMachineGraph;
	if (!GroundMachine) return TEXT("ERROR: SM_Ground not found under Locmotion/Ground.");
	UAnimStateNode* Idle = FindState(GroundMachine, TEXT("Idle"));
	UAnimStateNode* Move = FindState(GroundMachine, TEXT("Move"));
	if (!Idle || !Move) return TEXT("ERROR: SM_Ground has no Idle or Move state.");
	int32 FirstPriority = 0;
	for (UEdGraphNode* Node : GroundMachine->Nodes)
	{
		const UAnimStateTransitionNode* Transition = Cast<UAnimStateTransitionNode>(Node);
		if (!Transition || Transition->GetPreviousState() != Idle) continue;
		if (Transition->GetNextState() == Move) return TEXT("OK: Idle -> Move already exists; unchanged.");
		FirstPriority = FMath::Min(FirstPriority, Transition->PriorityOrder);
	}
	Blueprint->Modify();
	GroundMachine->Modify();
	if (!AddTransition(GroundMachine, Idle, Move, FirstPriority - 1, 0.2f, TEXT("bLandIntoMove")))
		return TEXT("ERROR: Idle -> Move rule connection failed; discard unsaved changes.");
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	const FString Error = Compile(Blueprint);
	return Error.IsEmpty() ? FString::Printf(TEXT("OK: SM_Ground Idle -> Move (bLandIntoMove, priority %d); unsaved."), FirstPriority - 1) : Error;
#else
	return TEXT("ERROR: Editor only.");
#endif
}

bool UWuwaRoleJumpEditorLibrary::ResampleBlendSpace(UBlendSpace* BlendSpace)
{
#if WITH_EDITOR
	if (!BlendSpace) return false;
	BlendSpace->Modify();
	BlendSpace->ValidateSampleData();
	BlendSpace->ResampleData();
	BlendSpace->MarkPackageDirty();
	return !BlendSpace->GetBlendSpaceData().IsEmpty();
#else
	return false;
#endif
}

UBlendSpace1D* UWuwaRoleJumpEditorLibrary::CreateJumpBlendSpace(const FString& AssetPath, UAnimSequence* Walk, UAnimSequence* Run)
{
#if WITH_EDITOR
	if (!Walk || !Run || !Walk->GetSkeleton() || Walk->GetSkeleton() != Run->GetSkeleton()) return nullptr;
	if (FPackageName::DoesPackageExist(AssetPath) || FindPackage(nullptr, *AssetPath)) return nullptr;
	UPackage* Package = CreatePackage(*AssetPath);
	UBlendSpace1D* BlendSpace = NewObject<UBlendSpace1D>(Package, *FPackageName::GetLongPackageAssetName(AssetPath), RF_Public | RF_Standalone | RF_Transactional);
	BlendSpace->SetSkeleton(Walk->GetSkeleton());
	// 原作 BlendParameters：DisplayName Walk/Run，Max 1，GridNum 2（Min 取默认 0）。BlendParameters 是受保护成员，走反射。
	const FStructProperty* Property = FindFProperty<FStructProperty>(UBlendSpace::StaticClass(), TEXT("BlendParameters"));
	if (!Property) return nullptr;
	FBlendParameter* Axis = Property->ContainerPtrToValuePtr<FBlendParameter>(BlendSpace, 0);
	Axis->DisplayName = TEXT("Walk/Run");
	Axis->Min = 0.f;
	Axis->Max = 1.f;
	Axis->GridNum = 2;
	BlendSpace->AddSample(Walk, FVector(0.f, 0.f, 0.f));
	BlendSpace->AddSample(Run, FVector(1.f, 0.f, 0.f));
	BlendSpace->ValidateSampleData();
	// 运行时插值用的 BlendSpaceData 要显式重建；不建的话播放器拿不到采样，时间不走、也没有根运动。
	BlendSpace->ResampleData();
	BlendSpace->PostEditChange();
	FAssetRegistryModule::AssetCreated(BlendSpace);
	Package->MarkPackageDirty();
	return BlendSpace;
#else
	return nullptr;
#endif
}
