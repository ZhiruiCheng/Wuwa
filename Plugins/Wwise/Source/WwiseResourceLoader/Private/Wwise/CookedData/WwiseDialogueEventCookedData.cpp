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

#include "Wwise/CookedData/WwiseDialogueEventCookedData.h"

#include "Wwise/Stats/ResourceLoader.h"

#include <inttypes.h>

#if WITH_EDITORONLY_DATA && UE_5_5_OR_LATER
#include "Serialization/CompactBinaryWriter.h"
#endif

FWwiseDialogueEventCookedData::FWwiseDialogueEventCookedData():
	DialogueEventId(0),
	SoundBanks(),
	AudioNodes()
{}

void FWwiseDialogueEventCookedData::Serialize(FArchive& Ar)
{
	UStruct* Struct = StaticStruct();
	check(Struct);

	if (Ar.WantBinaryPropertySerialization())
	{
		Struct->SerializeBin(Ar, this);
	}
	else
	{
		Struct->SerializeTaggedProperties(Ar, (uint8*)this, Struct, nullptr);
	}
}

void FWwiseDialogueEventCookedData::SerializeBulkData(FArchive& Ar, const FWwisePackagedFileSerializationOptions& Options)
{
	for (auto& SoundBank : SoundBanks)
	{
		SoundBank.SerializeBulkData(Ar, Options);
	}
	for (auto& AudioNode : AudioNodes)
	{
		AudioNode.Value.SerializeBulkData(Ar, Options);
	}
}

#if WITH_EDITORONLY_DATA && UE_5_5_OR_LATER
void FWwiseDialogueEventCookedData::GetPlatformCookDependencies(FWwiseCookEventContext& SaveContext, FCbWriter& Writer) const
{
	Writer << "DynEvent";
	Writer.BeginObject();

	Writer << "Id" << DialogueEventId;

	Writer << "SBs";
	Writer.BeginArray();
	for (auto& SoundBank : SoundBanks)
	{
		SoundBank.GetPlatformCookDependencies(SaveContext, Writer);
	}
	Writer.EndArray();

	Writer << "ANs";
	Writer.BeginArray();
	for (auto& AudioNode : AudioNodes)
	{
		Writer.BeginObject();
		AudioNode.Key.GetPlatformCookDependencies(SaveContext, Writer);
		AudioNode.Value.GetPlatformCookDependencies(SaveContext, Writer);
		Writer.EndObject();
	}
	Writer.EndArray();
	Writer.EndObject();
}
#endif

FString FWwiseDialogueEventCookedData::GetDebugString() const
{
	bool bFirst = true;
	auto Result = FString::Printf(TEXT("DynamicEvent %s (%" PRIu32 ")"), *DebugName.ToString(), DialogueEventId);
	if (SoundBanks.Num() > 0)
	{
		if (bFirst)
		{
			Result += TEXT(" with ");
			bFirst = false;
		}
		else
		{
			Result += TEXT(", ");
		}
		Result += FString::Printf(TEXT("%d SoundBank%s"), SoundBanks.Num(), SoundBanks.Num() > 1 ? TEXT("s") : TEXT(""));
	}
	if (AudioNodes.Num() > 0)
	{
		if (bFirst)
		{
			Result += TEXT(" with ");
			bFirst = false;
		}
		else
		{
			Result += TEXT(", ");
		}
		Result += FString::Printf(TEXT("%d AudioNode%s"), AudioNodes.Num(), AudioNodes.Num() > 1 ? TEXT("s") : TEXT(""));
	}
	return Result;
}
