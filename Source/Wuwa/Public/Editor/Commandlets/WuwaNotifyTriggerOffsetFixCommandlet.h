#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaNotifyTriggerOffsetFixCommandlet.generated.h"

/**
 * 按 UE 编辑器的规则刷新已有动画 / 蒙太奇里通知的触发偏移（WuwaAnimNotifyEditor::RefreshTriggerOffsets）：
 * 早先命令行装的通知没设偏移，放在第 0 帧的瞬时通知（子弹、镜头……）不会触发。
 *   -run=WuwaNotifyTriggerOffsetFix [-Path=/Game/Characters] [-Apply]
 * 默认只读，列出会变的通知；-Apply 才写，改动的资产先备份到 Saved/Backups/NotifyTriggerOffsetFix/<时间>/。
 */
UCLASS()
class WUWA_API UWuwaNotifyTriggerOffsetFixCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaNotifyTriggerOffsetFixCommandlet();
	virtual int32 Main(const FString& Params) override;
};
