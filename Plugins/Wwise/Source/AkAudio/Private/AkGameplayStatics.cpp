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
#include "AkGameplayStatics.h"

#include "AkAmbientSound.h"
#include "AkAudioDevice.h"
#include "AkAudioEvent.h"
#include "AkAudioType.h"
#include "AkComponent.h"
#include "AkEffectShareSet.h"
#include "AkRtpc.h"
#include "AkStateValue.h"
#include "AkSwitchValue.h"
#include "AkTrigger.h"
#include "Engine/GameEngine.h"
#include "EngineUtils.h"
#include "AkAcousticPortal.h"
#include "AkRoomComponent.h"
#include "AkAuxBus.h"
#include "Wwise/API/WwiseSoundEngineAPI.h"
#include "Wwise/API/WwiseSpatialAudioAPI.h"
#include "Wwise/WwiseExternalSourceManager.h"
#include "AkComponentHelpers.h"

#include "inttypes.h"
#include "WwiseInitBankLoader/WwiseInitBankLoader.h"


bool UAkGameplayStatics::m_bSoundEngineRecording = false;
FAkOutdoorsRoomParameters UAkGameplayStatics::m_CurrentOutDoorsRoomParameters = FAkOutdoorsRoomParameters();

/*-----------------------------------------------------------------------------
	UAkGameplayStatics.
-----------------------------------------------------------------------------*/

UAkGameplayStatics::UAkGameplayStatics(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Property initialization
}

class UAkComponent * UAkGameplayStatics::GetAkComponent(USceneComponent* AttachToComponent, bool& ComponentCreated, FName AttachPointName, FVector Location, EAttachLocation::
	Type LocationType)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::GetAkComponent"));
	if ( !AttachToComponent )
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::GetAkComponent: NULL AttachToComponent specified!"));
		return nullptr;
	}

	FAkAudioDevice * AkAudioDevice = FAkAudioDevice::Get();
	if( AkAudioDevice )
	{
		return AkAudioDevice->GetAkComponent(AttachToComponent, AttachPointName, &Location, LocationType,
			ComponentCreated);
	}

	return nullptr;
}

UAkComponent* UAkGameplayStatics::GetOrCreateAkComponent(USceneComponent* AttachToComponent, bool& ComponentCreated,
	FName AttachPointName)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::GetOrCreateAkComponent"));
	if ( !AttachToComponent )
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::GetAkComponent: NULL AttachToComponent specified!"));
		return nullptr;
	}

	FAkAudioDevice * AkAudioDevice = FAkAudioDevice::Get();
	if( AkAudioDevice )
	{
		return AkAudioDevice->GetAkComponent(AttachToComponent, AttachPointName, nullptr, EAttachLocation::KeepRelativeOffset,
			ComponentCreated);
	}

	return nullptr;
}

bool UAkGameplayStatics::IsEditor()
{
#if WITH_EDITOR
	return true;
#else
	return false;
#endif
}

bool UAkGameplayStatics::IsGame(UObject* WorldContextObject)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::IsGame"));
	EWorldType::Type WorldType = EWorldType::None;
	if (WorldContextObject)
	{
		UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
		if(World)
			WorldType = World->WorldType;
	}

	return WorldType == EWorldType::Game || WorldType == EWorldType::GamePreview || WorldType == EWorldType::PIE;
}

int32 UAkGameplayStatics::PostEvent(UAkAudioEvent* AkEvent, AActor* Actor, int32 CallbackMask,
	const FOnAkPostEventCallback& PostEventCallback, bool bStopWhenAttachedToDestroyed)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::PostEvent"));
	if (UNLIKELY(!IsValid(AkEvent)))
	{
		UE_LOG(LogAkAudio, Warning, TEXT("Failed to post invalid AkAudioEvent."))
		return AK_INVALID_PLAYING_ID;
	}
	
	return AkEvent->PostOnActor(Actor, PostEventCallback, CallbackMask, bStopWhenAttachedToDestroyed);
}

int32 UAkGameplayStatics::PostAndWaitForEndOfEvent(UAkAudioEvent* AkEvent, AActor* Actor,
	FLatentActionInfo LatentInfo, bool bStopWhenAttachedToDestroyed)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::PostAndWaitForEndOfEvent"));
	if (UNLIKELY(!IsValid(AkEvent)))
	{
		UE_LOG(LogAkAudio, Error, TEXT("Failed to post and wait invalid AkAudioEvent on actor '%s'."), IsValid(Actor) ? *Actor->GetName() : TEXT("(invalid)"));
		return AK_INVALID_PLAYING_ID;
	}

	return AkEvent->PostOnActorAndWait(Actor, bStopWhenAttachedToDestroyed, LatentInfo);
}

int32 UAkGameplayStatics::PostEventAtLocation(class UAkAudioEvent* AkEvent, FVector Location, FRotator Orientation, UObject* WorldContextObject)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::PostEventAtLocation"));
	if (LIKELY(IsValid(AkEvent)))
	{
		return AkEvent->PostAtLocation(Location, Orientation, {}, 0, WorldContextObject);
	}

	UE_LOG(LogAkAudio, Error, TEXT("Failed to post event at location: posting a NULL event is not allowed."));
	return AK_INVALID_PLAYING_ID;
}

UAkComponent* UAkGameplayStatics::SpawnAkComponentAtLocation(UObject* WorldContextObject, class UAkAudioEvent* AkEvent, FVector Location, FRotator Orientation, bool AutoPost, bool AutoDestroy /* = true*/)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SpawnAkComponentAtLocation"));
	AkDeviceAndWorld DeviceAndWorld(WorldContextObject);
	if (UNLIKELY(!DeviceAndWorld.IsValid()))
	{
		return nullptr;
	}
	return DeviceAndWorld.AkAudioDevice->SpawnAkComponentAtLocation(AkEvent, Location, Orientation, AutoPost, AutoDestroy, DeviceAndWorld.CurrentWorld);
}

