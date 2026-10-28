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

#include "Wwise/CookedData/WwiseAudioNodeCookedData.h"

#include "Wwise/Stats/ResourceLoader.h"

#include <inttypes.h>

#if WITH_EDITORONLY_DATA && UE_5_5_OR_LATER
#include "Serialization/CompactBinaryWriter.h"
#endif

FWwiseAudioNodeCookedData::FWwiseAudioNodeCookedData():
	AudioNodeId(AK_INVALID_UNIQUE_ID),
	SoundBanks(),
	Media(),
	ExternalSources()
{}

void FWwiseAudioNodeCookedData::Serialize(FArchive& Ar)
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

void FWwiseAudioNodeCookedData::SerializeBulkData(FArchive& Ar, const FWwisePackagedFileSerializationOptions& InOptions)
{
	// Switch Container Leaves are optional
	auto Options(InOptions);
	Options.bOptional = true;
	Options.ExtraLog += ", Switch Container";
	
	for (auto& SoundBank : SoundBanks)
	{
		SoundBank.SerializeBulkData(Ar, Options);
	}
	for (auto& MediaItem : Media)
	{
		MediaItem.SerializeBulkData(Ar, Options);
	}
}

#if WITH_EDITORONLY_DATA && UE_5_5_OR_LATER
void FWwiseAudioNodeCookedData::GetPlatformCookDependencies(FWwiseCookEventContext& SaveContext, FCbWriter& Writer) const
{
	Writer << "AN";
	Writer.BeginObject();
	Writer << "Id" << AudioNodeId;

	Writer << "SBs";
	Writer.BeginArray();
	for (const auto& SoundBank : SoundBanks)
	{
		SoundBank.GetPlatformCookDependencies(SaveContext, Writer);
	}
	Writer.EndArray();

	Writer << "Ms";
	Writer.BeginArray();
	for (const auto& MediaItem : Media)
	{
		MediaItem.GetPlatformCookDependencies(SaveContext, Writer);
	}
	Writer.EndArray();

	Writer << "ESs";
	Writer.BeginArray();
	for (const auto& ExternalSource : ExternalSources)
	{
		ExternalSource.GetPlatformCookDependencies(SaveContext, Writer);
	}
	Writer.EndArray();
	Writer.EndObject();
}
#endif

bool FWwiseAudioNodeCookedData::operator==(const FWwiseAudioNodeCookedData& Rhs) const
{
	if (AudioNodeId != AK_INVALID_UNIQUE_ID)
	{
		return AudioNodeId == Rhs.AudioNodeId;
	}
	
	if (Media.Num() != Rhs.Media.Num() ||
		ExternalSources.Num() != Rhs.ExternalSources.Num() ||
		SoundBanks.Num() != Rhs.SoundBanks.Num())
	{
		return false;
	}
	
	for (int i = 0; i < Media.Num(); ++i)
	{
		if (Media[i] != Rhs.Media[i])
		{
			return false;
		}
	}
	for (int i = 0;i < ExternalSources.Num(); ++i)
	{
		if (ExternalSources[i] != Rhs.ExternalSources[i])
		{
			return false;
		}
	}
	for (int i = 0; i < SoundBanks.Num(); ++i)
	{
		if (SoundBanks[i] != Rhs.SoundBanks[i])
		{
			return false;
		}
	}
	return true;
}

FString FWwiseAudioNodeCookedData::GetDebugString() const
{
	FString Result{ TEXT("AudioNode") };
	if (!DebugName.IsNone())
	{
		Result += TEXT(" ") + DebugName.ToString();
	}
	if (AudioNodeId != AK_INVALID_UNIQUE_ID)
	{
		Result += TEXT(" (") + FString::FromInt(AudioNodeId) + TEXT(")");
	}

	bool bFirst = true;
	if (SoundBanks.Num() > 0)
	{
		if (bFirst)
		{
			Result += TEXT(" with ");
			bFirst = false;
		}
		else
		{
			Result += TEXT(" and ");
		}
		Result += FString::Printf(TEXT("%d SoundBank%s"), SoundBanks.Num(), SoundBanks.Num() > 1 ? TEXT("s") : TEXT(""));
	}
	if (Media.Num() > 0)
	{
		if (bFirst)
		{
			Result += TEXT(" with ");
			bFirst = false;
		}
		else if (ExternalSources.Num() > 0)
		{
			Result += TEXT(", ");
		}
		else
		{
			Result += TEXT(" and ");
		}
		Result += FString::Printf(TEXT("%d Media"), Media.Num());
	}
	if (ExternalSources.Num() > 0)
	{
		if (bFirst)
		{
			Result += TEXT(" with ");
			bFirst = false;
		}
		else
		{
			Result += TEXT(" and ");
		}
		Result += FString::Printf(TEXT("%d ExtSrc%s"), ExternalSources.Num(), ExternalSources.Num() > 1 ? TEXT("s") : TEXT(""));
	}
	return Result;
}
