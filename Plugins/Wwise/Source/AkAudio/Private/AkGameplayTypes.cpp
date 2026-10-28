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

/*=============================================================================
	AkAudioClasses.cpp:
=============================================================================*/

#include "AkGameplayTypes.h"

#include "AkAudioDevice.h"
#include "AkAudioEvent.h"
#include "AkCallbackInfoPool.h"
#include "AkComponent.h"
#include "WwiseUnrealDefines.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/GameEngine.h"
#include "EngineUtils.h"
#include "AkCallbackInfoPool.h"
#include "HAL/PlatformString.h"

UAkCallbackInfo* AkCallbackTypeHelpers::GetBlueprintableCallbackInfo(EAkCallbackType CallbackType, AkCombinedCallbackInfo* CallbackInfo)
{
	switch (CallbackType)
	{
	case EAkCallbackType::EndOfEvent:
		return UAkEventCallbackInfo::Create(&CallbackInfo->eventInfo);
	case EAkCallbackType::EndOfDynamicSequenceItem:
		return UAkDynamicSequenceItemCallbackInfo::Create(CallbackInfo);
	case EAkCallbackType::Marker:
		return UAkMarkerCallbackInfo::Create(CallbackInfo);
	case EAkCallbackType::Duration:
		return UAkDurationCallbackInfo::Create(CallbackInfo);
	case EAkCallbackType::Starvation:
		return UAkEventCallbackInfo::Create(&CallbackInfo->eventInfo);
	case EAkCallbackType::MusicPlayStarted:
		return UAkEventCallbackInfo::Create(&CallbackInfo->eventInfo);
	case EAkCallbackType::MusicSyncBeat:
	case EAkCallbackType::MusicSyncBar:
	case EAkCallbackType::MusicSyncEntry:
	case EAkCallbackType::MusicSyncExit:
	case EAkCallbackType::MusicSyncGrid:
	case EAkCallbackType::MusicSyncUserCue:
	case EAkCallbackType::MusicSyncPoint:
		return UAkMusicSyncCallbackInfo::Create(CallbackInfo);
	case EAkCallbackType::MIDIEvent:
		return UAkMIDIEventCallbackInfo::Create(CallbackInfo);
	default: 
		return nullptr;
	}
}

