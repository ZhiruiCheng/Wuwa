#pragma once

#include "CoreMinimal.h"
#include "Game/LGUI/View/WuwaUiView.h"
#include "WuwaMailBoxView.generated.h"

/**
 * 邮箱界面（≈ 鸣潮 MailBoxView）：
 * - 0 号按钮"读一封" → MailController::RequestReadOne（View 不直接改 Model）；
 * - 1 号按钮关闭；2 号文字显示未读数。
 * - 显示前从 Model 刷新一次；显示期间监听 Event.Mail.Changed 再刷新。
 */
UCLASS()
class WUWA_API UWuwaMailBoxView : public UWuwaUiView
{
	GENERATED_BODY()
public:
	// 测试用
	UUIButtonComponent* GetReadButton() const { return GetButton(0); }
	FString GetUnreadText() const;
	int32 GetRefreshCount() const { return RefreshCount; }

protected:
	virtual void OnRegisterComponent() override;
	virtual void OnBeforeShow() override;
	virtual void OnAddEventListener() override;

private:
	void Refresh();
	int32 RefreshCount = 0;
};
