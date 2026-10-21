#include "Game/Module/Loading/WuwaLoadingView.h"

#include "Core/ActorComponent/UIText.h"
#include "Core/ActorComponent/UITexture.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "Game/Common/Resource/WuwaResourceSubsystem.h"
#include "Game/Module/Loading/WuwaLoadingController.h"
#include "Game/Module/Loading/WuwaLoadingModel.h"

namespace WuwaLoadingViewIndex
{
	constexpr int32 Background = 0;
	constexpr int32 Title = 1;
	constexpr int32 Description = 2;
	constexpr int32 BarFill = 3;
	constexpr int32 Percent = 4;
	constexpr int32 BarTrack = 5;
}

void UWuwaLoadingView::OnRegisterComponent()
{
	using namespace WuwaLoadingViewIndex;
	ComponentRegisterInfos = {
		{Background, UUITexture::StaticClass()},
		{Title, UUIText::StaticClass()},
		{Description, UUIText::StaticClass()},
		{BarFill, UUITexture::StaticClass()},
		{Percent, UUIText::StaticClass()},
		{BarTrack, UUIItem::StaticClass()},
	};
}
	
void UWuwaLoadingView::OnBeforeShow()
{
	DisplayedProgress = 0.f;
	HoldTimer = 0.f;
	ApplyShow();
	ApplyProgress(0.f);
	StopTicking();
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float DeltaSeconds)
	{
		return Tick(DeltaSeconds);
	}));
}

void UWuwaLoadingView::OnBeforeHide()
{
	StopTicking();
}

void UWuwaLoadingView::OnBeforeDestroy()
{
	StopTicking();
	if (BackgroundHandle.IsValid())
	{
		BackgroundHandle->CancelHandle();
		BackgroundHandle.Reset();
	}
}

void UWuwaLoadingView::StopTicking()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
}

void UWuwaLoadingView::ApplyShow()
{
	const UWuwaLoadingModel* Model = GetModel<UWuwaLoadingModel>();
	if (!Model)
	{
		return;
	}
	const FWuwaLoadingShowRow& Show = Model->GetShow();
	if (UUIText* Title = GetText(WuwaLoadingViewIndex::Title))
	{
		Title->SetText(Show.Title);
	}
	if (UUIText* Description = GetText(WuwaLoadingViewIndex::Description))
	{
		Description->SetText(Show.Description);
	}

	// 背景图几 MB，异步加载；回来之前显示预制体里的默认背景。
	if (Show.Background.IsNull())
	{
		return;
	}
	UWuwaResourceSubsystem* Resources = UWuwaResourceSubsystem::Get(this);
	if (!Resources)
	{
		return;
	}
	const TSoftObjectPtr<UTexture2D> Background = Show.Background;
	TWeakObjectPtr<UWuwaLoadingView> WeakThis(this);
	BackgroundHandle = Resources->LoadAsync({Background.ToSoftObjectPath()}, TEXT("LoadingBackground"), [WeakThis, Background]()
	{
		UWuwaLoadingView* Self = WeakThis.Get();
		if (!Self)
		{
			return;
		}
		Self->BackgroundHandle.Reset();
		UUITexture* Image = Self->GetComponentAt<UUITexture>(WuwaLoadingViewIndex::Background);
		if (Image && Background.Get())
		{
			Image->SetTexture(Background.Get());
		}
	}, UWuwaResourceSubsystem::HighPriority);
}

void UWuwaLoadingView::ApplyProgress(float Progress)
{
	const UUIItem* Track = GetItem(WuwaLoadingViewIndex::BarTrack);
	if (UUITexture* Fill = GetComponentAt<UUITexture>(WuwaLoadingViewIndex::BarFill))
	{
		Fill->SetWidth(Track ? Track->GetWidth() * Progress : 0.f);
		Fill->SetIsUIActive(Progress > 0.f);
	}
	if (UUIText* Percent = GetText(WuwaLoadingViewIndex::Percent))
	{
		Percent->SetText(FText::FromString(FString::Printf(TEXT("%d %%"), FMath::FloorToInt(Progress * 100.f))));
	}
}

bool UWuwaLoadingView::Tick(float DeltaSeconds)
{
	const UWuwaLoadingModel* Model = GetModel<UWuwaLoadingModel>();
	if (!Model)
	{
		return true;
	}
	const float Target = Model->GetTargetProgress();
	if (DisplayedProgress < Target)
	{
		const float Step = FMath::Max(MinSpeedPerSecond * DeltaSeconds, (Target - DisplayedProgress) * CatchUpRate * DeltaSeconds);
		DisplayedProgress = FMath::Min(Target, DisplayedProgress + Step);
		ApplyProgress(DisplayedProgress);
	}

	// 实际加载完成、显示也走到 100%：停一下让玩家看到满格，再关闭。
	if (Model->IsFinished() && DisplayedProgress >= 1.f)
	{
		HoldTimer += DeltaSeconds;
		if (HoldTimer >= HoldAtFullSeconds)
		{
			TickHandle.Reset();
			if (UWuwaLoadingController* Controller = GetController<UWuwaLoadingController>())
			{
				Controller->HandleLoadingViewFinished();
			}
			return false;
		}
	}
	return true;
}
