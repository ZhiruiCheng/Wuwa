#include "Game/LGUI/View/WuwaUiView.h"

#include "Components/AudioComponent.h"
#include "Core/Actor/UIBackgroundBlurActor.h"
#include "Core/ActorComponent/UIBackgroundBlur.h"
#include "Core/ActorComponent/UIItem.h"
#include "Engine/World.h"
#include "Game/LGUI/Core/WuwaUiCameraHandler.h"
#include "Game/LGUI/Core/WuwaUiManager.h"
#include "Kismet/GameplayStatics.h"
#include "LGUIBPLibrary.h"
#include "Sound/SoundBase.h"

void UWuwaUiView::Init(FName InViewName, int32 InViewId, const FWuwaUiViewConfig& InConfig, UUIItem* InLayerItem)
{
	ViewName = InViewName;
	ViewId = InViewId;
	Config = InConfig;
	LayerItem = InLayerItem;
}

void UWuwaUiView::Open(FWuwaUiDone OnDone)
{
	// 三步一次排进队列，而不是在回调里一步步接：
	// 这样中途插进来的 Close 一定排在 Show 后面，不会和 Start/Show 交错。
	CreateByPrefab(Config.Prefab, LayerItem.Get());
	Start();
	Show(MoveTemp(OnDone));
}

void UWuwaUiView::Prepare(FWuwaUiDone OnDone)
{
	CreateByPrefab(Config.Prefab, LayerItem.Get());
	Start(MoveTemp(OnDone));
}

void UWuwaUiView::Close(FWuwaUiDone OnDone)
{
	// 排队中从没创建过的界面（Stack/Float 的等待项）没有东西可隐藏。
	if (GetState() != EWuwaUiState::None)
	{
		Hide();
	}
	Destroy(MoveTemp(OnDone));
}

void UWuwaUiView::CloseMe()
{
	if (UWuwaUiManager* Manager = GetManager())
	{
		Manager->CloseViewById(ViewId);
		return;
	}
	Close();
}

// —— 打开效果 ——

void UWuwaUiView::OnShowStartImplement()
{
	// 第一次显示：打开音效、时间缩放、UI 相机。被栈上层盖住又露出来时不再重复。
	if (!bOpenEffectsApplied)
	{
		bOpenEffectsApplied = true;
		PlayUiSound(Config.OpenSound);
		if (UWuwaUiManager* Manager = GetManager())
		{
			if (Config.TimeDilation < 1.f)
			{
				Manager->PushTimeDilation(ViewId, Config.TimeDilation);
			}
			if (!Config.UiCameraMode.IsNone())
			{
				if (IWuwaUiCameraHandler* Camera = Manager->GetUiCameraHandler())
				{
					Camera->PushUiCamera(ViewId, ViewName, Config.UiCameraMode);
				}
			}
		}
	}
	// 模糊每次显示都要：被盖住时撤掉了。
	if (Config.bBlurBackground)
	{
		ShowBlur();
	}
}

void UWuwaUiView::OnAfterShowImplement()
{
	// 打开动画播完、完全挡住下面之后，才把下面各层藏起来。
	if (Config.bIsFullScreen)
	{
		SetFullScreenActive(true);
	}
}

void UWuwaUiView::OnHideStartImplement()
{
	// 开始关闭（动画还没播）就把下面各层恢复，关闭动画期间能看到下面。
	SetFullScreenActive(false);
	if (IsCloseRequested())
	{
		PlayUiSound(Config.CloseSound);
	}
}

void UWuwaUiView::OnAfterHideImplement()
{
	HideBlur();
}

void UWuwaUiView::OnBeforeDestroyImplement()
{
	// 不管怎么销毁（正常关闭、切地图立即销毁），生效过的效果都要撤销。
	HideBlur();
	SetFullScreenActive(false);
	if (bOpenEffectsApplied)
	{
		bOpenEffectsApplied = false;
		if (UWuwaUiManager* Manager = GetManager())
		{
			Manager->PopTimeDilation(ViewId);
			if (!Config.UiCameraMode.IsNone())
			{
				if (IWuwaUiCameraHandler* Camera = Manager->GetUiCameraHandler())
				{
					Camera->PopUiCamera(ViewId);
				}
			}
		}
	}
}

UWuwaUiManager* UWuwaUiView::GetManager() const
{
	return Cast<UWuwaUiManager>(GetOuter());
}

void UWuwaUiView::PlayUiSound(const TSoftObjectPtr<USoundBase>& Sound)
{
	if (Sound.IsNull())
	{
		return;
	}
	USoundBase* Loaded = Sound.LoadSynchronous();
	UAudioComponent* Component = Loaded ? UGameplayStatics::CreateSound2D(this, Loaded) : nullptr;
	if (Component)
	{
		// UI 音效：游戏暂停、时间缩放时照常播放。
		Component->bIsUISound = true;
		Component->Play();
		LastPlayedSound = Component;
	}
}

void UWuwaUiView::ShowBlur()
{
	UUIItem* Layer = LayerItem.Get();
	UUIItem* RootItem = GetRootItem();
	UWorld* World = GetWorld();
	if (BlurActor.IsValid() || !Layer || !RootItem || !World)
	{
		return;
	}

	// 放在同一层里、紧挨着界面的前面（层级越靠前画得越早 = 在界面后面），铺满整层（整个屏幕）。
	AUIBackgroundBlurActor* Blur = World->SpawnActor<AUIBackgroundBlurActor>();
	UUIBackgroundBlur* BlurItem = Blur ? Blur->GetUIBackgroundBlur() : nullptr;
	if (!BlurItem)
	{
		return;
	}
	BlurItem->AttachToComponent(Layer, FAttachmentTransformRules::KeepRelativeTransform);
	BlurItem->SetAnchorMin(FVector2D(0.f, 0.f));
	BlurItem->SetAnchorMax(FVector2D(1.f, 1.f));
	BlurItem->SetSizeDelta(FVector2D::ZeroVector);
	BlurItem->SetAnchoredPosition(FVector2D::ZeroVector);
	BlurItem->SetBlurStrength(Config.BlurStrength);
	BlurItem->SetHierarchyIndex(RootItem->GetHierarchyIndex());
	BlurActor = Blur;
}

void UWuwaUiView::HideBlur()
{
	AActor* Blur = BlurActor.Get();
	const UWorld* World = Blur ? Blur->GetWorld() : nullptr;
	// 世界正在拆除（切地图、结束 PIE）时交给世界去销毁，和界面根节点一样。
	if (Blur && World && !World->bIsTearingDown)
	{
		ULGUIBPLibrary::DestroyActorWithHierarchy(Blur, true);
	}
	BlurActor.Reset();
}

void UWuwaUiView::SetFullScreenActive(bool bActive)
{
	if (bFullScreenActive == bActive)
	{
		return;
	}
	bFullScreenActive = bActive;
	if (UWuwaUiManager* Manager = GetManager())
	{
		Manager->SetFullScreenView(ViewId, Config.Layer, bActive);
	}
}
