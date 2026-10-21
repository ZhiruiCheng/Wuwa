#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaClimbTraceSetupCommandlet.generated.h"

/**
 * 角色蓝图移动组件的攀爬检测 ObjectTypes：不含 WorldStatic 的（空、或者只配了 Pawn 之类）改成 WorldStatic + WorldDynamic，
 * 否则攀爬检测打不到墙体。默认只读；-Apply 才写入，先备份到 Saved/Backups/ClimbTraceSetup/<时间>/。
 */
UCLASS()
class WUWA_API UWuwaClimbTraceSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaClimbTraceSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
