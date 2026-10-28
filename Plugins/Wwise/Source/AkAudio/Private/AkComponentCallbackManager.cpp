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

#include "AkComponentCallbackManager.h"
#include "AkAudioDevice.h"
#include "AkInclude.h"
#include "Misc/ScopeLock.h"
#include "Misc/ScopeExit.h"
#include "Async/Async.h"
#include "AkCallbackInfoPool.h"
#include "AkComponent.h"
#include "Wwise/WwiseExternalSourceManager.h"
#include "Wwise/WwiseRetriggerableAsyncTask.h"
#include "UObject/UObjectThreadContext.h"

struct FAkComponentCallbackManager_Constants
{
	/// Optimization policy
	enum class Optimize
	{
		MemoryUsage,
		Speed,

		Value = MemoryUsage, ///< Set to either MemoryUsage or Speed
	};

	/// The default number of expected simultaneously playing sounds on a specific GameObject
	enum { ReserveSize = 8, };
};

FAkComponentCallbackManager* FAkComponentCallbackManager::Instance = nullptr;

FAkComponentCallbackManager* FAkComponentCallbackManager::GetInstance()
{
	return Instance;
}

#if WWISE_2025_1_OR_LATER
void FAkFunctionPtrEventCallbackPackage::HandleAction(AkCallbackType in_eType, AkEventCallbackInfo* in_pEventInfo, void* in_pCallbackInfo)
#else
void FAkFunctionPtrEventCallbackPackage::HandleAction(AkCallbackType in_eType, AkCallbackInfo* in_pCallbackInfo)
#endif
{
	FAkComponentCallbackManager::Lock();
	if (bShouldExecute)
	{
		UE_LOG(LogAkAudio, VeryVerbose, TEXT("Executing callback for Cookie %p, type %d"), pUserCookie, in_eType)
#if WWISE_2025_1_OR_LATER
		pfnUserCallback(in_eType, in_pEventInfo, in_pCallbackInfo, pUserCookie);
#else
		in_pCallbackInfo->pCookie = pUserCookie;
		pfnUserCallback(in_eType, in_pCallbackInfo);
		in_pCallbackInfo->pCookie = (void*)this;
#endif
		UE_LOG(LogAkAudio, VeryVerbose, TEXT("Finsihed executing callback for Cookie %p, type %d"), pUserCookie, in_eType)
	}
	FAkComponentCallbackManager::Unlock();
}

void FAkFunctionPtrEventCallbackPackage::CancelCallback()
{
	UE_LOG(LogAkAudio, VeryVerbose, TEXT("Cancelling callback for Cookie %p"), pUserCookie)
	bShouldExecute = false;
	pfnUserCallback = nullptr;
	uUserFlags = 0;
}

