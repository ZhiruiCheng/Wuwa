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

#include "Wwise/WwiseFileState.h"
#include "Wwise/Stats/AsyncStats.h"

#include <inttypes.h>

#include "Wwise/WwiseFileHandlerModule.h"
#include "Wwise/WwiseGlobalCallbacks.h"
#include "Wwise/WwiseStreamableFileStateInfo.h"

FWwiseFileState::~FWwiseFileState()
{
	SCOPED_WWISEFILEHANDLER_EVENT_3(TEXT("FWwiseFileState::~FWwiseFileState"));
	UE_LOG(LogWwiseFileHandler, VeryVerbose, TEXT("FWwiseFileState::~FWwiseFileState dtor [%p]"), this);
	UE_CLOG(FileStateExecutionQueue, LogWwiseFileHandler, Error, TEXT("Dtor FWwiseFileState [%p]: Without closing the execution queue!"), this);
	UE_CLOG(LoadCount > 0, LogWwiseFileHandler, Error, TEXT("Dtor FWwiseFileState [%p]: With LoadCount still active!"), this);
	UE_CLOG(State != EState::Closed, LogWwiseFileHandler, Error, TEXT("Dtor FWwiseFileState [%p]: with State %s not closed!"), this, GetStateName());
}

FWwiseExecutionQueue* FWwiseFileState::GetBankExecutionQueue()
{
	auto* FileHandlerModule = IWwiseFileHandlerModule::GetModule();
	if (UNLIKELY(!FileHandlerModule))
	{
		UE_LOG(LogWwiseFileHandler, Log, TEXT("FWwiseFileState::GetBankExecutionQueue: WwiseFileHandlerModule is unloaded."));
		return nullptr;
	}
	return FileHandlerModule->GetBankExecutionQueue();
}

const TCHAR* FWwiseFileState::GetStateNameFor(EState State)
{
	switch (State)
	{
	case EState::Closed: return TEXT("Closed");
	case EState::Opening: return TEXT("Opening");
	case EState::Opened: return TEXT("Opened");
	case EState::Loading: return TEXT("Loading");
	case EState::Loaded: return TEXT("Loaded");
	case EState::Unloading: return TEXT("Unloading");
	case EState::Closing: return TEXT("Closing");
	}
	return TEXT("Unknown");
}

const TCHAR* FWwiseFileState::GetStateName() const
{
	return GetStateNameFor(State);
}

void FWwiseFileState::SetState(const TCHAR* const Caller, EState NewState)
{
	UE_LOG(LogWwiseFileHandler, VeryVerbose, TEXT("%s [%p] %s %" PRIu32 ": State %s -> %s"),
		Caller, this, GetManagingTypeName(), GetShortId(),
		GetStateName(), GetStateNameFor(NewState));
	State = NewState;
}

void FWwiseFileState::SetStateFrom(const TCHAR* const Caller, EState ExpectedState, EState NewState)
{
	UE_CLOG(ExpectedState != State, LogWwiseFileHandler, Fatal, TEXT("%s [%p] %s %" PRIu32 ": State %s -> %s. Expected %s!"),
		Caller, this, GetManagingTypeName(), GetShortId(), GetStateName(),
		GetStateNameFor(NewState), GetStateNameFor(ExpectedState));
	SetState(Caller, NewState);
}

void FWwiseFileState::IncrementCountAsync(EWwiseFileStateOperationOrigin InOperationOrigin,
                                          FIncrementCountCallback&& InCallback)
{
	SCOPED_WWISEFILEHANDLER_EVENT_4(TEXT("FWwiseFileState::IncrementCountAsync"));
	if (UNLIKELY(!FileStateExecutionQueue))
	{
		UE_LOG(LogWwiseFileHandler, Error, TEXT("FWwiseFileState::IncrementCountAsync on a termed asset! Failing immediately!"));
		return InCallback(false);
	}

	auto NewValue = ++OpenedInstances;
	auto* Op = new FWwiseFileStateIncrementOperation(InOperationOrigin, MoveTemp(InCallback));
	UE_LOG(LogWwiseFileHandler, Verbose, TEXT("FWwiseFileState::IncrementCountAsync [%p] %s %" PRIu32 ": ++OpenedInstances %d with Op %p"),
		this, GetManagingTypeName(), GetShortId(), NewValue, Op);

	OpQueue.Push(Op);
	ProcessNextOperation();
}

void FWwiseFileState::IncrementCountAsyncDone(FWwiseAsyncCycleCounter&& InOpCycleCounter, FIncrementCountCallback&& InCallback, bool bInResult)
{
	SCOPED_WWISEFILEHANDLER_EVENT_4(TEXT("FWwiseFileState::IncrementCountAsync Callback"));
	InOpCycleCounter.Stop();
	DEC_DWORD_STAT(STAT_WwiseFileHandlerStateOperationsBeingProcessed);

	auto* DoneOp = CurrentOp.exchange(nullptr);
	ProcessNextOperation();
	
	InCallback(bInResult);
	delete DoneOp;
}

