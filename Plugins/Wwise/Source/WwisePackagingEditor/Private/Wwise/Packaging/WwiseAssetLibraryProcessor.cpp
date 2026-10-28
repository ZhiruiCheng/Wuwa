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

#include "Wwise/Packaging/WwiseAssetLibraryProcessor.h"

#include "AssetDefinition.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Async/ParallelFor.h"
#include "Settings/ProjectPackagingSettings.h"
#include "Wwise/Packaging/WwiseAssetLibraryFilter.h"
#include "Wwise/Packaging/WwiseAssetLibraryInfo.h"
#include "Wwise/Packaging/WwiseAssetLibraryFilteringSharedData.h"
#include "Wwise/Metadata/WwiseMetadataLanguage.h"
#include "Wwise/WwiseAllowShrinking.h"
#include "Wwise/WwiseGuidConverter.h"
#include "Wwise/Packaging/WwisePackagingData.h"
#include "Wwise/Packaging/WwisePackagingUtils.h"
#include "Wwise/Stats/Packaging.h"

FCriticalSection FWwiseAssetLibraryProcessor::IsFilteringCrit;

FWwiseAssetLibraryFilteringSharedData* FWwiseAssetLibraryProcessor::InstantiateSharedData(FWwiseProjectDatabase& ProjectDatabase)
{
	return new FWwiseAssetLibraryFilteringSharedData(ProjectDatabase);
}

void FWwiseAssetLibraryProcessor::RetrieveAssetMap(FWwiseAssetLibraryFilteringSharedData& Shared)
{
	Shared.Sources.Empty();
	Shared.Remaining.Empty();
	RetrieveSoundBankMap(Shared);
	RetrieveMediaMap(Shared);
}

void FWwiseAssetLibraryProcessor::RetrieveMediaMap(FWwiseAssetLibraryFilteringSharedData& Shared)
{
	const WwiseMediaGlobalIdsMap& Media{Shared.Db.GetMediaFiles()};
	const auto Num { Media.Size() };
	Shared.Sources.Reserve(Shared.Sources.Num() + Num);
	Shared.Remaining.Reserve(Shared.Sources.Num());

	for (const auto& Item : Media)
	{
		WwiseDBPair<const WwiseDatabaseLocalizableIdKey, WwiseRefMedia> Pair(Item);
		if (!FilterMedia(Pair.GetSecond()))
		{
			continue;
		}
		const auto Pos { Shared.Sources.Emplace(WwiseAnyRef::Create(Pair.GetSecond())) };
		Shared.Remaining.Add(Pos);
	}
}

void FWwiseAssetLibraryProcessor::RetrieveSoundBankMap(FWwiseAssetLibraryFilteringSharedData& Shared)
{
	const auto& SoundBanks { Shared.Db.GetSoundBanks() };
	const auto Num { SoundBanks.Size() };
	Shared.Sources.Reserve(Shared.Sources.Num() + Num);
	Shared.Remaining.Reserve(Shared.Sources.Num());

	for (const auto& Item : SoundBanks)
	{
		WwiseDBPair<const WwiseDatabaseLocalizableIdKey, WwiseRefSoundBank> Pair(Item);
		if (!FilterSoundBank(Pair.GetSecond()))
		{
			continue;
		}
		const auto Pos { Shared.Sources.Emplace(WwiseAnyRef::Create(Pair.GetSecond())) };
		Shared.Remaining.Add(Pos);
	}
}

bool FWwiseAssetLibraryProcessor::FilterMedia(const WwiseRefMedia& Media)
{
	if (UNLIKELY(!Media.IsValid()))
	{
		return false;
	}

	const auto Metadata { Media.GetMedia() };

	const auto Location { Metadata->Location };
	if (UNLIKELY(Location == WwiseMetadataMediaLocation::Unknown
		|| Location == WwiseMetadataMediaLocation::OtherBank))
	{
		return false;
	}

	if (Location == WwiseMetadataMediaLocation::Memory && !Metadata->bStreaming)
	{
		return false;
	}

	return true;
}

bool FWwiseAssetLibraryProcessor::FilterSoundBank(const WwiseRefSoundBank& SoundBank)
{
	if (UNLIKELY(!SoundBank.IsValid()))
	{
		return false;
	}

	return true;
}

