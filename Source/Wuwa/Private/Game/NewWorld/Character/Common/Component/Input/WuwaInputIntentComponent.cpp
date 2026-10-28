#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputIntentComponent.h"

#include "InputAction.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Core/Tag/WuwaGameTags.h"

UWuwaInputIntentComponent::UWuwaInputIntentComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

#pragma region 写入

void UWuwaInputIntentComponent::RecordInputEvent(const FWuwaInputEvent& InputEvent)
{
	check(IsInGameThread());
	if (!InputEvent.InputTag.IsValid())
	{
		return;
	}

	if (InputEvent.InputTag == FWuwaGameTags::Get().Player_Common_Movement_Move)
	{
		// 移动是连续轴：按下/持续时记录当前值，松开/取消时归零。
		const bool bEnded = InputEvent.Phase == EWuwaInputPhase::Released || InputEvent.Phase == EWuwaInputPhase::Canceled;
		SetMoveAxis(bEnded ? FVector2D::ZeroVector : InputEvent.Value.Get<FVector2D>());
		return;
	}
	RecordActionInput(InputEvent);
}

void UWuwaInputIntentComponent::SetMoveAxis(FVector2D NewAxis)
{
	MoveAxis = NewAxis;
}

void UWuwaInputIntentComponent::RecordActionInput(const FWuwaInputEvent& InputEvent)
{
	const UWorld* World = GetWorld();
	if (!IsValid(InputEvent.SourceAction) || !World)
	{
		return;
	}

	// 一个 Tag 可以由多个 Action 触发（比如键盘和手柄），所以记录所有按住它的来源。
	const TWeakObjectPtr<const UInputAction> Source(InputEvent.SourceAction.Get());
	if (InputEvent.Phase == EWuwaInputPhase::Pressed)
	{
		FHeldInput& State = HeldInputs.FindOrAdd(InputEvent.InputTag);
		for (auto It = State.ActiveSources.CreateIterator(); It; ++It)
		{
			if (!It->IsValid())
			{
				It.RemoveCurrent();
			}
		}
		if (State.ActiveSources.IsEmpty())
		{
			const double Now = World->GetTimeSeconds();
			const double Timestamp = FMath::IsFinite(InputEvent.Timestamp) && InputEvent.Timestamp > 0.0 ? InputEvent.Timestamp : Now;
			State.PressedAt = FMath::Clamp(Timestamp, 0.0, Now);
		}
		State.ActiveSources.Add(Source);
	}
	else if (InputEvent.Phase == EWuwaInputPhase::Released || InputEvent.Phase == EWuwaInputPhase::Canceled)
	{
		if (FHeldInput* State = HeldInputs.Find(InputEvent.InputTag))
		{
			State->ActiveSources.Remove(Source);
			if (State->ActiveSources.IsEmpty())
			{
				HeldInputs.Remove(InputEvent.InputTag);
			}
		}
	}
	// Triggered（持续采样）和 Held（按满阈值）都不改变按住状态：不重新计时，也不让清空后的输入复活。
}

void UWuwaInputIntentComponent::ResetInputs()
{
	HeldInputs.Empty();
	MoveAxis = FVector2D::ZeroVector;
}

#pragma endregion

#pragma region 读取

FWuwaMoveIntent UWuwaInputIntentComponent::GetMoveIntent() const
{
	FWuwaMoveIntent Result;
	Result.Axis = MoveAxis;
	const float Threshold = FMath::Clamp(MoveInputThreshold, 0.f, 1.f);
	Result.bHasInput = MoveAxis.SizeSquared() > FMath::Square(Threshold);
	if (Result.bHasInput)
	{
		// 控制朝向是游戏操作朝向，镜头震动和演出偏移不会改变移动意图；没有控制器时为零旋转。
		const APawn* OwnerPawn = Cast<APawn>(GetOwner());
		const FRotator YawOnly(0.f, OwnerPawn ? OwnerPawn->GetControlRotation().Yaw : 0.f, 0.f);
		const FRotationMatrix Basis(YawOnly);
		const FVector Direction = Basis.GetUnitAxis(EAxis::X) * MoveAxis.Y + Basis.GetUnitAxis(EAxis::Y) * MoveAxis.X;
		const FVector2D Planar = FVector2D(Direction.X, Direction.Y).GetSafeNormal();
		Result.WorldDirection = FVector(Planar.X, Planar.Y, 0.f);
	}
	return Result;
}

FWuwaInputActionState UWuwaInputIntentComponent::GetActionState(FGameplayTag InputTag) const
{
	FWuwaInputActionState Result;
	const FHeldInput* State = HeldInputs.Find(InputTag);
	const UWorld* World = GetWorld();
	if (!State || !World)
	{
		return Result;
	}
	for (const TWeakObjectPtr<const UInputAction>& Source : State->ActiveSources)
	{
		if (Source.IsValid())
		{
			Result.bHeld = true;
			Result.HeldSeconds = static_cast<float>(FMath::Max(0.0, World->GetTimeSeconds() - State->PressedAt));
			break;
		}
	}
	return Result;
}

FWuwaPlayerInputState UWuwaInputIntentComponent::GetPlayerInputState() const
{
	FWuwaPlayerInputState State;
	const FWuwaMoveIntent Move = GetMoveIntent();
	State.MoveAxis = Move.Axis;
	State.bHasMoveInput = Move.bHasInput;
	State.MoveWorldDirection = Move.WorldDirection;
	const FWuwaInputActionState SprintInput = GetActionState(FWuwaGameTags::Get().Input_Combat_Dash);
	State.bSprintHeld = SprintInput.bHeld;
	State.SprintHeldSeconds = SprintInput.HeldSeconds;
	return State;
}

#pragma endregion

void UWuwaInputIntentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetInputs();
	Super::EndPlay(EndPlayReason);
}