void UAkGameplayStatics::SetRTPCValue(const UAkRtpc* RTPCValue, float Value, int32 InterpolationTimeMs, AActor* Actor)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetRTPCValue"));
	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if (AudioDevice)
	{
		if (RTPCValue)
		{
			AudioDevice->SetRTPCValue(RTPCValue, Value, InterpolationTimeMs, Actor);
		}
	}
}

void UAkGameplayStatics::GetRTPCValue(const UAkRtpc* RTPCValue, int32 PlayingID, ERTPCValueType InputValueType, float& Value, ERTPCValueType& OutputValueType, AActor* Actor)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::GetRTPCValue"));
	if (FAkAudioDevice * AudioDevice = FAkAudioDevice::Get())
	{
		AK::SoundEngine::Query::RTPCValue_type RTPCType = (AK::SoundEngine::Query::RTPCValue_type)InputValueType;

		AkGameObjectID IdToGet = AK_INVALID_GAME_OBJECT;
		if (Actor != nullptr)
		{
			UAkComponent * ComponentToGet = AudioDevice->GetAkComponent(Actor->GetRootComponent(), FName(), NULL, EAttachLocation::KeepRelativeOffset);
			IdToGet = ComponentToGet->GetAkGameObjectID();
		}

		if (RTPCValue)
		{
			AudioDevice->GetRTPCValue(RTPCValue, IdToGet, PlayingID, Value, RTPCType);
		}

		OutputValueType = (ERTPCValueType)RTPCType;
	}
}

void UAkGameplayStatics::ResetRTPCValue(UAkRtpc const* RTPCValue, int32 InterpolationTimeMs, AActor* Actor)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::ResetRTPCValue"));
	if (FAkAudioDevice* AudioDevice = FAkAudioDevice::Get())
	{
		AkGameObjectID IdToGet = AK_INVALID_GAME_OBJECT;
		if (Actor != nullptr)
		{
			UAkComponent* ComponentToGet = AudioDevice->GetAkComponent(Actor->GetRootComponent(), FName(), NULL, EAttachLocation::KeepRelativeOffset);
			IdToGet = ComponentToGet->GetAkGameObjectID();
		}

		if (RTPCValue == NULL)
		{
			UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::ResetRTPCValue: No parameter specified!"));
			return;
		}


		AKRESULT Result = AK_Success;

		if (RTPCValue)
		{
			Result = AudioDevice->ResetRTPCValue(RTPCValue, IdToGet, InterpolationTimeMs);
		}
		else
		{
			UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::ResetRTPCValue: Could not reset RTPC value, valid RTPC value not provided"));
		}

		if (Result == AK_IDNotFound)
		{
			UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::ResetRTPCValue: Could not reset RTPC value, RTPC %s not found"), *RTPCValue->GetName());
		}
		else if (Result != AK_Success)
		{
			UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::ResetRTPCValue: Could not reset RTPC value!"));
		}
	}
}

void UAkGameplayStatics::SetState(const UAkStateValue* StateValue)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetState"));
	if(FAkAudioDevice * AudioDevice = FAkAudioDevice::Get())
	{
		if (StateValue)
		{
			AudioDevice->SetState(StateValue);
		}
	}
}

void UAkGameplayStatics::PostTrigger(const UAkTrigger* TriggerValue, AActor* Actor)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::PostTrigger"));
	if ( Actor == NULL )
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::PostTrigger: NULL Actor specified!"));
		return;
	}

	if(FAkAudioDevice * AudioDevice = FAkAudioDevice::Get())
	{
		if (TriggerValue)
		{
			AudioDevice->PostTrigger(TriggerValue, Actor);
		}
	}
}

void UAkGameplayStatics::SetSwitch(const UAkSwitchValue* SwitchValue, AActor* Actor)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetSwitch"));
	if (Actor == NULL)
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::SetSwitch: NULL Actor specified!"));
		return;
	}
	
	if (FAkAudioDevice * AudioDevice = FAkAudioDevice::Get())
	{
		if (SwitchValue)
		{
			AudioDevice->SetSwitch(SwitchValue, Actor);
		}
	}
}

void UAkGameplayStatics::SetMultiplePositions(UAkComponent* GameObjectAkComponent, TArray<FTransform> Positions,
                                              EAkMultiPositionType MultiPositionType /*= EAkMultiPositionType::MultiPositionType_MultiDirections*/)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetMultiplePositions"));
	if (GameObjectAkComponent == NULL)
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::SetMultiplePositions: NULL Component specified!"));
		return;
	}

	FAkAudioDevice * pAudioDevice = FAkAudioDevice::Get();
    if (pAudioDevice)
    {
        pAudioDevice->SetMultiplePositions(GameObjectAkComponent, Positions, MultiPositionType);
    }
}

void UAkGameplayStatics::SetMultipleChannelEmitterPositions(UAkComponent* GameObjectAkComponent,
	TArray<EAkChannelConfiguration> ChannelMasks,
	TArray<FTransform> Positions,
	EAkMultiPositionType MultiPositionType
)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetMultipleChannelEmitterPositions"));
	if (GameObjectAkComponent == NULL)
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::SetMultipleChannelEmitterPositions: NULL Component specified!"));
		return;
	}

	FAkAudioDevice * pAudioDevice = FAkAudioDevice::Get();
    if (pAudioDevice)
    {
        pAudioDevice->SetMultiplePositions(GameObjectAkComponent, ChannelMasks, Positions, MultiPositionType);
    }
}