#if WWISE_2025_1_OR_LATER
AkCombinedCallbackInfo* AkCallbackTypeHelpers::CopyWwiseCallbackInfo(AkCallbackType CallbackType, AkEventCallbackInfo* EventCallbackInfo, void* SourceCallbackInfo)
{
	SCOPED_AKAUDIO_EVENT_4(TEXT("AkCallbackTypeHelpers::CopyWwiseCallbackInfo"));
	size_t szBase = sizeof(AkCombinedCallbackInfo);
	size_t szSourceInfo = 0;
	size_t szExtraInfo = 0;

	// First, determine the total size to allocate
	switch (CallbackType)
	{
	case AK_Marker:
	{
		szSourceInfo = sizeof(AkMarkerCallbackInfo);
		const char* SourceLabel = ((AkMarkerCallbackInfo*)SourceCallbackInfo)->strLabel;
		szExtraInfo = SourceLabel ? FPlatformString::Strlen(SourceLabel) + 1 : 0;
		break;
	}
    case AK_EndOfDynamicSequenceItem:
    {
        szSourceInfo = sizeof(AkDynamicSequenceItemCallbackInfo);
        break;
    }
	case AK_Duration:
	{
		szSourceInfo = sizeof(AkDurationCallbackInfo);
		break;
	}
	case AK_MusicSyncBeat:
	case AK_MusicSyncBar:
	case AK_MusicSyncEntry:
	case AK_MusicSyncExit:
	case AK_MusicSyncGrid:
	case AK_MusicSyncUserCue:
	case AK_MusicSyncPoint:
	{
		szSourceInfo = sizeof(AkMusicSyncCallbackInfo);
		const char* SourceUserCue = ((AkMusicSyncCallbackInfo*)SourceCallbackInfo)->pszUserCueName;
		szExtraInfo = SourceUserCue ? FPlatformString::Strlen(SourceUserCue) + 1 : 0;
		break;
	}
	case AK_MIDIEvent:
	{
		szSourceInfo = sizeof(AkMIDIEventCallbackInfo);
		break;
	}
	default:
		// No extra size to allocate
		break;
	}

	// Allocate the memory for the copy
	AkCombinedCallbackInfo* CbInfoCopy = (AkCombinedCallbackInfo*)FMemory::Malloc(szBase + szSourceInfo + szExtraInfo);
	if (CbInfoCopy == nullptr)
		return CbInfoCopy;

	uint8_t* SourceCopy = (uint8_t*)(reinterpret_cast<uint8_t*>(CbInfoCopy) + szBase);
	uint8_t* ExtraCopy = SourceCopy + szSourceInfo;

	// Copy the event data
	FMemory::Memcpy(CbInfoCopy, EventCallbackInfo, sizeof(AkEventCallbackInfo));

#if WWISE_2025_1_OR_LATER
	// Assign the callback pointer to the type-specific callback info
	CbInfoCopy->pCallbackInfo = SourceCopy;
#endif

	// Copy the source data, if any
	if (szSourceInfo > 0)
	{
		FMemory::Memcpy(SourceCopy, SourceCallbackInfo, szSourceInfo);
	}

	// Copy the 'extra' data, if any
	if (szExtraInfo > 0)
	{
		switch (CallbackType)
		{
		case AK_Marker:
			reinterpret_cast<AkMarkerCallbackInfo*>(SourceCopy)->strLabel = (const char*)ExtraCopy;
			FPlatformString::Strncpy((char*)ExtraCopy, static_cast<AkMarkerCallbackInfo*>(SourceCallbackInfo)->strLabel, szExtraInfo);
			break;
		case AK_MusicSyncBeat:
		case AK_MusicSyncBar:
		case AK_MusicSyncEntry:
		case AK_MusicSyncExit:
		case AK_MusicSyncGrid:
		case AK_MusicSyncUserCue:
		case AK_MusicSyncPoint:
			reinterpret_cast<AkMusicSyncCallbackInfo*>(SourceCopy)->pszUserCueName = (char*)ExtraCopy;
			FPlatformString::Strncpy((char*)ExtraCopy, static_cast<AkMusicSyncCallbackInfo*>(SourceCallbackInfo)->pszUserCueName, szExtraInfo);
			break;
		}
	}
	return CbInfoCopy;
}
#else
AkCombinedCallbackInfo* AkCallbackTypeHelpers::CopyWwiseCallbackInfo(AkCallbackType CallbackType, AkCallbackInfo* SourceCallbackInfo)
{
	SCOPED_AKAUDIO_EVENT_4(TEXT("AkCallbackTypeHelpers::CopyWwiseCallbackInfo"));
	switch (CallbackType)
	{
	case AK_EndOfEvent:
	case AK_Starvation:
	case AK_MusicPlayStarted:
	{
		AkEventCallbackInfo* CbInfoCopy = (AkEventCallbackInfo*)FMemory::Malloc(sizeof(AkEventCallbackInfo));
		FMemory::Memcpy(CbInfoCopy, SourceCallbackInfo, sizeof(AkEventCallbackInfo));
		return (AkCombinedCallbackInfo*)CbInfoCopy;
	}
	case AK_EndOfDynamicSequenceItem:
		{
			AkDynamicSequenceItemCallbackInfo* CbInfoCopy = (AkDynamicSequenceItemCallbackInfo*)FMemory::Malloc(sizeof(AkDynamicSequenceItemCallbackInfo));
			FMemory::Memcpy(CbInfoCopy, SourceCallbackInfo, sizeof(AkDynamicSequenceItemCallbackInfo));
			return (AkCombinedCallbackInfo*)CbInfoCopy;
		}
	case AK_Marker:
	{
		const char* SourceLabel = ((AkMarkerCallbackInfo*)SourceCallbackInfo)->strLabel;
		int32 LabelSize = SourceLabel ? FPlatformString::Strlen(SourceLabel) + 1 : 0;
		AkMarkerCallbackInfo* CbInfoCopy = (AkMarkerCallbackInfo*)FMemory::Malloc(sizeof(AkMarkerCallbackInfo) + LabelSize);
		FMemory::Memcpy(CbInfoCopy, SourceCallbackInfo, sizeof(AkMarkerCallbackInfo));

		if (SourceLabel)
		{
			CbInfoCopy->strLabel = reinterpret_cast<const char*>(CbInfoCopy) + sizeof(AkMarkerCallbackInfo);
			FPlatformString::Strcpy(const_cast<char*>(CbInfoCopy->strLabel), LabelSize - 1, SourceLabel);
		}
		return (AkCombinedCallbackInfo*)CbInfoCopy;
	}
	case AK_Duration:
	{
		AkDurationCallbackInfo* CbInfoCopy = (AkDurationCallbackInfo*)FMemory::Malloc(sizeof(AkDurationCallbackInfo));
		FMemory::Memcpy(CbInfoCopy, SourceCallbackInfo, sizeof(AkDurationCallbackInfo));
		return (AkCombinedCallbackInfo*)CbInfoCopy;
	}
	case AK_MusicSyncBeat:
	case AK_MusicSyncBar:
	case AK_MusicSyncEntry:
	case AK_MusicSyncExit:
	case AK_MusicSyncGrid:
	case AK_MusicSyncUserCue:
	case AK_MusicSyncPoint:
	{
		const char* SourceUserCue = ((AkMusicSyncCallbackInfo*)SourceCallbackInfo)->pszUserCueName;
		int32 UserCueSize = SourceUserCue ? FPlatformString::Strlen(SourceUserCue) + 1 : 0;
		AkMusicSyncCallbackInfo* CbInfoCopy = (AkMusicSyncCallbackInfo*)FMemory::Malloc(sizeof(AkMusicSyncCallbackInfo) + UserCueSize);
		FMemory::Memcpy(CbInfoCopy, SourceCallbackInfo, sizeof(AkMusicSyncCallbackInfo));

		//SourceUserCue is either null or a non-empty string
		if (SourceUserCue)
		{
			CbInfoCopy->pszUserCueName = reinterpret_cast<char*>(CbInfoCopy) + sizeof(AkMusicSyncCallbackInfo);
			FPlatformString::Strcpy(const_cast<char*>(CbInfoCopy->pszUserCueName), UserCueSize, SourceUserCue);
		}
		return (AkCombinedCallbackInfo*)CbInfoCopy;
	}
	case AK_MIDIEvent:
	{
		AkMIDIEventCallbackInfo* CbInfoCopy = (AkMIDIEventCallbackInfo*)FMemory::Malloc(sizeof(AkMIDIEventCallbackInfo));
		FMemory::Memcpy(CbInfoCopy, SourceCallbackInfo, sizeof(AkMIDIEventCallbackInfo));
		return (AkCombinedCallbackInfo*)CbInfoCopy;
	}
	default:
		return nullptr;
	}
}
#endif

