#include "Game/LGUI/Component/WuwaUiTweenGroup.h"

#include "Core/ActorComponent/UIBaseRenderable.h"
#include "Core/ActorComponent/UICanvasGroup.h"
#include "Core/ActorComponent/UIItem.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/Actor.h"
#include "GeometryModifier/UIEffectTextAnimation.h"
#include "Misc/App.h"

namespace
{
	/** 缓动后的进度（0~1 的输入，LTween 同一套公式）。 */
	float Ease(ELTweenEase Ease, const UCurveFloat* Curve, float T, float D)
	{
		switch (Ease)
		{
		case ELTweenEase::InQuad: return ULTweener::InQuad(1.f, 0.f, T, D);
		case ELTweenEase::OutQuad: return ULTweener::OutQuad(1.f, 0.f, T, D);
		case ELTweenEase::InOutQuad: return ULTweener::InOutQuad(1.f, 0.f, T, D);
		case ELTweenEase::InCubic: return ULTweener::InCubic(1.f, 0.f, T, D);
		case ELTweenEase::OutCubic: return ULTweener::OutCubic(1.f, 0.f, T, D);
		case ELTweenEase::InOutCubic: return ULTweener::InOutCubic(1.f, 0.f, T, D);
		case ELTweenEase::InQuart: return ULTweener::InQuart(1.f, 0.f, T, D);
		case ELTweenEase::OutQuart: return ULTweener::OutQuart(1.f, 0.f, T, D);
		case ELTweenEase::InOutQuart: return ULTweener::InOutQuart(1.f, 0.f, T, D);
		case ELTweenEase::InSine: return ULTweener::InSine(1.f, 0.f, T, D);
		case ELTweenEase::OutSine: return ULTweener::OutSine(1.f, 0.f, T, D);
		case ELTweenEase::InOutSine: return ULTweener::InOutSine(1.f, 0.f, T, D);
		case ELTweenEase::InExpo: return ULTweener::InExpo(1.f, 0.f, T, D);
		case ELTweenEase::OutExpo: return ULTweener::OutExpo(1.f, 0.f, T, D);
		case ELTweenEase::InOutExpo: return ULTweener::InOutExpo(1.f, 0.f, T, D);
		case ELTweenEase::InCirc: return ULTweener::InCirc(1.f, 0.f, T, D);
		case ELTweenEase::OutCirc: return ULTweener::OutCirc(1.f, 0.f, T, D);
		case ELTweenEase::InOutCirc: return ULTweener::InOutCirc(1.f, 0.f, T, D);
		case ELTweenEase::InElastic: return ULTweener::InElastic(1.f, 0.f, T, D);
		case ELTweenEase::OutElastic: return ULTweener::OutElastic(1.f, 0.f, T, D);
		case ELTweenEase::InOutElastic: return ULTweener::InOutElastic(1.f, 0.f, T, D);
		case ELTweenEase::InBack: return ULTweener::InBack(1.f, 0.f, T, D);
		case ELTweenEase::OutBack: return ULTweener::OutBack(1.f, 0.f, T, D);
		case ELTweenEase::InOutBack: return ULTweener::InOutBack(1.f, 0.f, T, D);
		case ELTweenEase::InBounce: return ULTweener::InBounce(1.f, 0.f, T, D);
		case ELTweenEase::OutBounce: return ULTweener::OutBounce(1.f, 0.f, T, D);
		case ELTweenEase::InOutBounce: return ULTweener::InOutBounce(1.f, 0.f, T, D);
		case ELTweenEase::CurveFloat:
			if (Curve) return Curve->GetFloatValue(D > KINDA_SMALL_NUMBER ? T / D : 1.f);
			return ULTweener::Linear(1.f, 0.f, T, D);
		default: return ULTweener::Linear(1.f, 0.f, T, D);
		}
	}
}

UWuwaUiTweenGroup::UWuwaUiTweenGroup()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.bTickEvenWhenPaused = true;
}

void UWuwaUiTweenGroup::Play()
{
	States.SetNum(Tweens.Num());
	for (FState& State : States) State = FState();
	Elapsed = 0.f;
	bPlaying = Tweens.Num() > 0;
	SetComponentTickEnabled(bPlaying);
	// 第 0 帧就应用（没有延迟的补间立刻到起始值，同 LTween 开始时立即更新）
	Advance(0.f);
}

void UWuwaUiTweenGroup::Stop()
{
	bPlaying = false;
	SetComponentTickEnabled(false);
}

void UWuwaUiTweenGroup::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	// 不受时间膨胀影响：用真实帧时间
	Advance(static_cast<float>(FApp::GetDeltaTime()));
}

