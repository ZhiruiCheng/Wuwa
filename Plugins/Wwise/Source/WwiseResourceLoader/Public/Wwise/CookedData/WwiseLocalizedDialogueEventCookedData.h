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

#include "Wwise/CookedData/WwiseDialogueEventCookedData.h"
#include "Wwise/CookedData/WwiseGroupValueCookedData.h"
#include "Wwise/CookedData/WwiseLanguageCookedData.h"

#include "WwiseLocalizedDialogueEventCookedData.generated.h"

/**
 * Group argument requirement in a dialogue event. Used for validation.
 */
USTRUCT(BlueprintType)
struct WWISERESOURCELOADER_API FWwiseDialogueArgumentItem
{
	GENERATED_BODY()

public:
	FWwiseDialogueArgumentItem() = default;
	static FWwiseDialogueArgumentItem FromGroupValue(const FWwiseGroupValueCookedData& GroupValue)
	{
		FWwiseDialogueArgumentItem Result;
		Result.Type = GroupValue.Type;
		Result.GroupId = GroupValue.GroupId;
		Result.DebugName = GroupValue.DebugName;
		return Result;
	}
	
	UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly, Category = "Wwise")
	EWwiseGroupType Type{ EWwiseGroupType::Unknown };

	/**
	 * Short ID for the Group.
	*/
	UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly, Category = "Wwise")
	int32 GroupId{ 0 };

	/**
	 * Optional debug name. Can be empty in release, contain the name, or the full path of the asset.
	*/
	UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly, Category = "Wwise")
	FName DebugName;

	bool operator==(const FWwiseDialogueArgumentItem& Rhs) const
	{
		return Type == Rhs.Type
			&& GroupId == Rhs.GroupId;
	}
};
inline uint32 GetTypeHash(const FWwiseDialogueArgumentItem& Key)
{
	return HashCombine(GetTypeHash(Key.Type), GetTypeHash(Key.GroupId));
}


/**
 * Group argument requirement's position in a dialogue event.
 */
USTRUCT(BlueprintType)
struct WWISERESOURCELOADER_API FWwiseDialogueArgumentPosition
{
	GENERATED_BODY()

public:
	FWwiseDialogueArgumentPosition() = default;
	
	UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly, Category = "Wwise")
	TArray<int> Positions;
	bool operator==(const FWwiseDialogueArgumentPosition& Rhs) const
	{
		if (Positions.Num() != Rhs.Positions.Num())
		{
			return false;
		}
		for (int i = 0; i < Positions.Num(); i++)
		{
			if (Positions[i] != Rhs.Positions[i])
			{
				return false;
			}
		}
		return true;
	}
	bool operator !=(const FWwiseDialogueArgumentPosition& Rhs) const
	{
		return !(*this == Rhs);
	}
};

/**
 * Dialogue Event, as cooked in the final product.
 */
USTRUCT(BlueprintType)
struct WWISERESOURCELOADER_API FWwiseLocalizedDialogueEventCookedData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly, Category = "Wwise")
	TMap<FWwiseLanguageCookedData, FWwiseDialogueEventCookedData> DialogueEventLanguageMap;

	/**
	 * Optional debug name. Can be empty in release, contain the name, or the full path of the asset.
	*/
	UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly, Category = "Wwise")
	FName DebugName;
	
	/**
	* Short ID for the Dialogue Event.
	*/
	UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly, Category = "Wwise")
	int32 DialogueEventId{ 0 };

	/**
	 * Enumeration of the arguments required for this dialogue event, pointing to the position where it should be stored.
	 */
	UPROPERTY(BlueprintReadOnly, VisibleInstanceOnly, Category = "Wwise")
	TMap<FWwiseDialogueArgumentItem, FWwiseDialogueArgumentPosition> RequiredArguments;

	FWwiseLocalizedDialogueEventCookedData();

	void Serialize(FArchive& Ar);
	void SerializeBulkData(FArchive& Ar, const FWwisePackagedFileSerializationOptions& Options);
#if WITH_EDITORONLY_DATA && UE_5_5_OR_LATER
	void GetPlatformCookDependencies(FWwiseCookEventContext& SaveContext, FCbWriter& Writer) const;
#endif
};
