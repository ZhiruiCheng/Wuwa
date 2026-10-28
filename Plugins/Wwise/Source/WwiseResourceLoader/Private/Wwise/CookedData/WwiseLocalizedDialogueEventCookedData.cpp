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

#include "Wwise/CookedData/WwiseLocalizedDialogueEventCookedData.h"

#if WITH_EDITORONLY_DATA && UE_5_5_OR_LATER
#include "Serialization/CompactBinaryWriter.h"
#endif

FWwiseLocalizedDialogueEventCookedData::FWwiseLocalizedDialogueEventCookedData():
	DialogueEventLanguageMap(),
	DebugName(),
	DialogueEventId(0)
{}

void FWwiseLocalizedDialogueEventCookedData::Serialize(FArchive& Ar)
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

void FWwiseLocalizedDialogueEventCookedData::SerializeBulkData(FArchive& Ar, const FWwisePackagedFileSerializationOptions& InOptions)
{
	for (auto& DynamicEvent : DialogueEventLanguageMap)
	{
		auto Options(InOptions);
		if(DynamicEvent.Key != DynamicEvent.Key.Sfx)
		{
			Options.bOptional = true;
		}
		DynamicEvent.Value.SerializeBulkData(Ar, Options);
	}
}

#if WITH_EDITORONLY_DATA && UE_5_5_OR_LATER
void FWwiseLocalizedDialogueEventCookedData::GetPlatformCookDependencies(FWwiseCookEventContext& SaveContext, FCbWriter& Writer) const
{
	Writer << "LocDynEvents";
	Writer.BeginObject();
	Writer << "DynEventId" << DialogueEventId;

	{
		Writer << "Langs";
		Writer.BeginArray();
		TArray<FWwiseLanguageCookedData> Languages;
		DialogueEventLanguageMap.GetKeys(Languages);
		Languages.Sort();
	
		for (const auto& Language : Languages)
		{
			DialogueEventLanguageMap[Language].GetPlatformCookDependencies(SaveContext, Writer);
		}
		Writer.EndArray();
	}

	Writer.EndObject();
}
#endif
