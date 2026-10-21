#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaDebugPrintSetupCommandlet.generated.h"

/**
 * 把每帧刷屏的蓝图 Print String 换成带频道开关的 Wuwa Debug Print（UWuwaDebugLibrary::DebugPrint）：
 * 原地替换节点，连线、打印内容、颜色、时长都不变，只多一个 Channel（BP_RoleBase → MoveState，ABP_RoleBase → AnimParams）。
 * 出错才打印的提示（BP_InputBase、WBP_HPBar）不动。默认只读；-Apply 才写入，先备份到 Saved/Backups/DebugPrintSetup/<时间>/。
 */
UCLASS()
class WUWA_API UWuwaDebugPrintSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaDebugPrintSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
