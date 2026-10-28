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

#include "Wwise/WwiseSoundEngine_2025_1.h"
#include "Wwise/WwiseSoundEngineModule.h"
#include "Wwise/API_2025_1/WwiseCommAPI_2025_1.h"
#include "Wwise/API_2025_1/WwiseMemoryMgrAPI_2025_1.h"
#include "Wwise/API_2025_1/WwiseMonitorAPI_2025_1.h"
#include "Wwise/API_2025_1/WwiseSoundEngineAPI_2025_1.h"
#include "Wwise/API_2025_1/WwiseSpatialAudioAPI_2025_1.h"
#include "Wwise/API_2025_1/WwiseStreamMgrAPI_2025_1.h"
#include "Wwise/API_2025_1/WwisePlatformAPI_2025_1.h"

IWwiseCommAPI* FWwiseSoundEngine_2025_1::GetComm()
{
	return new FWwiseCommAPI_2025_1;
}

IWwiseMemoryMgrAPI* FWwiseSoundEngine_2025_1::GetMemoryMgr()
{
	return new FWwiseMemoryMgrAPI_2025_1;
}

IWwiseMonitorAPI* FWwiseSoundEngine_2025_1::GetMonitor()
{
	return new FWwiseMonitorAPI_2025_1;
}

IWwiseSoundEngineAPI* FWwiseSoundEngine_2025_1::GetSoundEngine()
{
	return new FWwiseSoundEngineAPI_2025_1;
}

IWwiseSpatialAudioAPI* FWwiseSoundEngine_2025_1::GetSpatialAudio()
{
	return new FWwiseSpatialAudioAPI_2025_1;
}

IWwiseStreamMgrAPI* FWwiseSoundEngine_2025_1::GetStreamMgr()
{
	return new FWwiseStreamMgrAPI_2025_1;
}

IWwisePlatformAPI* FWwiseSoundEngine_2025_1::GetPlatform()
{
	return new FWwisePlatformAPI;
}
