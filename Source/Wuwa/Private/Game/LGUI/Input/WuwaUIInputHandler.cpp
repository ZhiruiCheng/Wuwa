#include "Game/LGUI/Input/WuwaUIInputHandler.h"

#include "Game/Common/WuwaGameTags.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Input/WuwaInputTypes.h"
#include "Game/LGUI/Core/WuwaUiManager.h"
#include "GameFramework/PlayerController.h"

UWuwaUIInputHandler::UWuwaUIInputHandler()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UWuwaUiManager* UWuwaUIInputHandler::GetUiManager() const
{
	const APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	return PlayerController ? UWuwaUiManager::Get(PlayerController) : nullptr;
}


bool UWuwaUIInputHandler::HandleWuwaInput_Implementation(const FWuwaInputEvent& InputEvent)
{
	if (InputEvent.InputTag == FWuwaGameTags::WuwaGamePlayTags.Player_UI_CloseUI)
	{
		HandleExitUI(InputEvent);
	}
	else if (InputEvent.InputTag == FWuwaGameTags::WuwaGamePlayTags.Player_UI_ShowMouse)
	{
		HandleShowMouseIA(InputEvent);
	}
	return false;
}

void UWuwaUIInputHandler::HandleExitUI(const FWuwaInputEvent& InputEvent)
{
	
}

void UWuwaUIInputHandler::HandleShowMouseIA(const FWuwaInputEvent& InputEvent)
{
	// 按住期间显示鼠标，松开（或被打断）时收回。按住时每帧的 Triggered 不用管。
	switch (InputEvent.Phase)
	{
	case EWuwaInputPhase::Pressed:
		SetFreeCursor(true);
		break;
	case EWuwaInputPhase::Released:
	case EWuwaInputPhase::Canceled:
		SetFreeCursor(false);
		break;
	default:
		break;
	}
}

void UWuwaUIInputHandler::SetFreeCursor(bool bFree)
{
	APlayerController* PlayerController = Cast<APlayerController>(GetOwner());
	if (!PlayerController || !PlayerController->IsLocalController() || bFreeCursor == bFree)
	{
		return;
	}
	bFreeCursor = bFree;

	// 显示和收回都从屏幕中心开始：鸣潮按下 Alt 时鼠标出现在中间，松开后也回到中间再锁定。
	int32 ViewportX = 0, ViewportY = 0;
	PlayerController->GetViewportSize(ViewportX, ViewportY);
	PlayerController->SetMouseLocation(ViewportX / 2, ViewportY / 2);

	if (bFree)
	{
		// 游戏 + UI：键盘照常进游戏（才能收到 Alt 松开），鼠标可以点 LGUI 界面；光标不锁在窗口里。
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		PlayerController->SetInputMode(InputMode);
		PlayerController->SetShowMouseCursor(true);
		// 移动鼠标、按住拖动时镜头不跟着转。
		PlayerController->SetIgnoreLookInput(true);
		// 显示鼠标期间只响应 UI：屏蔽技能、移动、镜头、切人这几条路由（Owner 是自己，只有自己能解除）。
		if (AWuwaPlayerController* WuwaController = Cast<AWuwaPlayerController>(PlayerController))
		{
			const FWuwaGameTags& GameTags = FWuwaGameTags::Get();
			FGameplayTagContainer BlockedRoutes;
			//阻止InputRouter的输入
			BlockedRoutes.AddTag(GameTags.Input_Route_Ability);
			BlockedRoutes.AddTag(GameTags.Input_Route_Movement);
			BlockedRoutes.AddTag(GameTags.Input_Route_Camera);
			BlockedRoutes.AddTag(GameTags.Input_Route_Formation);
			WuwaController->AddInputBlock(this, BlockedRoutes);
		}
	}
	else
	{
		// 回到纯游戏输入：视口重新捕获并锁定鼠标，光标隐藏。
		PlayerController->SetInputMode(FInputModeGameOnly());
		PlayerController->SetShowMouseCursor(false);
		PlayerController->SetIgnoreLookInput(false);
		if (AWuwaPlayerController* WuwaController = Cast<AWuwaPlayerController>(PlayerController))
		{
			WuwaController->RemoveInputBlock(this);
		}
	}
}



