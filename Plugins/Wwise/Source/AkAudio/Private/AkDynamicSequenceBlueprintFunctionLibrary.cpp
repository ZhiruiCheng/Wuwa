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

#include "AkDynamicSequenceBlueprintFunctionLibrary.h"

#include "AkComponentCallbackManager.h"
#include "AkDialogueEvent.h"
#include "AkDynamicSequencePlaylist.h"

#include <inttypes.h>

UAkDynamicSequencePlaylistItem* UAkDynamicSequenceBlueprintFunctionLibrary::CreateDynamicSequencePlaylistItemFromAudioNode(
	UAkAudioNode* AudioNode, int DelayMs, UObject* CustomData)
{
	SCOPED_AKAUDIO_EVENT_3(TEXT("UAkDynamicSequenceLibrary::CreateDynamicSequencePlaylistItemFromAudioNode"));
	if (!IsValid(AudioNode))
	{
		return nullptr;
	}
	auto* Item{ NewObject<UAkDynamicSequencePlaylistItem>()};
	if (!Item)
	{
		return nullptr;
	}
	UE_LOG(LogAkAudio, VeryVerbose, TEXT("UAkDynamicSequenceLibrary::CreateDynamicSequencePlaylistItemFromAudioNode %s -> %p"), *AudioNode->GetName(), Item);
	Item->AudioNode = AudioNode;
	Item->DelayMs = DelayMs;
	Item->CustomData = CustomData;
	return Item;
}

UAkDynamicSequencePlaylistItem* UAkDynamicSequenceBlueprintFunctionLibrary::CreateDynamicSequencePlaylistItemFromDialogueEvent(
	UAkDialogueEvent* DialogueEvent, const TArray<UAkGroupValue*>& Arguments, bool bOrderedPath, int DelayMs, UObject* CustomData)
{
	SCOPED_AKAUDIO_EVENT_3(TEXT("UAkDynamicSequenceLibrary::CreateDynamicSequencePlaylistItemFromDialogueEvent"));
	if (!IsValid(DialogueEvent))
	{
		return nullptr;
	}
	UE_LOG(LogAkAudio, VeryVerbose, TEXT("UAkDynamicSequenceLibrary::CreateDynamicSequencePlaylistItemFromDialogueEvent %s"), *DialogueEvent->GetName());
	UAkAudioNode* AudioNode = DialogueEvent->Resolve(Arguments, bOrderedPath, {});
	if (!AudioNode)
	{
		return nullptr;
	}
	return CreateDynamicSequencePlaylistItemFromAudioNode(AudioNode, DelayMs, CustomData);
}

UAkDynamicSequence* UAkDynamicSequenceBlueprintFunctionLibrary::OpenDynamicSequence(
	int64 GameObjectId, const FString& GameObjectName, int32 CallbackMask, const FOnAkPostEventCallback& OpenSequenceCallback,
	const FAkDynamicSequenceTransition& DefaultTransition, bool bSampleAccurate, bool bNewInstance)
{
	SCOPED_AKAUDIO_EVENT_3(TEXT("UAkDynamicSequenceLibrary::OpenDynamicSequence"));
	UE_LOG(LogAkAudio, VeryVerbose, TEXT("UAkDynamicSequenceLibrary::OpenDynamicSequence %s Go %" PRIi64), *GameObjectName, GameObjectId);
	auto* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) return {};

	auto* AudioDevice = FAkAudioDevice::Get();
	if (UNLIKELY(!AudioDevice)) return {};
	
	auto* const DynamicSequence{ NewObject<UAkDynamicSequence>() };
	if (UNLIKELY(!DynamicSequence))
	{
		UE_LOG(LogAkAudio, Error, TEXT("UAkDynamicSequenceLibrary::OpenDynamicSequence %s Go %" PRIi64": Could not instantiate new DynamicSequence."), *GameObjectName, GameObjectId);
		return {};
	}
	DynamicSequence->GameObjectUserCallback = OpenSequenceCallback;

	AudioDevice->RegisterGameObject(GameObjectId, GameObjectName);
	
	FOnAkPostEventCallback DynamicSequenceCallback;
	DynamicSequenceCallback.BindUFunction(DynamicSequence,
		GET_FUNCTION_NAME_CHECKED_TwoParams(UAkDynamicSequence, OnGameObjectCallback, EAkCallbackType, UAkCallbackInfo*));
	CallbackMask |= AK_EndOfDynamicSequenceItem;

	auto* CallbackManager = AudioDevice->GetCallbackManager();
	auto CallbackPackage = CallbackManager->CreateCallbackPackage(DynamicSequenceCallback, CallbackMask, GameObjectId, false, AK_INVALID_UNIQUE_ID);
	if (UNLIKELY(!CallbackPackage))
	{
		UE_LOG(LogAkAudio, Error, TEXT("UAkDynamicSequenceLibrary::OpenDynamicSequence %s Go %" PRIi64": Could not create Callback Package."), *GameObjectName, GameObjectId);
		return {};
	}

	auto PlayingID = SoundEngine->DynamicSequence->Open(
		GameObjectId,
		AkCallbackTypeHelpers::GetCallbackMaskFromBlueprintMask(CallbackMask),
		&FAkComponentCallbackManager::AkComponentCallback,
		CallbackPackage,
		bSampleAccurate ? AK::SoundEngine::DynamicSequence::DynamicSequenceType_SampleAccurate : AK::SoundEngine::DynamicSequence::DynamicSequenceType_NormalTransition);

	if (UNLIKELY(PlayingID == AK_INVALID_PLAYING_ID))
	{
		UE_LOG(LogAkAudio, Error, TEXT("UAkDynamicSequenceLibrary::OpenDynamicSequence %s Go %" PRIi64": Could not open DynamicSequence."), *GameObjectName, GameObjectId);
		return {};
	}
	DynamicSequence->PlayingID = PlayingID;
	DynamicSequence->DynamicSequenceTransition = DefaultTransition;
	
	return DynamicSequence;
}

