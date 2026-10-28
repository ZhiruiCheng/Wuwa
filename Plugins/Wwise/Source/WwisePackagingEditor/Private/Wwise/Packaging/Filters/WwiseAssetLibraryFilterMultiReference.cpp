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

#include "Wwise/Packaging/Filters/WwiseAssetLibraryFilterMultiReference.h"

#include "Wwise/Packaging/WwiseAssetLibraryFilteringSharedData.h"

bool UWwiseAssetLibraryFilterMultiReference::IsAssetAvailable(const FWwiseAssetLibraryFilteringSharedData& Shared,
		const WwiseAnyRef& Asset) const
{
	int Id;
	const WwiseRefType AssetType = Asset.GetType();
	bool bIsMedia = false;
	if(AssetType == WwiseRefType::Media)
	{
		Id = Asset.GetMedia()->Id;
		bIsMedia = true;
	}
	else if(AssetType == WwiseRefType::SoundBank)
	{
		Id = Asset.GetSoundBank()->Id;
	}
	else
	{
		return false;
	}
	if (Shared.Db.GetUsageCount(Asset) == 1)
	{
		return false;
	}
	int count = 0;
	for (auto& UnrealAsset : Shared.AssetsData)
	{
		auto GuidValue = UnrealAsset.TagsAndValues.FindTag(GET_MEMBER_NAME_CHECKED(FWwiseObjectInfo, WwiseGuid));
		auto ShortIdValue = UnrealAsset.TagsAndValues.FindTag(GET_MEMBER_NAME_CHECKED(FWwiseObjectInfo, WwiseShortId));
		auto NameValue = UnrealAsset.TagsAndValues.FindTag(GET_MEMBER_NAME_CHECKED(FWwiseGroupValueInfo, WwiseName));
		if (UnrealAsset.AssetClassPath.GetAssetName().ToString().Contains("AkAudioEvent"))
		{
			FWwiseEventInfo EventInfo;
			EventInfo.WwiseGuid = FGuid(GuidValue.AsString());
			EventInfo.WwiseShortId = FCString::Strtoui64(*ShortIdValue.GetValue(), NULL, 10);
			EventInfo.WwiseName = NameValue.AsName();
			auto Events = Shared.Db.GetEvent(EventInfo);

			for (auto& Event : Events)
			{
				if (bIsMedia)
				{
					auto Medias = Event.GetAllMedia(Shared.Db.GetMediaFiles());
					for (auto& Media : Medias)
					{
						if (Media.Key == Id)
						{
							count++;
						}
					}
				}
				else
				{
					if (auto SoundBank = Event.GetSoundBank())
					{
						if (SoundBank->Id == Id)
						{
							count++;
						}
					}
				}
			}
		}
		if (UnrealAsset.AssetClassPath.GetAssetName().ToString().Contains("AkAuxBus"))
		{
			FWwiseObjectInfo ObjectInfo;
			ObjectInfo.WwiseGuid = FGuid(GuidValue.AsString());
			ObjectInfo.WwiseShortId = FCString::Strtoui64(*ShortIdValue.GetValue(), NULL, 10);
			ObjectInfo.WwiseName = NameValue.AsName();
			auto Bus = Shared.Db.GetAuxBus(ObjectInfo);
			if (bIsMedia)
			{
				auto Medias = Bus.GetSoundBankMedia(Shared.Db.GetMediaFiles());
				for (auto& Media : Medias)
				{
					if (Media.Key == Id)
					{
						count++;
					}
				}
			}
			else
			{
				if (auto SoundBank = Bus.GetSoundBank())
				{
					if (SoundBank->Id == Id)
					{
						count++;
					}
				}
			}
		}
		
		if(count > 1)
		{
			return true;
		}
	}
	return false;
}

#define LOCTEXT_NAMESPACE "AssetTypeActions"

FText FAssetTypeActions_WwiseAssetLibraryFilterMultiReference::GetName() const
{
	return LOCTEXT("AssetTypeActions_WwiseAssetLibraryFilterMultiReference", "Wwise Asset Library Filter : Multiple Reference");
}

UClass* FAssetTypeActions_WwiseAssetLibraryFilterMultiReference::GetSupportedClass() const
{
	return UWwiseAssetLibraryFilterMultiReference::StaticClass();
}

#undef LOCTEXT_NAMESPACE
