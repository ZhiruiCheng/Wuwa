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

#include "AkDynamicSequence.h"

#include "AkAudioNode.h"
#include "AkDialogueEvent.h"
#include "AkDynamicSequenceBlueprintFunctionLibrary.h"
#include "AkDynamicSequencePlaylist.h"
#include "Wwise/WwiseSoundEngineUtils.h"
#include "Wwise/API/WwiseSoundEngineAPI.h"
#include "Wwise/Stats/AkAudio.h"

#include <inttypes.h>


void UAkDynamicSequence::BeginDestroy()
{
	UObject::BeginDestroy();
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}
	if (PlayingID == AK_INVALID_PLAYING_ID)
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkDynamicSequence::BeginDestroy was called on a DynamicSequence with an invalid PlayingID."));
		return;
	}

	SCOPED_AKAUDIO_EVENT(TEXT("UAkDynamicSequence::BeginDestroy"));
	UE_LOG(LogAkAudio, VeryVerbose, TEXT("UAkDynamicSequence::BeginDestroy %s. Closing PlayingID %" PRIi32), *GetName(), PlayingID);
	Stop();

	auto* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) return;

	const auto Result = SoundEngine->DynamicSequence->Close(PlayingID);
	UE_CLOG(Result != AK_Success, LogAkAudio, Log, TEXT("UAkDynamicSequence::BeginDestroy %s. Closing PlayingID %" PRIi32 " failed: %s"), *GetName(), PlayingID,
		WwiseUnrealHelper::GetResultString(Result));
}

bool UAkDynamicSequence::PostPlaylistItem(UAkDynamicSequencePlaylistItem* PlaylistItem, bool bPlayImmediately)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkDynamicSequence::PostPlaylistItem"));
	if (!IsValid(PlaylistItem))
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkDynamicSequence::PostPlaylistItem %s No Playlist Item"),
			*GetName());
		return false;
	}
	if (!IsValid(PlaylistItem->AudioNode))
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkDynamicSequence::PostPlaylistItem %s No AudioNode in Playlist Item %s"),
			*GetName(), *PlaylistItem->GetName());
		return false;
	}

	auto* CurrentPlaylist{ ModifyPlaylist() };
	if (UNLIKELY(!CurrentPlaylist))
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkDynamicSequence::PostPlaylistItem %s: Could not modify Playlist to add %s"),
			*GetName(), *PlaylistItem->AudioNode->AudioNodeCookedData.GetDebugString());
		return false;
	}
	CurrentPlaylist->AddAndCommit(PlaylistItem);
	if (bPlayImmediately)
	{
		UE_LOG(LogAkAudio, Verbose, TEXT("UAkDynamicSequence::PostPlaylistItem %s: Added %s to playlist and started playing."),
			*GetName(), *PlaylistItem->AudioNode->AudioNodeCookedData.GetDebugString());
		Play();
	}
	else
	{
		UE_LOG(LogAkAudio, Verbose, TEXT("UAkDynamicSequence::PostPlaylistItem %s: Added %s to playlist."),
			*GetName(), *PlaylistItem->AudioNode->AudioNodeCookedData.GetDebugString());
	}
	return true;
}

bool UAkDynamicSequence::PostAudioNode(UAkAudioNode* AudioNode, bool bPlayImmediately)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkDynamicSequence::PostAudioNode"));
	auto* Item{ UAkDynamicSequenceBlueprintFunctionLibrary::CreateDynamicSequencePlaylistItemFromAudioNode(AudioNode, 0, nullptr) };
	if (UNLIKELY(!Item))
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkDynamicSequence::PostAudioNode %s AudioNode %s %" PRIu32 ": Could not create Playlist Item"),
			*GetName(), *AudioNode->GetName(), AudioNode->GetShortID());
		return false;
	}
	return PostPlaylistItem(Item, bPlayImmediately);
}

bool UAkDynamicSequence::PostDialogueEventInPlaylist(UAkDialogueEvent* DialogueEvent, const TArray<UAkGroupValue*>& Arguments, bool bOrderedPath,
	bool bPlayImmediately)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkDynamicSequence::PostDialogueEvent"));
	auto* Item{ UAkDynamicSequenceBlueprintFunctionLibrary::CreateDynamicSequencePlaylistItemFromDialogueEvent(DialogueEvent, Arguments, bOrderedPath, 0, nullptr) };
	if (UNLIKELY(!Item))
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkDynamicSequence::PostDialogueEvent %s %s %" PRIu32 ": Could not create Playlist Item"),
			*GetName(), *DialogueEvent->GetName(), DialogueEvent->GetShortID());
		return false;
	}
	return PostPlaylistItem(Item, bPlayImmediately);
}

