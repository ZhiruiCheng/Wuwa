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

#include "Wwise/CookedData/WwiseSoundBankCookedData.h"
#include "Wwise/CookedData/WwiseExternalSourceCookedData.h"
#include "Wwise/CookedData/WwiseMediaCookedData.h"
#include "Wwise/WwiseUnrealVersion.h"

#include "WwiseAudioNodeCookedData.generated.h"

UENUM(BlueprintType)
enum class EWwiseAssetDestroyOptions : uint8
{
	StopEventOnDestroy,
	WaitForEventEnd
};

UENUM(BlueprintType)
enum class EWwiseAudioNodeLoading : uint8
{
	AlwaysLoad UMETA(DisplayName = "Always Load Media"),
	LoadOnReference UMETA(DisplayName = "Load Media Only When Referenced"),
	LoadOnResolve UMETA(DisplayName = "Load Media Only When Resolved"),
	LoadOnEnqueue UMETA(DisplayName = "Load Media Only When Enqueued")
};

USTRUCT(BlueprintType)
struct WWISERESOURCELOADER_API FWwiseAudioNodeCookedData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly, Category = "Wwise")
	int32 AudioNodeId; 

	UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly, Category = "Wwise")
	TArray<FWwiseSoundBankCookedData> SoundBanks;

	UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly, Category = "Wwise")
	TArray<FWwiseMediaCookedData> Media;
	
	UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly, Category = "Wwise")
	TArray<FWwiseExternalSourceCookedData> ExternalSources;

	UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly, Category = "Wwise")
	EWwiseAudioNodeLoading AudioNodeLoading = EWwiseAudioNodeLoading::AlwaysLoad;
	
	UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly, Category = "Wwise")
	EWwiseAssetDestroyOptions DestroyOptions = EWwiseAssetDestroyOptions::StopEventOnDestroy;

	/**
	 * @brief Optional debug name. Can be empty in release, contain the name, or the full path of the asset.
	 */
	UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly, Category = "Wwise")
	FName DebugName;

	FWwiseAudioNodeCookedData();

	void Serialize(FArchive& Ar);
	void SerializeBulkData(FArchive& Ar, const FWwisePackagedFileSerializationOptions& Options);
#if WITH_EDITORONLY_DATA && UE_5_5_OR_LATER
	void GetPlatformCookDependencies(FWwiseCookEventContext& SaveContext, FCbWriter& Writer) const;
#endif
	
	bool operator==(const FWwiseAudioNodeCookedData& Rhs) const;

	FString GetDebugString() const;
	bool IsInitialized() const
	{
		return SoundBanks.Num() > 0 || Media.Num() > 0 || ExternalSources.Num() > 0;
	}
};
