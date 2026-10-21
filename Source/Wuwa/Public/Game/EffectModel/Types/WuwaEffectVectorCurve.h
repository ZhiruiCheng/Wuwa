#pragma once

#include "CoreMinimal.h"
#include "Curves/CurveFloat.h"
#include "WuwaEffectVectorCurve.generated.h"

/**
 * 对应原作 KuroCurveVector 的“常量或三轴曲线”。
 * 原作 Curve[0..2] 在本项目显式命名为 CurveX/Y/Z，避免当前 UnrealSharp 丢失固定数组后两轴。
 * 复用 UE 的 FRuntimeFloatCurve 编辑器；不兼容原资源的二进制布局。
 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaEffectVectorCurve
{
	GENERATED_BODY()

	FWuwaEffectVectorCurve() : FWuwaEffectVectorCurve(FVector::ZeroVector) {}

	explicit FWuwaEffectVectorCurve(const FVector& DefaultValue) : Constant(DefaultValue)
	{
		// Scale 的空曲线也保持 (1,1,1)，避免切换到曲线模式就把物体缩为零。
		CurveX.EditorCurveData.SetDefaultValue(static_cast<float>(DefaultValue.X));
		CurveY.EditorCurveData.SetDefaultValue(static_cast<float>(DefaultValue.Y));
		CurveZ.EditorCurveData.SetDefaultValue(static_cast<float>(DefaultValue.Z));
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Value")
	bool bUseCurve = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Value", meta = (EditCondition = "!bUseCurve", EditConditionHides))
	FVector Constant = FVector::ZeroVector;

	/** 各轴可内嵌编辑，也可指定外部 CurveFloat；时间由未来的 Spec 提供。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Value", meta = (EditCondition = "bUseCurve", EditConditionHides))
	FRuntimeFloatCurve CurveX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Value", meta = (EditCondition = "bUseCurve", EditConditionHides))
	FRuntimeFloatCurve CurveY;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Value", meta = (EditCondition = "bUseCurve", EditConditionHides))
	FRuntimeFloatCurve CurveZ;

	/** 按播放开始后的秒数取值；常量模式忽略时间。 */
	FVector Evaluate(float Seconds) const
	{
		if (!bUseCurve) return Constant;
		return FVector(EvaluateAxis(CurveX, Constant.X, Seconds), EvaluateAxis(CurveY, Constant.Y, Seconds),
			EvaluateAxis(CurveZ, Constant.Z, Seconds));
	}

	static float EvaluateAxis(const FRuntimeFloatCurve& Curve, double Fallback, float Seconds)
	{
		const FRichCurve* Rich = Curve.GetRichCurveConst();
		return Rich ? Rich->Eval(Seconds, static_cast<float>(Fallback)) : static_cast<float>(Fallback);
	}
};
