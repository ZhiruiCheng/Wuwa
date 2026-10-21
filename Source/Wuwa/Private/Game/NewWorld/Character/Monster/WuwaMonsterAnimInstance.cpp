#include "Game/NewWorld/Character/Monster/WuwaMonsterAnimInstance.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimSequenceBase.h"
#include "Animation/AnimNodeBase.h"
#include "Game/NewWorld/Character/Common/WuwaCharactorBase.h"

const FName FWuwaMonsterAnimInstanceProxy::SlotName(TEXT("DefaultSlot"));

void FWuwaMonsterAnimInstanceProxy::Initialize(UAnimInstance* InAnimInstance)
{
	FAnimInstanceProxy::Initialize(InAnimInstance);
	RegisterSlotNodeWithAnimInstance(SlotName);
}

void FWuwaMonsterAnimInstanceProxy::PreUpdate(UAnimInstance* InAnimInstance, float DeltaSeconds)
{
	FAnimInstanceProxy::PreUpdate(InAnimInstance, DeltaSeconds);
	const AWuwaCharactorBase* Owner = Cast<AWuwaCharactorBase>(InAnimInstance->GetOwningActor());
	Idle = Owner ? Owner->IdleAnimation.Get() : nullptr;
}

void FWuwaMonsterAnimInstanceProxy::Update(float DeltaSeconds)
{
	IdleTime += DeltaSeconds;
	// 同 FAnimNode_Slot::Update_AnyThread：取蒙太奇在这个插槽上的权重，并登记本节点权重
	GetSlotWeight(SlotName, SlotNodeWeight, SourceWeight, TotalNodeWeight);
	UpdateSlotNodeWeight(SlotName, SlotNodeWeight, 1.f);
}

void FWuwaMonsterAnimInstanceProxy::EvaluateIdle(FPoseContext& Output) const
{
	const UAnimSequence* Sequence = Cast<UAnimSequence>(Idle);
	const float Length = Sequence ? Sequence->GetPlayLength() : 0.f;
	if (!Sequence || Length <= 0.f)
	{
		Output.ResetToRefPose();
		return;
	}
	FAnimationPoseData PoseData(Output);
	Sequence->GetAnimationPose(PoseData, FAnimExtractContext(static_cast<double>(FMath::Fmod(IdleTime, Length)), false));
}

bool FWuwaMonsterAnimInstanceProxy::Evaluate(FPoseContext& Output)
{
	// 同 FAnimNode_Slot::Evaluate_AnyThread
	if (SlotNodeWeight <= ZERO_ANIMWEIGHT_THRESH)
	{
		EvaluateIdle(Output);
		return true;
	}
	FPoseContext Source(Output);
	if (SourceWeight > ZERO_ANIMWEIGHT_THRESH) EvaluateIdle(Source);
	else Source.ResetToRefPose();
	const FAnimationPoseData SourcePose(Source);
	FAnimationPoseData OutputPose(Output);
	SlotEvaluatePose(SlotName, SourcePose, SourceWeight, OutputPose, SlotNodeWeight, TotalNodeWeight);
	return true;
}
