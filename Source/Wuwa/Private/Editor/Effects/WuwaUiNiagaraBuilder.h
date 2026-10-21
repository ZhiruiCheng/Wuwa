#pragma once

#include "CoreMinimal.h"

class UNiagaraSystem;
class UStaticMesh;
class UMaterialInterface;

/**
 * 重建界面特效（原作 NS_Fx_LGUI_*）用的 Niagara 系统：每层一个 CPU 发射器，粒子属性全部写成 Custom HLSL 表达式
 * （出生阶段 / 更新阶段各一个 Set Parameters 模块），表达式由 ArtSource 的配方脚本按原作脚本解码出的数值生成。
 * 和 WuwaReferenceNiagaraBuilder（攻击特效）分开：界面特效不需要它那套圆环 / 圆锥分布，而需要逐帧的更新表达式。
 * 系统由 UWuwaUINiagara 播放（模拟在 CPU 上跑，粒子数据读出来画进 LGUI 画布）。
 */
namespace WuwaUiNiagaraBuilder
{
	/** 一个粒子属性：Name 如 Particles.Scale，Type 为 float / vec2 / vec3 / vec4 / color / quat / position，Hlsl 是一个表达式。 */
	struct FAttribute
	{
		FString Name;
		FString Type;
		FString Hlsl;
	};

	struct FEmitterSpec
	{
		FName Name;
		UStaticMesh* Mesh = nullptr; // 为空则是精灵层
		UMaterialInterface* Material = nullptr;
		/** 精灵：锚点（UV 空间）、翻页图集的行列。 */
		FVector2D Pivot = FVector2D(0.5, 0.5);
		FVector2D SubImageSize = FVector2D(1, 1);
		int32 SpawnCount = 1;
		float SpawnTime = 0.f;
		/** 发射器一次播放的时长（单次，不循环）。 */
		float LoopDuration = 1.f;
		int32 SortOrder = 0;
		int32 RandomSeed = 1;
		TArray<FAttribute> Spawn;
		TArray<FAttribute> Update;
	};

	/** 替换 System 内的发射器并编译；不保存资产。 */
	bool BuildSystem(UNiagaraSystem* System, const TArray<FEmitterSpec>& Specs);
}