void UAkGameplayStatics::SetMultipleChannelMaskEmitterPositions(UAkComponent* GameObjectAkComponent,
	TArray<FAkChannelMask> ChannelMasks,
	TArray<FTransform> Positions,
	EAkMultiPositionType MultiPositionType
)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetMultipleChannelMaskEmitterPositions"));
	if (GameObjectAkComponent == NULL)
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::SetMultipleChannelMaskEmitterPositions: NULL Component specified!"));
		return;
	}

	FAkAudioDevice * pAudioDevice = FAkAudioDevice::Get();
	if (pAudioDevice)
	{
		pAudioDevice->SetMultiplePositions(GameObjectAkComponent, ChannelMasks, Positions, MultiPositionType);
	}
}

void UAkGameplayStatics::UseReverbVolumes(bool inUseReverbVolumes, class AActor* Actor )
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::UseReverbVolumes"));
	if ( Actor == NULL )
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::UseReverbVolumes: NULL Actor specified!"));
		return;
	}

	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if( AudioDevice )
	{
		UAkComponent * ComponentToSet = AudioDevice->GetAkComponent(Actor->GetRootComponent(), FName(), NULL, EAttachLocation::KeepRelativeOffset);
		if( ComponentToSet != NULL )
		{
			ComponentToSet->bUseReverbVolumes = inUseReverbVolumes;
		}
	}
}

void UAkGameplayStatics::SetReflectionsOrder(int Order, bool RefreshPaths)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetReflectionsOrder"));
	if (Order > 4 || Order < 0)
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::SetReflectionsOrder: Invalid reflection order value (%d). Clamping between 0 and 4."), Order);
		Order = FMath::Clamp(Order, 0, 4);
	}

	FAkAudioDevice* AudioDevice = FAkAudioDevice::Get();
	if (AudioDevice)
	{
		AudioDevice->SetReflectionsOrder(Order, RefreshPaths);
	}
}

void UAkGameplayStatics::SetDiffractionOrder(int InDiffractionOrder, bool bInUpdatePaths)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetDiffractionOrder"));
	if (InDiffractionOrder < 0)
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::SetDiffractionOrder: Invalid diffraction order value (%d). Clamping to 0."), InDiffractionOrder);
		InDiffractionOrder = 0;
	}

	auto* SpatialAudio = IWwiseSpatialAudioAPI::Get();
	if (UNLIKELY(!SpatialAudio)) return;
	SpatialAudio->SetDiffractionOrder(InDiffractionOrder, bInUpdatePaths);
}

void UAkGameplayStatics::SetMaxEmitterRoomAuxSends(int InMaxEmitterRoomAuxSends)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetMaxEmitterRoomAuxSends"));
	if (InMaxEmitterRoomAuxSends < 0)
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::SetMaxEmitterRoomAuxSends: Invalid MaxEmitterRoomAuxSends value (%d). Clamping to 0."), InMaxEmitterRoomAuxSends);
		InMaxEmitterRoomAuxSends = 0;
	}

	auto* SpatialAudio = IWwiseSpatialAudioAPI::Get();
	if (UNLIKELY(!SpatialAudio)) return;
	SpatialAudio->SetMaxEmitterRoomAuxSends(InMaxEmitterRoomAuxSends);
}

void UAkGameplayStatics::SetNumberOfPrimaryRays(int InNbPrimaryRays)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetNumberOfPrimaryRays"));
	if (InNbPrimaryRays < 0)
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::SetNumberOfPrimaryRays: Invalid number of Primary Rays (%d). Clamping to 0."), InNbPrimaryRays);
		InNbPrimaryRays = 0;
	}

	auto* SpatialAudio = IWwiseSpatialAudioAPI::Get();
	if (UNLIKELY(!SpatialAudio)) return;
	SpatialAudio->SetNumberOfPrimaryRays(InNbPrimaryRays);
}

void UAkGameplayStatics::SetLoadBalancingSpread(int InNbFrames)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetLoadBalancingSpread"));
	if (InNbFrames < 1)
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::SetLoadBalancingSpread: Invalid number of frames (%d). Clamping to 1."), InNbFrames);
		InNbFrames = 1;
	}

	auto* SpatialAudio = IWwiseSpatialAudioAPI::Get();
	if (UNLIKELY(!SpatialAudio)) return;
	SpatialAudio->SetLoadBalancingSpread(InNbFrames);
}

void UAkGameplayStatics::SetPortalObstructionAndOcclusion(UAkPortalComponent* PortalComponent, float ObstructionValue, float OcclusionValue, bool Transition)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetPortalObstructionAndOcclusion"));
	PortalComponent->SetPortalObstructionAndOcclusion(ObstructionValue, OcclusionValue, Transition);
}

void UAkGameplayStatics::SetMaxGlobalReflectionPaths(int InMaxReflectionPaths)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetMaxGlobalReflectionPaths"));
	if (InMaxReflectionPaths < 0)
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::SetMaxGlobalReflectionPaths: Invalid number of Reflection Paths (%d). Reflection path limit has been disabled."), InMaxReflectionPaths);
		InMaxReflectionPaths = 0;
	}

	auto* SpatialAudio = IWwiseSpatialAudioAPI::Get();
	if (UNLIKELY(!SpatialAudio)) return;
	SpatialAudio->SetMaxGlobalReflectionPaths(InMaxReflectionPaths);
}

void UAkGameplayStatics::SetMaxDiffractionPaths(int InMaxDiffractionPaths, UAkGameObject* InGameObject)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetMaxDiffractionPaths"));
	auto* SpatialAudio = IWwiseSpatialAudioAPI::Get();
	if (UNLIKELY(!SpatialAudio)) return;
	SpatialAudio->SetMaxDiffractionPaths(InMaxDiffractionPaths, InGameObject == nullptr ? AK_INVALID_GAME_OBJECT : InGameObject->GetAkGameObjectID());
}

