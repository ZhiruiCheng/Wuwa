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
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Wwise/Info/WwiseDialogueEventInfo.h"

#include "WwiseDynamicEventInfoLibrary.generated.h"

UCLASS()
class WWISERESOURCELOADER_API UWwiseDynamicEventInfoLibrary: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
		
public:
	UFUNCTION(BlueprintPure, Category = "Wwise|DynamicEventInfo", Meta = (BlueprintThreadSafe, DisplayName = "Make DynamicEventInfo"))
	static
	UPARAM(DisplayName="DynamicEvent Info") FWwiseDialogueEventInfo
	MakeStruct(
		const FGuid& WwiseGuid,
		int32 WwiseShortId,
		const FString& WwiseName,
		EWwiseAudioNodeLoading AudioNodeLoading,
		EWwiseAssetDestroyOptions DestroyOptions,
		int32 HardCodedSoundBankShortId = 0)
	{
		return FWwiseDialogueEventInfo(WwiseGuid, (uint32)WwiseShortId, FName(WwiseName), AudioNodeLoading, DestroyOptions, (uint32)HardCodedSoundBankShortId);
	}

	UFUNCTION(BlueprintPure, Category = "Wwise|DynamicEventInfo", Meta = (BlueprintThreadSafe, DisplayName = "Break DynamicEventInfo"))
	static void
	BreakStruct(
		UPARAM(DisplayName="DynamicEvent Info") FWwiseDialogueEventInfo Ref,
		FGuid& OutWwiseGuid,
		int32& OutWwiseShortId,
		FString& OutWwiseName,
		EWwiseAudioNodeLoading& AudioNodeLoading,
		EWwiseAssetDestroyOptions& OutDestroyOptions,
		int32& OutHardCodedSoundBankShortId)
	{
		OutWwiseGuid = Ref.WwiseGuid;
		OutWwiseShortId = (int32)Ref.WwiseShortId;
		OutWwiseName = Ref.WwiseName.ToString();
		AudioNodeLoading = Ref.AudioNodeLoading;
		OutDestroyOptions = Ref.DestroyOptions;
		OutHardCodedSoundBankShortId = (int32)Ref.HardCodedSoundBankShortId;
	}

	UFUNCTION(BlueprintPure, Category = "Wwise|DynamicEvent Info", Meta = (BlueprintThreadSafe))
	static
	UPARAM(DisplayName="GUID") FGuid
	GetWwiseGuid(
		UPARAM(DisplayName="DynamicEvent Info") const FWwiseDialogueEventInfo& Ref)
	{
		return Ref.WwiseGuid;
	}
	
	UFUNCTION(BlueprintPure, Category = "Wwise|DynamicEvent Info", Meta = (BlueprintThreadSafe))
	static
	UPARAM(DisplayName="Short Id") int32
	GetWwiseShortId(
		UPARAM(DisplayName="DynamicEvent Info") const FWwiseDialogueEventInfo& Ref)
	{
		return (int32)Ref.WwiseShortId;
	}

	UFUNCTION(BlueprintPure, Category = "Wwise|DynamicEvent Info", Meta = (BlueprintThreadSafe))
	static
	UPARAM(DisplayName="Name") FString
	GetWwiseName(
		UPARAM(DisplayName="DynamicEvent Info") const FWwiseDialogueEventInfo& Ref)
	{
		return Ref.WwiseName.ToString();
	}

	UFUNCTION(BlueprintPure, Category = "Wwise|DynamicEvent Info", Meta = (BlueprintThreadSafe))
	static
	UPARAM(DisplayName="Switch Container Loading") EWwiseAudioNodeLoading
	GetAudioNodeLoading(
		UPARAM(DisplayName="DynamicEvent Info") const FWwiseDialogueEventInfo& Ref)
	{
		return Ref.AudioNodeLoading;
	}

	UFUNCTION(BlueprintPure, Category = "Wwise|DynamicEvent Info", Meta = (BlueprintThreadSafe))
	static
	UPARAM(DisplayName="Destroy Options") EWwiseAssetDestroyOptions
	GetDestroyOptions(
		UPARAM(DisplayName="DynamicEvent Info") const FWwiseDialogueEventInfo& Ref)
	{
		return Ref.DestroyOptions;
	}

	UFUNCTION(BlueprintPure, Category = "Wwise|DynamicEvent Info", Meta = (BlueprintThreadSafe))
	static
	UPARAM(DisplayName="Short Id") int32
	GetHardCodedSoundBankShortId(
		UPARAM(DisplayName="DynamicEvent Info") const FWwiseDialogueEventInfo& Ref)
	{
		return (int32)Ref.HardCodedSoundBankShortId;
	}

	UFUNCTION(BlueprintPure, Category = "Wwise|DynamicEvent Info", Meta = (BlueprintThreadSafe))
	static
	UPARAM(DisplayName="Struct Out") FWwiseDialogueEventInfo
	SetWwiseGuid(
		UPARAM(DisplayName="DynamicEvent Info") const FWwiseDialogueEventInfo& Ref,
		const FGuid& WwiseGuid)
	{
		auto Result = Ref;
		Result.WwiseGuid = WwiseGuid;
		return Result;
	}
	
	UFUNCTION(BlueprintPure, Category = "Wwise|DynamicEvent Info", Meta = (BlueprintThreadSafe))
	static
	UPARAM(DisplayName="Struct Out") FWwiseDialogueEventInfo
	SetWwiseShortId(
		UPARAM(DisplayName="DynamicEvent Info") const FWwiseDialogueEventInfo& Ref,
		int32 WwiseShortId)
	{
		auto Result = Ref;
		Result.WwiseShortId = WwiseShortId;
		return Result;
	}

	UFUNCTION(BlueprintPure, Category = "Wwise|DynamicEvent Info", Meta = (BlueprintThreadSafe))
	static
	UPARAM(DisplayName="Struct Out") FWwiseDialogueEventInfo
	SetWwiseName(
		UPARAM(DisplayName="DynamicEvent Info") const FWwiseDialogueEventInfo& Ref,
		const FString& WwiseName)
	{
		auto Result = Ref;
		Result.WwiseName = FName(WwiseName);
		return Result;
	}

	UFUNCTION(BlueprintPure, Category = "Wwise|DynamicEvent Info", Meta = (BlueprintThreadSafe))
	static
	UPARAM(DisplayName="Struct Out") FWwiseDialogueEventInfo
	SetSwitchContainerLoading(
		UPARAM(DisplayName="DynamicEvent Info") const FWwiseDialogueEventInfo& Ref,
		const EWwiseAudioNodeLoading& AudioNodeLoading)
	{
		auto Result = Ref;
		Result.AudioNodeLoading = AudioNodeLoading;
		return Result;
	}

	UFUNCTION(BlueprintPure, Category = "Wwise|DynamicEvent Info", Meta = (BlueprintThreadSafe))
	static
	UPARAM(DisplayName="Struct Out") FWwiseDialogueEventInfo
	SetDestroyOptions(
		UPARAM(DisplayName="DynamicEvent Info") const FWwiseDialogueEventInfo& Ref,
		const EWwiseAssetDestroyOptions& DestroyOptions)
	{
		auto Result = Ref;
		Result.DestroyOptions = DestroyOptions;
		return Result;
	}

	UFUNCTION(BlueprintPure, Category = "Wwise|DynamicEvent Info", Meta = (BlueprintThreadSafe))
	static
	UPARAM(DisplayName="Struct Out") FWwiseDialogueEventInfo
	SetHardCodedSoundBankShortId(
		UPARAM(DisplayName="DynamicEvent Info") const FWwiseDialogueEventInfo& Ref,
		int32 HardCodedSoundBankShortId = 0)
	{
		auto Result = Ref;
		Result.HardCodedSoundBankShortId = (uint32)HardCodedSoundBankShortId;
		return Result;
	}
};