AkCallbackType AkCallbackTypeHelpers::GetCallbackMaskFromBlueprintMask(int32 BlueprintCallbackType)
{
	return (AkCallbackType)BlueprintCallbackType;
}

EAkCallbackType AkCallbackTypeHelpers::GetBlueprintCallbackTypeFromAkCallbackType(AkCallbackType CallbackType)
{
	uint32 BitIndex = 0;
	uint32 CbType = (uint32)CallbackType >> 1;
	while (CbType != 0)
	{
		CbType >>= 1;
		BitIndex++;
	}
	return (EAkCallbackType)BitIndex;
}

UAkCallbackInfo::UAkCallbackInfo( class FObjectInitializer const & ObjectInitializer) :
	Super(ObjectInitializer)
{}

UAkCallbackInfo* UAkCallbackInfo::Create(AkGameObjectID GameObjectID)
{
	FAkAudioDevice* AudioDevice = FAkAudioDevice::Get();
	if (UNLIKELY(!AudioDevice))
	{
		return nullptr;
	}
	
	auto* CallbackInfoPool = AudioDevice->GetAkCallbackInfoPool();
	if (!CallbackInfoPool)
	{
		return nullptr;
	}
    
	auto CbInfo = CallbackInfoPool->Acquire<UAkCallbackInfo>();
	if (CbInfo)
	{
		CbInfo->AkComponent = UAkComponent::GetAkComponent(GameObjectID);
	}
	return CbInfo;
}