void UWuwaUiTweenGroup::Advance(float DeltaSeconds)
{
	if (!bPlaying) return;
	Elapsed += DeltaSeconds * Speed;
	bool bAllDone = true;
	for (int32 Index = 0; Index < Tweens.Num(); ++Index)
	{
		FState& State = States[Index];
		if (State.bFinished) continue;
		const FWuwaUiTween& Tween = Tweens[Index];
		FVector4 Value;
		bool bFinished = false;
		if (!Evaluate(Tween, Elapsed, Value, bFinished))
		{
			bAllDone = false;
			continue;
		}
		if (!State.bStarted)
		{
			State.bStarted = true;
			for (const FWuwaUiTweenEvent& Event : Tween.OnStart) Apply(Event, Value);
		}
		for (const FWuwaUiTweenEvent& Event : Tween.OnUpdate) Apply(Event, Value);
		if (bFinished)
		{
			State.bFinished = true;
			for (const FWuwaUiTweenEvent& Event : Tween.OnComplete) Apply(Event, Value);
		}
		else
		{
			bAllDone = false;
		}
	}
	if (bAllDone) Stop();
}

bool UWuwaUiTweenGroup::Evaluate(const FWuwaUiTween& Tween, float Time, FVector4& OutValue, bool& bOutFinished)
{
	bOutFinished = false;
	const float Local = Time - Tween.StartDelay;
	if (Local < 0.f) return false;
	const float Duration = FMath::Max(Tween.Duration, 0.f);
	// 循环：Once 一轮；其他按 LoopCount（-1 无限）
	const int32 Cycles = Tween.Loop == ELTweenLoop::Once ? 1 : Tween.LoopCount;
	int32 Cycle = Duration > KINDA_SMALL_NUMBER ? FMath::FloorToInt(Local / Duration) : 0;
	float T = Duration > KINDA_SMALL_NUMBER ? Local - Cycle * Duration : Duration;
	if (Cycles > 0 && (Cycle >= Cycles || Duration <= KINDA_SMALL_NUMBER))
	{
		Cycle = FMath::Max(Cycles - 1, 0);
		T = Duration;
		bOutFinished = true;
	}
	float Progress = Ease(Tween.Ease, Tween.Curve, T, Duration);
	FVector4 From = Tween.From, To = Tween.To;
	if (Tween.Loop == ELTweenLoop::Yoyo && Cycle % 2 == 1)
	{
		Swap(From, To);
	}
	else if (Tween.Loop == ELTweenLoop::Incremental && Cycle > 0)
	{
		const FVector4 Change = To - From;
		From += Change * static_cast<float>(Cycle);
		To += Change * static_cast<float>(Cycle);
	}
	OutValue = From + (To - From) * Progress;
	return true;
}

void UWuwaUiTweenGroup::Apply(const FWuwaUiTweenEvent& Event, const FVector4& TweenValue)
{
	AActor* Target = Event.Target.Get();
	if (!IsValid(Target)) return;
	const FVector4 Value = Event.bUseTweenValue ? TweenValue * Event.ValueScale : Event.FixedValue;
	UUIItem* Item = Cast<UUIItem>(Target->GetRootComponent());
	switch (Event.Action)
	{
	case EWuwaUiTweenAction::SelectorOffset:
	{
		TInlineComponentArray<UUIEffectTextAnimation*> Animations(Target);
		for (UUIEffectTextAnimation* Animation : Animations)
		{
			if (Animation->GetFName() == Event.Component) Animation->SetSelectorOffset(Value.X);
		}
		break;
	}
	case EWuwaUiTweenAction::Alpha:
		if (UUIBaseRenderable* Renderable = Cast<UUIBaseRenderable>(Item))
		{
			Renderable->SetAlpha(Value.X);
		}
		else if (UUICanvasGroup* Group = Target->FindComponentByClass<UUICanvasGroup>())
		{
			Group->SetAlpha(Value.X);
		}
		break;
	case EWuwaUiTweenAction::Scale:
		if (Item) Item->SetRelativeScale3D(FVector(1.0, Value.X, Value.Y));   // LGUI3 界面平面：Y 水平、Z 竖直
		break;
	case EWuwaUiTweenAction::AnchorOffsetX:
		if (Item) Item->SetAnchoredPosition(FVector2D(Value.X, Item->GetAnchoredPosition().Y));
		break;
	case EWuwaUiTweenAction::AnchorOffsetY:
		if (Item) Item->SetAnchoredPosition(FVector2D(Item->GetAnchoredPosition().X, Value.X));
		break;
	case EWuwaUiTweenAction::Color:
		if (UUIBaseRenderable* Renderable = Cast<UUIBaseRenderable>(Item))
		{
			Renderable->SetColor(FLinearColor(Value.X, Value.Y, Value.Z, Value.W).ToFColor(false));
		}
		break;
	case EWuwaUiTweenAction::Active:
		if (Item) Item->SetIsUIActive(Value.X != 0.0);
		break;
	}
}
