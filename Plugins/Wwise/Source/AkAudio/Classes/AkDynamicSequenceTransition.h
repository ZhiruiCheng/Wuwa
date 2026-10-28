/*******************************************************************************
The content of this file includes portions of the proprietary AUDIOKINETIC Wwise
Technology released in source code form as part of the game integration package.
The content of this file may not be used without valid licenses to the
AUDIOKINETIC Wwise Technology.
Note that the use of the game engine is subject to the Unreal(R) Engine End User
License Agreement at https://www.unrealengine.com/en-US/eula/unreal
 
License Usage
 
Licensees holding valid licenses to the AUDIOKINETIC Wwise Technology may use
this file in accordance with the end user license agreement provided with the
software or, alternatively, in accordance with the terms contained
in a written agreement between you and Audiokinetic Inc.
Copyright (c) 2026 Audiokinetic Inc.
*******************************************************************************/

#pragma once
#include "AkGameplayTypes.h"
#include "AkDynamicSequenceTransition.generated.h"

/**
 * Transition Settings used in AkDynamicSequence.
 */
USTRUCT(BlueprintType)
struct AKAUDIO_API FAkDynamicSequenceTransition
{
	GENERATED_BODY()

	FAkDynamicSequenceTransition(){};
	
	UPROPERTY(Category = "Audiokinetic|AkDynamicSequenceTransition", BlueprintReadWrite)
	int32 TransitionDurationMs = 0;

	UPROPERTY(Category = "Audiokinetic|AkDynamicSequenceTransition", BlueprintReadWrite)
	EAkCurveInterpolation FadeCurve = EAkCurveInterpolation::Linear;
	bool operator==(const FAkDynamicSequenceTransition& AkDynamicSequenceTransition) const
	{
		return TransitionDurationMs == AkDynamicSequenceTransition.TransitionDurationMs &&
			FadeCurve == AkDynamicSequenceTransition.FadeCurve;	
	}
};
