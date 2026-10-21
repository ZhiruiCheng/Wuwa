#pragma once

#include "CoreMinimal.h"
#include "Game/LGUI/Mvc/WuwaControllerBase.h"
#include "WuwaSkillButtonUiController.generated.h"

class APlayerController;
class AWuwaPlayerController;
struct FWuwaInputEvent;

/**
 * 技能按钮的按键反馈（≈ 鸣潮 SkillButtonPanel 用 InputDistributeController.BindActions 监听按键动作）：
 * 监听本地玩家 PC 分发出去的输入（AWuwaPlayerController::OnInputRouted），按下的 InputTag 在 UWuwaUISettings::SkillButtonInputs
 * 里有对应按钮时，发 Event.Hud.SkillButtonPressed。界面只听事件，不直接找 PC。
 * 被屏蔽的输入（比如打开了界面）不会分发，也就不会有反馈；技能是否真的放出来不影响（原作也是按下就播）。
 */
UCLASS()
class WUWA_API UWuwaSkillButtonUiController : public UWuwaControllerBase
{
	GENERATED_BODY()

protected:
	virtual void OnInit() override;
	virtual void OnClear() override;
	virtual void OnLeaveLevel() override;

private:
	void HandlePlayerControllerChanged(APlayerController* NewController);
	void Bind(AWuwaPlayerController* NewController);
	void Unbind();
	void HandleInputRouted(const FWuwaInputEvent& InputEvent);

	FDelegateHandle PlayerControllerChangedHandle;
	TWeakObjectPtr<AWuwaPlayerController> BoundController;
	FDelegateHandle InputRoutedHandle;
};
