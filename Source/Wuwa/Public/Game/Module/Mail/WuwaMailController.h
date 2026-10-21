#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Game/LGUI/Mvc/WuwaControllerBase.h"
#include "WuwaMailController.generated.h"

class UWuwaUiView;

/**
 * 邮件业务（≈ 鸣潮 MailController）：界面按钮调这里 → "请求服务器" → 回包后改 MailModel → 发 Event.Mail.Changed。
 * 这里没有服务器，用一段延迟模拟网络往返。
 */
UCLASS()
class WUWA_API UWuwaMailController : public UWuwaControllerBase
{
	GENERATED_BODY()
public:
	static const FName MailBoxViewName;

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 打开邮箱的入口：业务方调这个，不直接写界面名。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Mail")
	UWuwaUiView* OpenMailBox();

	/** 读第一封未读邮件。请求进行中再调会被忽略（防止连点重复请求）。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Mail")
	void RequestReadOne();

	/** 邮件功能开关（GM、测试用）；关掉时邮箱打不开。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Mail")
	void SetMailEnabled(bool bEnabled);

	bool IsRequestInFlight() const { return ReadRequestTicker.IsValid(); }

	/** 模拟的网络延迟（秒）。 */
	float FakeLatencySeconds = 0.2f;

protected:
	virtual void OnAddOpenViewCheckFunction() override;
	virtual void OnClear() override;

private:
	/** "服务器回包"：改 Model，再发事件。 */
	void HandleReadResponse(int32 MailId);

	/** 用核心 Ticker 而不是世界的 TimerManager：和真实网络回调一样，不会因为切地图丢掉。 */
	FTSTicker::FDelegateHandle ReadRequestTicker;
};
