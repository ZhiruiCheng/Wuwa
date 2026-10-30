#pragma once

#include "CoreMinimal.h"
#include "Game/LGUI/Mvc/WuwaControllerBase.h"
#include "WuwaMainHudController.generated.h"

class APlayerController;
class UWuwaUiView;

/**
 * 主 HUD 的业务（≈ 鸣潮 BattleUiControl 里 OpenBattleViewFromLoading / GetMainViewName 那部分）：
 * 提供打开主 HUD 的入口，并决定哪些地图要主 HUD。
 *
 * 什么时候打开由进入地图的加载流程决定（UWuwaLoadingController，≈ 鸣潮 GameModeController.Load）：
 * 队伍就绪后在加载界面下面打开主 HUD，HUD 显示出来再关加载界面。
 */
UCLASS()
class WUWA_API UWuwaMainHudController : public UWuwaControllerBase
{
	GENERATED_BODY()

public:
	static const FName MainHudViewName;

	/** 打开主 HUD 的入口；已经打开就返回已有的那个。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Hud")
	UWuwaUiView* OpenMainHud();

	/** 这个 PC 所在的地图要不要主 HUD：有队伍组件 = 玩家控制着队伍角色的游戏地图（≈ 鸣潮按世界类型选主界面）。 */
	static bool ShouldShowMainHud(const APlayerController* PlayerController);

	/**
	 * 隐藏 / 恢复主 HUD（播技能特写镜头时用）。
	 * 给 C# 序列镜头用：LGUI 的类型 C# 拿不到，所以在这里替它做。主 HUD 没打开时什么都不做。
	 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Hud")
	static void SetMainHudHidden(APlayerController* PlayerController, bool bHidden);
};
