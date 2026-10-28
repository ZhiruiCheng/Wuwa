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

#include "AkAudioDevice.h"
#include "WwiseUnrealDefines.h"

class IAkUserEventCallbackPackage
{
public:
	/** Copy of the user callback flags, for use in our own callback */
	uint32 uUserFlags;

	uint32 KeyHash;

	bool HasExternalSources = false;

	uint32 EventId;

	IAkUserEventCallbackPackage()
		: uUserFlags(0)
	{}

	IAkUserEventCallbackPackage(uint32 in_Flags, uint32 in_Hash, bool in_HasExternalSources, uint32 in_EventId)
		: uUserFlags(in_Flags)
		, KeyHash(in_Hash)
		, HasExternalSources(in_HasExternalSources)
		, EventId(in_EventId)
	{}

	virtual ~IAkUserEventCallbackPackage() {}

#if WWISE_2025_1_OR_LATER
	virtual void HandleAction(AkCallbackType in_eType, AkEventCallbackInfo* in_pEventInfo, void* in_pCallbackInfo) = 0;
#else
	virtual void HandleAction(AkCallbackType in_eType, AkCallbackInfo* in_pCallbackInfo) = 0;
#endif
	virtual void CancelCallback() {};
};

class FAkFunctionPtrEventCallbackPackage : public IAkUserEventCallbackPackage
{
public:
	FAkFunctionPtrEventCallbackPackage(AkCallbackFunc CbFunc, void* Cookie, uint32 Flags, uint32 in_Hash, bool in_HasExternalSources, uint32 in_EventId)
		: IAkUserEventCallbackPackage(Flags, in_Hash, in_HasExternalSources, in_EventId)
		  , pfnUserCallback(CbFunc)
		  , pUserCookie(Cookie)
		  , bShouldExecute(true)
	{
	}

#if WWISE_2025_1_OR_LATER
	virtual void HandleAction(AkCallbackType in_eType, AkEventCallbackInfo* in_pEventInfo, void* in_pCallbackInfo) override;
#else
	virtual void HandleAction(AkCallbackType in_eType, AkCallbackInfo* in_pCallbackInfo) override;
#endif
	virtual void CancelCallback() override;

private:
	/** Copy of the user callback, for use in our own callback */
	AkCallbackFunc pfnUserCallback;

	/** Copy of the user cookie, for use in our own callback */
	void* pUserCookie;

	/* Whether the callback should be executed, false if it has been cancelled. */
	TAtomic<bool> bShouldExecute;
};

class FAkBlueprintDelegateEventCallbackPackage : public IAkUserEventCallbackPackage
{
public:
	FAkBlueprintDelegateEventCallbackPackage(FOnAkPostEventCallback PostEventCallback, uint32 Flags, uint32 in_Hash, bool in_HasExternalSources, uint32 in_EventId)
		: IAkUserEventCallbackPackage(Flags, in_Hash, in_HasExternalSources, in_EventId)
		, BlueprintCallback(PostEventCallback)
	{}

#if WWISE_2025_1_OR_LATER
	virtual void HandleAction(AkCallbackType in_eType, AkEventCallbackInfo* in_pEventInfo, void* in_pCallbackInfo) override;
#else
	virtual void HandleAction(AkCallbackType in_eType, AkCallbackInfo* in_pCallbackInfo) override;
#endif
	virtual void CancelCallback() override;

private:
	FOnAkPostEventCallback BlueprintCallback;
};

