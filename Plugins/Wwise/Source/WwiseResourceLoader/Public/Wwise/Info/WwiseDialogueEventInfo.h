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

#include "WwiseObjectInfo.h"
#include "Wwise/CookedData/WwiseDialogueEventCookedData.h"

#include "WwiseDialogueEventInfo.generated.h"

USTRUCT(BlueprintType, Meta = (Category = "Wwise", DisplayName = "Dynamic Event Info", HasNativeMake = "/Script/WwiseResourceLoader.WwiseDynamicEventInfoLibrary:MakeStruct", HasNativeBreak = "/Script/WwiseResourceLoader.WwiseDynamicEventInfoLibrary:BreakStruct"))
struct WWISERESOURCELOADER_API FWwiseDialogueEventInfo: public FWwiseObjectInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Info")
	EWwiseAudioNodeLoading AudioNodeLoading = EWwiseAudioNodeLoading::AlwaysLoad;

	UPROPERTY(EditAnywhere, Category = "Info")
	EWwiseAssetDestroyOptions DestroyOptions = EWwiseAssetDestroyOptions::StopEventOnDestroy;

	FWwiseDialogueEventInfo() :
		FWwiseObjectInfo(),
		AudioNodeLoading(EWwiseAudioNodeLoading::AlwaysLoad),
		DestroyOptions(EWwiseAssetDestroyOptions::StopEventOnDestroy)
	{}

	FWwiseDialogueEventInfo(const FWwiseDialogueEventInfo& InDialogueEventInfo):
		FWwiseObjectInfo(InDialogueEventInfo),
		AudioNodeLoading(InDialogueEventInfo.AudioNodeLoading),
		DestroyOptions(InDialogueEventInfo.DestroyOptions)
	{}

	FWwiseDialogueEventInfo(
		const FGuid& InWwiseGuid,
		int32 InWwiseShortId,
		const FString& InWwiseName,
		EWwiseAudioNodeLoading InAudioNodeLoading = EWwiseAudioNodeLoading::AlwaysLoad,
		EWwiseAssetDestroyOptions InDestroyOptions = EWwiseAssetDestroyOptions::StopEventOnDestroy,
		uint32 InHardCodedSoundBankShortId = 0) :
		FWwiseObjectInfo(InWwiseGuid, InWwiseShortId, InWwiseName),
		AudioNodeLoading(InAudioNodeLoading),
		DestroyOptions(InDestroyOptions)
	{}

	FWwiseDialogueEventInfo(
		const FGuid& InWwiseGuid,
		int32 InWwiseShortId,
		const FName& InWwiseName,
		EWwiseAudioNodeLoading InAudioNodeLoading = EWwiseAudioNodeLoading::AlwaysLoad,
		EWwiseAssetDestroyOptions InDestroyOptions = EWwiseAssetDestroyOptions::StopEventOnDestroy,
		uint32 InHardCodedSoundBankShortId = 0) :
		FWwiseObjectInfo(InWwiseGuid, InWwiseShortId, InWwiseName),
		AudioNodeLoading(InAudioNodeLoading),
		DestroyOptions(InDestroyOptions)
	{}

	FWwiseDialogueEventInfo(uint32 InWwiseShortId, const FString& InWwiseName) :
		FWwiseObjectInfo(InWwiseShortId, InWwiseName),
		AudioNodeLoading(EWwiseAudioNodeLoading::AlwaysLoad),
		DestroyOptions(EWwiseAssetDestroyOptions::StopEventOnDestroy)
	{}

	FWwiseDialogueEventInfo(uint32 InWwiseShortId, const FName& InWwiseName) :
		FWwiseObjectInfo(InWwiseShortId, InWwiseName),
		AudioNodeLoading(EWwiseAudioNodeLoading::AlwaysLoad),
		DestroyOptions(EWwiseAssetDestroyOptions::StopEventOnDestroy)
	{}

	FWwiseDialogueEventInfo(uint32 InWwiseShortId) :
		FWwiseObjectInfo(InWwiseShortId),
		AudioNodeLoading(EWwiseAudioNodeLoading::AlwaysLoad),
		DestroyOptions(EWwiseAssetDestroyOptions::StopEventOnDestroy)
	{}
};

inline uint32 GetTypeHash(const FWwiseDialogueEventInfo& InValue)
{
	return HashCombine(HashCombine(HashCombine(
		GetTypeHash(InValue.WwiseGuid),
		GetTypeHash(InValue.WwiseShortId)),
		GetTypeHash(InValue.WwiseName)),
		GetTypeHash(InValue.HardCodedSoundBankShortId));
}