#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaBulletTableSetupCommandlet.generated.h"

/**
 * 子弹表：ArtSource/Bullet/<角色>.bullets.json（prepare_bullets.py 从原作 DT_ReBulletDataMain 生成）→ 配方里写的
 * DataTable（行结构 FWuwaBulletInfo，行名 = 子弹 ID）。整表按配方重建。
 * 写完表后，把角色目录（表所在 Data 目录的上一级）里角色蓝图的 BulletTable 指向它。
 * -AssignOnly：不重建表，只做指向（子弹编辑器里改过的数据会保留）。
 * -OnlyFields=A,B：不重建表，只把配方里这几个字段写进现有表的同名行（新加字段时用，其余数据保留）。
 * 默认只读，打印将要写入的行数和与现有表的差异；-Apply 才写入，已有资产先备份到 Saved/Backups/BulletTableSetup/<时间>/。
 */
UCLASS()
class WUWA_API UWuwaBulletTableSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaBulletTableSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