void UAkGameplayStatics::SetSmoothingConstant(float InSmoothingConstantMs, UAkGameObject* InGameObject)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetSmoothingConstant"));
	auto* SpatialAudio = IWwiseSpatialAudioAPI::Get();
	if (UNLIKELY(!SpatialAudio)) return;
	SpatialAudio->SetSmoothingConstant(InSmoothingConstantMs, InGameObject == nullptr ? AK_INVALID_GAME_OBJECT : InGameObject->GetAkGameObjectID());
}

void UAkGameplayStatics::SetAdjacentRoomBleed(float InAdjacentRoomBleed)
{
	auto* SpatialAudio = IWwiseSpatialAudioAPI::Get();
	if (UNLIKELY(!SpatialAudio)) return;
	SpatialAudio->SetAdjacentRoomBleed(std::max(InAdjacentRoomBleed, 0.f));
}

void UAkGameplayStatics::SetGameObjectToPortalObstruction(UAkComponent* GameObjectAkComponent, UAkPortalComponent* PortalComponent, float ObstructionValue)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetGameObjectToPortalObstruction"));
	if (ObstructionValue > 1.f || ObstructionValue < 0.f)
	{
		FString gameObjectName;
		GameObjectAkComponent->GetAkGameObjectName(gameObjectName);
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::SetGameObjectToPortalObstruction: Setting an invalid obstruction value (%.6g) between Game Object %s and Portal %s. Clamping between 0.0 and 1.0."), ObstructionValue, *gameObjectName, *PortalComponent->GetPortalName());
		ObstructionValue = FMath::Clamp(ObstructionValue, 0.f, 1.f);
	}

	FAkAudioDevice* AudioDevice = FAkAudioDevice::Get();
	if (AudioDevice)
	{
		AudioDevice->SetGameObjectToPortalObstruction(GameObjectAkComponent, PortalComponent, ObstructionValue);
	}
}

void UAkGameplayStatics::SetPortalToPortalObstruction(UAkPortalComponent* PortalComponent0, UAkPortalComponent* PortalComponent1, float ObstructionValue)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetPortalToPortalObstruction"));
	if (ObstructionValue > 1.f || ObstructionValue < 0.f)
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::SetGameObjectToPortalObstruction: Setting an invalid obstruction value (%.6g) between Portals %s and %s. Clamping between 0.0 and 1.0."), ObstructionValue, *PortalComponent0->GetPortalName(), *PortalComponent1->GetPortalName());
		ObstructionValue = FMath::Clamp(ObstructionValue, 0.f, 1.f);
	}

	FAkAudioDevice* AudioDevice = FAkAudioDevice::Get();
	if (AudioDevice)
	{
		AudioDevice->SetPortalToPortalObstruction(PortalComponent0, PortalComponent1, ObstructionValue);
	}
}

FAkOutdoorsRoomParameters UAkGameplayStatics::GetCurrentOutdoorsRoomParameters()
{
	return m_CurrentOutDoorsRoomParameters;
}

void UAkGameplayStatics::SetOutdoorsRoomParameters(FAkOutdoorsRoomParameters InOutdoorsRoomParameters)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetOutdoorsRoomParameters"));
	m_CurrentOutDoorsRoomParameters = InOutdoorsRoomParameters;

	AkRoomParams RoomParams = AkRoomParams();
	AkUInt32 AuxBusID = AK_INVALID_AUX_ID;

	if (m_CurrentOutDoorsRoomParameters.ReverbAuxBus)
	{
		AuxBusID = m_CurrentOutDoorsRoomParameters.ReverbAuxBus->GetShortID();
		if (AuxBusID == AK_INVALID_AUX_ID)
		{
			UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::SetOutdoorsRoomParameters: Returning invalid auxBusID for the AuxBus passed in parameters."));
		}
	}
	RoomParams.ReverbAuxBus = AuxBusID;

	RoomParams.ReverbLevel = m_CurrentOutDoorsRoomParameters.ReverbLevel;
	RoomParams.TransmissionLoss = m_CurrentOutDoorsRoomParameters.TransmissionLoss;
	RoomParams.RoomGameObj_AuxSendLevelToSelf = m_CurrentOutDoorsRoomParameters.AuxSendLevel;
	RoomParams.RoomGameObj_KeepRegistered = m_CurrentOutDoorsRoomParameters.KeepRegistered;
#if WWISE_2023_1_OR_LATER
	RoomParams.RoomPriority = 1;
#endif

	auto* SpatialAudio = IWwiseSpatialAudioAPI::Get();
	if (SpatialAudio)
	{
		SpatialAudio->SetRoom(
			AK::SpatialAudio::kOutdoorRoomID,
			RoomParams,
			"Outdoors"
		);
	}
}

void UAkGameplayStatics::ResetOutdoorsRoomParams()
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::ResetOutdoorsRoomParams"));
	auto* SpatialAudio = IWwiseSpatialAudioAPI::Get();
	if (SpatialAudio)
	{
		AKRESULT result = SpatialAudio->RemoveRoom(AK::SpatialAudio::kOutdoorRoomID);
		if (result == AK_Success)
		{
			m_CurrentOutDoorsRoomParameters = FAkOutdoorsRoomParameters();
		}
	}
}

int32 UAkGameplayStatics::PostEventOutdoors(UAkAudioEvent* AkEvent, int32 CallbackMask, const FOnAkPostEventCallback& PostEventCallback)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::PostEventOutdoors"));
	if (UNLIKELY(!IsValid(AkEvent)))
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::PostEventOutdoors: Failed to post invalid AkAudioEvent."))
			return AK_INVALID_PLAYING_ID;
	}

	// before posting an event to the outdoors room, we need to make sure the room Game Object ID exists
	// We need to set AkRoomParams.RoomGameObj_KeepRegistered to true
	if (!m_CurrentOutDoorsRoomParameters.KeepRegistered)
	{
		m_CurrentOutDoorsRoomParameters.KeepRegistered = true;
		SetOutdoorsRoomParameters(m_CurrentOutDoorsRoomParameters);
	}

	return AkEvent->PostOnGameObjectID(AK::SpatialAudio::kOutdoorRoomID.AsGameObjectID(), &PostEventCallback, nullptr, nullptr,
		AkCallbackTypeHelpers::GetCallbackMaskFromBlueprintMask(CallbackMask), nullptr);
}

