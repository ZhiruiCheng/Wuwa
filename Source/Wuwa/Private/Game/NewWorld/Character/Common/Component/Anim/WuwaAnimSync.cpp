#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimInstance.h"

#include "Animation/AnimClassInterface.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimNode_StateMachine.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "AnimNodes/AnimNode_BlendSpacePlayer.h"
#include "UObject/UnrealType.h"

namespace WuwaAnimSync
{
// Only presentation values. Never copy resource references, skill flags or arbitrary BP properties.
static const FName BlendParameters[] = { TEXT("SpeedMix"), TEXT("TurnMix"), TEXT("WalkRunMix"),
	TEXT("StepSizeMix"), TEXT("MoveMix"), TEXT("LeanAmount"), TEXT("CachePercentFB"), TEXT("CachePercentLR"),
	TEXT("TargetPlayRate"), TEXT("StepProgress") };

float ResolvePlayerTime(const UAnimSequenceBase* Asset, const FWuwaAnimPlayerSnapshot& Snapshot)
{
	const float Length = Asset ? Asset->GetPlayLength() : 0.f;
	if (Length <= SMALL_NUMBER) return 0.f;
	// One-shot acceleration has its own clock, not a locomotion foot phase. Keep its end at Length.
	if (!Snapshot.bLooping)
		return FMath::IsFinite(Snapshot.NormalizedTime) ? FMath::Clamp(Snapshot.NormalizedTime, 0.f, 1.f) * Length : 0.f;
	if (const UAnimSequence* Sequence = Cast<UAnimSequence>(Asset))
	{
		const auto& Position = Snapshot.MarkerPosition;
		if (!Position.PreviousMarkerName.IsNone() && !Position.NextMarkerName.IsNone()
			&& FMath::IsFinite(Position.PositionBetweenMarkers) && Position.PositionBetweenMarkers >= 0.f
			&& Position.PositionBetweenMarkers <= 1.f)
		{
			// The engine's GetFirstMatchingPosFromMarkerSyncPos returns zero on failure and
			// does not handle the last->first interval. A loop handoff needs both cases explicit.
			const auto& Markers = Sequence->AuthoredSyncMarkers;
			for (int32 Index = 0; Index < Markers.Num(); ++Index)
			{
				const int32 Next = (Index + 1) % Markers.Num();
				if (Markers[Index].MarkerName != Position.PreviousMarkerName || Markers[Next].MarkerName != Position.NextMarkerName) continue;
				const float End = Markers[Next].Time + (Next == 0 ? Length : 0.f);
				if (End > Markers[Index].Time)
					return FMath::Fmod(FMath::Lerp(Markers[Index].Time, End, Position.PositionBetweenMarkers), Length);
			}
		}
	}
	return FMath::IsFinite(Snapshot.NormalizedTime) ? FMath::Clamp(Snapshot.NormalizedTime, 0.f, 1.f) * Length : 0.f;
}

float ResolveBlendSpaceTime(const FAnimNode_BlendSpacePlayerBase& Player, const FWuwaAnimPlayerSnapshot& Snapshot)
{
	const UBlendSpace* BlendSpace = Player.GetBlendSpace();
	TArray<FBlendSampleData> Samples;
	int32 Triangle = INDEX_NONE;
	if (BlendSpace && BlendSpace->GetSamplesFromBlendInput(Player.GetFilteredPosition(), Samples, Triangle, true))
	{
		const FBlendSampleData* Best = nullptr;
		for (const auto& Sample : Samples)
		{
			const UAnimSequence* Sequence = BlendSpace->GetBlendSample(Sample.SampleDataIndex).Animation;
			if (Sequence && !Sequence->AuthoredSyncMarkers.IsEmpty() && (!Best || Sample.GetClampedWeight() > Best->GetClampedWeight())) Best = &Sample;
		}
		if (Best)
		{
			const UAnimSequence* Sequence = BlendSpace->GetBlendSample(Best->SampleDataIndex).Animation;
			if (Sequence && Sequence->GetPlayLength() > SMALL_NUMBER)
				return ResolvePlayerTime(Sequence, Snapshot) / Sequence->GetPlayLength();
		}
	}
	return FMath::Clamp(Snapshot.NormalizedTime, 0.f, 1.f);
}

// A non-looping stop loses marker sync after its final contact. Resume at that
// contact, not at its unrelated normalized clip time (which includes the idle tail).
bool StopTailContact(const UWuwaAnimInstance& Instance, FMarkerSyncAnimPosition& Out)
{
	if (Instance.bRoleStopSprint) return false; // These clips have no authored contacts.
	const UAnimSequence* Stop = Instance.bRoleStopWalk
		? (Instance.bRoleStopLeft ? Instance.RoleStopWalkLeft.Get() : Instance.RoleStopWalkRight.Get())
		: (Instance.bRoleStopLeft ? Instance.RoleStopRunLeft.Get() : Instance.RoleStopRunRight.Get());
	if (!Stop || Stop->AuthoredSyncMarkers.Num() < 2) return false;
	const auto& Last = Stop->AuthoredSyncMarkers.Last();
	if (Last.MarkerName != TEXT("Left") && Last.MarkerName != TEXT("Right")) return false;
	for (TFieldIterator<FStructProperty> It(Instance.GetClass()); It; ++It)
	{
		if (!It->Struct->IsChildOf(FAnimNode_SequencePlayer::StaticStruct())) continue;
		const auto* Player = It->ContainerPtrToValuePtr<FAnimNode_SequencePlayer>(&Instance);
		if (Player->GetSequence() != Stop || Player->GetGroupName() != TEXT("Locomotion")
			|| Player->GetCachedBlendWeight() <= ZERO_ANIMWEIGHT_THRESH || Player->GetAccumulatedTime() < Last.Time) continue;
		Out = FMarkerSyncAnimPosition(Last.MarkerName,
			Last.MarkerName == TEXT("Left") ? FName(TEXT("Right")) : FName(TEXT("Left")), 0.f);
		return true;
	}
	return false;
}
}