void FWwiseFileState::DecrementCountAsync(EWwiseFileStateOperationOrigin InOperationOrigin,
	FDeleteFileStateFunction&& InDeleteState, FDecrementCountCallback&& InCallback)
{
	SCOPED_WWISEFILEHANDLER_EVENT_4(TEXT("FWwiseFileState::DecrementCountAsync"));
	if (UNLIKELY(!FileStateExecutionQueue))
	{
		UE_LOG(LogWwiseFileHandler, Error, TEXT("FWwiseFileState::DecrementCountAsync on a termed asset! Failing immediately!"));
		return InCallback();
	}

	auto* Op = new FWwiseFileStateDecrementOperation(InOperationOrigin, MoveTemp(InDeleteState), MoveTemp(InCallback));
	UE_LOG(LogWwiseFileHandler, Verbose, TEXT("FWwiseFileState::DecrementCountAsync [%p] %s %" PRIu32 ": (--)OpenedInstances %d with Op %p"),
		this, GetManagingTypeName(), GetShortId(), OpenedInstances.load(), Op);
	
	OpQueue.Push(Op);
	ProcessNextOperation();
}

void FWwiseFileState::DecrementCountAsyncDone(FWwiseAsyncCycleCounter&& InOpCycleCounter, FDecrementCountCallback&& InCallback)
{
	SCOPED_WWISEFILEHANDLER_EVENT_4(TEXT("FWwiseFileState::DecrementCountAsyncDone Callback"));
	InOpCycleCounter.Stop();
	DEC_DWORD_STAT(STAT_WwiseFileHandlerStateOperationsBeingProcessed);

	auto* DoneOp = CurrentOp.exchange(nullptr);
	ProcessNextOperation();

	InCallback();
	delete DoneOp;
}

bool FWwiseFileState::CanDelete() const
{
	return OpenedInstances.load() == 0 && State == EState::Closed && LoadCount == 0;
}

FWwiseFileState::FWwiseFileStateOperation::FWwiseFileStateOperation():
	OperationOrigin(EWwiseFileStateOperationOrigin::Loading),
	OpCycleCounter(GET_STATID(STAT_WwiseFileHandlerStateOperationLatency))
{}

FWwiseFileState::FWwiseFileStateOperation::FWwiseFileStateOperation(EWwiseFileStateOperationOrigin InOperationOrigin):
	OperationOrigin(InOperationOrigin),
	OpCycleCounter(GET_STATID(STAT_WwiseFileHandlerStateOperationLatency))
{}

void FWwiseFileState::FWwiseFileStateIncrementOperation::StartOperation(FWwiseFileState& FileState)
{
	INC_DWORD_STAT(STAT_WwiseFileHandlerStateOperationsBeingProcessed);
	UE_LOG(LogWwiseFileHandler, VeryVerbose, TEXT("FWwiseFileState::FWwiseFileStateIncrementOperation::StartOperation [%p] %s %" PRIu32 ": Op [%p]"),
		&FileState, FileState.GetManagingTypeName(), FileState.GetShortId(), this);
	FileState.IncrementCount(OperationOrigin, [this, FileState = FileState.AsShared()](bool bInResult) mutable
	{
		FileState->IncrementCountAsyncDone(MoveTemp(OpCycleCounter), MoveTemp(Callback), bInResult);
	});
}

void FWwiseFileState::FWwiseFileStateDecrementOperation::StartOperation(FWwiseFileState& FileState)
{
	INC_DWORD_STAT(STAT_WwiseFileHandlerStateOperationsBeingProcessed);
	UE_LOG(LogWwiseFileHandler, VeryVerbose, TEXT("FWwiseFileState::FWwiseFileStateDecrementOperation::StartOperation [%p] %s %" PRIu32 ": Op [%p]"),
		&FileState, FileState.GetManagingTypeName(), FileState.GetShortId(), this);
	FileState.DecrementCount(OperationOrigin, MoveTemp(DeleteState), [this, FileState = FileState.AsShared()]() mutable
	{
		FileState->DecrementCountAsyncDone(MoveTemp(OpCycleCounter), MoveTemp(Callback));
	});
}

FWwiseFileState::FWwiseFileState()
{
	UE_LOG(LogWwiseFileHandler, VeryVerbose, TEXT("FWwiseFileState::FWwiseFileState Ctor [%p]"), this);
}

