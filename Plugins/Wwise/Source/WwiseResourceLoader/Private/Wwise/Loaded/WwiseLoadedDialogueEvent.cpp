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

#include "Wwise/Loaded/WwiseLoadedDialogueEvent.h"
#include "Wwise/Stats/ResourceLoader.h"

#include <inttypes.h>


FWwiseLoadedDialogueEventInfo::FWwiseLoadedDialogueEventInfo(const FWwiseLocalizedDialogueEventCookedData& InDynamicEvent,
	const FWwiseLanguageCookedData& InLanguage):
	LocalizedDialogueEventCookedData(InDynamicEvent),
	LanguageRef(InLanguage)
{}

bool FWwiseLoadedDialogueEventInfo::FLoadedData::IsLoaded() const
{
	return bLoadedAudioNodes || LoadedSoundBanks.Num() > 0;
}

FString FWwiseLoadedDialogueEventInfo::GetDebugString() const
{
	if (const auto* CookedData = LocalizedDialogueEventCookedData.DialogueEventLanguageMap.Find(LanguageRef))
	{
		return FString::Printf(TEXT("%s in language %s (%" PRIu32 ")"),
			*CookedData->GetDebugString(),
			*LanguageRef.LanguageName.ToString(), LanguageRef.LanguageId);
	}
	else
	{
		return FString::Printf(TEXT("DynamicEvent %s (%" PRIu32 ") unset for language %s (%" PRIu32 ")"),
			*LocalizedDialogueEventCookedData.DebugName.ToString(), LocalizedDialogueEventCookedData.DialogueEventId,
			*LanguageRef.LanguageName.ToString(), LanguageRef.LanguageId);
	}
}

FWwiseLoadedDialogueEventInfo::FWwiseLoadedDialogueEventInfo(const FWwiseLoadedDialogueEventInfo& InOriginal):
	LocalizedDialogueEventCookedData(InOriginal.LocalizedDialogueEventCookedData),
	LanguageRef(InOriginal.LanguageRef)
{
}