EAkResult UAkDynamicSequence::Play(const FAkDynamicSequenceTransition OverrideTransition)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkDynamicSequence::Play"));
	UE_LOG(LogAkAudio, Verbose, TEXT("UAkDynamicSequence::Play %s PlayingID %" PRIi32), *GetName(), PlayingID);
	auto* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) return EAkResult::NotInitialized;

	FAkDynamicSequenceTransition Transition = OverrideTransition == FAkDynamicSequenceTransition() ? DynamicSequenceTransition : OverrideTransition;
	const auto Result = SoundEngine->DynamicSequence->Play(PlayingID, Transition.TransitionDurationMs, static_cast<AkCurveInterpolation>(Transition.FadeCurve));
	if (Result == AK_Success)
	{
		UE_LOG(LogAkAudio, VeryVerbose, TEXT("UAkDynamicSequence::Play %s PlayingID %" PRIi32": Play. Locking Dynamic Sequence."),
			*GetName(), PlayingID);

		if (State != EAkDynamicSequenceState::Playing)
		{
			State = EAkDynamicSequenceState::Playing;
			AddToRoot();
		}
	}
	
	return static_cast<EAkResult>(Result);
}

EAkResult UAkDynamicSequence::Pause(const FAkDynamicSequenceTransition OverrideTransition)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkDynamicSequence::Pause"));
	UE_LOG(LogAkAudio, Verbose, TEXT("UAkDynamicSequence::Pause %s PlayingID %" PRIi32), *GetName(), PlayingID);
	auto* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) return EAkResult::NotInitialized;

	FAkDynamicSequenceTransition Transition = OverrideTransition == FAkDynamicSequenceTransition() ? DynamicSequenceTransition : OverrideTransition;
	return static_cast<EAkResult>(
		SoundEngine->DynamicSequence->Pause(PlayingID, Transition.TransitionDurationMs, static_cast<AkCurveInterpolation>(Transition.FadeCurve))
			);
}

EAkResult UAkDynamicSequence::Resume(const FAkDynamicSequenceTransition OverrideTransition)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkDynamicSequence::Resume"));
	UE_LOG(LogAkAudio, Verbose, TEXT("UAkDynamicSequence::Resume %s PlayingID %" PRIi32), *GetName(), PlayingID);
	auto* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) return EAkResult::NotInitialized;
	
	FAkDynamicSequenceTransition Transition = OverrideTransition == FAkDynamicSequenceTransition() ? DynamicSequenceTransition : OverrideTransition;
	return static_cast<EAkResult>(
		SoundEngine->DynamicSequence->Resume(PlayingID, Transition.TransitionDurationMs, static_cast<AkCurveInterpolation>(Transition.FadeCurve))
			);
}

EAkResult UAkDynamicSequence::Stop(const FAkDynamicSequenceTransition OverrideTransition)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkDynamicSequence::Stop"));
	UE_LOG(LogAkAudio, Verbose, TEXT("UAkDynamicSequence::Stop %s PlayingID %" PRIi32 ". Unlocking Dynamic Sequence."), *GetName(), PlayingID);
	auto* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) return EAkResult::NotInitialized;
	
	FAkDynamicSequenceTransition Transition = OverrideTransition == FAkDynamicSequenceTransition() ? DynamicSequenceTransition : OverrideTransition;
	const auto Result = static_cast<EAkResult>(
		SoundEngine->DynamicSequence->Stop(PlayingID, Transition.TransitionDurationMs, static_cast<AkCurveInterpolation>(Transition.FadeCurve))
			);

	if (Result == EAkResult::Success)
	{
		State = EAkDynamicSequenceState::Stopping;
	}
	return Result;
}

EAkResult UAkDynamicSequence::Break()
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkDynamicSequence::Break"));
	UE_LOG(LogAkAudio, Verbose, TEXT("UAkDynamicSequence::Break %s PlayingID %" PRIi32), *GetName(), PlayingID);
	auto* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) return EAkResult::NotInitialized;
	
	const auto Result = static_cast<EAkResult>(
		SoundEngine->DynamicSequence->Break(PlayingID)
			);

	if (Result == EAkResult::Success)
	{
		State = EAkDynamicSequenceState::Stopping;
	}
	return Result;
}