void FWwiseFileState::Term()
{
	SCOPED_WWISEFILEHANDLER_EVENT_3(TEXT("FWwiseFileState::Term"));
	if (UNLIKELY(!FileStateExecutionQueue))
	{
		UE_LOG(LogWwiseFileHandler, Error, TEXT("FWwiseFileState::Term [%p]: already Term!"), this);
		return;
	}
	if (UNLIKELY(OpenedInstances.load() > 0))
	{
		UE_LOG(LogWwiseFileHandler, Log, TEXT("FWwiseFileState::Term [%p] %s %" PRIu32 ": Terminating with active states. Waiting 10 loops before bailing out."),
			this, GetManagingTypeName(), GetShortId());
		for (int i = 0; OpenedInstances.load() > 0 && i < 10; ++i)
		{
			FileStateExecutionQueue->AsyncWait(WWISEFILEHANDLER_ASYNC_NAME("FWwiseFileState::Term Wait"), []{});
		}
		UE_CLOG(OpenedInstances.load() > 0, LogWwiseFileHandler, Error, TEXT("FWwiseFileState::Term [%p] %s %" PRIu32 ": Terminating with active states. This might cause a crash."),
			this, GetManagingTypeName(), GetShortId());
	}

	UE_LOG(LogWwiseFileHandler, Verbose, TEXT("FWwiseFileState::Term [%p] %s %" PRIu32 ": Terminating."),
		this, GetManagingTypeName(), GetShortId());

	UE_CLOG(CurrentOp.load() != nullptr, LogWwiseFileHandler, Error, TEXT("FWwiseFileState::Term [%p] %s %" PRIu32 ": State Term with existing operation currently being executed."),
		this, GetManagingTypeName(), GetShortId());
	UE_CLOG(!OpQueue.IsEmpty(), LogWwiseFileHandler, Error, TEXT("FWwiseFileState::Term [%p] %s %" PRIu32 ": State Term with existing operations in the OpQueue."),
		this, GetManagingTypeName(), GetShortId());
	UE_CLOG(!IsEngineExitRequested() && UNLIKELY(State != EState::Closed), LogWwiseFileHandler, Warning, TEXT("FWwiseFileState::Term [%p] %s %" PRIu32 ": State Term unclosed file state %s. Leaking."),
		this, GetManagingTypeName(), GetShortId(), GetStateName());
	UE_CLOG(IsEngineExitRequested() && State != EState::Closed, LogWwiseFileHandler, VeryVerbose, TEXT("FWwiseFileState::Term [%p] %s %" PRIu32 ": State Term unclosed file state %s at exit. Leaking."),
		this, GetManagingTypeName(), GetShortId(), GetStateName());
	UE_CLOG(LoadCount != 0, LogWwiseFileHandler, Log, TEXT("FWwiseFileState::Term [%p] %s %" PRIu32 ": when there are still %d load count"),
		this, GetManagingTypeName(), GetShortId(), LoadCount);

	FileStateExecutionQueue->CloseAndDelete(); FileStateExecutionQueue = nullptr;
}

void FWwiseFileState::IncrementCount(EWwiseFileStateOperationOrigin InOperationOrigin,
                                     FIncrementCountCallback&& InCallback)
{
	check(FileStateExecutionQueue->IsRunningInThisThread());
	
	IncrementLoadCount(InOperationOrigin);

	IncrementCountOpen(InOperationOrigin, MoveTemp(InCallback));
}

void FWwiseFileState::IncrementCountOpen(EWwiseFileStateOperationOrigin InOperationOrigin,
	FIncrementCountCallback&& InCallback)
{
	SCOPED_WWISEFILEHANDLER_EVENT_F_3(TEXT("FWwiseFileState::IncrementCountOpen %s"), GetManagingTypeName());
	check(FileStateExecutionQueue->IsRunningInThisThread());
	
	if (!CanOpenFile())
	{
		IncrementCountLoad(InOperationOrigin, MoveTemp(InCallback));					// Continue
		return;
	}

	SetStateFrom(TEXT("FWwiseFileState::IncrementCountOpen"), EState::Closed, EState::Opening);

	OpenFile([this, InOperationOrigin, InCallback = MoveTemp(InCallback)]() mutable
	{
		SCOPED_WWISEFILEHANDLER_EVENT_F_4(TEXT("FWwiseFileState::IncrementCountOpen %s OpenFile"), GetManagingTypeName());
		IncrementCountLoad(InOperationOrigin, MoveTemp(InCallback));
	});
}

void FWwiseFileState::IncrementCountLoad(EWwiseFileStateOperationOrigin InOperationOrigin,
	FIncrementCountCallback&& InCallback)
{
	SCOPED_WWISEFILEHANDLER_EVENT_F_3(TEXT("FWwiseFileState::IncrementCountLoad %s"), GetManagingTypeName());
	check(FileStateExecutionQueue->IsRunningInThisThread());
	
	if (!CanLoadInSoundEngine())
	{
		IncrementCountDone(InOperationOrigin, MoveTemp(InCallback));					// Continue
		return;
	}

	SetStateFrom(TEXT("FWwiseFileState::IncrementCountLoad"), EState::Opened, EState::Loading);

	LoadInSoundEngine([this, InOperationOrigin, InCallback = MoveTemp(InCallback)]() mutable
	{
		SCOPED_WWISEFILEHANDLER_EVENT_F_4(TEXT("FWwiseFileState::IncrementCountLoad %s LoadInSoundEngine"), GetManagingTypeName());
		IncrementCountDone(InOperationOrigin, MoveTemp(InCallback));
	});
}