void FWwiseAssetLibraryProcessor::GetRelevantAssets(const TArray<FString>& PackagePaths, TArray<FAssetData>& RelevantAssets, const TArray<FString>& ExcludedPackagePaths)
{
	RelevantAssets.Empty();
	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.TagsAndValues.Add("WwiseGuid");
	for (auto& PackagePath : PackagePaths)
	{
		FString Package = WwisePackagingUtils::GetGameplayModuleRoot(PackagePath);
		Filter.PackagePaths.Add(FName(Package));
	}

	auto& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> WwiseAssets;
	AssetRegistryModule.GetAssets(Filter, WwiseAssets);
	
	for (auto& AssetData : WwiseAssets)
	{
		FString Package = WwisePackagingUtils::GetGameplayModuleRoot(AssetData.PackagePath.ToString());
		bool bIgnore = false;
		for (auto& ExcludedPackagePath : ExcludedPackagePaths)
		{
			if (ExcludedPackagePath.Contains(Package))
			{
				bIgnore = true;
			}
		}
		if (bIgnore)
		{
			continue;
		}
		RelevantAssets.AddUnique(AssetData);
	}
}

void FWwiseAssetLibraryProcessor::FilterLibraryAssets(FWwiseAssetLibraryFilteringSharedData& Shared, FWwiseAssetLibraryInfo& Library, bool bUpdateRemaining, bool bUpdateFilteredAssets)
{
	FScopeLock Lock(&IsFilteringCrit);
	auto GlobalMedias = Shared.Db.GetMediaFiles();
	
	const auto Num { Shared.Remaining.Num() };
	std::atomic_int32_t FilteredAssetsNum { 0 };
	TArray<int32> FilteredPosArray;
	FilteredPosArray.AddUninitialized(Num);

	Shared.Remaining.Compact();
	Shared.SkippedAssetsCount = 0;

	const auto Filters{ Library.GetFilters() };
	ParallelFor(Filters.Num(), [this, &Shared, &Library, &Filters](int32 Iter) mutable
	{
		if (const auto& Filter{ Filters[Iter] })
		{
			Filter->PreFilter(Shared, Library);
		}
	});
	
	const auto AssetDataNum { Shared.AssetsData.Num() };
	FWwisePackagingData PackagingData;

	TArray<FWwisePackagingId> PackagedIdList;
	PackagedIdList.SetNum(AssetDataNum);

	TArray<TArray<WwiseDBShortId>> MediaIdList;
	MediaIdList.SetNum(AssetDataNum);

	ParallelFor(AssetDataNum, [this, &Shared, &GlobalMedias, &PackagedIdList, &MediaIdList](int32 Iter)
	{
		const FAssetData& AssetData{ Shared.AssetsData[Iter] };
		
		auto GuidTag = AssetData.TagsAndValues.FindTag(GET_MEMBER_NAME_CHECKED(FWwiseObjectInfo, WwiseGuid));
		auto ShortIdTag = AssetData.TagsAndValues.FindTag(GET_MEMBER_NAME_CHECKED(FWwiseObjectInfo, WwiseShortId));
	
		if (GuidTag.IsSet() && ShortIdTag.IsSet())
		{
			FGuid Guid(GuidTag.AsString());
			int32 ShortId = FCString::Atoi(*ShortIdTag.GetValue());
			FWwisePackagingId packagingId(Guid, ShortId);
			PackagedIdList[Iter] = packagingId;
			
			TArray<WwiseDBShortId> mediaList;
			
			auto Events = Shared.Db.GetAllLanguageEvents(FWwiseEventInfo(Guid, ShortId, AssetData.AssetName.ToString()));
			auto Bus = Shared.Db.GetBus(FWwiseEventInfo(Guid, ShortId, AssetData.AssetName.ToString()));
			if (Bus.IsValid())
			{
				auto Medias = Bus.GetSoundBankMedia(GlobalMedias);
				for (auto& Media : Medias)
				{
					mediaList.Add(Media.Value.MediaId());
				}
			}
	
			for (auto& Event : Events)
			{
				auto Medias = Event.GetAllMedia(Shared.Db.GetMediaFiles());
				for (auto& Media : Medias)
				{
					mediaList.Add(Media.Value.MediaId());
				}
			}
	
			MediaIdList[Iter] = mediaList;
		}
	});
	
	for (const FWwisePackagingId& LocalId : PackagedIdList)
	{
		PackagingData.PackagingIds.Add(LocalId); 
	}

	for (const TArray<WwiseDBShortId>& LocalIds : MediaIdList)
	{
		PackagingData.MediaIds.Append(LocalIds);
	}
	
	ParallelFor(Num, [this, &Shared, &Library, &FilteredAssetsNum, &FilteredPosArray, &GlobalMedias, &PackagingData](int32 Iter)
	{
		const WwiseAnyRef& SourceRef{ Shared.Sources[Shared.Remaining[Iter]] };
		bool bShouldFilter = false;

		if (!Shared.bConsiderAssetsData)
		{
			bShouldFilter = true;
		}
		if(SourceRef.GetType() == WwiseRefType::SoundBank && SourceRef.GetSoundBank()->IsInitBank())
		{
			bShouldFilter = true;
		}
	
		if(SourceRef.GetType() == WwiseRefType::SoundBank)
		{
			for (auto& Event : SourceRef.GetSoundBank()->Events)
			{
				FWwisePackagingId NewEntry = FWwisePackagingId(Event.GUID, Event.Id);
				if (PackagingData.PackagingIds.Contains(NewEntry))
				{
					bShouldFilter = true;
					break;
				}
			}
		}
		else if(SourceRef.GetType() == WwiseRefType::Media)
		{
			if (PackagingData.MediaIds.Contains(SourceRef.GetId()))
			{
				bShouldFilter = true;
			}
		}
		
		if (bShouldFilter && FilterAsset(Shared, Library, SourceRef))
		{
			const auto IdToAdd = FilteredAssetsNum++;
			FilteredPosArray[IdToAdd] = Iter;
		}
	});

	ParallelFor(Filters.Num(), [this, &Shared, &Library, &Filters](int32 Iter) mutable
	{
		if (const auto& Filter{ Filters[Iter] })
		{
			Filter->PostFilter(Shared, Library);
		}
	});

	if (Num != FilteredAssetsNum)
	{
		FilteredPosArray.RemoveAt(FilteredAssetsNum, Num - FilteredAssetsNum, EWwiseAllowShrinking::No);
	}
	FilteredPosArray.Sort();

	auto& FilteredAssets { Shared.FilteredAssets };
	FilteredAssets.Empty(FilteredAssetsNum);
	for (const auto& FilteredPos : FilteredPosArray)
	{
		const WwiseAnyRef& SourceRef{ Shared.Sources[Shared.Remaining[FilteredPos]] };
		FWwiseAssetLibraryRef NewRef;
		CreateAssetLibraryRef(NewRef, SourceRef);
		FilteredAssets.Emplace(NewRef);
	}
	
	if (bUpdateRemaining)
	{
		for (auto Iter = FilteredAssetsNum - 1; Iter >= 0; --Iter)
		{
			Shared.Remaining.RemoveAt(FilteredPosArray[Iter]);
		}
	}

	if (bUpdateFilteredAssets)
	{
		Library.FilteredAssets.Empty();
		std::swap(Library.FilteredAssets, Shared.FilteredAssets);
	}
}