class FAkLatentActionEventCallbackPackage : public IAkUserEventCallbackPackage
{
public:
	FAkLatentActionEventCallbackPackage(FWaitEndOfEventAction* LatentAction, uint32 in_Hash, bool in_HasExternalSources, uint32 in_EventId)
		: IAkUserEventCallbackPackage(AK_EndOfEvent, in_Hash, in_HasExternalSources, in_EventId)
		, EndOfEventLatentAction(LatentAction)
	{
		LatentActionValidityToken = MakeShared<FPendingLatentActionValidityToken, ESPMode::ThreadSafe>();
		EndOfEventLatentAction->ValidityToken = LatentActionValidityToken;
	}

#if WWISE_2025_1_OR_LATER
	virtual void HandleAction(AkCallbackType in_eType, AkEventCallbackInfo* in_pEventInfo, void* in_pCallbackInfo) override;
#else
	virtual void HandleAction(AkCallbackType in_eType, AkCallbackInfo* in_pCallbackInfo) override;
#endif

private:
	TSharedPtr<FPendingLatentActionValidityToken, ESPMode::ThreadSafe> LatentActionValidityToken;
	FWaitEndOfEventAction* EndOfEventLatentAction;
};

class FAkComponentCallbackManager
{
public:
	static FAkComponentCallbackManager* GetInstance();

	static void Lock()
	{
		if (Instance)
		{
			Instance->CriticalSection.Lock();
		}
	}

	static void Unlock()
	{
		if (Instance)
		{
			Instance->CriticalSection.Unlock();
		}
	}

	/** Our own event callback */
#if WWISE_2025_1_OR_LATER
	static void AkComponentCallback(AkCallbackType in_eType, AkEventCallbackInfo* in_pEventInfo, void * in_pCallbackInfo, void* in_pCookie);
#else
	static void AkComponentCallback(AkCallbackType in_eType, AkCallbackInfo* in_pCallbackInfo);
#endif

	FAkComponentCallbackManager();
	~FAkComponentCallbackManager();

	IAkUserEventCallbackPackage* CreateCallbackPackage(AkCallbackFunc in_cbFunc, void* in_Cookie, uint32 in_Flags, AkGameObjectID in_gameObjID, bool HasExternalSources, uint32 in_EventId);
	IAkUserEventCallbackPackage* CreateCallbackPackage(FOnAkPostEventCallback BlueprintCallback, uint32 in_Flags, AkGameObjectID in_gameObjID, bool HasExternalSources, uint32 in_EventId);
	IAkUserEventCallbackPackage* CreateCallbackPackage(FWaitEndOfEventAction* LatentAction, AkGameObjectID in_gameObjID, bool HasExternalSources, uint32 in_EventId);
	void RemoveCallbackPackage(IAkUserEventCallbackPackage* in_Package, AkGameObjectID in_gameObjID);

	void CancelEventCallback(void* in_Cookie);
	void CancelEventCallback(const FOnAkPostEventCallback& in_Delegate);

	void RegisterGameObject(AkGameObjectID in_gameObjID);
	void UnregisterGameObject(AkGameObjectID in_gameObjID);

	bool HasActiveEvents(AkGameObjectID in_gameObjID);
	void GetActiveEvents(AkGameObjectID in_gameObjID, TArray<uint32>& out_EventIDs);

private:
	static FAkComponentCallbackManager* Instance;

	typedef TSet<IAkUserEventCallbackPackage*> PackageSet;

	void RemovePackageFromSet(PackageSet* in_pPackageSet, IAkUserEventCallbackPackage* in_pPackage, AkGameObjectID in_gameObjID);

	FCriticalSection CriticalSection;

	typedef WwiseUnrealHelper::AkGameObjectIdKeyFuncs<PackageSet, false> PackageSetGameObjectIDKeyFuncs;
	TMap<AkGameObjectID, PackageSet, FDefaultSetAllocator, PackageSetGameObjectIDKeyFuncs> GameObjectToPackagesMap;

	// Used for quick lookup in cancel
	uint32 inline GetKeyHash(void* Key);
	uint32 inline GetKeyHash(const FOnAkPostEventCallback& Key);

	void CancelKeyHash(uint32 HashToCancel);
	TMultiMap<uint32, IAkUserEventCallbackPackage*> UserCookieHashToPackageMap;
};