void UAkGameplayStatics::StopOutdoors()
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::StopOutdoors"));
	FAkAudioDevice* AudioDevice = FAkAudioDevice::Get();
	if (UNLIKELY(!AudioDevice))
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::StopActor: Could not retrieve audio device."));
		return;
	}

	AudioDevice->StopGameObjectID(AK::SpatialAudio::kOutdoorRoomID.AsGameObjectID());
}

void UAkGameplayStatics::SetOutputBusVolume(float BusVolume, class AActor* Actor)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetOutputBusVolume"));
	if (Actor == NULL)
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::SetOutputBusVolume: NULL Actor specified!"));
		return;
	}

	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if (AudioDevice)
	{
		UAkComponent * ComponentToSet = AudioDevice->GetAkComponent(Actor->GetRootComponent(), FName(), NULL, EAttachLocation::KeepRelativeOffset);
		if (ComponentToSet != NULL)
		{
			ComponentToSet->SetOutputBusVolume(BusVolume);
		}
	}
}

void UAkGameplayStatics::SetBusConfig(const FString& BusName, EAkChannelConfiguration ChannelConfiguration)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetBusConfig"));
	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if (UNLIKELY(!AudioDevice))
	{
		return;
	}
	AkChannelConfig config;
	FAkAudioDevice::GetChannelConfig(ChannelConfiguration, config);
	AudioDevice->SetBusConfig(BusName, config);
}

void UAkGameplayStatics::SetPanningRule(EPanningRule PanRule)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetPanningRule"));
	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if (UNLIKELY(!AudioDevice))
	{
		return;
	}

	AkPanningRule AkPanRule = (PanRule == EPanningRule::PanningRule_Headphones) ? AkPanningRule_Headphones : AkPanningRule_Speakers;
	AudioDevice->SetPanningRule(AkPanRule);
}

void UAkGameplayStatics::AddOutput(const FAkOutputSettings& in_Settings, FAkOutputDeviceID& out_DeviceID, UPARAM(ref) TArray<UAkComponent*>& in_ListenerIDs)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::AddOutput"));
	auto* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine))
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::AddOutput: SoundEngine not initialized, new output will not be added."));
		return;
	}
	if (UNLIKELY(!in_Settings.AudioDeviceShareSet)) 
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::AddOutput: No Audio Device ShareSet, new output will not be added."));
		return;
	}
#if WITH_EDITORONLY_DATA
	AkUInt32 ShortID = in_Settings.AudioDeviceShareSet->AudioDeviceShareSetInfo.WwiseShortId;
#else
	AkUInt32 ShortID = in_Settings.AudioDeviceShareSet->GetShortID();
#endif
	if (UNLIKELY(ShortID == AK_INVALID_UNIQUE_ID))
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::AddOutput: Short ID for %s is invalid, new output will not be added."), *in_Settings.AudioDeviceShareSet->GetName());
		return;
	}
	AkOutputSettings OutSettings;
	OutSettings.audioDeviceShareset = ShortID;
	OutSettings.idDevice = in_Settings.IdDevice;
	OutSettings.ePanningRule = (in_Settings.PanRule == EPanningRule::PanningRule_Headphones) ? AkPanningRule_Headphones : AkPanningRule_Speakers;
	FAkAudioDevice::GetChannelConfig(in_Settings.ChannelConfig, OutSettings.channelConfig);

	AkOutputDeviceID outputDeviceID;
	AKRESULT result = AK_Fail;
	if (in_ListenerIDs.Num() == 0)
	{
		result = SoundEngine->AddOutput(OutSettings, &outputDeviceID);
	}
	else 
	{
		TArray<AkGameObjectID> akGameObjectIDArray;
		for (int i = 0; i < in_ListenerIDs.Num(); i++)
		{
			akGameObjectIDArray.Add(in_ListenerIDs[i]->GetAkGameObjectID());
		}
		result = SoundEngine->AddOutput(OutSettings, &outputDeviceID, akGameObjectIDArray.GetData(), akGameObjectIDArray.Num());
	}
	if (result != AK_Success)
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::AddOutput: AddOutput has failed, new output will not be added. AkResult: %s"), WwiseUnrealHelper::GetResultString(result));
	}
	out_DeviceID.UInt64Value = outputDeviceID;
}

void UAkGameplayStatics::RemoveOutput(FAkOutputDeviceID in_OutputDeviceId)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::RemoveOutput"));
	auto* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine))
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::RemoveOutput: Could not fetch audio device, output will not be removed."));
		return;
	}

	SoundEngine->RemoveOutput(in_OutputDeviceId.UInt64Value);
}

void UAkGameplayStatics::ReplaceMainOutput(const FAkOutputSettings& MainOutputSettings)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::ReplaceMainOutput"));
	FAkAudioDevice* AudioDevice = FAkAudioDevice::Get();
	if (UNLIKELY(!AudioDevice))
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::ReplaceMainOutput: Could not fetch audio device, main output will not be replaced."));
		return;
	}
	if (UNLIKELY(!MainOutputSettings.AudioDeviceShareSet))
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::ReplaceMainOutput: No Audio Device ShareSet, new output will not be added."));
		return;
	}
#if WITH_EDITORONLY_DATA
	AkUInt32 ShortID = MainOutputSettings.AudioDeviceShareSet->AudioDeviceShareSetInfo.WwiseShortId;