void UAkCallbackInfo::Reset()
{
	AkComponent = nullptr;
}

UAkEventCallbackInfo::UAkEventCallbackInfo(class FObjectInitializer const & ObjectInitializer) :
	Super(ObjectInitializer)
{}

UAkEventCallbackInfo* UAkEventCallbackInfo::Create(AkEventCallbackInfo* AkEventCbInfo)
{
	FAkAudioDevice* AudioDevice = FAkAudioDevice::Get();
    if (UNLIKELY(!AudioDevice))
    {
        return nullptr;
    }
	
	auto* CallbackInfoPool = AudioDevice->GetAkCallbackInfoPool();
	if (!CallbackInfoPool)
	{
		return nullptr;
	}
    
	auto CbInfo = CallbackInfoPool->Acquire<UAkEventCallbackInfo>();
	if (CbInfo)
	{
		CbInfo->AkComponent = UAkComponent::GetAkComponent(AkEventCbInfo->gameObjID);
		CbInfo->PlayingID = AkEventCbInfo->playingID;
		CbInfo->EventID = AkEventCbInfo->eventID;
	}
	return CbInfo;
}

UAkDynamicSequenceItemCallbackInfo::UAkDynamicSequenceItemCallbackInfo(class FObjectInitializer const& ObjectInitializer) :
	Super(ObjectInitializer)
{}

UAkDynamicSequenceItemCallbackInfo* UAkDynamicSequenceItemCallbackInfo::Create(AkCombinedCallbackInfo * akCallbackInfo)
{
	FAkAudioDevice* AudioDevice = FAkAudioDevice::Get();
	if (UNLIKELY(!AudioDevice))
	{
		return nullptr;
	}
	
	auto* CallbackInfoPool = AudioDevice->GetAkCallbackInfoPool();
	if (!CallbackInfoPool)
	{
		return nullptr;
	}
    
	auto CbInfo = CallbackInfoPool->Acquire<UAkDynamicSequenceItemCallbackInfo>();
	if (CbInfo)
	{
#if WWISE_2025_1_OR_LATER
		AkDynamicSequenceItemCallbackInfo* AkDynamicSequenceCbInfo = static_cast<AkDynamicSequenceItemCallbackInfo*>(akCallbackInfo->pCallbackInfo);
#else
		AkDynamicSequenceItemCallbackInfo* AkDynamicSequenceCbInfo = reinterpret_cast<AkDynamicSequenceItemCallbackInfo*>(akCallbackInfo);
#endif
		CbInfo->AkComponent = UAkComponent::GetAkComponent(akCallbackInfo->eventInfo.gameObjID);
		CbInfo->AudioNodeID = AkDynamicSequenceCbInfo->audioNodeID;
		CbInfo->PlayingID = akCallbackInfo->eventInfo.gameObjID;
		CbInfo->CustomInfo = static_cast<UObject*>(AkDynamicSequenceCbInfo->pCustomInfo);
	}
	return CbInfo;
}

UAkMIDIEventCallbackInfo::UAkMIDIEventCallbackInfo(class FObjectInitializer const & ObjectInitializer) :
	Super(ObjectInitializer)
{}

UAkMIDIEventCallbackInfo* UAkMIDIEventCallbackInfo::Create(AkCombinedCallbackInfo * akCallbackInfo)
{
	FAkAudioDevice* AudioDevice = FAkAudioDevice::Get();
	if (UNLIKELY(!AudioDevice))
	{
		return nullptr;
	}
	
	auto* CallbackInfoPool = AudioDevice->GetAkCallbackInfoPool();
	if (!CallbackInfoPool)
	{
		return nullptr;
	}
    
	auto CbInfo = CallbackInfoPool->Acquire<UAkMIDIEventCallbackInfo>();
	if (CbInfo)
	{
#if WWISE_2025_1_OR_LATER
		AkMIDIEventCallbackInfo* AkMIDICbInfo = static_cast<AkMIDIEventCallbackInfo*>(akCallbackInfo->pCallbackInfo);
#else
		AkMIDIEventCallbackInfo* AkMIDICbInfo = static_cast<AkMIDIEventCallbackInfo*>(&akCallbackInfo->eventInfo);
#endif

		CbInfo->AkComponent = UAkComponent::GetAkComponent(akCallbackInfo->eventInfo.gameObjID);
		CbInfo->PlayingID = akCallbackInfo->eventInfo.playingID;
		CbInfo->EventID = akCallbackInfo->eventInfo.eventID;
		CbInfo->AkMidiEvent = AkMIDICbInfo->midiEvent;
	}
	return CbInfo;
}