void FWwiseFileState::IncrementCountDone(EWwiseFileStateOperationOrigin InOperationOrigin,
	FIncrementCountCallback&& InCallback)
{
	SCOPED_WWISEFILEHANDLER_EVENT_F_3(TEXT("FWwiseFileState::IncrementCountDone %s"), GetManagingTypeName());
	check(FileStateExecutionQueue->IsRunningInThisThread());
	
	bool bResult;
	if (InOperationOrigin == EWwiseFileStateOperationOrigin::Streaming)
	{
		bResult = (State == EState::Loaded);
		if (UNLIKELY(!bResult))
		{
			UE_CLOG(CanLoadInSoundEngine(), LogWwiseFileHandler, Warning, TEXT("FWwiseFileState::IncrementCountDone [%p] %s %" PRIu32 ": Could not load file for streaming."),
				this, GetManagingTypeName(), GetShortId());
			UE_CLOG(!CanLoadInSoundEngine(), LogWwiseFileHandler, VeryVerbose, TEXT("FWwiseFileState::IncrementCountDone [%p] %s %" PRIu32 ": Streaming request aborted before load done."),
				this, GetManagingTypeName(), GetShortId());
		}
	}
	else
	{
		const auto bCanLoadInSoundEngine = CanLoadInSoundEngine();
		const auto bCanOpenFile = CanOpenFile();
		bResult = (State == EState::Loaded)
			|| (State == EState::Opened && !bCanLoadInSoundEngine)
			|| (State == EState::Closed && !bCanOpenFile);
		if (UNLIKELY(!bResult))
		{
			UE_LOG(LogWwiseFileHandler, Warning, TEXT("FWwiseFileState::IncrementCountDone [%p] %s %" PRIu32 ": Could not open file for asset loading. [State:%s CanLoad:%s CanOpen:%s LoadCount:%d]"),
				this, GetManagingTypeName(), GetShortId(),
				GetStateName(), bCanLoadInSoundEngine ? TEXT("true"):TEXT("false"), bCanOpenFile ? TEXT("true"):TEXT("false"), LoadCount);
		}
	}

	UE_CLOG(LIKELY(bResult), LogWwiseFileHandler, VeryVerbose, TEXT("FWwiseFileState::IncrementCountDone [%p] %s %" PRIu32 ": Done incrementing."),
		this, GetManagingTypeName(), GetShortId());
	{
		SCOPED_WWISEFILEHANDLER_EVENT_F_4(TEXT("FWwiseFileState::IncrementCountDone %s Callback"), GetManagingTypeName());
		InCallback(bResult);
	}
}

void FWwiseFileState::DecrementCount(EWwiseFileStateOperationOrigin InOperationOrigin,
                                  FDeleteFileStateFunction&& InDeleteState, FDecrementCountCallback&& InCallback)
{
	check(FileStateExecutionQueue->IsRunningInThisThread());
	
	DecrementLoadCount(InOperationOrigin);

	DecrementCountUnload(InOperationOrigin, MoveTemp(InDeleteState), MoveTemp(InCallback));
}

void FWwiseFileState::DecrementCountUnload(EWwiseFileStateOperationOrigin InOperationOrigin,
	FDeleteFileStateFunction&& InDeleteState, FDecrementCountCallback&& InCallback)
{
	SCOPED_WWISEFILEHANDLER_EVENT_F_3(TEXT("FWwiseFileState::DecrementCountUnload %s"), GetManagingTypeName());
	check(FileStateExecutionQueue->IsRunningInThisThread());
	
	if (!CanUnloadFromSoundEngine())
	{
		DecrementCountClose(InOperationOrigin, MoveTemp(InDeleteState), MoveTemp(InCallback));					// Continue
		return;
	}

	SetState(TEXT("FWwiseFileState::DecrementCountUnload"), EState::Unloading);

	UnloadFromSoundEngine([This = AsShared(), InOperationOrigin, InDeleteState = MoveTemp(InDeleteState), InCallback = MoveTemp(InCallback)](EResult InDefer) mutable
	{
		SCOPED_WWISEFILEHANDLER_EVENT_F_4(TEXT("FWwiseFileState::DecrementCountUnload %s UnloadFromSoundEngine"), This->GetManagingTypeName());
		This->DecrementCountUnloadCallback(InOperationOrigin, MoveTemp(InDeleteState), MoveTemp(InCallback), InDefer);
	});
}

void FWwiseFileState::DecrementCountUnloadCallback(EWwiseFileStateOperationOrigin InOperationOrigin,
	FDeleteFileStateFunction&& InDeleteState, FDecrementCountCallback&& InCallback, EResult InDefer)
{
	SCOPED_WWISEFILEHANDLER_EVENT_F_3(TEXT("FWwiseFileState::DecrementCountUnloadCallback %s"), GetManagingTypeName());
	check(FileStateExecutionQueue->IsRunningInThisThread());
	
	if (LIKELY(InDefer == EResult::Done))
	{
		DecrementCountClose(InOperationOrigin, MoveTemp(InDeleteState), MoveTemp(InCallback));				// Continue
	}
	else if (UNLIKELY(State != EState::Unloading))
	{
		UE_LOG(LogWwiseFileHandler, VeryVerbose, TEXT("FWwiseFileState::DecrementCountUnloadCallback [%p] %s %" PRIu32 ": State got changed to %s. Not unloading anymore."),
				this, GetManagingTypeName(), GetShortId(), GetStateName());
		DecrementCountClose(InOperationOrigin, MoveTemp(InDeleteState), MoveTemp(InCallback));		// Continue
	}
	else
	{
		SetState(TEXT("FWwiseFileState::DecrementCountUnloadCallback"), EState::Loaded);
		AsyncOpLater(WWISEFILEHANDLER_ASYNC_NAME("FWwiseFileState::DecrementCountUnloadCallback Deferred"), [this, InOperationOrigin, InDeleteState = MoveTemp(InDeleteState), InCallback = MoveTemp(InCallback)]() mutable
		{
			UE_LOG(LogWwiseFileHandler, VeryVerbose, TEXT("FWwiseFileState::DecrementCountUnloadCallback [%p] %s %" PRIu32 ": Retrying deferred unload"),
				this, GetManagingTypeName(), GetShortId());
			DecrementCountUnload(InOperationOrigin, MoveTemp(InDeleteState), MoveTemp(InCallback));		// Call ourselves back
		});
	}
}