#else
	AkUInt32 ShortID = MainOutputSettings.AudioDeviceShareSet->GetShortID();
#endif
	AkOutputSettings OutSettings;
	OutSettings.audioDeviceShareset = ShortID;
	OutSettings.idDevice = MainOutputSettings.IdDevice;
	OutSettings.ePanningRule = (MainOutputSettings.PanRule == EPanningRule::PanningRule_Headphones) ? AkPanningRule_Headphones : AkPanningRule_Speakers;
	FAkAudioDevice::GetChannelConfig(MainOutputSettings.ChannelConfig, OutSettings.channelConfig);
	AudioDevice->ReplaceMainOutput(OutSettings);
}

void UAkGameplayStatics::GetSpeakerAngles(TArray<float>& SpeakerAngles, float& HeightAngle, const FString& DeviceShareSet)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::GetSpeakerAngles"));
	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if (UNLIKELY(!AudioDevice))
	{
		return;
	}
	AkOutputDeviceID DeviceID = DeviceShareSet.IsEmpty() ? 0 : AudioDevice->GetOutputID(DeviceShareSet);
	AudioDevice->GetSpeakerAngles(SpeakerAngles, HeightAngle, DeviceID);

}

void UAkGameplayStatics::SetSpeakerAngles(const TArray<float>& SpeakerAngles, float HeightAngles, const FString& DeviceShareSet)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetSpeakerAngles"));
	FAkAudioDevice* AudioDevice = FAkAudioDevice::Get();
	if (UNLIKELY(!AudioDevice))
	{
		return;
	}
	AkOutputDeviceID DeviceID = DeviceShareSet.IsEmpty() ? 0 : AudioDevice->GetOutputID(DeviceShareSet);
	AudioDevice->SetSpeakerAngles(SpeakerAngles, HeightAngles, DeviceID);

}

void UAkGameplayStatics::SetOcclusionRefreshInterval(float RefreshInterval, class AActor* Actor)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetOcclusionRefreshInterval"));
	if (Actor == NULL)
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::SetOcclusionRefreshInterval: NULL Actor specified!"));
		return;
	}

	FAkAudioDevice* AudioDevice = FAkAudioDevice::Get();
	if (UNLIKELY(!AudioDevice))
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::SetOcclusionRefreshInterval: Could not retrieve audio device."));
		return;
	}

	if (RefreshInterval < 0)
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::SetOcclusionRefreshInterval: Setting actor %s to an invalid RefreshInterval value (%.6g). Clamping to 0.0."), *Actor->GetName(), RefreshInterval);
		RefreshInterval = 0;
	}

	UAkComponent* ComponentToSet = AudioDevice->GetAkComponent(Actor->GetRootComponent(), FName(), NULL, EAttachLocation::KeepRelativeOffset);
	if (ComponentToSet != NULL)
	{
		ComponentToSet->OcclusionRefreshInterval = RefreshInterval;
	}

	UAkPortalComponent* PortalComponent = AkComponentHelpers::GetChildComponentOfType<UAkPortalComponent>(*Actor->GetRootComponent());
	if(PortalComponent != NULL)
	{
		PortalComponent->ObstructionRefreshInterval = RefreshInterval;
	}
}
	

void UAkGameplayStatics::StopActor(class AActor* Actor)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::StopActor"));
	if ( Actor == NULL )
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::StopActor: NULL Actor specified!"));
		return;
	}

	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if (UNLIKELY(!AudioDevice))
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::StopActor: Could not retrieve audio device."));
		return;
	}

	AudioDevice->StopGameObject(AudioDevice->GetAkComponent(Actor->GetRootComponent(), FName(), NULL, EAttachLocation::KeepRelativeOffset));
}

void UAkGameplayStatics::StopAll()
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::StopAll"));
	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if (UNLIKELY(!AudioDevice))
	{
		return;
	}

	AudioDevice->StopAllSounds();
}

void UAkGameplayStatics::CancelEventCallback(const FOnAkPostEventCallback& PostEventCallback)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::CancelEventCallback"));
	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if (UNLIKELY(!AudioDevice))
	{
		return;
	}
	AudioDevice->CancelEventCallbackDelegate(PostEventCallback);
}

void UAkGameplayStatics::StartAllAmbientSounds(UObject* WorldContextObject)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::StartAllAmbientSounds"));
	AkDeviceAndWorld DeviceAndWorld(WorldContextObject);
	if (UNLIKELY(!DeviceAndWorld.IsValid()))
	{
		return;
	}

	for (FActorIterator It(DeviceAndWorld.CurrentWorld); It; ++It)
	{
		AAkAmbientSound* pAmbientSound = Cast<AAkAmbientSound>(*It);
		if (pAmbientSound != NULL)
		{
			UAkComponent* pComponent = pAmbientSound->AkComponent;
			if (pComponent && GWorld->Scene == pComponent->GetScene())
			{
				pAmbientSound->StartPlaying();
			}
		}
	}

}

void UAkGameplayStatics::StopAllAmbientSounds(UObject* WorldContextObject)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::StopAllAmbientSounds"));
	AkDeviceAndWorld DeviceAndWorld(WorldContextObject);
	if (UNLIKELY(!DeviceAndWorld.IsValid()))
	{
		return;
	}

	for (FActorIterator It(DeviceAndWorld.CurrentWorld); It; ++It)
	{
		AAkAmbientSound* pAmbientSound = Cast<AAkAmbientSound>(*It);
		if (pAmbientSound != NULL)
		{
			UAkComponent* pComponent = pAmbientSound->AkComponent;
			if (pComponent && GWorld->Scene == pComponent->GetScene())
			{
				pAmbientSound->StopPlaying();
			}
		}
	}
}

