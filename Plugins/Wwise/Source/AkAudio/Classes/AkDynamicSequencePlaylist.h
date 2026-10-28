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
#include "Wwise/API/WwiseSoundEngineAPI.h"
#include "AkDynamicSequencePlaylist.generated.h"

/**
 * Single Dynamic Sequence item.
 */
UCLASS(ClassGroup=Audiokinetic, BlueprintType, Transient)
class AKAUDIO_API UAkDynamicSequencePlaylistItem : public UObject
{
	GENERATED_BODY()

public:
	UAkDynamicSequencePlaylistItem() {}

	UPROPERTY(EditAnywhere, Category = "Audiokinetic|AkDynamicSequence", BlueprintReadWrite)
	TObjectPtr<UAkAudioNode> AudioNode;

	UPROPERTY(EditAnywhere, Category = "Audiokinetic|AkDynamicSequence", BlueprintReadWrite)
	int DelayMs{ 0 };

	UPROPERTY(EditAnywhere, Category = "Audiokinetic|AkDynamicSequence", BlueprintReadWrite)
	TObjectPtr<UObject> CustomData;
};

/**
 * Dynamic Sequence Playlist. Can be modified through UAkDynamicSequence::ModifyPlaylist.
 *
 * It should be short-lived, and should be committed immediately after changes with Commit(), and then discarded.
 */
UCLASS(ClassGroup=Audiokinetic, BlueprintType, Transient)
class AKAUDIO_API UAkDynamicSequencePlaylist : public UObject
{
	GENERATED_BODY()

public:
	IWwiseSoundEngineAPI::IDynamicSequence::IPlaylist* Playlist{ nullptr };
	bool bLocked{ false };
	
	UPROPERTY(Category = "Audiokinetic|AkDynamicSequence", BlueprintReadOnly)
	int32 PlayingID{ (int32)AK_INVALID_PLAYING_ID };

	/**
	 * List of sequentially playing Audio Nodes.
	 *
	 * Can be modified. Changes are only applied when the Commit() operation is called. 
	 */
	UPROPERTY(Category = "Audiokinetic|AkDynamicSequence", BlueprintReadWrite)
	TArray<TObjectPtr<UAkDynamicSequencePlaylistItem>> Items;

	/**
	 * Commits this playlist to the Dynamic Sequence.
	 *
	 * The playlist object should not be kept for future reference, it can only be commited once, and then, the object
	 * will be cached inside the UAkDynamicSequence.
	 *
	 * In order to modify the playlist a second time, you must retrieve it from the UAkDynamicSequence object again.
	 */
	UFUNCTION(Category = "Audiokinetic|AkDynamicSequence", BlueprintCallable)
	void Commit();

	/**
	 * Commits this playlist to the Dynamic Sequence when there aren't any changes.
	 *
	 * This is a convenience function. It assumes the playlist haven't received any change whatsoever, leaving everything alone.
	 * Use Commit() if operations are to be handled.
	 *
	 * Like Commit(), the playlist object should not be kept for future reference, it can only be commited once, and then, the
	 * object will be cached inside the UAkDynamicSequence.
	 */
	UFUNCTION(Category = "Audiokinetic|AkDynamicSequence", BlueprintCallable)
	void CommitWithoutChanges();

	/**
	 * Adds an item to the end of the playlist and commits the result.
	 *
	 * This is a convenience function. It assumes the playlist haven't received any change other prior to the AddAndCommit
	 * call, and will merely enqueue the new item to the end of the internal list, leaving everything else alone.
	 *
	 * Use Commit() if more than one operation is to be handled.
	 *
	 * Like Commit(), the playlist object should not be kept for future reference, it can only be commited once, and then, the
	 * object will be cached inside the UAkDynamicSequence.
	 */
	UFUNCTION(Category = "Audiokinetic|AkDynamicSequence", BlueprintCallable)
	void AddAndCommit(UAkDynamicSequencePlaylistItem* Item);

	/**
	 * Overwrite the current playlist with the new items and commits the result.
	 *
	 * This is a convenience function. Can be useful if an event happen in the game where it requires to overwrite the previously enqueued items
	 *
	 * Use Commit() if more than one operation is to be handled.
	 *
	 * Like Commit(), the playlist object should not be kept for future reference, it can only be commited once, and then, the
	 * object will be cached inside the UAkDynamicSequence.
	 */
	UFUNCTION(Category = "Audiokinetic|AkDynamicSequence", BlueprintCallable)
	void OverwriteAndCommit(TArray<UAkDynamicSequencePlaylistItem*> InItems);
};