UAkMarkerCallbackInfo::UAkMarkerCallbackInfo(class FObjectInitializer const & ObjectInitializer) :
	Super(ObjectInitializer)
{}

UAkMarkerCallbackInfo* UAkMarkerCallbackInfo::Create(AkCombinedCallbackInfo* akCallbackInfo)
{
	FAkAudioDevice* AudioDevice = FAkAudioDevice::Get();
	if (UNLIKELY(!AudioDevice))
	{
		return nullptr;
	}
	
	auto* CallbackInfoPool = AudioDevice->GetAkCallbackInfoPool();
	if (!CallbackInfoPool)
	{
		return nullptr;
	}
    
	auto CbInfo = CallbackInfoPool->Acquire<UAkMarkerCallbackInfo>();
	if (CbInfo)
	{
#if WWISE_2025_1_OR_LATER
		AkMarkerCallbackInfo* AkMarkerCbInfo = static_cast<AkMarkerCallbackInfo*>(akCallbackInfo->pCallbackInfo);
#else
		AkMarkerCallbackInfo* AkMarkerCbInfo = static_cast<AkMarkerCallbackInfo*>(&akCallbackInfo->eventInfo);
#endif

		CbInfo->AkComponent = UAkComponent::GetAkComponent(akCallbackInfo->eventInfo.gameObjID);
		CbInfo->PlayingID = akCallbackInfo->eventInfo.playingID;
		CbInfo->EventID = akCallbackInfo->eventInfo.eventID;
		CbInfo->Identifier = AkMarkerCbInfo->uIdentifier;
		CbInfo->Position = AkMarkerCbInfo->uPosition;
		CbInfo->Label = FString(AkMarkerCbInfo->strLabel);
	}
	return CbInfo;
}

UAkDurationCallbackInfo::UAkDurationCallbackInfo(class FObjectInitializer const & ObjectInitializer) :
	Super(ObjectInitializer)
{}

UAkDurationCallbackInfo* UAkDurationCallbackInfo::Create(AkCombinedCallbackInfo* akCallbackInfo)
{
	FAkAudioDevice* AudioDevice = FAkAudioDevice::Get();
	if (UNLIKELY(!AudioDevice))
	{
		return nullptr;
	}
	
	auto* CallbackInfoPool = AudioDevice->GetAkCallbackInfoPool();
	if (!CallbackInfoPool)
	{
		return nullptr;
	}
    
	auto CbInfo = CallbackInfoPool->Acquire<UAkDurationCallbackInfo>();
	if (CbInfo)
	{
#if WWISE_2025_1_OR_LATER
		AkDurationCallbackInfo* AkDurationCbInfo = static_cast<AkDurationCallbackInfo*>(akCallbackInfo->pCallbackInfo);
#else
		AkDurationCallbackInfo* AkDurationCbInfo = static_cast<AkDurationCallbackInfo*>(&akCallbackInfo->eventInfo);
#endif

		CbInfo->AkComponent = UAkComponent::GetAkComponent(akCallbackInfo->eventInfo.gameObjID);
		CbInfo->PlayingID = akCallbackInfo->eventInfo.playingID;
		CbInfo->EventID = akCallbackInfo->eventInfo.eventID;
		CbInfo->Duration = AkDurationCbInfo->fDuration;
		CbInfo->EstimatedDuration = AkDurationCbInfo->fEstimatedDuration;
		CbInfo->AudioNodeID = AkDurationCbInfo->audioNodeID;
		CbInfo->MediaID = AkDurationCbInfo->mediaID;
		CbInfo->bStreaming = AkDurationCbInfo->bStreaming;
	}
	return CbInfo;
}