void FWwiseFileState::DecrementCountClose(EWwiseFileStateOperationOrigin InOperationOrigin,
                                          FDeleteFileStateFunction&& InDeleteState, FDecrementCountCallback&& InCallback)
{
	SCOPED_WWISEFILEHANDLER_EVENT_F_3(TEXT("FWwiseFileState::DecrementCountClose %s"), GetManagingTypeName());
	check(FileStateExecutionQueue->IsRunningInThisThread());
	
	if (!CanCloseFile())
	{
		DecrementCountDone(InOperationOrigin, MoveTemp(InDeleteState), MoveTemp(InCallback));					// Continue
		return;
	}

	SetState(TEXT("FWwiseFileState::DecrementCountClose"), EState::Closing);

	CloseFile([This = AsShared(), InOperationOrigin, InDeleteState = MoveTemp(InDeleteState), InCallback = MoveTemp(InCallback)](EResult InDefer) mutable
	{
		SCOPED_WWISEFILEHANDLER_EVENT_F_4(TEXT("FWwiseFileState::DecrementCountClose %s CloseFile"), This->GetManagingTypeName());
		This->DecrementCountCloseCallback(InOperationOrigin, MoveTemp(InDeleteState), MoveTemp(InCallback), InDefer);
	});
}

void FWwiseFileState::DecrementCountCloseCallback(EWwiseFileStateOperationOrigin InOperationOrigin,
	FDeleteFileStateFunction&& InDeleteState, FDecrementCountCallback&& InCallback, EResult InDefer)
{
	SCOPED_WWISEFILEHANDLER_EVENT_F_3(TEXT("FWwiseFileState::DecrementCountCloseCallback %s"), GetManagingTypeName());
	check(FileStateExecutionQueue->IsRunningInThisThread());
	
	if (LIKELY(InDefer == EResult::Done))
	{
		DecrementCountDone(InOperationOrigin, MoveTemp(InDeleteState), MoveTemp(InCallback));				// Continue
		return;
	}

	UE_LOG(LogWwiseFileHandler, Verbose, TEXT("FWwiseFileState::DecrementCountCloseCallback [%p] %s %" PRIu32 ": Processing deferred Close."),
		this, GetManagingTypeName(), GetShortId());
	
	if (UNLIKELY(State != EState::Closing))
	{
		UE_LOG(LogWwiseFileHandler, VeryVerbose, TEXT("FWwiseFileState::DecrementCountCloseCallback [%p] %s %" PRIu32 ": State got changed to %s. Not closing anymore."),
				this, GetManagingTypeName(), GetShortId(), GetStateName());
		DecrementCountClose(InOperationOrigin, MoveTemp(InDeleteState), MoveTemp(InCallback));		// Continue
	}
	else
	{
		SetState(TEXT("FWwiseFileState::DecrementCountCloseCallback"), EState::Opened);
		AsyncOpLater(WWISEFILEHANDLER_ASYNC_NAME("FWwiseFileState::DecrementCountUnloadCallback Deferred"), [this, InOperationOrigin, InDeleteState = MoveTemp(InDeleteState), InCallback = MoveTemp(InCallback)]() mutable
		{
			UE_LOG(LogWwiseFileHandler, VeryVerbose, TEXT("FWwiseFileState::DecrementCountCloseCallback [%p] %s %" PRIu32 ": Retrying deferred close"),
				this, GetManagingTypeName(), GetShortId());
			DecrementCountClose(InOperationOrigin, MoveTemp(InDeleteState), MoveTemp(InCallback));		// Call ourselves back
		});
	}
}

void FWwiseFileState::DecrementCountDone(EWwiseFileStateOperationOrigin InOperationOrigin,
                                         FDeleteFileStateFunction&& InDeleteState, FDecrementCountCallback&& InCallback)
{
	SCOPED_WWISEFILEHANDLER_EVENT_F_3(TEXT("FWwiseFileState::DecrementCountDone %s"), GetManagingTypeName());
	check(FileStateExecutionQueue->IsRunningInThisThread());
	
	AsyncOp(TEXT("FWwiseFileState::DecrementCountDone Delete"), [This = AsShared(), DeleteState = MoveTemp(InDeleteState), Callback = MoveTemp(InCallback)]() mutable
	{
		if (This->CanDelete())
		{
			UE_LOG(LogWwiseFileHandler, VeryVerbose, TEXT("FWwiseFileState::DecrementCountDone [%p] %s %" PRIu32 ": Done decrementing. Deleting state."),
					&This.Get(), This->GetManagingTypeName(), This->GetShortId());
			SCOPED_WWISEFILEHANDLER_EVENT_F_4(TEXT("FWwiseFileState::DecrementCountDone %s Delete"), This->GetManagingTypeName());
			DeleteState(MoveTemp(Callback));
		}
		else
		{
			UE_LOG(LogWwiseFileHandler, VeryVerbose, TEXT("FWwiseFileState::DecrementCountDone [%p] %s %" PRIu32 ": Done decrementing."),
					&This.Get(), This->GetManagingTypeName(), This->GetShortId());
			SCOPED_WWISEFILEHANDLER_EVENT_F_4(TEXT("FWwiseFileState::DecrementCountDone %s Callback"), This->GetManagingTypeName());
			Callback();
		}
	});
}

