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

#include "Wwise/WwiseDeferredQueue.h"

#include "Async/Async.h"

#include "Wwise/Stats/AsyncStats.h"
#include "Wwise/Stats/Concurrency.h"

FWwiseDeferredQueue::FWwiseDeferredQueue(const TCHAR* InDebugName) :
	AsyncExecutionQueue(InDebugName)
{
}

FWwiseDeferredQueue::~FWwiseDeferredQueue()
{
	bClosing = true;
	if (!IsEmpty())
	{
		Wait();

		UE_CLOG(UNLIKELY(!IsEmpty()), LogWwiseConcurrency, Error,
			TEXT("FWwiseDeferredQueue::~FWwiseDeferredQueue: Still operations in queue while deleting Deferred Queue %s"), AsyncExecutionQueue.DebugName);
	}
}

void FWwiseDeferredQueue::AsyncDefer(FFunction&& InFunction)
{
	if (!bClosing)
	{
		AsyncOpQueue.Push(new FFunction( MoveTemp(InFunction) ));
	}
}

void FWwiseDeferredQueue::SyncDefer(FSyncFunction&& InFunction)
{
	if (!bClosing)
	{
		SyncOpQueue.Push(new FSyncFunction( MoveTemp(InFunction) ));
	}
}

void FWwiseDeferredQueue::GameDefer(FFunction&& InFunction)
{
	if (!bClosing)
	{
		GameOpQueue.Push(new FFunction( MoveTemp(InFunction) ));
	}
}

void FWwiseDeferredQueue::Run(AK::IAkGlobalPluginContext* InContext)
{
	SCOPED_WWISECONCURRENCY_EVENT_4(TEXT("FWwiseDeferredQueue::Run"));
	FWwiseAsyncCycleCounter OpCycleCounter(GET_STATID(STAT_WwiseConcurrencySync));

	UE_CLOG(UNLIKELY(Context), LogWwiseConcurrency, Error, TEXT("FWwiseDeferredQueue::Run: Executing two Run() at the same time in %s."), AsyncExecutionQueue.DebugName);
	Context = InContext;

	if (!AsyncOpQueue.IsEmpty())
	{
		AsyncExecutionQueue.Async(WWISECONCURRENCY_ASYNC_NAME("FWwiseDeferredQueue::Run"), [this]() mutable
		{
			AsyncExec();
		});
	}

	if (!GameOpQueue.IsEmpty() || OnGameRun.IsBound())
	{
		GameThreadExec();
	}

	if (!SyncOpQueue.IsEmpty() || OnSyncRunTS.IsBound())
	{
		SyncExec();
	}
	OnSyncRunTS.Broadcast(Context);

	Context = nullptr;
}

void FWwiseDeferredQueue::Wait()
{
	const bool bIsInGameThread = IsInGameThread();
	SCOPED_WWISECONCURRENCY_EVENT_F_4(TEXT("FWwiseDeferredQueue::Wait%s"), bIsInGameThread ? TEXT(" GameThread") : TEXT(""));
	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_WwiseConcurrencyGameThreadWait, bIsInGameThread);
	CONDITIONAL_SCOPE_CYCLE_COUNTER(STAT_WwiseConcurrencyWait, !bIsInGameThread);

	if (!AsyncOpQueue.IsEmpty())
	{
		AsyncExecutionQueue.AsyncWait(WWISECONCURRENCY_ASYNC_NAME("FWwiseDeferredQueue::Wait"), [this]() mutable
		{
			AsyncExec();
		});
	}
	if (!GameOpQueue.IsEmpty())
	{
		FEventRef Done;
		if (bIsInGameThread)
		{
			const bool bNeedToStartLoop = (GameThreadExecuting++ == 0);

			GameOpQueue.Push(new FFunction( [this, &Done]() mutable
			{
				Done->Trigger();
				--GameThreadExecuting;
				return EWwiseDeferredAsyncResult::Done;
			} ));

			if (bNeedToStartLoop)
			{
				while (GameThreadExecuting > 0)
				{
					if (FFunction* Func = GameOpQueue.Pop())
					{
						if ((*Func)() == EWwiseDeferredAsyncResult::KeepRunning)
						{
							GameDefer(MoveTemp(*Func));
						}
						delete Func;
					}
					else
					{
						break;
					}
				}
			}
		}
		else
		{
			GameOpQueue.Push(new FFunction( [&Done]() mutable
			{
				Done->Trigger();
				return EWwiseDeferredAsyncResult::Done;
			} ));
			GameThreadExec();
		}
		Done->Wait();
	}
	if (!SyncOpQueue.IsEmpty())
	{
		FWwiseAsyncCycleCounter OpCycleCounter(GET_STATID(STAT_WwiseConcurrencySync));

		SyncExec();
	}
}

