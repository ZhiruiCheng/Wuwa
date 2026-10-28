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

#include "Platforms/AkPlatform_Android/AkAndroidInitializationSettings.h"
#include "AkAudioDevice.h"
#include "WwiseDefines.h"

#if PLATFORM_ANDROID
#include "Android/AndroidApplication.h"
#endif


//////////////////////////////////////////////////////////////////////////
// FAkAndroidAdvancedInitializationSettings

FAkAndroidAdvancedInitializationSettings::FAkAndroidAdvancedInitializationSettings()
{
#if WWISE_2024_1_OR_LATER
	RoundFrameSizeToHardwareSize = false;
#else
	RoundFrameSizeToHardwareSize = true;
#endif
}

void FAkAndroidAdvancedInitializationSettings::FillInitializationStructure(FAkInitializationStructure& InitializationStructure) const
{
	Super::FillInitializationStructure(InitializationStructure);

#if PLATFORM_ANDROID
	InitializationStructure.PlatformInitSettings.bVerboseSink = bVerboseSink;

#if WWISE_2025_1_OR_LATER
	InitializationStructure.PlatformInitSettings.eAudioAPI = static_cast<AkAudioAPI>(AudioAPI | SpatializerAPI);
	InitializationStructure.PlatformInitSettings.eAudioPath = (AkAudioPath)AudioPath;
#else
	InitializationStructure.PlatformInitSettings.eAudioAPI = static_cast<AkAudioAPI>(AudioAPI);
	InitializationStructure.PlatformInitSettings.bEnableLowLatency = AudioPath != EAkAndroidAudioPath::Legacy;
	InitializationStructure.PlatformInitSettings.bRoundFrameSizeToHWSize = RoundFrameSizeToHardwareSize;
#endif

#endif
}


//////////////////////////////////////////////////////////////////////////
// UAkAndroidInitializationSettings

UAkAndroidInitializationSettings::UAkAndroidInitializationSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	CommonSettings.MainOutputSettings.PanningRule = EAkPanningRule::Headphones;
	CommonSettings.MainOutputSettings.ChannelConfigType = EAkChannelConfigType::Standard;
}

void UAkAndroidInitializationSettings::PostInitProperties()
{
	Super::PostInitProperties();
	if ((uint32)AdvancedSettings.AudioPath == (uint32)(-1))
	{
		AdvancedSettings.AudioPath = AdvancedSettings.UseLowLatencyMode_DEPRECATED ? EAkAndroidAudioPath::LowLatency : EAkAndroidAudioPath::Legacy;
	}
}

void UAkAndroidInitializationSettings::FillInitializationStructure(FAkInitializationStructure& InitializationStructure) const
{
	CommonSettings.FillInitializationStructure(InitializationStructure);
	CommunicationSettings.FillInitializationStructure(InitializationStructure);
	AdvancedSettings.FillInitializationStructure(InitializationStructure);

#if PLATFORM_ANDROID
	InitializationStructure.PlatformInitSettings.uSampleRate = CommonSettings.SampleRate;
	InitializationStructure.PlatformInitSettings.jActivity = FAndroidApplication::GetGameActivityThis();

#if USE_ANDROID_JNI
	// GJavaVM is defined only if USE_ANDROID_JNI=1
	extern JavaVM* GJavaVM;
	InitializationStructure.PlatformInitSettings.pJavaVM = GJavaVM;
#endif // USE_ANDROID_JNI
#endif // PLATFORM_ANDROID
}
