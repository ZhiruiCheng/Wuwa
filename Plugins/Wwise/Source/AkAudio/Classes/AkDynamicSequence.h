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
#include "AkAudioNode.h"
#include "AkDynamicSequenceTransition.h"
#include "AkGameplayTypes.h"
#include "AkDynamicSequence.generated.h"

class UAkDynamicSequencePlaylistItem;
class UAkGroupValue;
class UAkDialogueEvent;
class UAkAudioNode;
class UAkDynamicSequencePlaylist;

/**
 * Dynamic Sequence's Playback State.
 */
UENUM(BlueprintType)
enum class EAkDynamicSequenceState : uint8
{
	Stopped,
	Playing,
	Stopping
};

/**
 * Single Wwise Playing Object instance of a dynamic dialogue sequence.
 *
 * This can be instantiated through UAkGameObject::OpenDynamicSequence.
 */
UCLASS(ClassGroup=Audiokinetic, BlueprintType)
class AKAUDIO_API UAkDynamicSequence : public UObject
{
	GENERATED_BODY()

public:
	/// Playing ID, as provided by UAkGameObject::OpenDynamicSequence.
	UPROPERTY(Blueprintable, Category = "Audiokinetic|AkDynamicSequence", BlueprintReadOnly)
	int32 PlayingID{ (int32)AK_INVALID_PLAYING_ID };

	/// Calls Close on the Dynamic Sequence.
	virtual void BeginDestroy() override;

	UFUNCTION(BlueprintCallable, Category = "Audiokinetic|AkDynamicSequence")
	bool PostPlaylistItem(
		UAkDynamicSequencePlaylistItem* PlaylistItem,
		bool bPlayImmediately = true
	);

	UFUNCTION(BlueprintCallable, Category = "Audiokinetic|AkDynamicSequence")
	bool PostAudioNode(
		UAkAudioNode* AudioNode,
		bool bPlayImmediately = true
	);

	UFUNCTION(BlueprintCallable, Category = "Audiokinetic|AkDynamicSequence")
	bool PostDialogueEventInPlaylist(
		UAkDialogueEvent* DialogueEvent,
		const TArray<UAkGroupValue*>& Arguments,
		bool bOrderedPath = false,
		bool bPlayImmediately = true
	);
	
	/// Play specified Dynamic Sequence.
	/// 
	/// @param OverrideTransition Transition to use instead of the Default Transition.
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Audiokinetic|AkDynamicSequence")
	EAkResult Play(const FAkDynamicSequenceTransition OverrideTransition = FAkDynamicSequenceTransition());

	/// Pause specified Dynamic Sequence. 
	/// 
	/// @param OverrideTransition Transition to use instead of the Default Transition.
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Audiokinetic|AkDynamicSequence")
	EAkResult Pause(const FAkDynamicSequenceTransition OverrideTransition = FAkDynamicSequenceTransition());

	/// Resume specified Dynamic Sequence. 
	/// 
	/// @param OverrideTransition Transition to use instead of the Default Transition.
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Audiokinetic|AkDynamicSequence")
	EAkResult Resume(const FAkDynamicSequenceTransition OverrideTransition = FAkDynamicSequenceTransition());

	/// Stop specified Dynamic Sequence immediately.
	/// 
	/// To restart the sequence, call Play. The sequence will restart with the item that was in the 
	/// playlist after the item that was stopped.
	/// 
	/// @param OverrideTransition Transition to use instead of the Default Transition.
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Audiokinetic|AkDynamicSequence")
	EAkResult Stop(const FAkDynamicSequenceTransition OverrideTransition = FAkDynamicSequenceTransition());

	/// Break specified Dynamic Sequence.
	///
	/// The sequence will stop after the current item.
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Audiokinetic|AkDynamicSequence")
	EAkResult Break();

	/// Seek inside specified Dynamic Sequence.
	/// 
	/// It is only possible to seek in the first item of the sequence.
	/// If you seek past the duration of the first item, it will be skipped and an error will reported in the Capture Log and debug output.
	///
	/// @param PositionMS Position into the the sound, in milliseconds
	/// @param bSeekToNearestMarker Snap to the marker nearest to the seek position
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Audiokinetic|AkDynamicSequence")
	EAkResult Seek(
		int32 PositionMS,
		bool bSeekToNearestMarker
		);

	/// Seek inside specified Dynamic Sequence.
	/// 
	/// It is only possible to seek in the first item of the sequence.
	/// If you seek past the duration of the first item, it will be skipped and an error will reported in the Capture Log and debug output.
	///
	/// @param Percent Position into the the sound, in percentage of the whole duration
	/// @param bSeekToNearestMarker Snap to the marker nearest to the seek position
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Audiokinetic|AkDynamicSequence")
	EAkResult SeekPercent(
		float Percent,
		bool bSeekToNearestMarker
		);

	/// Get pause times.
	///
	/// @param PauseTime If sequence is currently paused, time when pause started, else 0
	/// @param Duration Total pause duration since last call to GetPauseTimes, excluding the time elapsed in the current pause
	UFUNCTION(BlueprintPure, BlueprintCosmetic, Category="Audiokinetic|AkDynamicSequence")
	EAkResult GetPauseTimes(
		int32& PauseTime,
		int32& Duration) const;

	/// Get currently playing item. Note that this may be different from the currently heard item
	/// when sequence is in sample-accurate mode.
	/// @param AudioNode Returned audio node of playing item.
	UFUNCTION(BlueprintPure, BlueprintCosmetic, Category="Audiokinetic|AkDynamicSequence")
	EAkResult GetPlayingItem(
		UAkAudioNode*& AudioNode) const;

	/// Lock the Playlist for editing.
	///
	/// Structure needs to be commited to allow the playlist to continue.
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, Category="Audiokinetic|AkDynamicSequence")
	UAkDynamicSequencePlaylist* ModifyPlaylist();

	/// Returns true if the dynamic sequence is currently playing.
	bool IsPlaying() const;

	/// Callback, as called by the SoundEngine.
	UFUNCTION()
	void OnGameObjectCallback(EAkCallbackType CallbackType, UAkCallbackInfo* CallbackInfo);

	/// Callback operation, as provided to AkGameObject::OpenDynamicSequence.
	UPROPERTY(Category = "Audiokinetic|AkDynamicSequence", BlueprintReadWrite)
	FOnAkPostEventCallback GameObjectUserCallback {};

	/// Structure holding the information of transitions of the sequence
	UPROPERTY(Category = "Audiokinetic|AkDynamicSequence", BlueprintReadWrite)
	FAkDynamicSequenceTransition DynamicSequenceTransition {};
	
protected:
	/// Cached Playlist object. Used for ModifyPlaylist to return a single instance.
	UPROPERTY()
	TObjectPtr<UAkDynamicSequencePlaylist> Playlist;

	/// List of items that disappeared from the Playlist while being played. Assumes no changes got done outside this system.
	UPROPERTY()
	TArray<TObjectPtr<UAkDynamicSequencePlaylistItem>> CurrentlyPlayingAudioNodes;

	UPROPERTY()
	EAkDynamicSequenceState State{ EAkDynamicSequenceState::Stopped };
};