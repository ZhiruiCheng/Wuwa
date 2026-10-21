#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "WuwaUiCameraHandler.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UWuwaUiCameraHandler : public UInterface
{
	GENERATED_BODY()
};

/**
 * UI 相机（≈ 鸣潮 UiCameraAnimationController.PushCameraHandle / PopCameraHandle）：由相机系统实现，
 * 启动时调用 UWuwaUiManager::SetUiCameraHandler 注册。
 * 配了 UiCameraMode 的界面第一次显示时 Push，销毁时 Pop。可能同时有多个（栈里叠着的几个界面都配了），
 * 实现方按栈处理：最后 Push 且还没 Pop 的那个生效，全部 Pop 完回到游戏相机。
 */
class WUWA_API IWuwaUiCameraHandler
{
	GENERATED_BODY()
public:
	virtual void PushUiCamera(int32 ViewId, FName ViewName, FName CameraMode) = 0;
	virtual void PopUiCamera(int32 ViewId) = 0;
};