void FWwiseFileState::IncrementLoadCount(EWwiseFileStateOperationOrigin InOperationOrigin)
{
	check(FileStateExecutionQueue->IsRunningInThisThread());
	
	const bool bIncrementStreamingCount = (InOperationOrigin == EWwiseFileStateOperationOrigin::Streaming);

	if (bIncrementStreamingCount) ++StreamingCount;
	++LoadCount;

	UE_LOG(LogWwiseFileHandler, Verbose, TEXT("FWwiseFileState::IncrementLoadCount [%p] %s %" PRIu32 ": ++LoadCount %d %sStreamingCount %d OpenedInstances %d"),
	       this, GetManagingTypeName(), GetShortId(), LoadCount, bIncrementStreamingCount ? TEXT("++") : TEXT(""), StreamingCount, OpenedInstances.load());
}

bool FWwiseFileState::CanOpenFile() const
{
	return State == EState::Closed && LoadCount > 0;
}

void FWwiseFileState::OpenFileSucceeded(FOpenFileCallback&& InCallback)
{
	AsyncOp(WWISEFILEHANDLER_ASYNC_NAME("FWwiseFileState::OpenFileSucceeded"), [this, InCallback = MoveTemp(InCallback)]() mutable
	{
		if (UNLIKELY(State != EState::Opening))
		{
			UE_LOG(LogWwiseFileHandler, Error, TEXT("FWwiseFileState::OpenFileSucceeded [%p] %s %" PRIu32 ": Succeeded opening while not in Opening state: %s"),
				this, GetManagingTypeName(), GetShortId(), GetStateName());
		}
		else
		{
			SetState(TEXT("FWwiseFileState::OpenFileSucceeded"), EState::Opened);
		}
		SCOPED_WWISEFILEHANDLER_EVENT_F_4(TEXT("FWwiseFileState::OpenFileSucceeded %s Callback"), GetManagingTypeName());
		InCallback();
	});
}

void FWwiseFileState::OpenFileFailed(FOpenFileCallback&& InCallback)
{
	AsyncOp(WWISEFILEHANDLER_ASYNC_NAME("FWwiseFileState::OpenFileFailed"), [this, InCallback = MoveTemp(InCallback)]() mutable
	{
		INC_DWORD_STAT(STAT_WwiseFileHandlerTotalErrorCount);
		if (UNLIKELY(State != EState::Opening))
		{
			UE_LOG(LogWwiseFileHandler, Error, TEXT("FWwiseFileState::OpenFileFailed [%p] %s %" PRIu32 ": Failed opening while not in Opening state: %s"),
				this, GetManagingTypeName(), GetShortId(), GetStateName());
		}
		else
		{
			SetState(TEXT("FWwiseFileState::OpenFileFailed"), EState::Closed);
		}
		SCOPED_WWISEFILEHANDLER_EVENT_F_4(TEXT("FWwiseFileState::OpenFileFailed %s Callback"), GetManagingTypeName());
		InCallback();
	});
}

bool FWwiseFileState::CanLoadInSoundEngine() const
{
	return State == EState::Opened && (!IsStreamedState() || StreamingCount > 0);
}

void FWwiseFileState::LoadInSoundEngineSucceeded(FLoadInSoundEngineCallback&& InCallback)
{
	AsyncOp(WWISEFILEHANDLER_ASYNC_NAME("FWwiseFileState::LoadInSoundEngineSucceeded"), [this, InCallback = MoveTemp(InCallback)]() mutable
	{
		if (UNLIKELY(State != EState::Loading))
		{
			UE_LOG(LogWwiseFileHandler, Error, TEXT("FWwiseFileState::LoadInSoundEngineSucceeded [%p] %s %" PRIu32 ": Succeeded loading while not in Loading state: %s"),
				this, GetManagingTypeName(), GetShortId(), GetStateName());
		}
		else
		{
			SetState(TEXT("FWwiseFileState::LoadInSoundEngineSucceeded"), EState::Loaded);
		}
		SCOPED_WWISEFILEHANDLER_EVENT_F_4(TEXT("FWwiseFileState::LoadInSoundEngineSucceeded %s Callback"), GetManagingTypeName());
		InCallback();
	});
}

void FWwiseFileState::LoadInSoundEngineFailed(FLoadInSoundEngineCallback&& InCallback)
{
	AsyncOp(WWISEFILEHANDLER_ASYNC_NAME("FWwiseFileState::LoadInSoundEngineFailed"), [this, InCallback = MoveTemp(InCallback)]() mutable
	{
		INC_DWORD_STAT(STAT_WwiseFileHandlerTotalErrorCount);
		if (UNLIKELY(State != EState::Loading))
		{
			UE_LOG(LogWwiseFileHandler, Error, TEXT("FWwiseFileState::LoadInSoundEngineFailed [%p] %s %" PRIu32 ": Failed loading while not in Loading state: %s"),
				this, GetManagingTypeName(), GetShortId(), GetStateName());
		}
		else
		{
			SetState(TEXT("FWwiseFileState::LoadInSoundEngineFailed"), EState::Opened);
		}
		SCOPED_WWISEFILEHANDLER_EVENT_F_4(TEXT("FWwiseFileState::LoadInSoundEngineFailed %s Callback"), GetManagingTypeName());
		InCallback();
	});
}

