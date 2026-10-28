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

#include "Wwise/Loaded/WwiseLoadedAudioNode.h"

#include "Wwise/Stats/ResourceLoader.h"

FWwiseLoadedAudioNodeInfo::FLoadedData::FLoadedData()
{
}

FWwiseLoadedAudioNodeInfo::FLoadedData::FLoadedData(const FLoadedData& Original) :
	LoadedSoundBanks(Original.LoadedSoundBanks),
	LoadedExternalSources(Original.LoadedExternalSources),
	LoadedMedia(Original.LoadedMedia),
	IsProcessing(Original.IsProcessing.load())
{
}

bool FWwiseLoadedAudioNodeInfo::FLoadedData::IsLoaded() const
{
	return LoadedSoundBanks.Num() > 0 || LoadedExternalSources.Num() > 0 || LoadedMedia.Num() > 0;
}

FWwiseLoadedAudioNodeInfo::FWwiseLoadedAudioNodeInfo(
	const FWwiseAudioNodeCookedData& AudioNode):
	Condition({}),
	AudioNode(AudioNode)
{}

FWwiseLoadedAudioNodeInfo::FWwiseLoadedAudioNodeInfo(
	const FWwiseGroupValueCookedDataSet& Condition, const FWwiseAudioNodeCookedData& AudioNode):
	Condition(Condition),
	AudioNode(AudioNode)
{}

bool FWwiseLoadedAudioNodeInfo::HaveAllKeys() const
{
	if (UNLIKELY(Condition.GroupValues.Num() < LoadedGroupValues.Num()))
	{
		UE_LOG(LogWwiseResourceLoader, Error, TEXT("Have more keys loaded (%d) than existing in key (%d) @ %p for key %s"),
			LoadedGroupValues.Num(), Condition.GroupValues.Num(), &LoadedData, *Condition.GetDebugString());
		return true;
	}

	return Condition.GroupValues.Num() == LoadedGroupValues.Num();
}

FString FWwiseLoadedAudioNodeInfo::GetDebugString() const
{
	return AudioNode.GetDebugString();
}

