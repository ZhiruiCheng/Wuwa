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
	AkAmbientSound.cpp:
=============================================================================*/

#include "AkAmbientSound.h"
#include "AkAudioDevice.h"
#include "AkComponent.h"
#include "AkAudioEvent.h"
#include "Wwise/WwiseExternalSourceManager.h"

/*------------------------------------------------------------------------------------
	AAkAmbientSound
------------------------------------------------------------------------------------*/

AAkAmbientSound::AAkAmbientSound(const class FObjectInitializer& ObjectInitializer) :
Super(ObjectInitializer)
{
	SCOPED_AKAUDIO_EVENT_3(TEXT("AAkAmbientSound Ctor"));
	// Property initialization
	StopWhenOwnerIsDestroyed = true;
	CurrentlyPlaying = false;
	
	static const FName ComponentName = TEXT("AkAudioComponent0");
	AkComponent = ObjectInitializer.CreateOptionalDefaultSubobject<UAkComponent>(this, ComponentName);

	if (AkComponent)
	{
		AkComponent->StopWhenOwnerDestroyed = StopWhenOwnerIsDestroyed;
		RootComponent = AkComponent;
	}

	SetHidden(true);
	AutoPost = false;
}

AAkAmbientSound::AAkAmbientSound() : CurrentlyPlaying(false) {}

void AAkAmbientSound::PostLoad()
{
	SCOPED_AKAUDIO_EVENT_3(TEXT("AAkAmbientSound::PostLoad"));
	Super::PostLoad();
#if WITH_EDITOR
	if( AkAudioEvent_DEPRECATED )
	{
		AkComponent->AkAudioEvent = AkAudioEvent_DEPRECATED;
	}
#endif
}

void AAkAmbientSound::PostInitializeComponents()
{
	SCOPED_AKAUDIO_EVENT_3(TEXT("AAkAmbientSound::PostInitializeComponents"));
	Super::PostInitializeComponents();
	AkComponent->UpdateAkLateReverbComponentList(AkComponent->GetComponentLocation());
}

void AAkAmbientSound::BeginPlay()
{
	SCOPED_AKAUDIO_EVENT_3(TEXT("AAkAmbientSound::BeginPlay"));
	Super::BeginPlay();
	if (AutoPost)
	{
		StartAmbientSound();
	}
}


#if WITH_EDITOR
void AAkAmbientSound::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if( AkComponent )
	{
		// Reset audio component.
		if( IsCurrentlyPlaying() )
		{
			StartPlaying();
		}
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void AAkAmbientSound::StartAmbientSound()
{
	SCOPED_AKAUDIO_EVENT(TEXT("AAkAmbientSound::StartAmbientSound"));
	StartPlaying();
}

void AAkAmbientSound::StopAmbientSound()
{
	SCOPED_AKAUDIO_EVENT(TEXT("AAkAmbientSound::StopAmbientSound"));
	StopPlaying();
}

void AAkAmbientSound::StartPlaying()
{
	SCOPED_AKAUDIO_EVENT_2(TEXT("AAkAmbientSound::StartPlaying"));
	if( !IsCurrentlyPlaying() )
	{
		if (AkComponent->AkAudioEvent)
		{
			AkComponent->AkAudioEvent->PostOnActor(this, nullptr, nullptr, nullptr, (AkCallbackType)0, nullptr, StopWhenOwnerIsDestroyed);
		}
		else
		{
			UE_LOG(LogAkAudio, Warning, TEXT("AAkAmbientSound::StartPlaying: Failed to post invalid AkAudioEvent on actor '%s'."), *this->GetName());
		}
	}
}

void AAkAmbientSound::StopPlaying()
{
	SCOPED_AKAUDIO_EVENT_2(TEXT("AAkAmbientSound::StopPlaying"));
	if( IsCurrentlyPlaying() )
	{
		// State of CurrentlyPlaying gets updated in UAkComponent::Stop() through the EndOfEvent callback.
		AkComponent->Stop();
	}
}

bool AAkAmbientSound::IsCurrentlyPlaying()
{
	return AkComponent != nullptr && AkComponent->HasActiveEvents();
}
