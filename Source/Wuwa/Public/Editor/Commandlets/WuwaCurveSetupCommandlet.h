#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaCurveSetupCommandlet.generated.h"

/**
 * 通用的 CurveFloat 导入：配方 JSON { source, curves: [ { path, keys: [ { time, value, interp, tangentMode, arrive, leave } ], preInfinity, postInfinity } ] }
 * （由 ArtSource 下的 prepare_*.py 用 PakExport 导出的原作曲线关键帧生成）。已有的资产原地覆盖。
 * 默认只读，打印差异；-Apply 才写入，已有资产先备份到 Saved/Backups/CurveSetup/<时间>/。
 *   -run=WuwaCurveSetup -Recipe=ArtSource/Bullet/changli.curves.json [-Apply]
 */
UCLASS()
class WUWA_API UWuwaCurveSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaCurveSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