void UAkGameplayStatics::ClearSoundBanksAndMedia()
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::ClearSoundBanksAndMedia"));
	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if (UNLIKELY(!AudioDevice))
	{
		return;
	}
	AudioDevice->ClearSoundBanksAndMedia();
}

void UAkGameplayStatics::LoadInitBank()
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::LoadInitBank"));
	auto* InitBankLoader = FWwiseInitBankLoader::Get();
	if(UNLIKELY(!InitBankLoader))
	{
		UE_LOG(LogAkAudio, Error, TEXT("LoadInitBank: WwiseInitBankLoader is not initialized."));
		return;
	}

	InitBankLoader->LoadInitBank();
}

void UAkGameplayStatics::UnloadInitBank()
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::UnloadInitBank"));
	auto* InitBankLoader = FWwiseInitBankLoader::Get();
	if(UNLIKELY(!InitBankLoader))
	{
		UE_LOG(LogAkAudio, Error, TEXT("UnloadInitBank: WwiseInitBankLoader is not initialized."));
		return;
	}

	InitBankLoader->UnloadInitBank();
}

void UAkGameplayStatics::StartOutputCapture(const FString& Filename)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::StartOutputCapture"));
	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if (UNLIKELY(!AudioDevice))
	{
		return;
	}
	FString name = Filename;
	if (!name.EndsWith(".wav"))
	{
		name += ".wav";
	}
	AudioDevice->StartOutputCapture(name);
}

void UAkGameplayStatics::AddOutputCaptureMarker(const FString& MarkerText)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::AddOutputCaptureMarker"));
	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if( AudioDevice )
	{
		AudioDevice->AddOutputCaptureMarker(MarkerText);
	} 
}

void UAkGameplayStatics::StopOutputCapture()
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::StopOutputCapture"));
	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if( AudioDevice )
	{
		AudioDevice->StopOutputCapture();
	}
}

void UAkGameplayStatics::StartProfilerCapture(const FString& Filename)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::StartProfilerCapture"));
	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if( AudioDevice )
	{
		FString name = Filename;
		if( !name.EndsWith(".prof") )
		{
			name += ".prof";
		}
		AudioDevice->StartProfilerCapture(name);
	} 
}

void UAkGameplayStatics::StopProfilerCapture()
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::StopProfilerCapture"));
	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if( AudioDevice )
	{
		AudioDevice->StopProfilerCapture();
	}
}

FString UAkGameplayStatics::GetCurrentAudioCulture()
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::GetCurrentAudioCulture"));
	FAkAudioDevice* AudioDevice = FAkAudioDevice::Get();
	if (AudioDevice)
	{
		return AudioDevice->GetCurrentAudioCulture();
	}

	return FString();
}

TArray<FString> UAkGameplayStatics::GetAvailableAudioCultures()
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::GetAvailableAudioCultures"));
	FAkAudioDevice* AudioDevice = FAkAudioDevice::Get();
	if (AudioDevice)
	{
		return AudioDevice->GetAvailableAudioCultures();
	}

	return TArray<FString>();
}

void UAkGameplayStatics::SetCurrentAudioCulture(const FString& AudioCulture, FLatentActionInfo LatentInfo, UObject* WorldContextObject)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetCurrentAudioCulture"));
	AkDeviceAndWorld DeviceAndWorld(WorldContextObject);
	FLatentActionManager& LatentActionManager = DeviceAndWorld.CurrentWorld->GetLatentActionManager();
	FSetCurrentAudioCultureAction* NewAction = LatentActionManager.FindExistingAction<FSetCurrentAudioCultureAction>(LatentInfo.CallbackTarget, LatentInfo.UUID);
	if (!NewAction)
	{
		NewAction = new FSetCurrentAudioCultureAction(LatentInfo);
		LatentActionManager.AddNewAction(LatentInfo.CallbackTarget, LatentInfo.UUID, NewAction);
	}

	if (FAkAudioDevice* AudioDevice = FAkAudioDevice::Get())
	{
		AudioDevice->SetCurrentAudioCultureAsync(AudioCulture, NewAction);
	}
	else
	{
		NewAction->ActionDone = true;
	}
}

void UAkGameplayStatics::SetCurrentAudioCultureAsync(const FString& AudioCulture, const FOnSetCurrentAudioCultureCallback& Completed)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetCurrentAudioCultureAsync"));
	if (FAkAudioDevice* AudioDevice = FAkAudioDevice::Get())
	{
		AudioDevice->SetCurrentAudioCultureAsync(AudioCulture, FOnSetCurrentAudioCultureCompleted::CreateLambda([Completed](bool Succeeded) {
			Completed.ExecuteIfBound(Succeeded);
		}));
	}
}

UObject* UAkGameplayStatics::GetAkAudioTypeUserData(const UAkAudioType* Instance, const UClass* Type)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::GetAkAudioTypeUserData"));
	if (UNLIKELY(!Instance))
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::GetAkAudioTypeUserData: nullptr Instance specified!"));
		return nullptr;
	}
	if (UNLIKELY(!Type))
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::GetAkAudioTypeUserData: nullptr Type specified!"));
		return nullptr;
	}
	
	for (auto const& Entry : Instance->UserData)
	{
		if (LIKELY(Entry && IsValid(Entry)) && Entry->GetClass()->IsChildOf(Type))
		{
			return Entry;
		}
	}

	return nullptr;
}