UAkMusicSyncCallbackInfo::UAkMusicSyncCallbackInfo(class FObjectInitializer const & ObjectInitializer) :
	Super(ObjectInitializer)
{}

UAkMusicSyncCallbackInfo* UAkMusicSyncCallbackInfo::Create(AkCombinedCallbackInfo* akCallbackInfo)
{
	FAkAudioDevice* AudioDevice = FAkAudioDevice::Get();
	if (UNLIKELY(!AudioDevice))
	{
		return nullptr;
	}
	
	auto* CallbackInfoPool = AudioDevice->GetAkCallbackInfoPool();
	if (!CallbackInfoPool)
	{
		return nullptr;
	}
    
	auto CbInfo = CallbackInfoPool->Acquire<UAkMusicSyncCallbackInfo>();
	if (CbInfo)
	{
#if WWISE_2025_1_OR_LATER
		AkMusicSyncCallbackInfo* AkMusicCbInfo = static_cast<AkMusicSyncCallbackInfo*>(akCallbackInfo->pCallbackInfo);
#else
		AkMusicSyncCallbackInfo* AkMusicCbInfo = reinterpret_cast<AkMusicSyncCallbackInfo*>(akCallbackInfo);
#endif

		CbInfo->AkComponent = UAkComponent::GetAkComponent(akCallbackInfo->eventInfo.gameObjID);
		CbInfo->PlayingID = akCallbackInfo->eventInfo.playingID;
		CbInfo->SegmentInfo = AkMusicCbInfo->segmentInfo;
		CbInfo->MusicSyncType = AkCallbackTypeHelpers::GetBlueprintCallbackTypeFromAkCallbackType(AkMusicCbInfo->musicSyncType);
		CbInfo->UserCueName = FString(AkMusicCbInfo->pszUserCueName);
	}
	return CbInfo;
}

EAkMidiEventType UAkMIDIEventCallbackInfo::GetType()
{
	return (EAkMidiEventType)AkMidiEvent.byType;
}

uint8 UAkMIDIEventCallbackInfo::GetChannel()
{
	// Add one here so we report "Artist" channel number (between 1 and 16), instead of reporting the underlying value of 0-F.
	return AkMidiEvent.byChan + 1;
}

bool UAkMIDIEventCallbackInfo::GetGeneric(FAkMidiGeneric& AsGeneric)
{
	AsGeneric = FAkMidiGeneric(AkMidiEvent);
	return true;
}

bool UAkMIDIEventCallbackInfo::GetNoteOn(FAkMidiNoteOnOff& AsNoteOn)
{
	if (GetType() != EAkMidiEventType::AkMidiEventTypeNoteOn)
	{
		return false;
	}

	AsNoteOn = FAkMidiNoteOnOff(AkMidiEvent);
	return true;
}

bool UAkMIDIEventCallbackInfo::GetNoteOff(FAkMidiNoteOnOff& AsNoteOff)
{
	if (GetType() != EAkMidiEventType::AkMidiEventTypeNoteOff)
	{
		return false;
	}

	AsNoteOff = FAkMidiNoteOnOff(AkMidiEvent);
	return true;
}

bool UAkMIDIEventCallbackInfo::GetCc(FAkMidiCc& AsCc)
{
	if (GetType() != EAkMidiEventType::AkMidiEventTypeController)
	{
		return false;
	}

	AsCc = FAkMidiCc(AkMidiEvent);
	return true;
}

bool UAkMIDIEventCallbackInfo::GetPitchBend(FAkMidiPitchBend& AsPitchBend)
{
	if (GetType() != EAkMidiEventType::AkMidiEventTypePitchBend)
	{
		return false;
	}

	AsPitchBend = FAkMidiPitchBend(AkMidiEvent);
	return true;
}

bool UAkMIDIEventCallbackInfo::GetNoteAftertouch(FAkMidiNoteAftertouch& AsNoteAftertouch)
{
	if (GetType() != EAkMidiEventType::AkMidiEventTypeNoteAftertouch)
	{
		return false;
	}

	AsNoteAftertouch = FAkMidiNoteAftertouch(AkMidiEvent);
	return true;
}

