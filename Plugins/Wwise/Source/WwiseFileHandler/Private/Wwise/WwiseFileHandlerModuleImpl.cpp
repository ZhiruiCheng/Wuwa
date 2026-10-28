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

#include "Wwise/WwiseFileHandlerModuleImpl.h"

#include "Wwise/WwiseDeferredQueue.h"
#include "Wwise/WwiseSoundBankManagerImpl.h"
#include "Wwise/WwiseExternalSourceManagerImpl.h"
#include "Wwise/WwiseFileCache.h"
#include "Wwise/WwiseGlobalCallbacks.h"
#include "Wwise/WwiseMediaManagerImpl.h"
#include "Wwise/WwiseIOHookImpl.h"
#include "Wwise/API/WwiseSoundEngineAPI.h"
#include "Wwise/Stats/FileHandler.h"

IMPLEMENT_MODULE(FWwiseFileHandlerModule, WwiseFileHandler)

FWwiseFileHandlerModule::FWwiseFileHandlerModule()
{
}

IWwiseSoundBankManagerPtr FWwiseFileHandlerModule::GetSoundBankManager()
{
	if (LIKELY(!IsEngineExitRequested()) && LIKELY(SoundBankManager))
	{
		return SoundBankManager;
	}

	Lock.ReadLock();
	if (SoundBankManager)
	{
		Lock.ReadUnlock();
	}
	else
	{
		Lock.ReadUnlock();
		Lock.WriteLock();
		if (LIKELY(!SoundBankManager))
		{
			SoundBankManager.Reset();
			SoundBankManager = InstantiateSoundBankManager();
		}
		Lock.WriteUnlock();
	}
	return SoundBankManager;
}

IWwiseExternalSourceManagerPtr FWwiseFileHandlerModule::GetExternalSourceManager()
{
	if (LIKELY(!IsEngineExitRequested()) && LIKELY(ExternalSourceManager))
	{
		return ExternalSourceManager;
	}

	Lock.ReadLock();
	if (ExternalSourceManager)
	{
		Lock.ReadUnlock();
	}
	else
	{
		Lock.ReadUnlock();
		Lock.WriteLock();
		if (LIKELY(!ExternalSourceManager))
		{
			ExternalSourceManager.Reset();
			ExternalSourceManager = InstantiateExternalSourceManager();
		}
		Lock.WriteUnlock();
	}
	return ExternalSourceManager;
}

IWwiseMediaManagerPtr FWwiseFileHandlerModule::GetMediaManager()
{
	if (LIKELY(!IsEngineExitRequested()) && LIKELY(MediaManager))
	{
		return MediaManager;
	}

	Lock.ReadLock();
	if (MediaManager)
	{
		Lock.ReadUnlock();
	}
	else
	{
		Lock.ReadUnlock();
		Lock.WriteLock();
		if (LIKELY(!MediaManager))
		{
			MediaManager.Reset();
			MediaManager = InstantiateMediaManager();
		}
		Lock.WriteUnlock();
	}
	return MediaManager;
}

FWwiseFileCache* FWwiseFileHandlerModule::GetFileCache()
{
	if (LIKELY(!IsEngineExitRequested()) && LIKELY(FileCache))
	{
		return FileCache.Get();
	}

	Lock.ReadLock();
	if (FileCache)
	{
		Lock.ReadUnlock();
	}
	else
	{
		Lock.ReadUnlock();
		Lock.WriteLock();
		if (LIKELY(!FileCache))
		{
			FileCache.Reset(InstantiateFileCache());
		}
		Lock.WriteUnlock();
	}
	return FileCache.Get();
}

FWwiseExecutionQueue* FWwiseFileHandlerModule::GetBankExecutionQueue()
{
	if (LIKELY(!IsEngineExitRequested()) && LIKELY(BankExecutionQueue))
	{
		return BankExecutionQueue.Get();
	}

	Lock.ReadLock();
	if (BankExecutionQueue)
	{
		Lock.ReadUnlock();
	}
	else
	{
		Lock.ReadUnlock();
		Lock.WriteLock();
		if (LIKELY(!BankExecutionQueue))
		{
			if (!IsEngineExitRequested())
			{
				BankExecutionQueue.Reset(InstantiateBankExecutionQueue());
			}
		}
		Lock.WriteUnlock();
	}
	return BankExecutionQueue.Get();
}

FWwiseIOHook* FWwiseFileHandlerModule::InstantiateIOHook()
{
	return new FWwiseIOHookImpl;
}

IWwiseSoundBankManagerPtr FWwiseFileHandlerModule::InstantiateSoundBankManager()
{
	UE_LOG(LogWwiseFileHandler, Verbose, TEXT("Initializing default SoundBank Manager."));
	return MakeShared<FWwiseSoundBankManagerImpl>();
}

IWwiseExternalSourceManagerPtr FWwiseFileHandlerModule::InstantiateExternalSourceManager()
{
	UE_LOG(LogWwiseFileHandler, Verbose, TEXT("Initializing default External Source Manager."));
	return MakeShared<FWwiseExternalSourceManagerImpl>();
}