void UAkGameplayStatics::SetDistanceProbe(AActor* Listener, AActor* DistanceProbe)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetDistanceProbe"));
	if (Listener == nullptr)
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::SetDistanceProbe: NULL Listener specified!"));
		return;
	}

	if (FAkAudioDevice* AudioDevice = FAkAudioDevice::Get())
	{
		UAkComponent * ListenerAkComponent = AudioDevice->GetAkComponent(Listener->GetRootComponent(), FName(), NULL, EAttachLocation::KeepRelativeOffset);

		UAkComponent* DistanceProbeAkComponent = 
			DistanceProbe != nullptr ?
			AudioDevice->GetAkComponent(DistanceProbe->GetRootComponent(), FName(), NULL, EAttachLocation::KeepRelativeOffset) :
			nullptr;

		AudioDevice->SetDistanceProbe(ListenerAkComponent, DistanceProbeAkComponent);
	}
}

bool UAkGameplayStatics::SetOutputDeviceEffect(const FAkOutputDeviceID InDeviceID, const int32 InEffectIndex, const UAkEffectShareSet* InEffectShareSet)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetOutputDeviceEffect"));
	if(UNLIKELY(!InEffectShareSet)) 
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::SetOutputDeviceEffect: NULL Effect ShareSet specified!"));
		return false;
	}

	auto* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) return false;

	UE_LOG(LogAkAudio, Verbose, TEXT("UAkGameplayStatics::SetOutputDeviceEffect: DeviceID: %" PRIu64 ", InEffectIndex: %d, EffectShareSet Asset Name: %s, EffectShareSet ShortID: %" PRIu32 "."), 
		InDeviceID.UInt64Value, InEffectIndex, *InEffectShareSet->GetName(), InEffectShareSet->GetShortID());

	AKRESULT Result =  SoundEngine->SetOutputDeviceEffect(InDeviceID.UInt64Value, InEffectIndex, InEffectShareSet->GetShortID());
	return Result == AK_Success;
}

bool UAkGameplayStatics::SetBusEffectByName(const FString InBusName, const int32 InEffectIndex, const UAkEffectShareSet* InEffectShareSet)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetBusEffectByName"));
	if(UNLIKELY(!InEffectShareSet)) 
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::SetBusEffectByName: NULL Effect ShareSet specified!"));
		return false;
	}
	auto* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) return false;

	UE_LOG(LogAkAudio, Verbose, TEXT("UAkGameplayStatics::SetBusEffectByName: BusName: %s, InEffectIndex: %d,  EffectShareSet Asset Name: %s, EffectShareSet ShortID: %" PRIu32 "."), 
		*InBusName, InEffectIndex, *InEffectShareSet->GetName(), InEffectShareSet->GetShortID());

	AKRESULT Result = SoundEngine->SetBusEffect(TCHAR_TO_AK(*InBusName), InEffectIndex, InEffectShareSet->GetShortID());
	return Result == AK_Success;
}

bool UAkGameplayStatics::SetBusEffectByID(const FAkUniqueID InBusID, const int32 InEffectIndex, const UAkEffectShareSet* InEffectShareSet)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetBusEffectByID"));
	if(UNLIKELY(!InEffectShareSet)) 
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::SetBusEffectByID: NULL Effect ShareSet specified!"));
		return false;
	}
	auto* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) return false;

	UE_LOG(LogAkAudio, Verbose, TEXT("UAkGameplayStatics::SetBusEffectByID: BusID: %" PRIu32 ", InEffectIndex: %d, EffectShareSet Asset Name: %s, EffectShareSet ShortID: %" PRIu32 "."), 
		InBusID.UInt32Value, InEffectIndex, *InEffectShareSet->GetName(), InEffectShareSet->GetShortID());
	AKRESULT Result = SoundEngine->SetBusEffect(InBusID.UInt32Value, InEffectIndex, InEffectShareSet->GetShortID());
	return Result == AK_Success;
}

bool UAkGameplayStatics::SetAuxBusEffect(const UAkAuxBus* InAuxBus, const int32 InEffectIndex, const UAkEffectShareSet* InEffectShareSet)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetAuxBusEffect"));
	if(UNLIKELY(!InEffectShareSet)) 
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::SetAuxBusEffect: NULL Effect ShareSet specified!"));
		return false;
	}
	if(UNLIKELY(!InAuxBus)) 
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::SetAuxBusEffect: NULL Aux Bus specified!"));
		return false;
	}
	auto* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) return false;

	UE_LOG(LogAkAudio, Verbose, TEXT("UAkGameplayStatics::SetAuxBusEffect: AuxBus Asset Name: %s, AuxBus Short ID: %" PRIu32 ", InEffectIndex: %d, EffectShareSet Asset Name: %s, EffectShareSet ShortID: %" PRIu32 "."), 
		*InAuxBus->GetName(), InAuxBus->GetShortID(), InEffectIndex, *InEffectShareSet->GetName(), InEffectShareSet->GetShortID());
	AKRESULT Result = SoundEngine->SetBusEffect(InAuxBus->GetShortID(), InEffectIndex, InEffectShareSet->GetShortID());
	return Result == AK_Success;
}

bool UAkGameplayStatics::SetActorMixerEffect(const FAkUniqueID InAudioNodeID,const  int32 InEffectIndex, const UAkEffectShareSet* InEffectShareSet)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkGameplayStatics::SetActorMixerEffect"));
	if(UNLIKELY(!InEffectShareSet)) 
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkGameplayStatics::SetActorMixerEffect: NULL Effect ShareSet specified!"));
		return false;
	}
	auto* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) return false;

	UE_LOG(LogAkAudio, Verbose, TEXT("UAkGameplayStatics::SetActorMixerEffect: AudioNodeID: %" PRIu32 ", InEffectIndex: %d, EffectShareSet Asset Name: %s, EffectShareSet ShortID: %" PRIu32 "."), 
		InAudioNodeID.UInt32Value, InEffectIndex, *InEffectShareSet->GetName(), InEffectShareSet->GetShortID());
	AKRESULT Result = SoundEngine->SetActorMixerEffect(InAudioNodeID.UInt32Value, InEffectIndex, InEffectShareSet->GetShortID());
	return Result == AK_Success;
}
