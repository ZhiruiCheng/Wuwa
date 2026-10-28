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

#include "Engine/EngineTypes.h"
#include "AkInclude.h"
#include "InitializationSettings/AkInitializationSettings.h"
#include "InitializationSettings/AkPlatformInitializationSettingsBase.h"

#include "AkAndroidInitializationSettings.generated.h"

UENUM(Meta = (Bitmask))
enum class EAkAndroidAudioAPI : uint32
{
	AAudio,
	OpenSL_ES
};

UENUM(Meta = (Bitmask))
enum class EAkAndroidSpatializerAPI : uint32
{
	DolbyAtmos = 8,
	AndroidSpatializer = 9,
};

UENUM()
enum class EAkAndroidAudioPath : uint32
{
	Legacy,
	LowLatency,
	Exclusive,
};

USTRUCT()
struct FAkAndroidAdvancedInitializationSettings : public FAkAdvancedInitializationSettingsWithMultiCoreRendering
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Ak Initialization Settings", meta = (Bitmask, BitmaskEnum = "/Script/AkAudio.EAkAndroidAudioAPI", ToolTip = "Main audio API to allow using for audio output. Enable both to let Wwise decide the best audio API for the device."))
	uint32 AudioAPI = (1 << (uint32)EAkAndroidAudioAPI::AAudio) | (1 << (uint32)EAkAndroidAudioAPI::OpenSL_ES);

	UPROPERTY(EditAnywhere, Category = "Ak Initialization Settings", meta = (Bitmask, BitmaskEnum = "/Script/AkAudio.EAkAndroidSpatializerAPI", ToolTip = "Spatializer API to allow using for 3D audio support. Note that Android Spatializer has noticeable latency issues. Disabling all spatializer APIs will effectively disable 3D audio."))
	uint32 SpatializerAPI = (1 << (uint32)EAkAndroidSpatializerAPI::DolbyAtmos);

	UPROPERTY(EditAnywhere, Category = "Ak Initialization Settings", meta = (ToolTip = "Which audio path to use. Legacy gives best compatibility with the widest range of devices but noticeably high latency. Exclusive has best latency but has several drawbacks and bad compatibility. LowLatency is a good balance between the two."))
	EAkAndroidAudioPath AudioPath = (EAkAndroidAudioPath)(-1); // Invalid value for initial project migration.

	UPROPERTY(EditAnywhere, Category = "Ak Initialization Settings", meta = (ToolTip = "(deprecated) Rounds the pipeline buffer size to a multiple of the hardware-preferred frame size. This setting is deprecated. This has no impact on performance and should be left to false (the default)."))
	bool RoundFrameSizeToHardwareSize = false;

	UPROPERTY(Config)
	bool UseLowLatencyMode_DEPRECATED = true;

	UPROPERTY(EditAnywhere, Category = "Ak Initialization Settings", meta = (ToolTip = "Enable this to inspect sink behavior. Useful for debugging non-standard Android devices."))
	bool bVerboseSink = false;

	FAkAndroidAdvancedInitializationSettings();

	void FillInitializationStructure(FAkInitializationStructure& InitializationStructure) const;
};


UCLASS(config = Game, defaultconfig)
class AKAUDIO_API UAkAndroidInitializationSettings : public UAkPlatformInitializationSettingsBase
{
	GENERATED_BODY()

public:
	virtual const TCHAR* GetConfigOverridePlatform() const override
	{
		return TEXT("Android");
	}

	UAkAndroidInitializationSettings(const class FObjectInitializer& ObjectInitializer);

	virtual void PostInitProperties() override;

	void FillInitializationStructure(FAkInitializationStructure& InitializationStructure) const override;

	UPROPERTY(Config, EditAnywhere, Category = "Initialization")
	FAkCommonInitializationSettingsWithSampleRate CommonSettings;

	UPROPERTY(Config, EditAnywhere, Category = "Initialization")
	FAkCommunicationSettingsWithSystemInitialization CommunicationSettings;

	UPROPERTY(Config, EditAnywhere, Category = "Initialization", AdvancedDisplay)
	FAkAndroidAdvancedInitializationSettings AdvancedSettings;

	UFUNCTION()
	void MigrateMultiCoreRendering(bool NewValue) 
	{
		AdvancedSettings.EnableMultiCoreRendering = NewValue;
	}
};
