#pragma once

#include "CoreMinimal.h"
#include "Game/LGUI/Mvc/WuwaModelBase.h"
#include "WuwaMailModel.generated.h"

USTRUCT(BlueprintType)
struct WUWA_API FWuwaMail
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Mail") int32 Id = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Mail") FString Title;
	UPROPERTY(BlueprintReadOnly, Category = "Mail") bool bRead = false;
};

/**
 * 邮件数据（≈ 鸣潮 MailModel）：只存数据、提供查询；修改方法只给 MailController 用，Model 自己不发事件。
 * 跨地图保留（LocalPlayerSubsystem），所以切地图后未读数还在。
 */
UCLASS()
class WUWA_API UWuwaMailModel : public UWuwaModelBase
{
	GENERATED_BODY()
public:
	const TArray<FWuwaMail>& GetMails() const { return Mails; }
	int32 GetUnreadCount() const;
	const FWuwaMail* FindFirstUnread() const;
	bool IsMailEnabled() const { return bMailEnabled; }

	// —— 以下只给 MailController 调 ——
	void SetMails(TArray<FWuwaMail> InMails) { Mails = MoveTemp(InMails); }
	/** 标记已读；返回是否真的改了。 */
	bool MarkRead(int32 MailId);
	void SetMailEnabled(bool bEnabled) { bMailEnabled = bEnabled; }

protected:
	virtual void OnInit() override;

private:
	UPROPERTY(Transient) TArray<FWuwaMail> Mails;
	bool bMailEnabled = true;
};
