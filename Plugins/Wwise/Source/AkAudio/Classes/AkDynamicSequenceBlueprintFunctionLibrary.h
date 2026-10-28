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
#include "AkDynamicSequence.h"

#include "Kismet/BlueprintFunctionLibrary.h"

#include "AkDynamicSequenceBlueprintFunctionLibrary.generated.h"

class UAkDialogueEvent;
class UAkGroupValue;

/**
 * Convenience operations for Dynamic Sequences. 
 */
UCLASS()
class AKAUDIO_API UAkDynamicSequenceBlueprintFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Audiokinetic|AkDynamicSequence", meta = (Keywords = "Wwise"))
	static UAkDynamicSequencePlaylistItem* CreateDynamicSequencePlaylistItemFromAudioNode(
		UAkAudioNode* AudioNode,
		int DelayMs = 0, UObject* CustomData = nullptr);

	UFUNCTION(BlueprintPure, Category = "Audiokinetic|AkDynamicSequence", meta = (Keywords = "Wwise"))
	static UAkDynamicSequencePlaylistItem* CreateDynamicSequencePlaylistItemFromDialogueEvent(
		UAkDialogueEvent* DialogueEvent, const TArray<UAkGroupValue*>& Arguments,
		bool bOrderedPath = false, int DelayMs = 0, UObject* CustomData = nullptr);
	
	static UAkDynamicSequence* OpenDynamicSequence(
		int64 GameObjectId, const FString& GameObjectName,
		int32 CallbackMask, const FOnAkPostEventCallback& OpenSequenceCallback,
		const FAkDynamicSequenceTransition& DefaultTransition,
		bool bSampleAccurate = false, bool bNewInstance = false);
};
