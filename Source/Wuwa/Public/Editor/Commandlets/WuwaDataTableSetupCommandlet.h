#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaDataTableSetupCommandlet.generated.h"

/**
 * 通用的 DataTable 导入：配方 JSON { table, struct, source, rows: [ { Name, 字段... } ] }（由 ArtSource 下的 prepare_*.py 生成）。
 *   默认：整表按配方重建（表不存在时按 struct 新建）。
 *   -OnlyFields=A,B：只把这几个字段写进现有表的同名行，其余字段（手改过的）不动；配方里多出的行不加，配方行可以只含这几个字段。
 * 默认只读，打印差异；-Apply 才写入，已有资产先备份到 Saved/Backups/DataTableSetup/<时间>/。
 *   -run=WuwaDataTableSetup -Recipe=ArtSource/Combat/lockon.json [-OnlyFields=...] [-Apply]
 */
UCLASS()
class WUWA_API UWuwaDataTableSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaDataTableSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