#if WWISE_2025_1_OR_LATER
void FAkBlueprintDelegateEventCallbackPackage::HandleAction(AkCallbackType in_eType, AkEventCallbackInfo* in_pEventInfo, void* in_pCallbackInfo)
#else
void FAkBlueprintDelegateEventCallbackPackage::HandleAction(AkCallbackType in_eType, AkCallbackInfo* in_pCallbackInfo)
#endif
{
	FAkComponentCallbackManager::Lock();
	if (BlueprintCallback.IsBound())
	{
#if WWISE_2025_1_OR_LATER
		AkCombinedCallbackInfo* cbInfoCopy = AkCallbackTypeHelpers::CopyWwiseCallbackInfo(in_eType, in_pEventInfo, in_pCallbackInfo);
#else
		AkCombinedCallbackInfo* cbInfoCopy = AkCallbackTypeHelpers::CopyWwiseCallbackInfo(in_eType, in_pCallbackInfo);
#endif
		EAkCallbackType BlueprintCallbackType = AkCallbackTypeHelpers::GetBlueprintCallbackTypeFromAkCallbackType(in_eType);
		FOnAkPostEventCallback CachedBlueprintCallback;
		{
			CachedBlueprintCallback = BlueprintCallback;
		}
		auto* Task = new FWwiseRetriggerableAsyncTask(ENamedThreads::GameThread, [cbInfoCopy, BlueprintCallbackType, CachedBlueprintCallback]
		{
			if (FUObjectThreadContext::Get().IsRoutingPostLoad)
			{
				UE_LOG(LogAkAudio, Verbose, TEXT("FAkBlueprintDelegateEventCallbackPackage::HandleAction: Delaying Blueprint execution because IsRoutingPostLoad."));
				return EWwiseDeferredAsyncResult::KeepRunning;
			}

			if (!cbInfoCopy)
			{
				UE_LOG(LogAkAudio, Log, TEXT("FAkBlueprintDelegateEventCallbackPackage::HandleAction: Could not get CallbackInfo structure, callback will be ignored."));
				return EWwiseDeferredAsyncResult::Done;
			}

			ON_SCOPE_EXIT {
				FMemory::Free(cbInfoCopy);
			};

			if (!CachedBlueprintCallback.IsBound())
			{
				UE_LOG(LogAkAudio, Log, TEXT("FAkBlueprintDelegateEventCallbackPackage::HandleAction: Blueprint delegate is not bound, it will be ignored."));
				return EWwiseDeferredAsyncResult::Done;
			}

			UAkComponent* akComponent = (UAkComponent*)cbInfoCopy->eventInfo.gameObjID;

			if (cbInfoCopy->eventInfo.gameObjID != DUMMY_GAMEOBJ && !IsValid(akComponent))
			{
				UE_LOG(LogAkAudio, Log, TEXT("FAkBlueprintDelegateEventCallbackPackage::HandleAction: Could not get valid AkComponent, callback will be ignored."));
				return EWwiseDeferredAsyncResult::Done;
			}

			UAkCallbackInfo*  BlueprintAkCallbackInfo = AkCallbackTypeHelpers::GetBlueprintableCallbackInfo(BlueprintCallbackType, cbInfoCopy);
			CachedBlueprintCallback.ExecuteIfBound(BlueprintCallbackType, BlueprintAkCallbackInfo);

			if (auto AudioDevice = FAkAudioDevice::Get())
			{
				if (auto CallbackInfoPool = AudioDevice->GetAkCallbackInfoPool())
				{
					CallbackInfoPool->Release(BlueprintAkCallbackInfo);
				}
			}

			return EWwiseDeferredAsyncResult::Done;
		});
		Task->ScheduleTask();
	}
	FAkComponentCallbackManager::Unlock();
}

void FAkBlueprintDelegateEventCallbackPackage::CancelCallback()
{
	BlueprintCallback.Clear();
	uUserFlags = 0;
}

#if WWISE_2025_1_OR_LATER
void FAkLatentActionEventCallbackPackage::HandleAction(AkCallbackType in_eType, AkEventCallbackInfo* in_pEventInfo, void* in_pCallbackInfo)
#else
void FAkLatentActionEventCallbackPackage::HandleAction(AkCallbackType in_eType, AkCallbackInfo* in_pCallbackInfo)
#endif
{
	// Don't access EndOfEventLatentAction if it's been deleted already
	if (!LatentActionValidityToken->bValid)
	{
		return;
	}

	if (EndOfEventLatentAction)
	{
		EndOfEventLatentAction->EventFinished = true;
	}
}

