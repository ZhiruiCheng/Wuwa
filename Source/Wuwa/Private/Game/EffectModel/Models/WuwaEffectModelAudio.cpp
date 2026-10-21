#include "Game/EffectModel/Models/WuwaEffectModelAudio.h"
#include "Sound/SoundBase.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"

EDataValidationResult UWuwaEffectModelAudio::IsDataValid(FDataValidationContext& Context) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	if (AudioBackend == EWuwaEffectAudioBackend::UnrealSound)
	{
		if (!IsValid(Sound))
		{
			Context.AddError(NSLOCTEXT("WuwaEffectModel", "MissingUnrealSound", "UnrealSound requires a Sound resource; AudioEvent is a separate Wwise event reference."));
			Result = EDataValidationResult::Invalid;
		}
		if (IsValid(TrailingSound) && TrailingSound->IsLooping())
		{
			Context.AddError(NSLOCTEXT("WuwaEffectModel", "LoopingAudioTail", "TrailingSound must finish by itself; a looping tail would remain after the effect ends."));
			Result = EDataValidationResult::Invalid;
		}
	}
	else if (AudioBackend == EWuwaEffectAudioBackend::WwiseEvent)
	{
		if (!AudioEvent.IsValid() || (!TrailingAudioEvent.IsNull() && !TrailingAudioEvent.IsValid()) || FadeOutCurve < 0)
		{
			Context.AddError(NSLOCTEXT("WuwaEffectModel", "InvalidWwiseEvent", "WwiseEvent requires a valid AudioEvent asset path, an optional valid trailing event path, and a non-negative fade curve identifier."));
			Result = EDataValidationResult::Invalid;
		}
	}
	else
	{
		Context.AddError(NSLOCTEXT("WuwaEffectModel", "InvalidAudioBackend", "AudioBackend is not a supported resource selection."));
		Result = EDataValidationResult::Invalid;
	}

	if (!FMath::IsFinite(FadeOutTime) || FadeOutTime < 0.f)
	{
		Context.AddError(NSLOCTEXT("WuwaEffectModel", "InvalidAudioFadeTime", "FadeOutTime must be finite and non-negative."));
		Result = EDataValidationResult::Invalid;
	}
	for (const FVector& Offset : LocationOffsets)
	{
		if (Offset.ContainsNaN())
		{
			Context.AddError(NSLOCTEXT("WuwaEffectModel", "InvalidAudioLocationOffset", "Audio location offsets must contain finite coordinates."));
			Result = EDataValidationResult::Invalid;
			break;
		}
	}
	return Result;
}
#endif