void FWwiseFileState::DecrementLoadCount(EWwiseFileStateOperationOrigin InOperationOrigin)
{
	const bool bDecrementStreamingCount = (InOperationOrigin == EWwiseFileStateOperationOrigin::Streaming);

	if (bDecrementStreamingCount) --StreamingCount;
	--LoadCount;
	--OpenedInstances;

	UE_LOG(LogWwiseFileHandler, Verbose, TEXT("FWwiseFileState::DecrementLoadCount [%p] %s %" PRIu32 ": --LoadCount %d %sStreamingCount %d --OpenedInstances %d"),
	       this, GetManagingTypeName(), GetShortId(), LoadCount, bDecrementStreamingCount ? TEXT("--") : TEXT(""), StreamingCount, OpenedInstances.load());
}

bool FWwiseFileState::CanUnloadFromSoundEngine() const
{
	return State == EState::Loaded && ((IsStreamedState() && StreamingCount == 0) || (!IsStreamedState() && LoadCount == 0));
}

void FWwiseFileState::UnloadFromSoundEngineDone(FUnloadFromSoundEngineCallback&& InCallback)
{
	AsyncOp(WWISEFILEHANDLER_ASYNC_NAME("FWwiseFileState::UnloadFromSoundEngineDone"), [this, InCallback = MoveTemp(InCallback)]() mutable
	{
		if (UNLIKELY(State != EState::Unloading))
		{
			UE_LOG(LogWwiseFileHandler, Error, TEXT("FWwiseFileState::UnloadFromSoundEngineDone [%p] %s %" PRIu32 ": Done unloading while not in Unloading state: %s"),
				this, GetManagingTypeName(), GetShortId(), GetStateName());
		}
		else
		{
			SetState(TEXT("FWwiseFileState::UnloadFromSoundEngineDone"), EState::Opened);
		}
		
		SCOPED_WWISEFILEHANDLER_EVENT_F_4(TEXT("FWwiseFileState::UnloadFromSoundEngineDone %s Callback"), GetManagingTypeName());
		InCallback(EResult::Done);
	});
}

void FWwiseFileState::UnloadFromSoundEngineToClosedFile(FUnloadFromSoundEngineCallback&& InCallback)
{
	AsyncOp(WWISEFILEHANDLER_ASYNC_NAME("FWwiseFileState::UnloadFromSoundEngineToClosedFile"), [this, InCallback = MoveTemp(InCallback)]() mutable
	{
		if (UNLIKELY(State != EState::Unloading))
		{
			UE_LOG(LogWwiseFileHandler, Error, TEXT("FWwiseFileState::UnloadFromSoundEngineToClosedFile [%p] %s %" PRIu32 ": Done unloading while not in Unloading state: %s"),
				this, GetManagingTypeName(), GetShortId(), GetStateName());
		}
		else
		{
			SetState(TEXT("FWwiseFileState::UnloadFromSoundEngineToClosedFile"), EState::Closed);
		}
		
		SCOPED_WWISEFILEHANDLER_EVENT_F_4(TEXT("FWwiseFileState::UnloadFromSoundEngineToClosedFile %s Callback"), GetManagingTypeName());
		InCallback(EResult::Done);
	});
}

void FWwiseFileState::UnloadFromSoundEngineDefer(FUnloadFromSoundEngineCallback&& InCallback)
{
	AsyncOp(WWISEFILEHANDLER_ASYNC_NAME("FWwiseFileState::UnloadFromSoundEngineDefer"), [this, InCallback = MoveTemp(InCallback)]() mutable
	{
		if (UNLIKELY(State != EState::Unloading))
		{
			UE_LOG(LogWwiseFileHandler, Error, TEXT("FWwiseFileState::UnloadFromSoundEngineDefer [%p] %s %" PRIu32 ": Deferring unloading while not in Unloading state: %s"),
				this, GetManagingTypeName(), GetShortId(), GetStateName());
			SCOPED_WWISEFILEHANDLER_EVENT_F_4(TEXT("FWwiseFileState::UnloadFromSoundEngineDefer %s Callback"), GetManagingTypeName());
			InCallback(EResult::Done);
		}
		else
		{
			UE_LOG(LogWwiseFileHandler, VeryVerbose, TEXT("FWwiseFileState::UnloadFromSoundEngineDefer [%p] %s %" PRIu32 ": Deferring Unload"),
				this, GetManagingTypeName(), GetShortId());
			SCOPED_WWISEFILEHANDLER_EVENT_F_4(TEXT("FWwiseFileState::UnloadFromSoundEngineDefer %s Callback"), GetManagingTypeName());
			InCallback(EResult::Deferred);
		}
	});
}

bool FWwiseFileState::CanCloseFile() const
{
	return State == EState::Opened && LoadCount == 0;
}