#if WWISE_2025_1_OR_LATER
void FAkComponentCallbackManager::AkComponentCallback(AkCallbackType in_eType, AkEventCallbackInfo* in_pEventInfo, void* in_pCallbackInfo, void* in_pCookie)
{
	auto pPackage = (IAkUserEventCallbackPackage*)in_pCookie;
	AkEventCallbackInfo* EventInfo = in_pEventInfo;
#else
void FAkComponentCallbackManager::AkComponentCallback(AkCallbackType in_eType, AkCallbackInfo* in_pCallbackInfo)
{
	auto pPackage = (IAkUserEventCallbackPackage*)in_pCallbackInfo->pCookie;
	AkEventCallbackInfo* EventInfo = static_cast<AkEventCallbackInfo*>(in_pCallbackInfo);
#endif

	if(!Instance->HasActiveEvents(EventInfo->gameObjID))
	{
		// No active events on this gameObjectID, callbacks received here should not have happened.
		if (in_eType == AK_EndOfEvent && pPackage != nullptr)
		{
			if (auto* Device = FAkAudioDevice::Get())
			{
				Device->RemovePlayingID(EventInfo->eventID, EventInfo->playingID);
			}

			if(pPackage->HasExternalSources)
			{
				if (auto ExternalSourceManager = IWwiseExternalSourceManager::Get())
				{
					ExternalSourceManager->OnEndOfEvent(EventInfo->playingID);
				}
			}
		}
		return;
	}

	if (Instance && pPackage)
	{
		const auto& gameObjID = EventInfo->gameObjID;
		bool deletePackage = false;

		if (in_eType == AK_EndOfEvent)
		{
			deletePackage = true;
			if (auto* Device = FAkAudioDevice::Get())
			{
				Device->RemovePlayingID(EventInfo->eventID, EventInfo->playingID);
			}

			if(pPackage->HasExternalSources)
			{
				if (auto ExternalSourceManager = IWwiseExternalSourceManager::Get())
				{
					ExternalSourceManager->OnEndOfEvent(EventInfo->playingID);
				}
			}
		}

		if ((pPackage->uUserFlags & in_eType) != 0)
		{
#if WWISE_2025_1_OR_LATER
			pPackage->HandleAction(in_eType, in_pEventInfo, in_pCallbackInfo);
#else
			pPackage->HandleAction(in_eType, in_pCallbackInfo);
#endif
		}

		{
			FScopeLock Lock(&Instance->CriticalSection);
			auto pPackageSet = Instance->GameObjectToPackagesMap.Find(gameObjID);
			if (pPackageSet && in_eType == AK_EndOfEvent)
			{
				Instance->RemovePackageFromSet(pPackageSet, pPackage, gameObjID);
			}
		}

		if (deletePackage)
		{
			delete pPackage;
		}
	}
}

FAkComponentCallbackManager::FAkComponentCallbackManager()
{
	if (Instance != nullptr)
	{
		UE_LOG(LogAkAudio, Error, TEXT("FAkComponentCallbackManager has already been instantiated."));
	}

	Instance = this;
}

FAkComponentCallbackManager::~FAkComponentCallbackManager()
{
	for (auto& Item : GameObjectToPackagesMap)
	{
		for (auto pPackage : Item.Value)
		{
			delete pPackage;
		}
	}

	Instance = nullptr;
}

IAkUserEventCallbackPackage* FAkComponentCallbackManager::CreateCallbackPackage(AkCallbackFunc in_cbFunc, void* in_Cookie, uint32 in_Flags, AkGameObjectID in_gameObjID, bool HasExternalSources, uint32 in_EventId)
{
	uint32 KeyHash = GetKeyHash(in_Cookie);
	auto pPackage = new FAkFunctionPtrEventCallbackPackage(in_cbFunc, in_Cookie, in_Flags, KeyHash, HasExternalSources, in_EventId);
	if (pPackage)
	{
		FScopeLock Lock(&CriticalSection);
		GameObjectToPackagesMap.FindOrAdd(in_gameObjID).Add(pPackage);
		UserCookieHashToPackageMap.Add(KeyHash, pPackage);
	}

	return pPackage;
}

IAkUserEventCallbackPackage* FAkComponentCallbackManager::CreateCallbackPackage(FOnAkPostEventCallback BlueprintCallback, uint32 in_Flags, AkGameObjectID in_gameObjID, bool HasExternalSources, uint32 in_EventId)
{
	uint32 KeyHash = GetKeyHash(BlueprintCallback);
	auto pPackage = new FAkBlueprintDelegateEventCallbackPackage(BlueprintCallback, in_Flags, KeyHash, HasExternalSources, in_EventId);
	if (pPackage)
	{
		FScopeLock Lock(&CriticalSection);
		GameObjectToPackagesMap.FindOrAdd(in_gameObjID).Add(pPackage);
		UserCookieHashToPackageMap.Add(KeyHash, pPackage);
	}

	return pPackage;
}

IAkUserEventCallbackPackage* FAkComponentCallbackManager::CreateCallbackPackage(FWaitEndOfEventAction* LatentAction, AkGameObjectID in_gameObjID, bool HasExternalSources, uint32 in_EventId)
{
	auto pPackage = new FAkLatentActionEventCallbackPackage(LatentAction, 0, HasExternalSources, in_EventId);
	if (pPackage)
	{
		FScopeLock Lock(&CriticalSection);
		GameObjectToPackagesMap.FindOrAdd(in_gameObjID).Add(pPackage);
	}

	return pPackage;
}

void FAkComponentCallbackManager::RemoveCallbackPackage(IAkUserEventCallbackPackage* in_Package, AkGameObjectID in_gameObjID)
{
	{
		FScopeLock Lock(&CriticalSection);
		auto pPackageSet = GameObjectToPackagesMap.Find(in_gameObjID);
		if (pPackageSet)
		{
			RemovePackageFromSet(pPackageSet, in_Package, in_gameObjID);
		}
	}

	delete in_Package;
}

void FAkComponentCallbackManager::CancelEventCallback(void* in_Cookie)
{
	CancelKeyHash(GetKeyHash(in_Cookie));
}

void FAkComponentCallbackManager::CancelEventCallback(const FOnAkPostEventCallback& in_Delegate)
{
	CancelKeyHash(GetKeyHash(in_Delegate));
}

void FAkComponentCallbackManager::CancelKeyHash(uint32 HashToCancel)
{
	FScopeLock AutoLock(&CriticalSection);

	TArray<IAkUserEventCallbackPackage*> PackagesToCancel;
	UserCookieHashToPackageMap.MultiFind(HashToCancel, PackagesToCancel);

	for (auto iter = PackagesToCancel.CreateConstIterator(); iter; ++iter)
	{
		if (*iter)
		{
			(*iter)->CancelCallback();
		}
	}
}

void FAkComponentCallbackManager::RegisterGameObject(AkGameObjectID in_gameObjID)
{
	if constexpr(FAkComponentCallbackManager_Constants::Optimize::Value == FAkComponentCallbackManager_Constants::Optimize::Speed)
	{
		FScopeLock Lock(&CriticalSection);
		GameObjectToPackagesMap.FindOrAdd(in_gameObjID).Reserve(FAkComponentCallbackManager_Constants::ReserveSize);
	}
}

void FAkComponentCallbackManager::UnregisterGameObject(AkGameObjectID in_gameObjID)
{
	// Do not cancel callbacks with the SoundEngine, as we need them for the 
	// playingID bookkeeping. Deleting the packages will ensure we do not callback
	// into objects that may have been destroyed.

	FScopeLock Lock(&CriticalSection);
	auto pPackageSet = GameObjectToPackagesMap.Find(in_gameObjID);
	if (pPackageSet)
	{
		for (auto pPackage : *pPackageSet)
		{
			pPackage->CancelCallback();
			UserCookieHashToPackageMap.Remove(pPackage->KeyHash, pPackage);
		}

		GameObjectToPackagesMap.Remove(in_gameObjID);
	}
}

bool FAkComponentCallbackManager::HasActiveEvents(AkGameObjectID in_gameObjID)
{
	FScopeLock Lock(&CriticalSection);
	auto pPackageSet = GameObjectToPackagesMap.Find(in_gameObjID);
	return pPackageSet && pPackageSet->Num() > 0;
}

void FAkComponentCallbackManager::GetActiveEvents(AkGameObjectID in_gameObjID, TArray<uint32>& out_EventIDs)
{
	FScopeLock Lock(&CriticalSection);
	auto pPackageSet = GameObjectToPackagesMap.Find(in_gameObjID);
	for (auto& pPackage : *pPackageSet)
	{
		if (pPackage->EventId != AK_INVALID_UNIQUE_ID)
		{
			out_EventIDs.AddUnique(pPackage->EventId);
		}
	}
}

void FAkComponentCallbackManager::RemovePackageFromSet(FAkComponentCallbackManager::PackageSet* in_pPackageSet, IAkUserEventCallbackPackage* in_pPackage, AkGameObjectID in_gameObjID)
{
	// No need for a lock here because those calling this function are already locking
	in_pPackageSet->Remove(in_pPackage);
	UserCookieHashToPackageMap.Remove(in_pPackage->KeyHash, in_pPackage);
	if (FAkComponentCallbackManager_Constants::Optimize::Value == FAkComponentCallbackManager_Constants::Optimize::MemoryUsage)
	{
		if (in_pPackageSet->Num() == 0)
		{
			GameObjectToPackagesMap.Remove(in_gameObjID);
		}
	}
}

uint32 FAkComponentCallbackManager::GetKeyHash(void* Key)
{
	return GetTypeHash(Key);
}

uint32 FAkComponentCallbackManager::GetKeyHash(const FOnAkPostEventCallback& Key)
{
	return HashCombine(GetTypeHash(Key.GetUObject()), GetTypeHash(Key.GetFunctionName()));
}

