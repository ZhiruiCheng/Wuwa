#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaPlayEffectRescaleCommandlet.generated.h"

/**
 * 列出 / 缩放蒙太奇上 Play Effect 通知的偏移和缩放。
 * 背景：2.0 模型的 Root 插槽带 100 倍缩放，长离的通知曾在数据上手动除以 100 来抵消；
 * 现在由 UWuwaAnimNotify_PlayEffect 在代码里去掉插槽缩放，数据要乘回 100（等价变换，画面不变）。
 * 默认只列出；-Apply 时把 LocationOffset 和 Scale 乘以 -Factor（默认 100）并保存（先备份到 Saved/Backups/PlayEffectRescale/<时间>/）。
 * -Montages=<对象路径>;<对象路径>...
 */
UCLASS()
class WUWAANIMATIONEDITOR_API UWuwaPlayEffectRescaleCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaPlayEffectRescaleCommandlet();
	virtual int32 Main(const FString& Params) override;
};
