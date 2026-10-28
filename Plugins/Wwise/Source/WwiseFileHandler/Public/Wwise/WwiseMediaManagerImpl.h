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

#include "Wwise/WwiseMediaManager.h"
#include "Wwise/WwiseFileHandlerBase.h"

class WWISEFILEHANDLER_API FWwiseMediaManagerImpl : public IWwiseMediaManager, public FWwiseFileHandlerBase
{
public:
	FWwiseMediaManagerImpl();
	virtual ~FWwiseMediaManagerImpl() override;

	virtual const TCHAR* GetManagingTypeName() const override { return TEXT("Media"); }

	virtual void LoadMedia(const FWwiseMediaCookedData& InMediaCookedData, FLoadMediaCallback&& InCallback) override;
	virtual void UnloadMedia(const FWwiseMediaCookedData& InMediaCookedData, FUnloadMediaCallback&& InCallback) override;
	virtual void SetGranularity(AkUInt32 InStreamingGranularity) override;
	
	virtual void SetMedia(AkSourceSettings& InSource, FLoadMediaCallback&& InCallback) override;
	virtual void UnsetMedia(AkSourceSettings& InSource, FLoadMediaCallback&& InCallback) override;

	virtual void DoTerm() override;
	virtual void DoPostTerm() override;

	virtual IWwiseStreamingManagerHooks& GetStreamingHooks() override final { return *this; }

protected:
	uint32 StreamingGranularity;

	FCriticalSection MediaOpCriticalSection;
	uint32 SetMediaCount { 0 };
	TArray<AkSourceSettings> SetMediaOps;
	TArray<FLoadMediaCallback> SetMediaCallbacks;

	uint32 UnsetMediaCount { 0 };
	TArray<AkSourceSettings> UnsetMediaOps;
	TArray<FLoadMediaCallback> UnsetMediaCallbacks;

	bool bProcessTerm{ false };

	virtual FWwiseFileStateSharedPtr CreateOp(const FWwiseMediaCookedData& InMediaCookedData);
	
	static FWwiseExecutionQueue* GetBankExecutionQueue();
	virtual void AddUnsetMediaOp(AkSourceSettings& InSource, FLoadMediaCallback&& InCallback);
	virtual void DoSetMedia();
	virtual void DoUnsetMedia();
};
