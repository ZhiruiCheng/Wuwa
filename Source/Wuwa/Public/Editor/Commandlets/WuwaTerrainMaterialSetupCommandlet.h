#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaTerrainMaterialSetupCommandlet.generated.h"

/**
 * 地形材质：读 ArtSource/Terrain/<区域>.material.json（prepare_terrain_material.py 生成），建
 *  - 贴图：原作 Texture2DArray 的切片 PNG → <textureFolder>/T_Landscape_<切片>_D / _N（法线按法线贴图压缩）；
 *  - 主材质 M_WuwaLandscape：Layer1..LayerN 用 LandscapeLayerBlend 按权重混合，每层一组参数
 *    L<n>_Color / L<n>_Normal（贴图）、L<n>_Scale（平铺尺寸，厘米，按世界坐标平铺）、L<n>_Tint、L<n>_Roughness；
 *    贴图采样器共用（WorldGroupSettings），20 张贴图也不超过采样器上限；
 *  - 材质实例：每个原作材质实例一个（参数取原作的切片、平铺、颜色、粗糙度）；
 *  - 图层信息 Layer<n>_LayerInfo（层名 Layer<n>，和原作一致，导入权重时用）。
 * 默认只读；-Apply 才写入，已有资产先备份到 Saved/Backups/TerrainMaterialSetup/<时间>/。
 */
UCLASS()
class WUWA_API UWuwaTerrainMaterialSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaTerrainMaterialSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
