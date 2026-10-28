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
#include "AssetRegistry/AssetData.h"
#include "Wwise/Ref/WwiseRefCollections.h"

class FWwisePackagingId
{
public:
	FWwisePackagingId(WwiseDBGuid inGuid, int32 inShortId)
	{
		guid = inGuid;
		shortID = inShortId;
	}
	
	FWwisePackagingId()
	{
		guid = WwiseDBGuid();
		shortID = 0;
	}

	WwiseDBGuid Guid() const {return guid;}
	int32 ShortId() const {return shortID;}

private:
	WwiseDBGuid guid;
	int32 shortID;

};

inline bool operator==(const FWwisePackagingId& inLhs, const FWwisePackagingId& inRhs)
{
	return inLhs.ShortId() == inRhs.ShortId()  && inLhs.Guid() == inRhs.Guid();
}

inline uint32 GetTypeHash(const FWwisePackagingId& InPackagingId)
{
	return HashCombine(GetTypeHash(InPackagingId.ShortId()), GetTypeHash(InPackagingId.Guid()));
}

class FWwisePackagingData
{
public:
    FWwisePackagingData(){};
	TSet<FWwisePackagingId> PackagingIds;
	TSet<WwiseDBShortId> MediaIds;
};
