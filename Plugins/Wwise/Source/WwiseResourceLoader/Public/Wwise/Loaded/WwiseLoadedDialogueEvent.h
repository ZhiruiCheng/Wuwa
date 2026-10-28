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

#include "Wwise/CookedData/WwiseLocalizedDialogueEventCookedData.h"
#include "Wwise/Loaded/WwiseLoadedGroupValue.h"

struct WWISERESOURCELOADER_API FWwiseLoadedDialogueEventInfo
{
	FWwiseLoadedDialogueEventInfo(const FWwiseLocalizedDialogueEventCookedData& InDynamicEvent, const FWwiseLanguageCookedData& InLanguage);
	FWwiseLoadedDialogueEventInfo& operator=(const FWwiseLoadedDialogueEventInfo&) = delete;

	const FWwiseLocalizedDialogueEventCookedData LocalizedDialogueEventCookedData;
	FWwiseLanguageCookedData LanguageRef;

	struct WWISERESOURCELOADER_API FLoadedData
	{
		FLoadedData() {}
		FLoadedData(const FLoadedData&) = delete;
		FLoadedData& operator=(const FLoadedData&) = delete;

		TArray<const FWwiseSoundBankCookedData*> LoadedSoundBanks;
		std::atomic<int> IsProcessing{0};

		bool bLoadedAudioNodes = false;

		bool IsLoaded() const;
	} LoadedData;

	FString GetDebugString() const;

private:
	friend class TDoubleLinkedList<FWwiseLoadedDialogueEventInfo>::TDoubleLinkedListNode;
	FWwiseLoadedDialogueEventInfo(const FWwiseLoadedDialogueEventInfo& InOriginal);
};

using FWwiseLoadedDialogueEventList = TDoubleLinkedList<FWwiseLoadedDialogueEventInfo>;
using FWwiseLoadedDialogueEventListNode = FWwiseLoadedDialogueEventList::TDoubleLinkedListNode;
using FWwiseLoadedDialogueEventPtr = FWwiseLoadedDialogueEventListNode*;
using FWwiseLoadedDialogueEventPtrAtomic = std::atomic<FWwiseLoadedDialogueEventPtr>;
using FWwiseLoadedDialogueEventPromise = TWwisePromise<FWwiseLoadedDialogueEventPtr>;
using FWwiseLoadedDialogueEventFuture = TWwiseFuture<FWwiseLoadedDialogueEventPtr>;
