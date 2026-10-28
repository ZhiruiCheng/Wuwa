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

#include "Wwise/WwiseUnitTests.h"

#if WWISE_UNIT_TESTS
#include "Wwise/Mock/WwiseMockFileState.h"
#include <atomic>

#include "Wwise/Stats/FileHandler.h"

WWISE_TEST_CASE(FileHandler_FileState_Smoke, "Wwise::FileHandler::FileState_Smoke", "[ApplicationContextMask][SmokeFilter]")
{
	SECTION("Static")
	{
		static_assert(!std::is_constructible<FWwiseFileState>::value, "File State cannot be constructed through a default parameter");
		static_assert(!std::is_copy_constructible<FWwiseFileState>::value, "Cannot copy a File State");
		static_assert(!std::is_copy_assignable<FWwiseFileState>::value, "Cannot assign to a File State");
		static_assert(!std::is_move_constructible<FWwiseFileState>::value, "Cannot move-construct a File State");
	}

	SECTION("Instantiation")
	{
		FWwiseMockFileState(0);
		FWwiseMockFileState(1);
		FWwiseMockFileState(2);
		FWwiseMockFileState(3);
	}

	SECTION("Loading Streaming File")
	{
		FEventRef Done;
		auto File{ MakeShared<FWwiseMockFileState>(10) };
		File->bIsStreamedState = FWwiseMockFileState::OptionalBool::True;

		bool bDeleted{ false };
		File->IncrementCountAsync(EWwiseFileStateOperationOrigin::Loading, [&File, &Done, &bDeleted](bool bResult) mutable
		{
			WWISE_CHECK(bResult);
			WWISE_CHECK(File->State == FWwiseFileState::EState::Opened);
			File->DecrementCountAsync(EWwiseFileStateOperationOrigin::Loading,
				[&File, &bDeleted](FWwiseFileState::FDecrementCountCallback&& Callback) mutable
				{
					WWISE_CHECK(File->State == FWwiseFileState::EState::Closed);
					bDeleted = true;
					Callback();
				},
				[&Done, &bDeleted]() mutable
				{
					WWISE_CHECK(bDeleted);
					Done->Trigger();
				});
		});
		WWISE_CHECK(Done->Wait(1000));
	}

	SECTION("Streaming File")
	{
		FEventRef Done;
		auto File{ MakeShared<FWwiseMockFileState>(20) };
		File->bIsStreamedState = FWwiseMockFileState::OptionalBool::True;
		
		bool bDeleted{ false };
		File->IncrementCountAsync(EWwiseFileStateOperationOrigin::Streaming, [File, &Done, &bDeleted](bool bResult) mutable
		{
			WWISE_CHECK(bResult);
			WWISE_CHECK(File->State == FWwiseFileState::EState::Loaded);
			File->DecrementCountAsync(EWwiseFileStateOperationOrigin::Streaming,
				[File, &bDeleted](FWwiseFileState::FDecrementCountCallback&& Callback) mutable
				{
					WWISE_CHECK(File->State == FWwiseFileState::EState::Closed);
					bDeleted = true;
					Callback();
				},
				[&Done, &bDeleted]() mutable
				{
					WWISE_CHECK(bDeleted);
					Done->Trigger();
				});
		});
		WWISE_CHECK(Done->Wait(1000));
	}

	SECTION("Delete in Decrement")
	{
		FEventRef Done;
		auto File { MakeShared<FWwiseMockFileState>(30) };

		bool bDeleted{ false };
		File->IncrementCountAsync(EWwiseFileStateOperationOrigin::Loading, [File, &Done, &bDeleted](bool bResult) mutable
		{
			WWISE_CHECK(bResult);
			WWISE_CHECK(File->State == FWwiseFileState::EState::Loaded);
			File->DecrementCountAsync(EWwiseFileStateOperationOrigin::Loading,
				[File, &bDeleted](FWwiseFileState::FDecrementCountCallback&& Callback) mutable
				{
					WWISE_CHECK(File->State == FWwiseFileState::EState::Closed);
					bDeleted = true;
					Callback();
				},
				[&Done, &bDeleted]() mutable
				{
					WWISE_CHECK(bDeleted);
					Done->Trigger();
				});
		});
		WWISE_CHECK(Done->Wait(1000));
	}

	SECTION("Ordered callbacks")
	{
		FEventRef Done;
		auto File{ MakeShared<FWwiseMockFileState>(40) };
		File->bIsStreamedState = FWwiseMockFileState::OptionalBool::True;

		int Order = 0;
		constexpr const int Count = 10;

		for (int NumOp = 0; NumOp < Count; ++NumOp)
		{
			File->IncrementCountAsync(EWwiseFileStateOperationOrigin::Loading, [NumOp, &Order](bool bResult) mutable
			{
				WWISE_CHECK(NumOp*4+0 == Order);
				Order++;
			});
			File->DecrementCountAsync(EWwiseFileStateOperationOrigin::Loading,
				[](FWwiseFileState::FDecrementCountCallback&& Callback) mutable
				{
					Callback();
				},
				[NumOp, &Order]() mutable
				{
					WWISE_CHECK(NumOp*4+1 == Order);
					Order++;
				});
			File->IncrementCountAsync(EWwiseFileStateOperationOrigin::Loading, [NumOp, &Order](bool bResult) mutable
			{
				WWISE_CHECK(NumOp*4+2 == Order);
				Order++;
			});
			File->DecrementCountAsync(EWwiseFileStateOperationOrigin::Loading,
				[&Done](FWwiseFileState::FDecrementCountCallback&& Callback) mutable
				{
					Callback();
					Done->Trigger();
				},
				[NumOp, &Order]() mutable
				{
					WWISE_CHECK(NumOp*4+3 == Order);
					Order++;
				});
		}
		WWISE_CHECK(Done->Wait(10000));
	}
}