EAkResult UAkDynamicSequence::Seek(int32 PositionMS, bool bSeekToNearestMarker)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkDynamicSequence::Seek"));
	UE_LOG(LogAkAudio, Verbose, TEXT("UAkDynamicSequence::Seek %s PlayingID %" PRIi32), *GetName(), PlayingID);
	auto* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) return EAkResult::NotInitialized;
	
	return static_cast<EAkResult>(
		SoundEngine->DynamicSequence->Seek(PlayingID, PositionMS, bSeekToNearestMarker)
			);
}

EAkResult UAkDynamicSequence::SeekPercent(float Percent, bool bSeekToNearestMarker)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkDynamicSequence::SeekPercent"));
	UE_LOG(LogAkAudio, Verbose, TEXT("UAkDynamicSequence::SeekPercent %s PlayingID %" PRIi32), *GetName(), PlayingID);
	auto* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) return EAkResult::NotInitialized;
	
	return static_cast<EAkResult>(
		SoundEngine->DynamicSequence->Seek(PlayingID, Percent, bSeekToNearestMarker)
			);
}

EAkResult UAkDynamicSequence::GetPauseTimes(int32& PauseTime, int32& Duration) const
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkDynamicSequence::GetPauseTimes"));
	UE_LOG(LogAkAudio, Verbose, TEXT("UAkDynamicSequence::GetPauseTimes %s PlayingID %" PRIi32), *GetName(), PlayingID);
	auto* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) return EAkResult::NotInitialized;

	AkUInt32 ReturnedPauseTime;
	AkUInt32 ReturnedDuration;
	const auto Result = static_cast<EAkResult>(
		SoundEngine->DynamicSequence->GetPauseTimes(PlayingID, ReturnedPauseTime, ReturnedDuration)
			);
	if (LIKELY(Result == EAkResult::Success))
	{
		PauseTime = (int32)ReturnedPauseTime;
		Duration = (int32)ReturnedDuration;
	}
	return Result;
}

EAkResult UAkDynamicSequence::GetPlayingItem(UAkAudioNode*& AudioNode) const
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkDynamicSequence::GetPlayingItem"));
	UE_LOG(LogAkAudio, Verbose, TEXT("UAkDynamicSequence::GetPlayingItem %s PlayingID %" PRIi32), *GetName(), PlayingID);
	auto* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) return EAkResult::NotInitialized;

	AkUniqueID AudioNodeID{ AK_INVALID_UNIQUE_ID };
	void* CustomInfo{ nullptr };
	const auto Result = static_cast<EAkResult>(
		SoundEngine->DynamicSequence->GetPlayingItem(PlayingID, AudioNodeID, CustomInfo)
			);
	if (UNLIKELY(Result != EAkResult::Success || AudioNodeID == AK_INVALID_UNIQUE_ID || CustomInfo == nullptr))
	{
		return Result;
	}

	auto Item = static_cast<UAkDynamicSequencePlaylistItem*>(CustomInfo);
	if (!Item)
	{
		AudioNode = nullptr;
		return Result;
	}
	
	AudioNode = Item->AudioNode;
	if (!AudioNode)
	{
		return Result;
	}
	
#if WITH_EDITORONLY_DATA
	if (AudioNode->AudioNodeInfo.WwiseShortId != AudioNodeID)
	{
		UE_LOG(LogAkAudio, Error, TEXT("UAkDynamicSequence::GetPlayingItem %s PlayingID %" PRIi32": Playing Item doesn't seem the same than was initially provided."),
			*GetName(), PlayingID);
		return EAkResult::InvalidParameter;
	}
#endif
	
	return Result;
}

