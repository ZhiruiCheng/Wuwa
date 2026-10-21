#pragma once

#include "CoreMinimal.h"
#include "Game/EffectModel/Types/WuwaEffectVectorCurve.h"

namespace WuwaEffectTransform
{
	/** Model 的位移、欧拉角（度；原作约定 X=Roll、Y=Pitch、Z=Yaw）、缩放在某一时刻的局部变换。 */
	inline FTransform Evaluate(const FWuwaEffectVectorCurve& Location, const FWuwaEffectVectorCurve& Rotation,const FWuwaEffectVectorCurve& Scale, float Seconds)
	{
		const FVector Euler = Rotation.Evaluate(Seconds);
		return FTransform(FRotator(Euler.Y, Euler.Z, Euler.X), Location.Evaluate(Seconds), Scale.Evaluate(Seconds));
	}

	inline bool IsAnimated(const FWuwaEffectVectorCurve& Location, const FWuwaEffectVectorCurve& Rotation,const FWuwaEffectVectorCurve& Scale)
	{
		return Location.bUseCurve || Rotation.bUseCurve || Scale.bUseCurve;
	}
}