/** The request crosses threads through the owning proxy; it never reads the outgoing UObject. */
struct FWuwaAnimSyncProxy final : FAnimInstanceProxy
{
	explicit FWuwaAnimSyncProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}
	FWuwaAnimSyncSnapshot Pending;
	virtual void Initialize(UAnimInstance* Instance) override
	{
		Pending = {};
		FAnimInstanceProxy::Initialize(Instance);
	}

	virtual void UpdateAnimationNode(const FAnimationUpdateContext& Context) override
	{
		const auto* Instance = static_cast<const UWuwaAnimInstance*>(GetAnimInstanceObject());
		const auto* Ground = GetStateMachineInstanceFromName(TEXT("SM_Ground"));
		const bool bWasStop = Ground && Ground->GetCurrentState() != INDEX_NONE
			&& Ground->GetStateInfo(Ground->GetCurrentState()).StateName == TEXT("Stop");
		FWuwaAnimPlayerSnapshot TailContact;
		const bool bResumeTail = bWasStop && Instance->LocomotionData.bStateGround && Instance->LocomotionData.bGroundMoveActive
			&& !GetSyncGroupPosition(TEXT("Locomotion")).IsValid()
			&& WuwaAnimSync::StopTailContact(*Instance, TailContact.MarkerPosition);
		const bool bRestore = Pending.IsValid() && Instance->LocomotionData.bStateGround && Instance->LocomotionData.bGroundMoveActive;
		if (!bRestore) Pending = {}; // Destination policy may reject movement after the snapshot was taken.
		if (bRestore)
		{
			// Outer first: entering Ground initializes its nested state machine.
			for (const auto& Names : { TPair<FName, FName>(TEXT("Locmotion"), TEXT("Ground")),
				TPair<FName, FName>(TEXT("SM_Ground"), Pending.GroundState) })
			{
				auto* Machine = const_cast<FAnimNode_StateMachine*>(GetStateMachineInstanceFromName(Names.Key));
				if (Machine && Machine->GetCurrentState() != INDEX_NONE)
				{
					const int32 State = Machine->GetStateIndex(Names.Value);
					if (State != INDEX_NONE) Machine->SetState(Context, State, false);
				}
			}
			for (const auto& Parameter : Pending.BlendParameters)
			{
				const FNumericProperty* Property = FindFProperty<FNumericProperty>(GetAnimInstanceObject()->GetClass(), Parameter.Key);
				if (Property && Property->IsFloatingPoint())
					Property->SetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(GetAnimInstanceObject()), Parameter.Value);
			}
		}

		FAnimInstanceProxy::UpdateAnimationNode(Context);
		const FName NewState = Ground && Ground->GetCurrentState() != INDEX_NONE
			? Ground->GetStateInfo(Ground->GetCurrentState()).StateName : NAME_None;
		if (!bRestore && bResumeTail && (NewState == TEXT("StartMove") || NewState == TEXT("Move")))
		{
			// Seed only re-entering leg loops, after graph initialization and before sync ticking.
			// The state transition and LocomotionStart additive retain their normal behavior.
			for (TFieldIterator<FStructProperty> It(Instance->GetClass()); It; ++It)
			{
				if (!It->Struct->IsChildOf(FAnimNode_AssetPlayerBase::StaticStruct())) continue;
				auto* Player = It->ContainerPtrToValuePtr<FAnimNode_AssetPlayerBase>(GetAnimInstanceObject());
				if (!Player->IsLooping() || Player->GetGroupName() != TEXT("Locomotion")
					|| Player->GetCachedBlendWeight() <= ZERO_ANIMWEIGHT_THRESH) continue;
				if (It->Struct->IsChildOf(FAnimNode_BlendSpacePlayerBase::StaticStruct()))
					Player->SetAccumulatedTime(WuwaAnimSync::ResolveBlendSpaceTime(*static_cast<FAnimNode_BlendSpacePlayerBase*>(Player), TailContact));
				else if (It->Struct->IsChildOf(FAnimNode_SequencePlayer::StaticStruct()))
					Player->SetAccumulatedTime(WuwaAnimSync::ResolvePlayerTime(static_cast<FAnimNode_SequencePlayer*>(Player)->GetSequence(), TailContact));
			}
		}
		if (bRestore)
		{
			// Players have initialized and selected the destination role's assets. Tick records hold
			// pointers to their time accumulators; seed now, before the engine ticks sync groups.
			for (const FWuwaAnimPlayerSnapshot& Entry : Pending.Players)
			{
				const FStructProperty* Property = FindFProperty<FStructProperty>(GetAnimInstanceObject()->GetClass(), Entry.NodeName);
				if (!Property) continue;
				if (Entry.bBlendSpace && Property->Struct->IsChildOf(FAnimNode_BlendSpacePlayerBase::StaticStruct()))
				{
					auto* Player = Property->ContainerPtrToValuePtr<FAnimNode_BlendSpacePlayerBase>(GetAnimInstanceObject());
					if (Player->IsLooping() && Player->GetCachedBlendWeight() > ZERO_ANIMWEIGHT_THRESH)
					{
						Player->SnapToPosition(Entry.BlendPosition);
						Player->SetAccumulatedTime(WuwaAnimSync::ResolveBlendSpaceTime(*Player, Entry));
					}
				}
				else if (!Entry.bBlendSpace && Property->Struct->IsChildOf(FAnimNode_SequencePlayer::StaticStruct()))
				{
					auto* Player = Property->ContainerPtrToValuePtr<FAnimNode_SequencePlayer>(GetAnimInstanceObject());
					if (Player->GetSequence() && Player->IsLooping() == Entry.bLooping && Player->GetCachedBlendWeight() > ZERO_ANIMWEIGHT_THRESH)
						Player->SetAccumulatedTime(WuwaAnimSync::ResolvePlayerTime(Player->GetSequence(), Entry));
				}
			}
			Pending = {}; // Never seek repeatedly on subsequent frames.
		}
	}
};

