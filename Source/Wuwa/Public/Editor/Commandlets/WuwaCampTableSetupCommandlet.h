#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaCampTableSetupCommandlet.generated.h"

/**
 * 阵营表：ArtSource/Combat/camps.json（prepare_camps.py 从原作 DT_Camp 生成）→ 配方里写的 DataTable（行结构 FWuwaCampRow）。
 * 整表按配方重建。默认只读，打印差异；-Apply 才写入，已有资产先备份到 Saved/Backups/CampTableSetup/<时间>/。
 */
UCLASS()
class WUWA_API UWuwaCampTableSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaCampTableSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
