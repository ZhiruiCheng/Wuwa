#pragma once

#include "Engine/DataAsset.h"
#include "Game/EffectModel/Types/WuwaEffectCurveTypes.h"
#include "WuwaCharacterMaterialControllerData.generated.h"

/**
 * 原作 CharMaterialControlFloatGroup：开始 / 循环 / 结束三段各一个值。
 * 有曲线时按本段进度（0~1）取样，否则取常量（原作 bUseCurve 为 false 时即使存了曲线也只用常量）。
 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaMaterialControlFloatGroup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material Controller")
	FWuwaEffectFloatCurve Start{0.f};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material Controller")
	FWuwaEffectFloatCurve Loop{0.f};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material Controller")
	FWuwaEffectFloatCurve End{0.f};

	float Evaluate(uint8 Phase, float Factor) const
	{
		return (Phase == 0 ? Start : Phase == 1 ? Loop : End).Evaluate(Factor);
	}
};

/** 原作 CharMaterialControlColorGroup。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaMaterialControlColorGroup
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material Controller")
	FWuwaEffectColorCurve Start{FLinearColor::Transparent};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material Controller")
	FWuwaEffectColorCurve Loop{FLinearColor::Transparent};

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material Controller")
	FWuwaEffectColorCurve End{FLinearColor::Transparent};

	FLinearColor Evaluate(uint8 Phase, float Factor) const
	{
		return (Phase == 0 ? Start : Phase == 1 ? Loop : End).Evaluate(Factor);
	}
};

/**
 * 角色材质控制器数据，对应原作 PD_CharacterControllerData（效果类型，DataType 0）。
 * 播放时把各组按时间轴取样写进角色材质的同名参数（原作 RenderConfig 的参数名），结束时还原。
 * 目前只实现了边缘光（UseRim）；描边、溶解、贴图采样等原作字段还没有对应的角色材质支持，不在这里出现。
 */
UCLASS(BlueprintType)
class WUWA_API UWuwaCharacterMaterialControllerData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** 原作 LoopTime：开始段、循环段、结束段的时长（秒）。效果类型播完三段即结束。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0", Units = "s"))
	float StartTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0", Units = "s"))
	float LoopTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0", Units = "s"))
	float EndTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Timing")
	bool bIgnoreTimeDilation = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rim")
	bool bUseRim = false;

	/** 写入 E_Rim_UseTex：边缘光是否乘 E_Tex_Color 的通道。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rim", meta = (EditCondition = "bUseRim"))
	bool bRimUseTex = false;

	/** 写入 E_Rim_Channel：原作 RenderUtil.GetSelectedChannel 的结果，x 为 -1 时用 RGB 全部。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rim", meta = (EditCondition = "bUseRim"))
	FLinearColor RimChannel = FLinearColor(-1.f, 0.f, 0.f, 0.f);

	/** 写入 E_Rim_RimRange：1-|N·V| 的指数。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rim", meta = (EditCondition = "bUseRim"))
	FWuwaMaterialControlFloatGroup RimRange;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rim", meta = (EditCondition = "bUseRim"))
	FWuwaMaterialControlColorGroup RimColor;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rim", meta = (EditCondition = "bUseRim"))
	FWuwaMaterialControlFloatGroup RimIntensity;

	float WholeTime() const { return StartTime + LoopTime + EndTime; }
};
