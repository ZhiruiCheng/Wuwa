#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaTerrainImportCommandlet.generated.h"

/**
 * 原作地形区域 → 新关卡里的 Landscape：读 ArtSource/Terrain/<区域>.terrain.json（prepare_terrain.py 生成）和它指向的 16 位高度图，
 * 新建配方里写的关卡，按原作的顶点间距、高度缩放和世界位置建 Landscape（组件 62 四边形、2×2 子区块，与原作一致），
 * 再放一套基础的户外光照（太阳、天空大气、天光、高度雾、体积云）和 Boss 点旁边的 PlayerStart。
 * 默认只读，打印将要创建的内容；-Apply 才写入，已有关卡先备份到 Saved/Backups/TerrainImport/<时间>/ 再整关重建。
 * Landscape 编辑层合并需要 GPU：要带 -AllowCommandletRendering（不能用 -NullRHI）。
 */
UCLASS()
class WUWA_API UWuwaTerrainImportCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaTerrainImportCommandlet();
	virtual int32 Main(const FString& Params) override;
};
