#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "WuwaHitTypes.generated.h"

/**
 * 受击动作（原作 EHitAnim，UserDefinedEnum 的 Names）：受击效果表里的 被击动作，受击者按它挑受击蒙太奇。
 */
UENUM(BlueprintType)
enum class EWuwaHitAnim : uint8
{
	LightLeft = 0 UMETA(DisplayName = "轻左"),
	LightRight = 1 UMETA(DisplayName = "轻右"),
	HeavyLeft = 2 UMETA(DisplayName = "重左"),
	HeavyRight = 3 UMETA(DisplayName = "重右"),
	KnockUp = 4 UMETA(DisplayName = "击飞"),
	KnockDown = 5 UMETA(DisplayName = "击倒"),
	Suppress = 6 UMETA(DisplayName = "压制"),
	Parried = 7 UMETA(DisplayName = "被弹反"),
	LightFront = 8 UMETA(DisplayName = "轻前"),
	LightBack = 9 UMETA(DisplayName = "轻后"),
	HeavyFront = 10 UMETA(DisplayName = "重前"),
	HeavyBack = 11 UMETA(DisplayName = "重后"),
	WeaknessBreak = 12 UMETA(DisplayName = "被破弱"),
};

/**
 * 受击效果表的一行（≈ 原作角色 Data/DT_HitEffect，行结构 SHitEffect），行名是子弹表的 被击效果。
 * 表属于攻击者（原作 GetBulletHitData(攻击者, 名字)）。地面受击速度是受击者朝向下的局部速度（X 负 = 后退），按 100 / 质量 缩放。
 * 空中受击、滞空、落地反弹这些字段还没用到，先不导入。
 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaHitEffectInfo : public FTableRowBase
{
	GENERATED_BODY()

	/** 原作 被击动作。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit")
	EWuwaHitAnim BeHitAnim = EWuwaHitAnim::LightLeft;

	/** 原作 受击朝向Z轴偏转（度）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit")
	float RotationOffsetZ = 0.f;

	/** 原作 命中硬直时间（秒）：这段时间受击者带硬直标签（AI 不能行动）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit")
	float StiffTime = 0.f;

	/** 原作 地面受击速度（受击者局部，只用 XY）、地面受击移动时间（秒，≤ 0 不移动）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit")
	FVector GroundSpeed = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Hit")
	float GroundMoveTime = 0.f;
};