UAkDynamicSequencePlaylist* UAkDynamicSequence::ModifyPlaylist()
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkDynamicSequence::ModifyPlaylist"));
	UE_LOG(LogAkAudio, Verbose, TEXT("UAkDynamicSequence::ModifyPlaylist %s PlayingID %" PRIi32), *GetName(), PlayingID);
	auto* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) return {};
	if (Playlist && UNLIKELY(Playlist->bLocked)) return {};

	auto* LockedPlaylist = SoundEngine->DynamicSequence->LockPlaylist(PlayingID);
	if (UNLIKELY(!LockedPlaylist)) return {};

	if (!Playlist)
	{
		Playlist = NewObject<UAkDynamicSequencePlaylist>();
	}
	UAkDynamicSequencePlaylist* Result = Playlist.Get();
	if (UNLIKELY(!Result))
	{
		UE_LOG(LogAkAudio, Error, TEXT("UAkDynamicSequence::ModifyPlaylist %s PlayingID %" PRIi32": Could not create a new playlist."),
			*GetName(), PlayingID);
		return Result;
	}

	// Assumes the playlist never got modified by anything else than SoundEngine. Refreshes Unreal list with the SoundEngine list.
	// Anything that is removed is assumed to be playing (there might be more than one).
	Result->Playlist = LockedPlaylist;
	Result->PlayingID = PlayingID;

	auto& Items{ Result->Items };
	const auto PreviousItems = Items;
	Items.Empty(LockedPlaylist->Length());
	for (uint32 Iter = 0; Iter < LockedPlaylist->Length(); ++Iter)
	{
		Items.Add(static_cast<UAkDynamicSequencePlaylistItem*>((*LockedPlaylist)[Iter].pCustomInfo));
	}
	Result->bLocked = true;

	// Check for Currently Playing Audio Nodes
	for (const auto& PreviousItem : PreviousItems)
	{
		if (!Items.Contains(PreviousItem))
		{
			UE_LOG(LogAkAudio, VeryVerbose, TEXT("UAkDynamicSequence::ModifyPlaylist %s PlayingID %" PRIi32 ": Adding Currently Playing Item %p"), *GetName(), PlayingID, PreviousItem.Get());
			CurrentlyPlayingAudioNodes.Add(PreviousItem);
		}
	}
	
	return Result;
}

bool UAkDynamicSequence::IsPlaying() const
{
	return State != EAkDynamicSequenceState::Stopped;
}

void UAkDynamicSequence::OnGameObjectCallback(EAkCallbackType CallbackType, UAkCallbackInfo* CallbackInfo)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkDynamicSequence::OnGameObjectCallback"));
	ON_SCOPE_EXIT
	{
		GameObjectUserCallback.ExecuteIfBound(CallbackType, CallbackInfo);
	};

	if (CallbackType == EAkCallbackType::EndOfDynamicSequenceItem)
	{
		// Updating the Playlist cache. This keeps the pointer to the AudioNodes that are still used, setting unused ones into CurrentlyPlayingAudioNodes.
		UE_LOG(LogAkAudio, VeryVerbose, TEXT("UAkDynamicSequence::OnGameObjectCallback %s PlayingID %" PRIi32": End of Item. Refreshing playlist."),
			*GetName(), PlayingID);
		bool bEmptyPlaylist = true;
		if (auto* UpdatingPlaylist = ModifyPlaylist())
		{
			bEmptyPlaylist = UpdatingPlaylist->Playlist->IsEmpty();
			UpdatingPlaylist->CommitWithoutChanges();
		}

		// Stop the playback if done.
		if (auto* DynSeqItemCallbackInfo = Cast<UAkDynamicSequenceItemCallbackInfo>(CallbackInfo))
		{
			auto CurrentlyCompletedItem = Cast<UAkDynamicSequencePlaylistItem>(DynSeqItemCallbackInfo->CustomInfo);
			const bool Removed = CurrentlyPlayingAudioNodes.RemoveSingle(CurrentlyCompletedItem) == 1;
			UE_CLOG(Removed, LogAkAudio, VeryVerbose, TEXT("UAkDynamicSequence::OnGameObjectCallback %s PlayingID %" PRIi32": End of Item. Removing %p."),
				*GetName(), PlayingID, CurrentlyCompletedItem);
		}

		UE_LOG(LogAkAudio, VeryVerbose, TEXT("UAkDynamicSequence::OnGameObjectCallback %s PlayingID %" PRIi32": End of Item. Remaining %d playing Audio Nodes."),
			*GetName(), PlayingID, CurrentlyPlayingAudioNodes.Num());

		if (CurrentlyPlayingAudioNodes.Num() == 0)
		{
			if (State == EAkDynamicSequenceState::Playing && !bEmptyPlaylist)
			{
				UE_LOG(LogAkAudio, VeryVerbose, TEXT("UAkDynamicSequence::OnGameObjectCallback %s PlayingID %" PRIi32": End of Item. Playing and has a Playlist. Keep playing."),
					*GetName(), PlayingID);
				return;
			}

			UE_LOG(LogAkAudio, Verbose, TEXT("UAkDynamicSequence::OnGameObjectCallback %s PlayingID %" PRIi32": Stopped playing."),
				*GetName(), PlayingID);
			RemoveFromRoot();
			State = EAkDynamicSequenceState::Stopped;
		}
	}
}
