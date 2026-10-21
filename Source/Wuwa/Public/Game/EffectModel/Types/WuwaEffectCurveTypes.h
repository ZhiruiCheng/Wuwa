#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "Game/EffectModel/Types/WuwaEffectVectorCurve.h"
#include "WuwaEffectCurveTypes.generated.h"

/** 对应 KuroCurveFloat：一个常量，或一条可内嵌/外部引用的曲线。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaEffectFloatCurve
{
	GENERATED_BODY()

	FWuwaEffectFloatCurve() : FWuwaEffectFloatCurve(0.f) {}
	explicit FWuwaEffectFloatCurve(float DefaultValue) : Constant(DefaultValue)
	{
		Curve.EditorCurveData.SetDefaultValue(DefaultValue);
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Value")
	bool bUseCurve = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Value", meta = (EditCondition = "!bUseCurve", EditConditionHides))
	float Constant = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Value", meta = (EditCondition = "bUseCurve", EditConditionHides))
	FRuntimeFloatCurve Curve;

	/** 按播放开始后的秒数取值；常量模式忽略时间。 */
	float Evaluate(float Seconds) const
	{
		return bUseCurve ? FWuwaEffectVectorCurve::EvaluateAxis(Curve, Constant, Seconds) : Constant;
	}
};

/** 对应 KuroCurveLinearColor。四通道显式声明，使 UnrealSharp 能完整读写。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaEffectColorCurve
{
	GENERATED_BODY()

	FWuwaEffectColorCurve() : FWuwaEffectColorCurve(FLinearColor::White) {}
	explicit FWuwaEffectColorCurve(const FLinearColor& DefaultValue) : Constant(DefaultValue)
	{
		CurveR.EditorCurveData.SetDefaultValue(DefaultValue.R);
		CurveG.EditorCurveData.SetDefaultValue(DefaultValue.G);
		CurveB.EditorCurveData.SetDefaultValue(DefaultValue.B);
		CurveA.EditorCurveData.SetDefaultValue(DefaultValue.A);
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Value")
	bool bUseCurve = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Value", meta = (EditCondition = "!bUseCurve", EditConditionHides))
	FLinearColor Constant = FLinearColor::White;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Value", meta = (EditCondition = "bUseCurve", EditConditionHides))
	FRuntimeFloatCurve CurveR;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Value", meta = (EditCondition = "bUseCurve", EditConditionHides))
	FRuntimeFloatCurve CurveG;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Value", meta = (EditCondition = "bUseCurve", EditConditionHides))
	FRuntimeFloatCurve CurveB;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Value", meta = (EditCondition = "bUseCurve", EditConditionHides))
	FRuntimeFloatCurve CurveA;

	/** 按播放开始后的秒数取值；常量模式忽略时间。 */
	FLinearColor Evaluate(float Seconds) const
	{
		if (!bUseCurve) return Constant;
		return FLinearColor(FWuwaEffectVectorCurve::EvaluateAxis(CurveR, Constant.R, Seconds),
			FWuwaEffectVectorCurve::EvaluateAxis(CurveG, Constant.G, Seconds),
			FWuwaEffectVectorCurve::EvaluateAxis(CurveB, Constant.B, Seconds),
			FWuwaEffectVectorCurve::EvaluateAxis(CurveA, Constant.A, Seconds));
	}
};
