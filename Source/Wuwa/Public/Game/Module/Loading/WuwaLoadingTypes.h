#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "WuwaLoadingTypes.generated.h"

class UTexture2D;

/** 加载界面的一条展示内容（≈ 鸣潮 LoadingShowData 里的一项）：背景图 + 标题 + 描述，按权重随机。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaLoadingShowRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading")
	TSoftObjectPtr<UTexture2D> Background;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading")
	FText Title;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading", meta = (MultiLine = true))
	FText Description;

	/** 随机时的权重；0 表示不参与随机。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Loading", meta = (ClampMin = "0"))
	float Weight = 1.f;
};

/**
 * 进入游戏地图的加载阶段（≈ 鸣潮 ELoadingPhase 的 OPENLEVEL / PRELOAD / … 分段）。
 * 每段在总进度里占一个固定比例，段内进度 0~1 换算到这段区间里。
 */
UENUM(BlueprintType)
enum class EWuwaLoadingPhase : uint8
{
	/** 队伍角色资源（网格、动画、蒙太奇、特效……）异步加载。最重的一段。 */
	Formation,
	/** 生成队员、控制第一个角色（客户端：等队员复制过来）。 */
	SpawnRoles,
	/** 战斗预热：提前加载技能 / 命中时才用到的软引用资源，编译过期的 Niagara（UWuwaCombatWarmupSubsystem）。 */
	Warmup,
	/** 加载并打开主 HUD。 */
	MainHud,
	Count UMETA(Hidden)
};

namespace WuwaLoading
{
	/** 各阶段在总进度里的比例，加起来是 1。 */
	inline float PhaseWeight(EWuwaLoadingPhase Phase)
	{
		switch (Phase)
		{
		case EWuwaLoadingPhase::Formation: return 0.65f;
		case EWuwaLoadingPhase::SpawnRoles: return 0.1f;
		case EWuwaLoadingPhase::Warmup: return 0.05f;
		case EWuwaLoadingPhase::MainHud: return 0.2f;
		default: return 0.f;
		}
	}

	/** 这一阶段开始时的总进度。 */
	inline float PhaseStart(EWuwaLoadingPhase Phase)
	{
		float Start = 0.f;
		for (int32 Index = 0; Index < static_cast<int32>(Phase); ++Index)
		{
			Start += PhaseWeight(static_cast<EWuwaLoadingPhase>(Index));
		}
		return Start;
	}
}