WWISE_TEST_CASE(FileHandler_FileState, "Wwise::FileHandler::FileState", "[ApplicationContextMask][ProductFilter]")
{
	SECTION("Reloading Streaming File")
	{
		FEventRef Done;
		auto File{ MakeShared<FWwiseMockFileState>(1000) };
		File->bIsStreamedState = FWwiseMockFileState::OptionalBool::True;

		bool bDeleted{ false };
		bool bInitialDecrementDone{ false };
		File->IncrementCountAsync(EWwiseFileStateOperationOrigin::Loading, [&File, &bDeleted](bool bResult) mutable
		{
			WWISE_CHECK(bResult);
			WWISE_CHECK(File->State == FWwiseFileState::EState::Opened);
		});
		File->DecrementCountAsync(EWwiseFileStateOperationOrigin::Loading,
			[](FWwiseFileState::FDecrementCountCallback&& Callback) mutable
			{
				WWISE_CHECK(false);
				Callback();
			},
			[&File, &bDeleted, &bInitialDecrementDone]() mutable
			{
				bInitialDecrementDone = true;
			});
		File->IncrementCountAsync(EWwiseFileStateOperationOrigin::Loading, [&File, &bDeleted](bool bResult) mutable
		{
			WWISE_CHECK(bResult);
		});
		File->DecrementCountAsync(EWwiseFileStateOperationOrigin::Loading,
			[&File, &bDeleted](FWwiseFileState::FDecrementCountCallback&& Callback) mutable
			{
				WWISE_CHECK(File->State == FWwiseFileState::EState::Closed);
				bDeleted = true;
				Callback();
			},
			[&Done, &bDeleted]() mutable
			{
				WWISE_CHECK(bDeleted);
				Done->Trigger();
			});
		WWISE_CHECK(Done->Wait(1000));
		WWISE_CHECK(bInitialDecrementDone);
	}

	SECTION("Restreaming File")
	{
		FEventRef Done;
		auto File{ MakeShared<FWwiseMockFileState>(1010) };
		File->bIsStreamedState = FWwiseMockFileState::OptionalBool::True;

		bool bDeleted{ false };
		bool bInitialDecrementDone{ false };
		File->IncrementCountAsync(EWwiseFileStateOperationOrigin::Streaming, [&File, &bDeleted](bool bResult) mutable
		{
		});
		File->DecrementCountAsync(EWwiseFileStateOperationOrigin::Streaming,
			[](FWwiseFileState::FDecrementCountCallback&& Callback) mutable
			{
				// Delete State should never be called, since the last one should delete our object 
				WWISE_CHECK(false);
				Callback();
			},
			[&File, &bDeleted, &bInitialDecrementDone]() mutable
			{
				bInitialDecrementDone = true;
			});
		File->IncrementCountAsync(EWwiseFileStateOperationOrigin::Streaming, [&File, &bDeleted](bool bResult) mutable
		{
		});
		File->DecrementCountAsync(EWwiseFileStateOperationOrigin::Streaming,
			[&File, &bDeleted](FWwiseFileState::FDecrementCountCallback&& Callback) mutable
			{
				WWISE_CHECK(File->State == FWwiseFileState::EState::Closed);
				bDeleted = true;
				Callback();
			},
			[&Done, &bDeleted]() mutable
			{
				WWISE_CHECK(bDeleted);
				Done->Trigger();
			});
		WWISE_CHECK(Done->Wait(1000));
		WWISE_CHECK(bInitialDecrementDone);
	}

	SECTION("Deferring Unload")
	{
		FEventRef Done;
		auto File{ MakeShared<FWwiseMockFileState>(10) };
		File->bIsStreamedState = FWwiseMockFileState::OptionalBool::True;
		File->UnloadFromSoundEngineDeferCount = 1;

		bool bDeleted{ false };
		File->IncrementCountAsync(EWwiseFileStateOperationOrigin::Loading, [&File, &Done, &bDeleted](bool bResult) mutable
		{
			WWISE_CHECK(bResult);
			WWISE_CHECK(File->State == FWwiseFileState::EState::Opened);
			File->DecrementCountAsync(EWwiseFileStateOperationOrigin::Loading,
				[&File, &bDeleted](FWwiseFileState::FDecrementCountCallback&& Callback) mutable
				{
					WWISE_CHECK(File->State == FWwiseFileState::EState::Closed);
					bDeleted = true;
					Callback();
				},
				[&Done, &bDeleted]() mutable
				{
					WWISE_CHECK(bDeleted);
					Done->Trigger();
				});
		});
		WWISE_CHECK(Done->Wait(1000));
	}

	SECTION("Deferring Close")
	{
		FEventRef Done;
		auto File{ MakeShared<FWwiseMockFileState>(10) };
		File->bIsStreamedState = FWwiseMockFileState::OptionalBool::True;
		File->CloseFileDeferCount = 1;

		bool bDeleted{ false };
		File->IncrementCountAsync(EWwiseFileStateOperationOrigin::Loading, [&File, &Done, &bDeleted](bool bResult) mutable
		{
			WWISE_CHECK(bResult);
			WWISE_CHECK(File->State == FWwiseFileState::EState::Opened);
			File->DecrementCountAsync(EWwiseFileStateOperationOrigin::Loading,
				[&File, &bDeleted](FWwiseFileState::FDecrementCountCallback&& Callback) mutable
				{
					WWISE_CHECK(File->State == FWwiseFileState::EState::Closed);
					bDeleted = true;
					Callback();
				},
				[&Done, &bDeleted]() mutable
				{
					WWISE_CHECK(bDeleted);
					Done->Trigger();
				});
		});
		WWISE_CHECK(Done->Wait(1000));
	}
}