void FWwiseFileState::CloseFileDone(FCloseFileCallback&& InCallback)
{
	AsyncOp(WWISEFILEHANDLER_ASYNC_NAME("FWwiseFileState::CloseFileDone"), [this, InCallback = MoveTemp(InCallback)]() mutable
	{
		if (UNLIKELY(State != EState::Closing))
		{
			UE_LOG(LogWwiseFileHandler, Error, TEXT("FWwiseFileState::CloseFileDone [%p] %s %" PRIu32 ": Done closing while not in Closing state: %s"),
				this, GetManagingTypeName(), GetShortId(), GetStateName());
		}
		else
		{
			SetState(TEXT("FWwiseFileState::CloseFileDone"), EState::Closed);
		}
		
		SCOPED_WWISEFILEHANDLER_EVENT_F_4(TEXT("FWwiseFileState::CloseFileDone %s Callback"), GetManagingTypeName());
		InCallback(EResult::Done);
	});
}

void FWwiseFileState::CloseFileDefer(FCloseFileCallback&& InCallback)
{
	AsyncOp(WWISEFILEHANDLER_ASYNC_NAME("FWwiseFileState::CloseFileDefer"), [this, InCallback = MoveTemp(InCallback)]() mutable
	{
		if (UNLIKELY(State != EState::Closing))
		{
			UE_LOG(LogWwiseFileHandler, Error, TEXT("FWwiseFileState::CloseFileDefer [%p] %s %" PRIu32 ": Deferring closing while not in Closing state: %s"),
				this, GetManagingTypeName(), GetShortId(), GetStateName());
			SCOPED_WWISEFILEHANDLER_EVENT_F_4(TEXT("FWwiseFileState::CloseFileDefer %s Callback"), GetManagingTypeName());
			InCallback(EResult::Done);
			return;
		}

		UE_LOG(LogWwiseFileHandler, Verbose, TEXT("FWwiseFileState::CloseFileDefer [%p] %s %" PRIu32 ": Deferring Close"),
			this, GetManagingTypeName(), GetShortId());
		SCOPED_WWISEFILEHANDLER_EVENT_F_4(TEXT("FWwiseFileState::CloseFileDefer %s Callback"), GetManagingTypeName());
		InCallback(EResult::Deferred);
	});
}

void FWwiseFileState::AsyncOp(const TCHAR* InDebugName, FBasicFunction&& Fct)
{
	if (UNLIKELY(!FileStateExecutionQueue))
	{
		UE_LOG(LogWwiseFileHandler, Error, TEXT("FWwiseFileState::AsyncOp: Doing async op on terminated state"));
		return Fct();
	}
	FileStateExecutionQueue->Async(InDebugName, [SharedThis = AsShared(), Fct = MoveTemp(Fct)]() mutable
	{
		Fct();
	});
}

void FWwiseFileState::AsyncOpLater(const TCHAR* InDebugName, FBasicFunction&& Fct)
{
	RegisterRecurringCallback();
	LaterOpQueue.Enqueue(FOpQueueItem(InDebugName, [SharedThis = AsShared(), Fct = MoveTemp(Fct)]() mutable
	{
		Fct();
	}));
}

void FWwiseFileState::ProcessLaterOpQueue()
{
	check(!FileStateExecutionQueue || FileStateExecutionQueue->IsRunningInThisThread());

	const auto ManagingTypeName = GetManagingTypeName();
	const auto ShortId = GetShortId();
	TArray<FOpQueueItem> OpQueueToAdd;
	for (FOpQueueItem* Op; (Op = LaterOpQueue.Peek()) != nullptr; LaterOpQueue.Pop())
	{
#if ENABLE_NAMED_EVENTS
		OpQueueToAdd.Add(FOpQueueItem(Op->DebugName, std::move(Op->Function)));
#else
		OpQueueToAdd.Add(FOpQueueItem(TEXT(""), std::move(Op->Function)));
#endif
	}

	if (OpQueueToAdd.Num() == 0)
	{
		return;
	}
	UE_LOG(LogWwiseFileHandler, VeryVerbose, TEXT("FWwiseFileState::ProcessLaterOpQueue [%p] %s %" PRIu32 ": Adding back %d operations to be executed."),
		this, ManagingTypeName, ShortId, OpQueueToAdd.Num());
		
	for (FOpQueueItem& Op : OpQueueToAdd)
	{
		// This operation can delete the file state
#if ENABLE_NAMED_EVENTS
		AsyncOp(Op.DebugName, MoveTemp(Op.Function));
#else
		AsyncOp(nullptr, MoveTemp(Op.Function));
#endif
	}
}

void FWwiseFileState::RegisterRecurringCallback()
{
	if (auto* GlobalCallbacks = FWwiseGlobalCallbacks::Get())
	{
		GlobalCallbacks->EndAsync([This = AsShared()]() mutable
		{
			This->AsyncOp(TEXT("FWwiseFileState::RegisterRecurringCallback"), [This]() mutable
			{
				This->ProcessLaterOpQueue();
			});
			return EWwiseDeferredAsyncResult::Done;
		});
	}
}

void FWwiseFileState::ProcessNextOperation()
{
	AsyncOp(TEXT("ProcessNextOperation"), [This = AsShared()]() mutable
	{
		static FWwiseFileStateOperation CanProcessNextFlag;
		FWwiseFileStateOperation* Expected{ nullptr };
		if (!This->CurrentOp.compare_exchange_strong(Expected, &CanProcessNextFlag))
		{
			return;
		}

		auto* OpToStart = This->OpQueue.Pop();
		This->CurrentOp.store(OpToStart);

		if (OpToStart)
		{
			This->CurrentOp.load()->StartOperation(This.Get());
		}
	});
}
