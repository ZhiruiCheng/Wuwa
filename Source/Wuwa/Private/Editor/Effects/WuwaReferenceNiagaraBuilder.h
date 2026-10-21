#pragma once

#include "CoreMinimal.h"

class UNiagaraSystem;
class UStaticMesh;
class UMaterialInterface;

/**
 * 按参数重建参考 Niagara 系统（替换系统内全部发射器）。
 * 取自旧刀光工具 WuwaSlashFxReferenceNiagara（f7eb7ad），Sprite 层改为按区间随机尺寸 / 半径 / 寿命，
 * 可选让尺寸乘 Engine.Owner.Scale（Niagara 的 Sprite 尺寸本身不受组件缩放影响）。
 */
namespace WuwaReferenceNiagaraBuilder
{
	/** 一层发射器的重建参数；不是运行时状态。 */
	/** 逐粒子参数表的一行（原作按发射器时间生成、出生时查曲线的发射器，离线求值得到；发射器局部空间、厘米）。 */
	struct FTableParticle
	{
		float Delay = 0.f;
		float Lifetime = .3f;
		FVector2D Size = FVector2D(10, 10);
		FVector Position = FVector::ZeroVector;
		FVector Velocity = FVector::ZeroVector;
		FLinearColor Color = FLinearColor::White;
	};

	struct FEmitterSpec
	{
		FName Name;
		UStaticMesh* Mesh = nullptr; // 为空则生成速度对齐的 Sprite 层。
		UMaterialInterface* Material = nullptr;
		float Lifetime = .32f;
		float LifetimeMax = .32f; // 仅 Sprite：每个粒子在 [Lifetime, LifetimeMax] 内随机。
		int32 Count = 1; // Mesh 层固定一个粒子。
		int32 SortOrder = 0;
		FVector PivotOffset = FVector::ZeroVector;
		bool bLocalSpace = true;

		/** Sprite 尺寸（厘米，未乘组件缩放），每个粒子在 Min..Max 之间取同一个随机比例。负值保留原贴图翻转。 */
		FVector2D SpriteSizeMin = FVector2D(12, 40);
		FVector2D SpriteSizeMax = FVector2D(12, 40);
		/** 尺寸是否再乘 Engine.Owner.Scale.x；Niagara 默认不让 Sprite 尺寸跟随组件缩放（原作同样如此）。 */
		bool bSizeFollowsOwnerScale = false;
		/** 发射环在发射器局部 XY 平面；RingHeight 为 Z 方向总厚度。 */
		float SpawnRadiusMin = 0.f;
		float SpawnRadiusMax = 0.f;
		float RingHeight = 8.f;
		float RingArcDegrees = 360.f;
		float RingStartAngleDegrees = 0.f;
		bool bClockwise = false;
		/** 初速度方向：false 沿环切线，true 沿半径向外（速度对齐的 Sprite 因此呈放射状）。 */
		bool bRadialVelocity = false;
		/** 初速度大小。 */
		float SpeedMin = 0.f;
		float SpeedMax = 0.f;
		FVector Gravity = FVector::ZeroVector;
		float Drag = 0.f;
		/** 初速度方向模式（来自原作出生脚本的采样）：0 沿环（切线 / 半径，见 bRadialVelocity），1 圆锥，2 全向球面。 */
		int32 VelocityMode = 0;
		/** 圆锥模式的轴向（发射器局部空间，会归一化）与半角（度）。 */
		FVector ConeDirection = FVector(1, 0, 0);
		float ConeAngleDegrees = 0.f;
		/** 出生位置整体偏移（发射器局部空间）。 */
		FVector PositionOffset = FVector::ZeroVector;
		/** 网格粒子的出生位置（发射器局部空间，原作出生脚本的 Position）。粒子本身放在这里，WPO 只做朝向 / 缩放：
		 *  粒子若停在系统原点、系统又挂在相机上，粒子正好落在相机位置，整层不渲染（长离大招 BurstBw01 的红色漩涡）。 */
		FVector MeshPosition = FVector::ZeroVector;
		/** Sprite：原作出生脚本的自定义对齐向量（非零时渲染器用 CustomAlignment）、自定义朝向向量（零 = 默认 +Z）、旋转（度）。 */
		FVector CustomAlignment = FVector::ZeroVector;
		FVector SpriteFacing = FVector::ZeroVector;
		float SpriteRotation = 0.f;
		/** 渲染器：朝向相机（否则用 +Z 的自定义朝向）、按速度对齐、锚点（UV 空间）。默认值是 Changli 羽毛层的写法。 */
		bool bFaceCamera = false;
		bool bVelocityAligned = true;
		FVector2D Pivot = FVector2D(0.5, 1.0);
		int32 RandomSeed = 913;
		/** 非空时：Count = Table.Num()，在 SpawnTime 一次爆发，每个粒子按 Engine.ExecIndex 取表里的寿命 / 尺寸 / 位置 / 速度 / 颜色，
		 * 不再用上面的随机分布。用于原作按发射器时间持续生成的层（同一出生时刻的粒子分到同一个发射器）。 */
		TArray<FTableParticle> Table;
		float SpawnTime = 0.f;

		/** 丝带（武器拖尾）层：世界空间、跟着组件位置持续出生，宽度按归一化年龄查 RibbonWidths（厘米，等间隔采样）。
		 * 原作是 SpawnPerUnit（每厘米出生数）；这里按 RibbonSpawnRate（个 / 秒）出生，见配方 ribbon.spawnRate 的说明。 */
		bool bRibbon = false;
		TArray<float> RibbonWidths;
		float RibbonSpawnRate = 60.f;
		float RibbonUVTilingLength = 1.f;
	};

	/** 替换 System 内的发射器并编译；不保存资产。 */
	bool BuildSystem(UNiagaraSystem* System, const TArray<FEmitterSpec>& Specs);
}