bool UAkMIDIEventCallbackInfo::GetChannelAftertouch(FAkMidiChannelAftertouch& AsChannelAftertouch)
{
	if (GetType() != EAkMidiEventType::AkMidiEventTypeChannelAftertouch)
	{
		return false;
	}

	AsChannelAftertouch = FAkMidiChannelAftertouch(AkMidiEvent);
	return true;
}

bool UAkMIDIEventCallbackInfo::GetProgramChange(FAkMidiProgramChange& AsProgramChange)
{
	if (GetType() != EAkMidiEventType::AkMidiEventTypeProgramChange)
	{
		return false;
	}

	AsProgramChange = FAkMidiProgramChange(AkMidiEvent);
	return true;
}

FAkSDKExternalSourceArray::FAkSDKExternalSourceArray(const TArray<FAkExternalSourceInfo>& BlueprintArray)
{
	for (auto& ExternalSourceInfo : BlueprintArray)
	{
#if WWISE_2025_1_OR_LATER
		char* CharArray = nullptr;
#else
		AkOSChar* OsCharArray = nullptr;
#endif
		void* MediaData = nullptr;
		AkUInt32 MediaSize = 0;

		if (ExternalSourceInfo.ExternalSourceAsset)
		{
			UE_LOG(LogAkAudio, Error, TEXT("FAkSDKExternalSourceArray: ExternalSourceAssets are not supported. Please migrate your project and use AkAudioEvent."));
			return;
		}
		else
		{
			auto ExternalFileName = ExternalSourceInfo.FileName;
			if (FPaths::GetExtension(ExternalFileName).IsEmpty())
			{
				ExternalFileName += TEXT(".wem");
			}
#if WWISE_2025_1_OR_LATER
			CharArray = (char*)FMemory::Malloc((ExternalFileName.Len() + 1) * sizeof(char));
			FPlatformString::Strncpy(CharArray, TCHAR_TO_UTF8(*(ExternalFileName)), ExternalFileName.Len() + 1);
			ExternalSourceArray.Emplace(CharArray, FAkAudioDevice::GetShortIDFromString(ExternalSourceInfo.ExternalSrcName), (AkCodecID)ExternalSourceInfo.CodecID);
#else
			OsCharArray = (AkOSChar*)FMemory::Malloc((ExternalFileName.Len() + 1) * sizeof(AkOSChar));
			FPlatformString::Strcpy(OsCharArray, ExternalFileName.Len(), TCHAR_TO_AK(*(ExternalFileName)));
			ExternalSourceArray.Emplace(OsCharArray, FAkAudioDevice::GetShortIDFromString(ExternalSourceInfo.ExternalSrcName), (AkCodecID)ExternalSourceInfo.CodecID);
#endif
		}
	}
}

FAkSDKExternalSourceArray::~FAkSDKExternalSourceArray()
{
}

void FWaitEndOfEventAsyncAction::UpdateOperation(FLatentResponse& Response)
{
	if (FuturePlayingID.IsReady())
	{
		*PlayingID = FuturePlayingID.Get();
		if (*PlayingID == AK_INVALID_PLAYING_ID)
		{
			EventFinished = true;
		}

		if (EventFinished)
		{
			Response.FinishAndTriggerIf(true, ExecutionFunction, OutputLink, CallbackTarget);
		}
	}
}

AkDeviceAndWorld::AkDeviceAndWorld(AActor* in_pActor) :
	AkAudioDevice(FAkAudioDevice::Get()),
	CurrentWorld(in_pActor ? in_pActor->GetWorld() : nullptr)
{
}

AkDeviceAndWorld::AkDeviceAndWorld(const UObject* in_pWorldContextObject) :
	AkAudioDevice(FAkAudioDevice::Get()),
	CurrentWorld(GEngine->GetWorldFromContextObject(in_pWorldContextObject, EGetWorldErrorMode::ReturnNull))
{}

bool AkDeviceAndWorld::IsValid() const
{
	return (CurrentWorld && CurrentWorld->AllowAudioPlayback() && AkAudioDevice);
}