/*
WWISE_TEST_CASE(FileHandler_FileState_Perf, "Wwise::FileHandler::FileState_Perf", "[ApplicationContextMask][PerfFilter]")
{
}
*/

WWISE_TEST_CASE(FileHandler_FileState_Stress, "Wwise::FileHandler::FileState_Stress", "[ApplicationContextMask][StressFilter]")
{
	SECTION("Stress Open and Streams")
	{
		constexpr const int StateCount = 200;
		constexpr const int LoadCount = 35;
		constexpr const int WiggleCount = 5;

		FEventRef Dones[StateCount];
		std::atomic<int> DoneCounts[StateCount];
		TSharedPtr<FWwiseMockFileState> Files[StateCount];
		for (int StateIter = 0; StateIter < StateCount; ++StateIter)
		{
			FEventRef& Done(Dones[StateIter]);
			auto& DoneCount(DoneCounts[StateIter]);
			DoneCount = 0;
			
			Files[StateIter] = MakeShared<FWwiseMockFileState>(10000 + StateIter);
			auto File = Files[StateIter];
			File->bIsStreamedState = FWwiseMockFileState::OptionalBool::True;

			for (int LoadIter = 0; LoadIter < LoadCount; ++LoadIter)
			{
				const EWwiseFileStateOperationOrigin FirstOp = (StateIter&1)==0 ? EWwiseFileStateOperationOrigin::Loading : EWwiseFileStateOperationOrigin::Streaming;
				const EWwiseFileStateOperationOrigin SecondOp = (StateIter&1)==1 ? EWwiseFileStateOperationOrigin::Loading : EWwiseFileStateOperationOrigin::Streaming;
				FFunctionGraphTask::CreateAndDispatchWhenReady([Op = FirstOp, &Done, &DoneCount, File, WiggleCount]() mutable
				{
					for (int WiggleIter = 0; WiggleIter < WiggleCount; ++WiggleIter)
					{
						File->IncrementCountAsync(Op, [](bool){});
						File->DecrementCountAsync(Op, [](FWwiseFileState::FDecrementCountCallback&& InCallback){ InCallback(); }, []{});
					}
					File->IncrementCountAsync(Op, [Op, &Done, &DoneCount, File, WiggleCount](bool)
					{
						for (int WiggleIter = 0; WiggleIter < WiggleCount; ++WiggleIter)
						{
							File->DecrementCountAsync(Op, [](FWwiseFileState::FDecrementCountCallback&& InCallback){ InCallback(); }, []{});
							File->IncrementCountAsync(Op, [](bool){});
						}
						File->DecrementCountAsync(Op, [](FWwiseFileState::FDecrementCountCallback&& InCallback)
						{
							InCallback();
						}, [&Done, &DoneCount]
						{
							++DoneCount;
						});
					});
				});
				FFunctionGraphTask::CreateAndDispatchWhenReady([Op = SecondOp, &Done, &DoneCount, File, WiggleCount, LoadIter]() mutable
				{
					for (int WiggleIter = 0; WiggleIter < WiggleCount; ++WiggleIter)
					{
						File->IncrementCountAsync(Op, [](bool){});
						File->DecrementCountAsync(Op, [](FWwiseFileState::FDecrementCountCallback&& InCallback){ InCallback(); }, []{});
					}
					File->IncrementCountAsync(Op, [Op, &Done, &DoneCount, File, WiggleCount, LoadIter](bool)
					{
						for (int WiggleIter = 0; WiggleIter < WiggleCount; ++WiggleIter)
						{
							File->DecrementCountAsync(Op, [](FWwiseFileState::FDecrementCountCallback&& InCallback){ InCallback(); }, []{});
							File->IncrementCountAsync(Op, [](bool){});
						}
						File->DecrementCountAsync(Op, [](FWwiseFileState::FDecrementCountCallback&& InCallback)
						{
							InCallback();
						}, [Op, File, &Done, &DoneCount, LoadIter, WiggleCount]
						{
							++DoneCount;
							if (LoadIter == LoadCount - 1)
							{
								FFunctionGraphTask::CreateAndDispatchWhenReady([Op, &Done, &DoneCount, File, WiggleCount]() mutable
								{
									File->IncrementCountAsync(Op, [](bool){});
									File->DecrementCountAsync(Op, [](FWwiseFileState::FDecrementCountCallback&& InCallback)
									{
										InCallback();
									}, [&Done]
									{
										Done->Trigger();		// This can be triggered earlier than the last operation 
									});
								});
								
							}
						});
					});
				});
			}
		}

		for (int StateIter = 0; StateIter < StateCount; ++StateIter)
		{
			FEventRef& Done(Dones[StateIter]);
			const auto Multiplicand = UE_LOG_ACTIVE(LogWwiseFileHandler, VeryVerbose) ? 80 : 1;
			WWISE_CHECK(Done->Wait(5000 * Multiplicand));
		}
		FPlatformProcess::Sleep(0.05f);
		for (int StateIter = 0; StateIter < StateCount; ++StateIter)
		{
			auto& DoneCount(DoneCounts[StateIter]);
			WWISE_CHECK(DoneCount == LoadCount * 2);
		}
	}	
}
#endif // WWISE_UNIT_TESTS