IWwiseMediaManagerPtr FWwiseFileHandlerModule::InstantiateMediaManager()
{
	UE_LOG(LogWwiseFileHandler, Verbose, TEXT("Initializing default Media Manager."));
	return MakeShared<FWwiseMediaManagerImpl>();
}

FWwiseFileCache* FWwiseFileHandlerModule::InstantiateFileCache()
{
	UE_LOG(LogWwiseFileHandler, Verbose, TEXT("Initializing default File Cache."));
	return new FWwiseFileCache;
}

FWwiseExecutionQueue* FWwiseFileHandlerModule::InstantiateBankExecutionQueue()
{
	UE_LOG(LogWwiseFileHandler, Verbose, TEXT("Initializing default Bank Execution Queue."));
	return new FWwiseExecutionQueue(TEXT("BankExecutionQueue"), EWwiseTaskPriority::BackgroundNormal);
}

void FWwiseFileHandlerModule::StartupModule()
{
	IWwiseFileHandlerModule::StartupModule();
}

void FWwiseFileHandlerModule::ShutdownModule()
{
	Lock.WriteLock();
	if (SoundBankManager.IsValid())
	{
		UE_LOG(LogWwiseFileHandler, Verbose, TEXT("Shutting down SoundBank Manager."));
		SoundBankManager.Reset();
	}
	if (ExternalSourceManager.IsValid())
	{
		UE_LOG(LogWwiseFileHandler, Verbose, TEXT("Shutting down External Source Manager."));
		ExternalSourceManager.Reset();
	}
	if (MediaManager.IsValid())
	{
		UE_LOG(LogWwiseFileHandler, Verbose, TEXT("Shutting down Media Manager."));
		MediaManager.Reset();
	}
	if (FileCache.IsValid())
	{
		UE_LOG(LogWwiseFileHandler, Verbose, TEXT("Shutting down File Cache."));
		FileCache.Reset();
	}
	if (BankExecutionQueue.IsValid())
	{
		UE_LOG(LogWwiseFileHandler, Verbose, TEXT("Shutting down Bank Execution Queue."));
		BankExecutionQueue.Release()->CloseAndDelete();
	}
	Lock.WriteUnlock();
	IWwiseFileHandlerModule::ShutdownModule();
}

void FWwiseFileHandlerModule::RegisterTermCallback()
{
	if (auto* GlobalCallbacks = FWwiseGlobalCallbacks::Get())
	{
		GlobalCallbacks->TermSync([this](AK::IAkGlobalPluginContext*) mutable
		{
			OnTerm();
			return EWwiseDeferredAsyncResult::Done;
		});
	}
}

void FWwiseFileHandlerModule::OnTerm()
{
	if (!IsAvailable())
	{
		return;
	}

	Lock.ReadLock();
	if (BankExecutionQueue.IsValid())
	{
		BankExecutionQueue->AsyncWait(TEXT("FWwiseHandlerModule::OnTerm"), [this]() mutable
		{
			DoTerm();
		});
	}
	else
	{
		DoTerm();
	}
	Lock.ReadUnlock();
}

void FWwiseFileHandlerModule::DoTerm()
{
	if (MediaManager.IsValid())
	{
		UE_LOG(LogWwiseFileHandler, Verbose, TEXT("Term Media Manager."));
		MediaManager->DoTerm();
	}
}

void FWwiseFileHandlerModule::OnPostTerm()
{
	if (!IsAvailable())
	{
		return;
	}

	Lock.ReadLock();
	if (BankExecutionQueue.IsValid())
	{
		BankExecutionQueue->AsyncWait(TEXT("FWwiseHandlerModule::OnPostTerm"), [this]() mutable
		{
			DoPostTerm();
		});
	}
	else
	{
		DoTerm();
	}
	Lock.ReadUnlock();
}

void FWwiseFileHandlerModule::RequestProcessBanks()
{
#if WWISE_2025_1_OR_LATER && !AK_ENABLE_BANK_MGR_THREAD
	bRequireProcessBanks = true;
	if (LIKELY(BankExecutionQueue))
	{
		BankExecutionQueue->Async(TEXT("FWwiseFileHandlerModule::RequestProcessBanks"), [this]() mutable
		{
			if (!bRequireProcessBanks.Exchange(false))
			{
				return;
			}

			if (auto* SoundEngine = IWwiseSoundEngineAPI::Get())
			{
				if (SoundEngine->IsInitialized())
				{
					SoundEngine->ProcessBanks();
				}
			}
		});
	}
#endif
}

void FWwiseFileHandlerModule::DoPostTerm()
{
	if (SoundBankManager.IsValid())
	{
		UE_LOG(LogWwiseFileHandler, Verbose, TEXT("PostTerm SoundBank Manager."));
		SoundBankManager->DoPostTerm();
	}
	if (ExternalSourceManager.IsValid())
	{
		UE_LOG(LogWwiseFileHandler, Verbose, TEXT("PostTerm External Source Manager."));
		ExternalSourceManager->DoPostTerm();
	}
	if (MediaManager.IsValid())
	{
		UE_LOG(LogWwiseFileHandler, Verbose, TEXT("PostTerm Media Manager."));
		MediaManager->DoPostTerm();
	}
}
