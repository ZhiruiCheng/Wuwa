#pragma once

#include "CoreMinimal.h"
#include "Game/LGUI/View/WuwaUiView.h"
#include "WuwaTestPopView.generated.h"

/**
 * 2a/2b 验收用：每个钩子打一行日志；0 号按钮关闭自己，1 号文字显示打开次数。
 * 配套控制台命令：Wuwa.Ui.Open / Wuwa.Ui.Close / Wuwa.Ui.Stress / Wuwa.Ui.OpenClose / Wuwa.Ui.Dump
 */
UCLASS()
class WUWA_API UWuwaTestPopView : public UWuwaUiView
{
	GENERATED_BODY()
public:
	/** 按调用顺序记下的钩子名，自动化测试用来检查顺序。 */
	const TArray<FName>& GetHookLog() const { return HookLog; }
	/** 自动化测试用：模拟点击、检查文字。 */
	UUIButtonComponent* GetCloseButton() const { return GetButton(0); }
	UUIText* GetCountText() const { return GetText(1); }
	/** 关闭按钮被点了几次（这个对象上）。 */
	int32 GetCloseClickCount() const { return CloseClickCount; }

protected:
	virtual void OnRegisterComponent() override;
	virtual void OnCreate() override;
	virtual void OnStart() override;
	virtual void OnBeforeShow() override;
	virtual void OnAfterShow() override;
	virtual void OnBeforeHide() override;
	virtual void OnAfterHide() override;
	virtual void OnBeforeDestroy() override;

private:
	TArray<FName> HookLog;
	int32 CloseClickCount = 0;
};