FAnimInstanceProxy* UWuwaAnimInstance::CreateAnimInstanceProxy() { return new FWuwaAnimSyncProxy(this); }
void UWuwaAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* InProxy) { delete static_cast<FWuwaAnimSyncProxy*>(InProxy); }

bool UWuwaAnimInstance::QueueRoleAnimation(const FWuwaAnimSyncSnapshot& Snapshot)
{
	check(IsInGameThread());
	FWuwaAnimSyncProxy& Proxy = GetProxyOnGameThread<FWuwaAnimSyncProxy>();
	Proxy.Pending = {};
	if (!Snapshot.IsValid() || !GetClass()->IsChildOf(Snapshot.GraphOwner.Get())) return false;
	if (Snapshot.GroundState != TEXT("Move") && Snapshot.GroundState != TEXT("StartMove")) return false;
	for (const FWuwaAnimPlayerSnapshot& Entry : Snapshot.Players)
	{
		const FStructProperty* Property = FindFProperty<FStructProperty>(GetClass(), Entry.NodeName);
		if (!Property || Property->GetOwnerClass() != Snapshot.GraphOwner.Get()
			|| !Property->Struct->IsChildOf(Entry.bBlendSpace ? FAnimNode_BlendSpacePlayerBase::StaticStruct() : FAnimNode_SequencePlayer::StaticStruct())) return false;
	}
	// NativeUpdateAnimation recomputes facts from the destination's restored movement.
	// Only seed the hysteresis memory here, not another role's UObject parameter object.
	LocomotionData.bHasMovingSpeed = Snapshot.Locomotion.bHasMovingSpeed;
	Proxy.Pending = Snapshot;
	return true;
}

