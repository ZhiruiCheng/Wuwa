#pragma once

#include "CoreMinimal.h"

class FMaterialRenderProxy;
class FPrimitiveDrawInterface;
struct FWuwaBulletInfo;

/**
 * 子弹判定体的绘制（子弹编辑器视口用）。几何含义与原作一致（见 EWuwaBulletShape）：
 * Box = 半长；Sphere X = 半径；Sector X 半径 / Y 总张角（以 +X 为中线左右各一半）/ Z 半高；Cylinder X 外半径 / Y 内半径 / Z 半高。
 */
namespace WuwaBulletDraw
{
	/** 在判定体变换 ShapeTransform 处画线框；Fill 不为空时再画一层半透明实体。 */
	void DrawShape(FPrimitiveDrawInterface* PDI, ERHIFeatureLevel::Type FeatureLevel, const FWuwaBulletInfo& Info, const FTransform& ShapeTransform,
		const FLinearColor& Color, float Thickness, const FMaterialRenderProxy* Fill, float DepthBias = 0.f);

	/** 判定体的包围球半径（视口取景用）。 */
	double GetBoundingRadius(const FWuwaBulletInfo& Info);

	/** 一句话描述尺寸（"半长 200 × 170 × 100" 之类），视口叠字用。 */
	FString DescribeSize(const FWuwaBulletInfo& Info);
}
