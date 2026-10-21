#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaDebugPrintAuditCommandlet.generated.h"

/**
 * 只读：列出 /Game 下所有蓝图（含动画蓝图）里的 Print String / Print Text 节点：
 * 资产、图、打印内容（字面量或连进来的节点）、是否上屏 / 进日志、由哪个事件驱动（往前追执行线）。
 * -Path=/Game/Characters 只扫某个目录。
 */
UCLASS()
class WUWA_API UWuwaDebugPrintAuditCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaDebugPrintAuditCommandlet();
	virtual int32 Main(const FString& Params) override;
};