void FWwiseDeferredQueue::AsyncExec()
{
	SCOPED_WWISECONCURRENCY_EVENT_4(TEXT("FWwiseDeferredQueue::AsyncExec"));
	SCOPE_CYCLE_COUNTER(STAT_WwiseConcurrencyAsync);

	if (AsyncOpQueue.IsEmpty())
	{
		return;
	}

	EWwiseDeferredAsyncState ExpectedAsyncState { EWwiseDeferredAsyncState::Idle };
	if (UNLIKELY(!AsyncState.compare_exchange_strong(ExpectedAsyncState, EWwiseDeferredAsyncState::Running)))
	{
		UE_LOG(LogWwiseConcurrency, Error, TEXT("FWwiseDeferredQueue::AsyncExec: AsyncState not Idle trying to set as Running for %s. Skipping."), AsyncExecutionQueue.DebugName);
		return;
	}
	AsyncOpQueue.Push(new FFunction( [this]() mutable
	{
		EWwiseDeferredAsyncState ExpectedAsyncState { EWwiseDeferredAsyncState::Running };
		if (UNLIKELY(!AsyncState.compare_exchange_strong(ExpectedAsyncState, EWwiseDeferredAsyncState::Done)))
		{
			UE_LOG(LogWwiseConcurrency, Error, TEXT("FWwiseDeferredQueue::AsyncExec: AsyncState not Running trying to set as Done for %s. Skipping."), AsyncExecutionQueue.DebugName);
		}		
		return EWwiseDeferredAsyncResult::Done;
	} ));

	while (AsyncState == EWwiseDeferredAsyncState::Running)
	{
		FFunction* Func = AsyncOpQueue.Pop();
		if (!Func)
		{
			break;
		}
		if ((*Func)() == EWwiseDeferredAsyncResult::KeepRunning)
		{
			AsyncDefer(MoveTemp(*Func));
		}
		delete Func;
	}

	ExpectedAsyncState = EWwiseDeferredAsyncState::Done;
	if (UNLIKELY(!AsyncState.compare_exchange_strong(ExpectedAsyncState, EWwiseDeferredAsyncState::Idle)))
	{
		UE_LOG(LogWwiseConcurrency, Error, TEXT("FWwiseDeferredQueue::AsyncExec: AsyncState not Done trying to set as Idle for %s. Skipping."), AsyncExecutionQueue.DebugName);
		return;
	}
}

void FWwiseDeferredQueue::SyncExec()
{
	SyncOpQueue.Push(new FSyncFunction( [this](AK::IAkGlobalPluginContext*) mutable
	{
		bSyncThreadDone = true;
		return EWwiseDeferredAsyncResult::Done;
	} ));

	SyncExecLoop();
}

void FWwiseDeferredQueue::SyncExecLoop()
{
	while (!bSyncThreadDone)
	{
		if (FSyncFunction* Func = SyncOpQueue.Pop())
		{
			if (!bClosing && (*Func)(Context) == EWwiseDeferredAsyncResult::KeepRunning)
			{
				SyncDefer(MoveTemp(*Func));
			}
			delete Func;
		}
		else
		{
			break;
		}
	}
	OnSyncRunTS.Broadcast(Context);
	bSyncThreadDone = false;
}

void FWwiseDeferredQueue::GameThreadExec()
{
	const bool bNeedToStartLoop = (GameThreadExecuting++ == 0);

	GameOpQueue.Push(new FFunction( [this]() mutable
	{
		--GameThreadExecuting;
		return EWwiseDeferredAsyncResult::Done;
	} ));

	if (bNeedToStartLoop)
	{
		GameThreadExecLoop();
	}
}

void FWwiseDeferredQueue::GameThreadExecLoop()
{
	AsyncTask(ENamedThreads::GameThread, [this]() mutable
	{
		SCOPED_WWISECONCURRENCY_EVENT_4(TEXT("FWwiseDeferredQueue::GameThreadExecLoop"));
		SCOPE_CYCLE_COUNTER(STAT_WwiseConcurrencyGameThread);

		while (GameThreadExecuting > 0)
		{
			if (FFunction* Func = GameOpQueue.Pop())
			{
				if ((*Func)() == EWwiseDeferredAsyncResult::KeepRunning)
				{
					GameDefer(MoveTemp(*Func));
				}
				delete Func;
			}
			else
			{
				break;
			}
		}

		OnGameRun.Broadcast();
	});
}
