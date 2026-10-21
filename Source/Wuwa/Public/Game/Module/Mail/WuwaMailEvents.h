#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"
#include "WuwaMailEvents.generated.h"

/** 邮件模块的事件（≈ 鸣潮 EEventName 里邮件相关的那几项）。 */
namespace WuwaMailEvents
{
	/** 邮件数据变了（读了、删了、来了新的）；参数 FWuwaMailChangedEvent。 */
	WUWA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Changed);
}

USTRUCT(BlueprintType)
struct WUWA_API FWuwaMailChangedEvent
{
	GENERATED_BODY()
	/** 变化的那封邮件；0 表示整体刷新。 */
	UPROPERTY(BlueprintReadOnly, Category = "Mail") int32 MailId = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Mail") int32 UnreadCount = 0;
};