FWuwaAnimSyncSnapshot UWuwaAnimInstance::CaptureRoleAnimation()
{
	check(IsInGameThread());
	const FAnimInstanceProxy& Proxy = GetProxyOnGameThread<FAnimInstanceProxy>();
	FWuwaAnimSyncSnapshot Result;
	if (!LocomotionData.bGroundMoveActive || !LocomotionData.bStateGround || IsAnyMontagePlaying()) return Result;
	const FAnimNode_StateMachine* Machine = Proxy.GetStateMachineInstanceFromName(TEXT("SM_Ground"));
	const IAnimClassInterface* Class = IAnimClassInterface::GetFromClass(GetClass());
	if (!Machine || !Class || Machine->GetCurrentState() == INDEX_NONE) return Result;
	Result.GroundState = Machine->GetStateInfo(Machine->GetCurrentState()).StateName;
	if (Result.GroundState != TEXT("Move") && Result.GroundState != TEXT("StartMove")) return {};
	const auto& Properties = Class->GetAnimNodeProperties();
	// The Move state consumes a cached pose whose BlendSpace lives in another graph.
	// Its PlayerNodeIndices therefore do not enumerate the actual locomotion players.
	for (const FStructProperty* Property : Properties)
	{
		const bool bBlendSpace = Property->Struct->IsChildOf(FAnimNode_BlendSpacePlayerBase::StaticStruct());
		if (!bBlendSpace && !Property->Struct->IsChildOf(FAnimNode_SequencePlayer::StaticStruct())) continue;
		const auto* Player = Property->ContainerPtrToValuePtr<FAnimNode_AssetPlayerBase>(this);
		const bool bLoop = Player->IsLooping() && Player->GetGroupName() == TEXT("Locomotion");
		const bool bStart = Result.GroundState == TEXT("StartMove") && !bBlendSpace && !Player->IsLooping()
			&& Player->GetGroupName() == TEXT("LocomotionStart");
		if ((!bLoop && !bStart) || !Player->GetAnimAsset() || Player->GetCachedBlendWeight() <= ZERO_ANIMWEIGHT_THRESH) continue;
		float NormalizedTime = Player->GetAccumulatedTime();
		if (!bBlendSpace)
		{
			const auto* Sequence = Cast<UAnimSequenceBase>(Player->GetAnimAsset());
			if (!Sequence || Sequence->GetPlayLength() <= SMALL_NUMBER) continue;
			NormalizedTime /= Sequence->GetPlayLength();
		}
		FWuwaAnimPlayerSnapshot& Entry = Result.Players.AddDefaulted_GetRef();
		Entry.NodeName = Property->GetFName();
		Entry.bBlendSpace = bBlendSpace;
		Entry.bLooping = Player->IsLooping();
		if (bBlendSpace) Entry.BlendPosition = Property->ContainerPtrToValuePtr<FAnimNode_BlendSpacePlayerBase>(this)->GetFilteredPosition();
		Entry.NormalizedTime = FMath::Clamp(NormalizedTime, 0.f, 1.f);
		if (bLoop) Entry.MarkerPosition = GetSyncGroupPosition(Player->GetGroupName());
		Result.GraphOwner = Property->GetOwnerClass();
	}
	Result.Locomotion = LocomotionData;
	for (FName Name : WuwaAnimSync::BlendParameters)
	{
		const FNumericProperty* Property = FindFProperty<FNumericProperty>(GetClass(), Name);
		if (Property && Property->IsFloatingPoint())
		{
			const double Value = Property->GetFloatingPointPropertyValue(Property->ContainerPtrToValuePtr<void>(this));
			if (FMath::IsFinite(Value)) Result.BlendParameters.Add(Name, Value);
		}
	}
	return Result;
}
