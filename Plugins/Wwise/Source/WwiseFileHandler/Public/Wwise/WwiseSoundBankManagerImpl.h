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

#include "Wwise/WwiseSoundBankManager.h"
#include "Wwise/CookedData/WwiseSoundBankCookedData.h"
#include "Wwise/WwiseFileState.h"
#include "Wwise/WwiseFileHandlerBase.h"

class WWISEFILEHANDLER_API FWwiseSoundBankManagerImpl : public IWwiseSoundBankManager, public FWwiseFileHandlerBase
{
public:
	FWwiseSoundBankManagerImpl();
	virtual ~FWwiseSoundBankManagerImpl() override;

	virtual const TCHAR* GetManagingTypeName() const override { return TEXT("SoundBank"); }
	virtual void LoadSoundBank(const FWwiseSoundBankCookedData& InSoundBankCookedData, FLoadSoundBankCallback&& InCallback) override;
	virtual void UnloadSoundBank(const FWwiseSoundBankCookedData& InSoundBankCookedData, FUnloadSoundBankCallback&& InCallback) override;
	virtual void SetGranularity(uint32 InStreamingGranularity) override;

	virtual void DoPostTerm() override;
	
	virtual IWwiseStreamingManagerHooks& GetStreamingHooks() override final { return *this; }

protected:
	uint32 StreamingGranularity;

	virtual FWwiseFileStateSharedPtr CreateOp(const FWwiseSoundBankCookedData& InSoundBankCookedData);
};
