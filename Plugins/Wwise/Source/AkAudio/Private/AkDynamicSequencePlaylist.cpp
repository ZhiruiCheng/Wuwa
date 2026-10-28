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

#include "AkDynamicSequencePlaylist.h"

#include "Wwise/Stats/AkAudio.h"

#include <inttypes.h>

void UAkDynamicSequencePlaylist::Commit()
{
	SCOPED_AKAUDIO_EVENT_2(TEXT("UAkDynamicSequencePlaylist::Commit"));
	UE_LOG(LogAkAudio, VeryVerbose, TEXT("UAkDynamicSequencePlaylist::Commit PlayingID %" PRIi32), PlayingID);
	if (UNLIKELY(!Playlist)) return;
	if (UNLIKELY(!bLocked)) return;

	auto* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) return;

	Playlist->RemoveAll(Items.Num());
	for (const auto& Item : Items)
	{
		if (!Item)
		{
			continue;
		}
		if (!Item->AudioNode)
		{
			continue;
		}

		if (!Item->AudioNode->IsLoaded())
		{
			Item->AudioNode->LoadData();
		}
		Playlist->Enqueue(Item->AudioNode->AudioNodeCookedData.AudioNodeId,
			(AkTimeMs)Item->DelayMs,
			Item);
	}

	SoundEngine->DynamicSequence->UnlockPlaylist(Playlist);
	bLocked = false;
}

void UAkDynamicSequencePlaylist::CommitWithoutChanges()
{
	SCOPED_AKAUDIO_EVENT_2(TEXT("UAkDynamicSequencePlaylist::CommitWithoutChanges"));
	UE_LOG(LogAkAudio, VeryVerbose, TEXT("UAkDynamicSequencePlaylist::CommitWithoutChanges PlayingID %" PRIi32), PlayingID);
	if (UNLIKELY(!Playlist)) return;
	if (UNLIKELY(!bLocked)) return;

	auto* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) return;

	SoundEngine->DynamicSequence->UnlockPlaylist(Playlist);
	bLocked = false;
}

void UAkDynamicSequencePlaylist::AddAndCommit(UAkDynamicSequencePlaylistItem* Item)
{
	SCOPED_AKAUDIO_EVENT_2(TEXT("UAkDynamicSequencePlaylist::AddAndCommit"));
	UE_LOG(LogAkAudio, VeryVerbose, TEXT("UAkDynamicSequencePlaylist::AddAndCommit PlayingID %" PRIi32), PlayingID);
	auto* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) return;
	if (UNLIKELY(!Playlist)) return;
	if (UNLIKELY(!bLocked)) return;
	if (UNLIKELY(Item == nullptr))
	{
		UE_LOG(LogAkAudio, Warning, TEXT("AddAndCommit was given NULL Items. Unlocking the sequence without commiting"));
		CommitWithoutChanges();
		return;
	}

	Items.Add(Item);
	Playlist->Enqueue(Item->AudioNode->AudioNodeCookedData.AudioNodeId,
		(AkTimeMs)Item->DelayMs,
		Item);

	SoundEngine->DynamicSequence->UnlockPlaylist(Playlist);
	bLocked = false;
}

void UAkDynamicSequencePlaylist::OverwriteAndCommit(TArray<UAkDynamicSequencePlaylistItem*> InItems)
{
	SCOPED_AKAUDIO_EVENT_2(TEXT("UAkDynamicSequencePlaylist::AddAndCommit"));
	UE_LOG(LogAkAudio, VeryVerbose, TEXT("UAkDynamicSequencePlaylist::AddAndCommit PlayingID %" PRIi32), PlayingID);
	if (UNLIKELY(!Playlist)) return;
	if (UNLIKELY(!bLocked)) return;

	auto* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) return;

	Items.Empty();
	Items = InItems;
	Commit();
}