bool FWwiseAssetLibraryProcessor::FilterAsset(const FWwiseAssetLibraryFilteringSharedData& Shared, const FWwiseAssetLibraryInfo& Library, const WwiseAnyRef& Item)
{
	auto Filters{ Library.GetFilters() };
	const auto Num { Filters.Num() };

	if (Num == 0)
	{
		return true;
	}
	if (Num == 1)
	{
		const auto Filter = Filters[0];
		return UNLIKELY(!Filter) || Filter->IsAssetAvailable(Shared, Item);
	}
	if (Num == 2)
	{
		const auto Filter0 = Filters[0];
		const auto Filter1 = Filters[1];
		return (UNLIKELY(!Filter0) || Filter0->IsAssetAvailable(Shared, Item))
			&& (UNLIKELY(!Filter1) || Filter1->IsAssetAvailable(Shared, Item));
	}

	// Calculate all filters in parallel
	TArray<bool> Result;
	Result.AddUninitialized(Num);
	
	ParallelFor(Num, [this, &Filters = Filters, &Result, &Shared, &Item](int32 Iter)
	{
		const auto Filter = Filters[Iter];
		Result[Iter] = UNLIKELY(!Filter) || Filter->IsAssetAvailable(Shared, Item);
	});
	
	for (const bool IndividualResult : Result)
	{
		if (!IndividualResult)
		{
			return false;
		}
	}
	return true;
}

bool FWwiseAssetLibraryProcessor::CreateAssetLibraryRef(FWwiseAssetLibraryRef& NewRef, const WwiseAnyRef& SourceRef)
{
	switch (SourceRef.GetType())
	{
	case WwiseRefType::Media:
		NewRef.Type = EWwiseAssetLibraryRefType::Media;
		NewRef.SoundBankId = SourceRef.GetMediaRef()->SoundBankId();
		NewRef.LanguageId = SourceRef.GetMediaRef()->LanguageId;
		break;
	case WwiseRefType::SoundBank:
		if (SourceRef.GetSoundBank()->IsInitBank())
		{
			NewRef.Type = EWwiseAssetLibraryRefType::InitBank;
		}
		else
		{
			NewRef.Type = EWwiseAssetLibraryRefType::SoundBank;
		}
		NewRef.LanguageId = SourceRef.GetSoundBankRef()->LanguageId;
		break;
	default: {}
	}

	NewRef.Guid = FWwiseGuidConverter::ToFGuid(SourceRef.GetGuid());
	NewRef.Id = SourceRef.GetId();
	NewRef.Name = FName(*SourceRef.GetName());
	return true;
}