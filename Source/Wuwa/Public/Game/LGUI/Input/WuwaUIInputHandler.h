#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Game/Input/IWuwaInputRouteHandler.h"
#include "WuwaUIInputHandler.generated.h"

class UWuwaUiManager;
struct FWuwaInputEvent;

/**
 * UI 的输入处理器：挂在 PlayerController 上，由 InputRouter 按 UI 的 RouteTag 把语义输入交过来，
 * 再转给 UI 框架（UiManager / 当前界面）。和 WuwaMoveInputHandler、WuwaAbilityInputHandlerComponent 是同一类东西。
 */
UCLASS(ClassGroup = (WuwaUI), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaUIInputHandler : public UActorComponent, public IIWuwaInputRouteHandler
{
	GENERATED_BODY()

public:
	UWuwaUIInputHandler();

	/** 返回 true 表示 UI 处理了这个输入。 */
	virtual bool HandleWuwaInput_Implementation(const FWuwaInputEvent& InputEvent) override;

	//具体业务
	void HandleShowMouseIA(const FWuwaInputEvent& InputEvent);
	void HandleExitUI(const FWuwaInputEvent& InputEvent);
	
protected:
	/** 拥有这个组件的 PlayerController 对应本地玩家的 UiManager。 */
	UWuwaUiManager* GetUiManager() const;

private:
	/** true：显示鼠标、可以自由移动、不转镜头；false：鼠标回到屏幕中心、隐藏并锁定（正常游戏状态）。 */
	void SetFreeCursor(bool bFree);

	/** 当前是不是按住 Alt 放出的鼠标；避免重复切换，也保证 SetIgnoreLookInput 的计数成对。 */
	bool bFreeCursor = false;
};
