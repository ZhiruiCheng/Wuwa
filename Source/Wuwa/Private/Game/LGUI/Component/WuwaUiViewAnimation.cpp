#include "Game/LGUI/Component/WuwaUiViewAnimation.h"

#include "Core/ActorComponent/UICanvasGroup.h"
#include "Core/ActorComponent/UIItem.h"
#include "GameFramework/Actor.h"
#include "LTweenManager.h"

UWuwaUiViewAnimation::UWuwaUiViewAnimation()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWuwaUiViewAnimation::PlayShow(TFunction<void()> OnDone)
{
	Play(ShowEffect, ShowDuration, ShowEase, true, MoveTemp(OnDone));
}

void UWuwaUiViewAnimation::PlayHide(TFunction<void()> OnDone)
{
	Play(HideEffect, HideDuration, HideEase, false, MoveTemp(OnDone));
}

void UWuwaUiViewAnimation::Stop()
{
	if (Tweener)
	{
		Tweener->Kill(false);
		Tweener = nullptr;
	}
}

void UWuwaUiViewAnimation::Play(EWuwaUiAnimEffect Effect, float Duration, ELTweenEase Ease, bool bShow, TFunction<void()> OnDone)
{
	Stop();
	CaptureRestState();

	if (Effect == EWuwaUiAnimEffect::None || Duration <= 0.f)
	{
		Apply(Effect, 1.f);
		if (OnDone) { OnDone(); }
		return;
	}

	PlayingEffect = Effect;
	Apply(Effect, bShow ? 0.f : 1.f);

	TWeakObjectPtr<UWuwaUiViewAnimation> WeakThis(this);
	Tweener = ULTweenManager::To(this,
		FLTweenFloatGetterFunction::CreateWeakLambda(this, [this]() { return Progress; }),
		FLTweenFloatSetterFunction::CreateWeakLambda(this, [this](float Value) { Apply(PlayingEffect, Value); }),
		bShow ? 1.f : 0.f, Duration);
	if (!Tweener)
	{
		// 没有 LTween 管理器（比如没有 GameInstance 的世界）：直接跳到结束状态。
		Apply(Effect, 1.f);
		if (OnDone) { OnDone(); }
		return;
	}

	// 界面可能正好暂停了游戏或放慢了时间：动画按真实时间播。
	Tweener->SetEase(Ease)->SetAffectByTimeDilation(false)->SetAffectByGamePause(false);
	Tweener->OnComplete([WeakThis, Effect, bShow, OnDone]()
	{
		if (UWuwaUiViewAnimation* Self = WeakThis.Get())
		{
			Self->Tweener = nullptr;
			if (!bShow)
			{
				// 关闭动画播完：界面马上会被设成不显示，先恢复原样，下次打开从头播。
				Self->Apply(Effect, 1.f);
			}
		}
		if (OnDone) { OnDone(); }
	});
}

void UWuwaUiViewAnimation::Apply(EWuwaUiAnimEffect Effect, float InProgress)
{
	Progress = InProgress;
	UUIItem* RootItem = GetRootItem();
	if (!RootItem)
	{
		return;
	}

	const bool bFade = Effect == EWuwaUiAnimEffect::Fade || Effect == EWuwaUiAnimEffect::FadeScale;
	const bool bScale = Effect == EWuwaUiAnimEffect::Scale || Effect == EWuwaUiAnimEffect::FadeScale;
	if (bFade || InProgress >= 1.f)
	{
		if (UUICanvasGroup* Group = bFade ? FindOrAddCanvasGroup() : CanvasGroup.Get())
		{
			Group->SetAlpha(bFade ? InProgress : 1.f);
		}
	}
	RootItem->SetRelativeScale3D(bScale ? RestScale * FMath::Lerp(ScaleFrom, 1.f, InProgress) : RestScale);

	FVector2D Offset = FVector2D::ZeroVector;
	if (Effect == EWuwaUiAnimEffect::SlideFromBottom)
	{
		Offset.Y = -SlideDistance * (1.f - InProgress);
	}
	else if (Effect == EWuwaUiAnimEffect::SlideFromRight)
	{
		Offset.X = SlideDistance * (1.f - InProgress);
	}
	RootItem->SetAnchoredPosition(RestPosition + Offset);
}

void UWuwaUiViewAnimation::CaptureRestState()
{
	// 第一次播放前记下"完全显示"时的样子；之后每次播完都会回到它。
	if (bCapturedRestState)
	{
		return;
	}
	if (const UUIItem* RootItem = GetRootItem())
	{
		RestScale = RootItem->GetRelativeScale3D();
		RestPosition = RootItem->GetAnchoredPosition();
		bCapturedRestState = true;
	}
}

UUIItem* UWuwaUiViewAnimation::GetRootItem() const
{
	const AActor* Owner = GetOwner();
	return Owner ? Cast<UUIItem>(Owner->GetRootComponent()) : nullptr;
}

UUICanvasGroup* UWuwaUiViewAnimation::FindOrAddCanvasGroup()
{
	if (!CanvasGroup)
	{
		AActor* Owner = GetOwner();
		CanvasGroup = Owner ? Owner->FindComponentByClass<UUICanvasGroup>() : nullptr;
		if (!CanvasGroup && Owner)
		{
			// 透明度要作用到整棵子树：LGUI 用 UICanvasGroup，预制体里没有就运行时加一个。
			CanvasGroup = NewObject<UUICanvasGroup>(Owner, TEXT("WuwaUiAnimCanvasGroup"));
			CanvasGroup->RegisterComponent();
		}
	}
	return CanvasGroup;
}
