#include "Game/Module/Hud/WuwaSkillButtonUiController.h"

#include "Engine/LocalPlayer.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Input/WuwaInputTypes.h"
#include "Game/LGUI/Core/WuwaUISettings.h"
#include "Game/Module/Hud/WuwaHudEvents.h"

void UWuwaSkillButtonUiController::OnInit()
{
	ULocalPlayer* LocalPlayer = GetLocalPlayer();
	if (!LocalPlayer)
	{
		return;
	}
	// 和 UWuwaFormationUiController 一样：本地玩家先于 PC 存在，PC 关联上时再绑
	PlayerControllerChangedHandle = LocalPlayer->OnPlayerControllerChanged().AddUObject(this, &UWuwaSkillButtonUiController::HandlePlayerControllerChanged);
	if (APlayerController* Existing = LocalPlayer->GetPlayerController(nullptr))
	{
		HandlePlayerControllerChanged(Existing);
	}
}

void UWuwaSkillButtonUiController::OnClear()
{
	Unbind();
	if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
	{
		LocalPlayer->OnPlayerControllerChanged().Remove(PlayerControllerChangedHandle);
	}
	PlayerControllerChangedHandle.Reset();
}

void UWuwaSkillButtonUiController::OnLeaveLevel()
{
	// PC 属于正在清理的世界；新地图的 PC 出现时经 OnPlayerControllerChanged 重新绑定
	Unbind();
}

void UWuwaSkillButtonUiController::HandlePlayerControllerChanged(APlayerController* NewController)
{
	Bind(Cast<AWuwaPlayerController>(NewController));
}

void UWuwaSkillButtonUiController::Bind(AWuwaPlayerController* NewController)
{
	if (BoundController.Get() == NewController)
	{
		return;
	}
	Unbind();
	if (!NewController)
	{
		return;
	}
	BoundController = NewController;
	InputRoutedHandle = NewController->OnInputRouted.AddUObject(this, &UWuwaSkillButtonUiController::HandleInputRouted);
}

void UWuwaSkillButtonUiController::Unbind()
{
	if (AWuwaPlayerController* Old = BoundController.Get())
	{
		Old->OnInputRouted.Remove(InputRoutedHandle);
	}
	InputRoutedHandle.Reset();
	BoundController.Reset();
}

void UWuwaSkillButtonUiController::HandleInputRouted(const FWuwaInputEvent& InputEvent)
{
	if (InputEvent.Phase != EWuwaInputPhase::Pressed)
	{
		return;
	}
	if (const EWuwaSkillButtonType* ButtonType = GetDefault<UWuwaUISettings>()->SkillButtonInputs.Find(InputEvent.InputTag))
	{
		FWuwaSkillButtonPressedEvent Event;
		Event.ButtonType = *ButtonType;
		EmitEvent(WuwaHudEvents::SkillButtonPressed, Event);
	}
}
