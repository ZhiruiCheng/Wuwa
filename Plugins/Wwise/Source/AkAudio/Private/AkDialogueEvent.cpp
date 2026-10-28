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

#include "AkDialogueEvent.h"

#include "AkAudioDevice.h"
#include "AkAudioNode.h"
#include "AkDynamicSequence.h"
#include "AkDynamicSequenceBlueprintFunctionLibrary.h"
#include "AkDynamicSequencePlaylist.h"
#include "AkGameObject.h"
#include "AkGroupValue.h"
#include "UObject/ObjectSaveContext.h"
#include "Wwise/WwiseResourceLoader.h"
#include "Wwise/API/WwiseSoundEngineAPI.h"
#include "Wwise/Loaded/WwiseLoadedEvent.h"
#include "Wwise/Stats/AkAudio.h"

#if WITH_EDITORONLY_DATA && UE_5_5_OR_LATER
#include "UObject/ObjectSaveContext.h"
#include "Serialization/CompactBinaryWriter.h"
#endif

#if WITH_EDITORONLY_DATA
#include "Wwise/WwiseProjectDatabase.h"
#include "Wwise/WwiseResourceCooker.h"
#endif

#include <inttypes.h>

#include "Wwise/WwiseGlobalCallbacks.h"

TMap<int32, TWeakObjectPtr<UAkAudioNode>> UAkDialogueEvent::KnownAudioNodes;

UAkAudioNode* UAkDialogueEvent::Resolve(TArray<UAkGroupValue*> Arguments, bool bOrderedPath, FAkDialogueEventCandidateCallback CandidateCallback)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkDialogueEvent::Resolve"));
	auto* SoundEngine = IWwiseSoundEngineAPI::Get();
	if (UNLIKELY(!SoundEngine)) return nullptr;
	if (UNLIKELY(!LoadedDialogueEvent.load())) return nullptr;
	const auto& LoadedData{ LoadedDialogueEvent.load()->GetValue() };
	
	// Build ArgumentValues
	TArray<AkArgumentValueID> ArgumentValues;
	const auto& RequiredArguments{ LoadedData.LocalizedDialogueEventCookedData.RequiredArguments };
	int RequiredCount{0};
	for (const auto& RequiredArgument : RequiredArguments)
	{
		RequiredCount += RequiredArgument.Value.Positions.Num();
	}
	if (bOrderedPath)
	{
		if (UNLIKELY(Arguments.Num() != RequiredCount))
		{
			UE_LOG(LogAkAudio, Error, TEXT("DialogueEvent::Resolve %s %" PRIu32 " Not enough arguments for ordered resolve: %d, expecting %d."),
				*GetName(), GetShortID(), Arguments.Num(), RequiredCount);
			return nullptr;
		}
		ArgumentValues.Reserve(RequiredCount);
		for (const auto* const& Argument : Arguments)
		{
			if (Argument && IsValid(Argument))
			{
				UE_LOG(LogAkAudio, VeryVerbose, TEXT("DialogueEvent::Resolve %s %" PRIu32 " Adding ordered argument #%d %s"),
					*GetName(), GetShortID(), ArgumentValues.Num(), *Argument->GroupValueCookedData.GetDebugString());
				ArgumentValues.Add(Argument->GetShortID());
			}
			else
			{
				UE_LOG(LogAkAudio, VeryVerbose, TEXT("DialogueEvent::Resolve %s %" PRIu32 " Adding fallback argument #%d"),
					*GetName(), GetShortID(), ArgumentValues.Num());
				ArgumentValues.AddZeroed(1);
			}
		}
	}
	else
	{
		ArgumentValues.AddZeroed(RequiredCount);
		for (const auto* const& Argument : Arguments)
		{
			if (Argument && IsValid(Argument))
			{
				if (const FWwiseDialogueArgumentPosition* Positions{
					RequiredArguments.Find(FWwiseDialogueArgumentItem::FromGroupValue(Argument->GroupValueCookedData)) })
				{
					bool bUpdated{false};
					for (int Position : Positions->Positions)
					{
						if (ArgumentValues[Position] == AK_FALLBACK_ARGUMENTVALUE_ID)
						{
							UE_LOG(LogAkAudio, VeryVerbose, TEXT("DialogueEvent::Resolve %s %" PRIu32 " Adding unordered argument #%d %s"),
								*GetName(), GetShortID(), Position, *Argument->GroupValueCookedData.GetDebugString());
							ArgumentValues[Position] = Argument->GetShortID();
							bUpdated = true;
							break;
						}
					}
					UE_CLOG(!bUpdated, LogAkAudio, Warning, TEXT("DialogueEvent::Resolve %s %" PRIu32 " has no empty slot for unordered argument %s. Ignoring."),
						*GetName(), GetShortID(), *Argument->GroupValueCookedData.GetDebugString());
				}
				else
				{
					UE_LOG(LogAkAudio, Warning, TEXT("DialogueEvent::Resolve %s %" PRIu32 " doesn't have any use for unordered argument %s. Ignoring."),
						*GetName(), GetShortID(), *Argument->GroupValueCookedData.GetDebugString());
				}
			}
		}
	}

	// Build Resolve callback
	AkUniqueID AudioNodeID{ AK_INVALID_UNIQUE_ID };
	struct FResolveCallbackInfo
	{
		FAkDialogueEventCandidateCallback CandidateCallback;
		UAkDialogueEvent* DialogueEvent;
		static bool Exec(AkUniqueID Event, AkUniqueID Candidate, void* Info)
		{
			const auto* ResolveCallbackInfo = static_cast<FResolveCallbackInfo*>(Info);
			if (ResolveCallbackInfo->CandidateCallback.IsBound())
			{
				const bool bResult =
					ResolveCallbackInfo->CandidateCallback.Execute((int32)Event, (int32)Candidate, ResolveCallbackInfo->DialogueEvent);
				UE_LOG(LogAkAudio, VeryVerbose, TEXT("DialogueEvent::Resolve::Callback::Exec Event %s %" PRIu32 " Candidate %" PRIu32 ": %s"),
					*ResolveCallbackInfo->DialogueEvent->GetName(), ResolveCallbackInfo->DialogueEvent->GetShortID(),
					Candidate, bResult ? TEXT("Accepted") : TEXT("Skipped"));
				return bResult;
			}
			else
			{
				UE_LOG(LogAkAudio, VeryVerbose, TEXT("DialogueEvent::Resolve::Callback::Exec Event %s %" PRIu32 " Candidate %" PRIu32 ": Auto-accepted"),
					*ResolveCallbackInfo->DialogueEvent->GetName(), ResolveCallbackInfo->DialogueEvent->GetShortID(),
					Candidate);
				return true;
			}
		}
	} ResolveCallbackInfo;
	ResolveCallbackInfo.CandidateCallback = CandidateCallback;
	ResolveCallbackInfo.DialogueEvent = this;

	// SoundEngine Resolve
	const auto AudioNodeId =
		SoundEngine->DynamicDialogue->ResolveDialogueEvent( (AkUniqueID)DialogueEventCookedData.DialogueEventId,
			ArgumentValues.GetData(), (AkUInt32)ArgumentValues.Num(),
			AK_INVALID_PLAYING_ID,
			FResolveCallbackInfo::Exec, &ResolveCallbackInfo);
	if (UNLIKELY(AudioNodeId == AK_INVALID_UNIQUE_ID))
	{
		UE_LOG(LogAkAudio, Error, TEXT("DialogueEvent::Resolve %s %" PRIu32 " Could not resolve Dialogue Event."),
			*GetName(), GetShortID());
		return nullptr;
	}


	auto* Result{ FetchAudioNodeObject(AudioNodeId) };
	if (UNLIKELY(!Result))
	{
		UE_LOG(LogAkAudio, Error, TEXT("DialogueEvent::Resolve %s %" PRIu32 " Could not fetch the Audio Node object for %" PRIu32 "."),
			*GetName(), GetShortID(), AudioNodeId);
		return nullptr;
	}

	if (Result->AudioNodeCookedData.AudioNodeLoading == EWwiseAudioNodeLoading::AlwaysLoad ||
		Result->AudioNodeCookedData.AudioNodeLoading == EWwiseAudioNodeLoading::LoadOnReference ||
		Result->AudioNodeCookedData.AudioNodeLoading == EWwiseAudioNodeLoading::LoadOnResolve)
	{
		if (!Result->IsLoaded())
		{
			UE_LOG(LogAkAudio, Verbose, TEXT("DialogueEvent::Resolve %s %" PRIu32 " Loading audio node object %s."),
				*GetName(), GetShortID(), *Result->AudioNodeCookedData.GetDebugString());
			Result->LoadData();
		}
	}
	return Result;
}

UAkAudioNode* UAkDialogueEvent::ResolveArguments(TArray<UAkGroupValue*> Arguments)
{
	return Resolve(Arguments, false, {});
}

UAkAudioNode* UAkDialogueEvent::ResolveOrderedArguments(TArray<UAkGroupValue*> Arguments)
{
	return Resolve(Arguments, true, {});
}

UAkAudioNode* UAkDialogueEvent::FetchAudioNodeObject(int32 AudioNodeId)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkDialogueEvent::FetchAudioNodeObject"));
	if (UNLIKELY(!LoadedDialogueEvent.load())) return nullptr;
	const auto& LoadedData{ LoadedDialogueEvent.load()->GetValue() };

	// Find AudioNode
	if (AudioNodeId != AK_INVALID_UNIQUE_ID)
	{
		// Check in the global array of all known Audio Nodes if we're already loaded.
		if (const auto* KnownAudioNode = KnownAudioNodes.Find(AudioNodeId))
		{
			if (const auto ValidAudioNode = KnownAudioNode->Get())
			{
				UE_LOG(LogAkAudio, VeryVerbose, TEXT("DialogueEvent::FetchAudioNodeObject %s %" PRIu32 " Fetched node %" PRIu32 " from global cache."),
					*GetName(), GetShortID(), AudioNodeId);

				// Store locally
				LoadedAudioNodes.Add(AudioNodeId, ValidAudioNode);
				
				return ValidAudioNode;
			}
		}
	}
	
	// Check in the local array of loaded Audio Nodes if we already have this one. 
	// If we have an AudioNode ID 0, it means it's a "fake" AudioNode that should be loaded for everything.
	UAkAudioNode* AudioNode{ nullptr };
	if (TObjectPtr<UAkAudioNode>* Candidate = LoadedAudioNodes.Find(AudioNodeId))
	{
		UE_LOG(LogAkAudio, VeryVerbose, TEXT("DialogueEvent::FetchAudioNodeObject %s %" PRIu32 " Retrieving node %" PRIu32 " from actual copy."),
			*GetName(), GetShortID(), AudioNodeId);

		AudioNode = (*Candidate);
	}
	else if (TObjectPtr<UAkAudioNode>* Candidate0 = LoadedAudioNodes.Find(0))
	{
		UE_LOG(LogAkAudio, VeryVerbose, TEXT("DialogueEvent::FetchAudioNodeObject %s %" PRIu32 " Creating node %" PRIu32 " from single AudioNode 0."),
			*GetName(), GetShortID(), AudioNodeId);

		AudioNode = NewObject<UAkAudioNode>();
		if (!AudioNode)
		{
			UE_LOG(LogAkAudio, Error, TEXT("DialogueEvent::FetchAudioNodeObject %s %" PRIu32 " Could not instantiate new node %" PRIu32 " from single AudioNode 0."),
				*GetName(), GetShortID(), AudioNodeId);

			return nullptr;
		}
		
#if WITH_EDITORONLY_DATA
		AudioNode->AudioNodeInfo = (*Candidate0)->AudioNodeInfo;
#endif
		AudioNode->AudioNodeCookedData = (*Candidate0)->AudioNodeCookedData;
		AudioNode->AudioNodeCookedData.AudioNodeId = AudioNodeId;		// This is the reason why we cannot reuse 0
	}
	if (AudioNode)
	{
		// Store globally
		if (AudioNodeId != AK_INVALID_UNIQUE_ID)
		{
			KnownAudioNodes.Add(AudioNodeId, AudioNode);
		}
		return AudioNode;
	}

	// Try to generate the AudioNode from the Cooked Data
	UE_LOG(LogAkAudio, VeryVerbose, TEXT("DialogueEvent::FetchAudioNodeObject %s %" PRIu32 " Retrieving node %" PRIu32 " from cooked data."),
		*GetName(), GetShortID(), AudioNodeId);
	const auto& CurrentCookedData{ LoadedData.LocalizedDialogueEventCookedData.DialogueEventLanguageMap[LoadedData.LanguageRef] };
	const auto& CurrentAudioNodes{ CurrentCookedData.AudioNodes };

	if (CurrentAudioNodes.Num() == 0)
	{
		UE_LOG(LogAkAudio, Warning, TEXT("DialogueEvent::FetchAudioNodeObject %s %" PRIu32 " No cooked AudioNode in DialogueEvent getting Node Id %" PRIu32 "."),
			*GetName(), GetShortID(), AudioNodeId);
		return nullptr;
	}
	
	// Depending on the AudioNode exposure in metadata, there is either a single AudioNode 0 or all the AudioNodes. 
	const FWwiseAudioNodeCookedData* AudioNodeCookedData { nullptr };
	for (const auto& Candidate : CurrentAudioNodes)
	{
		if (Candidate.Value.AudioNodeId == AK_INVALID_UNIQUE_ID || Candidate.Value.AudioNodeId == AudioNodeId)
		{
			AudioNodeCookedData = &Candidate.Value;
			break;
		}
	}

	if (!AudioNodeCookedData)
	{
		UE_LOG(LogAkAudio, Warning, TEXT("DialogueEvent::FetchAudioNodeObject %s %" PRIu32 " No AudioNode %" PRIu32 " in DialogueEvent."),
			*GetName(), GetShortID(), AudioNodeId);
		return nullptr;
	}

	AudioNode = NewObject<UAkAudioNode>();
	if (!AudioNode)
	{
		UE_LOG(LogAkAudio, Error, TEXT("DialogueEvent::FetchAudioNodeObject %s %" PRIu32 " Could not instantiate new node %" PRIu32 "."),
			*GetName(), GetShortID(), AudioNodeId);

		return nullptr;
	}

	AudioNode->AudioNodeCookedData = *AudioNodeCookedData;
	AudioNode->AudioNodeCookedData.AudioNodeId = AudioNodeId;
	
	LoadedAudioNodes.Add(AudioNodeId, AudioNode);
	if (AudioNodeId != AK_INVALID_UNIQUE_ID)
	{
		KnownAudioNodes.Add(AudioNodeId, AudioNode);
	}
	return AudioNode;
}

UAkDynamicSequence* UAkDialogueEvent::PostDialogueEvent(UAkGameObject* GameObject, const TArray<UAkGroupValue*>& Arguments, bool bOrderedPath, bool bNewInstance, bool bPlayImmediately, const FAkDynamicSequenceTransition Transition)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkDialogueEvent::PostDialogueEvent"));
	if (!IsValid(GameObject))
	{
		UE_LOG(LogAkAudio, Warning, TEXT("DialogueEvent::PostDialogueEvent %s %" PRIu32 " No GameObject"),
			*GetName(), GetShortID());
		return nullptr;
	}

	auto* DynamicSequence{GameObject->OpenDynamicSequence(0, {}, Transition, false, bNewInstance)};
	if (UNLIKELY(!DynamicSequence))
	{
		UE_LOG(LogAkAudio, Warning, TEXT("DialogueEvent::PostDialogueEvent %s %" PRIu32 " to Go %s: Could not open Dynamic Sequence"),
			*GetName(), GetShortID(), *GameObject->GetName());
		return nullptr;
	}

	return DynamicSequence->PostDialogueEventInPlaylist(this, Arguments, bOrderedPath, bPlayImmediately) ? DynamicSequence : nullptr;
}

UAkDynamicSequence* UAkDialogueEvent::PostAmbientDialogueEvent(const TArray<UAkGroupValue*>& Arguments, bool bOrderedPath, bool bNewInstance, bool bPlayImmediately, const FAkDynamicSequenceTransition Transition)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkDialogueEvent::PostAmbientDialogueEvent"));
	auto* DynamicSequence{UAkDynamicSequenceBlueprintFunctionLibrary::OpenDynamicSequence(DUMMY_GAMEOBJ, GetName(), 0, {}, Transition, false, bNewInstance)};
	if (UNLIKELY(!DynamicSequence))
	{
		UE_LOG(LogAkAudio, Warning, TEXT("DialogueEvent::PostAmbientDialogueEvent %s %" PRIu32 " Could not open Dynamic Sequence"),
			*GetName(), GetShortID());
		return nullptr;
	}

	return DynamicSequence->PostDialogueEventInPlaylist(this, Arguments, bOrderedPath, bPlayImmediately) ? DynamicSequence : nullptr;
}

#if WITH_EDITORONLY_DATA
void UAkDialogueEvent::PlayPreview()
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkDialogueEvent::PlayPreview"));
	UE_LOG(LogAkAudio, Log, TEXT("DialogueEvent::PlayPreview %s %" PRIu32), *GetName(), GetShortID());
	if (PlayingPreview)
	{
		PlayingPreview->Stop();
	}
	PlayingPreview = PostAmbientDialogueEvent(PreviewGroupValues, bPreviewOrderedPath, true);
}

void UAkDialogueEvent::StopPreview()
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkDialogueEvent::StopPreview"));
	UE_LOG(LogAkAudio, Log, TEXT("DialogueEvent::StopPreview %s %" PRIu32), *GetName(), GetShortID());
	if (PlayingPreview)
	{
		PlayingPreview->Stop();
	}
	PlayingPreview = nullptr;
}

void UAkDialogueEvent::TogglePreview()
{
	if (PlayingPreview && PlayingPreview->IsPlaying())
	{
		StopPreview();
	}
	else
	{
		PlayPreview();
	}
}
#endif

void UAkDialogueEvent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}
	FWwisePackagedFileSerializationOptions Options(this);
#if !UE_SERVER
#if WITH_EDITORONLY_DATA 
	if (Ar.IsCooking() && Ar.IsSaving() && !Ar.CookingTarget()->IsServerOnly())
	{
		FWwiseLocalizedDialogueEventCookedData CookedDataToArchive;
		if (auto* ResourceCooker = IWwiseResourceCooker::GetForArchive(Ar))
		{
			ResourceCooker->PrepareCookedData(CookedDataToArchive, this, GetValidatedInfo(DialogueEventInfo));
			FillMetadata(ResourceCooker->GetProjectDatabase());
		}
		CookedDataToArchive.Serialize(Ar);
		CookedDataToArchive.SerializeBulkData(Ar, Options);
	}
#else
	DialogueEventCookedData.Serialize(Ar);
	DialogueEventCookedData.SerializeBulkData(Ar, Options);
#endif
#endif
}

#if WITH_EDITORONLY_DATA && UE_5_5_OR_LATER
UE_COOK_DEPENDENCY_FUNCTION(HashWwiseDialogueEventDependenciesForCook, UAkAudioType::HashDependenciesForCook);

#if UE_5_6_OR_LATER
void UAkDialogueEvent::OnCookEvent(UE::Cook::ECookEvent CookEvent, UE::Cook::FCookEventContext& Context)
{
	ON_SCOPE_EXIT
	{
		Super::OnCookEvent(CookEvent, Context);
	};
#else
void UAkDialogueEvent::PreSave(FObjectPreSaveContext Context)
{
	ON_SCOPE_EXIT
	{
		Super::PreSave(Context);
	};
#endif
	if (!Context.IsCooking())
	{
		return;
	}

	auto* ResourceCooker = IWwiseResourceCooker::GetForPlatform(Context.GetTargetPlatform());
	if (UNLIKELY(!ResourceCooker))
	{
		return;
	}

	FWwiseLocalizedDialogueEventCookedData CookedDataToArchive;
	ResourceCooker->PrepareCookedData(CookedDataToArchive, this, GetValidatedInfo(DialogueEventInfo));
	FillMetadata(ResourceCooker->GetProjectDatabase());

	FCbWriter Writer;
	Writer.BeginObject();
	CookedDataToArchive.GetPlatformCookDependencies(Context, Writer);
	Writer.EndObject();
	
	WwiseCookEventContext::AddLoadBuildDependency(Context,
		UE::Cook::FCookDependency::Function(
			UE_COOK_DEPENDENCY_FUNCTION_CALL(HashWwiseDialogueEventDependenciesForCook), Writer.Save()));
}
#endif

#if WITH_EDITORONLY_DATA
void UAkDialogueEvent::FillInfo()
{
	auto* ResourceCooker = IWwiseResourceCooker::GetDefault();
	if (UNLIKELY(!ResourceCooker))
	{
		UE_LOG(LogAkAudio, Error, TEXT("UAkDialogueEvent::FillInfo: ResourceCooker not initialized"));
		return;
	}

	auto ProjectDatabase = ResourceCooker->GetProjectDatabase();
	if (UNLIKELY(!ProjectDatabase))
	{
		UE_LOG(LogAkAudio, Error, TEXT("UAkDialogueEvent::FillInfo: ProjectDatabase not initialized"));
		return;
	}

	const WwiseRefDialogueEvent DialogueEventRef = WwiseDataStructureScopeLock(*ProjectDatabase).GetDialogueEvent(GetValidatedInfo(DialogueEventInfo));
	if (UNLIKELY(!DialogueEventRef.IsValid()))
	{
		UE_LOG(LogAkAudio, Log, TEXT("UAkDialogueEvent::FillInfo (%s): Cannot fill Asset Info - DialogueEvent is not loaded"), *GetName());
		return;
	}

	const WwiseMetadataDialogueEvent* DialogueEventMetadata = DialogueEventRef.GetDialogueEvent();
	if (DialogueEventMetadata->Name.IsEmpty() || !DialogueEventMetadata->GUID.IsValid() || DialogueEventMetadata->Id == AK_INVALID_UNIQUE_ID) 
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkDialogueEvent::FillInfo: Valid object not found in Project Database"));
		return;
	}

	int A, B, C, D;
	DialogueEventMetadata->GUID.GetGuidValues(A, B, C, D);
	DialogueEventInfo.WwiseGuid = FGuid(A, B, C, D);
	DialogueEventInfo.WwiseShortId = DialogueEventMetadata->Id;
	DialogueEventInfo.WwiseName = FName(*DialogueEventMetadata->Name);
}
#endif

void UAkDialogueEvent::LoadDialogueEventData()
{
	SCOPED_AKAUDIO_EVENT_2(TEXT("LoadDialogueEventData"));
	FWwiseResourceLoaderPtr ResourceLoader = FWwiseResourceLoader::Get();
	if (UNLIKELY(!ResourceLoader))
	{
		return;
	}

	UnloadDialogueEventData(false);
	
#if WITH_EDITORONLY_DATA
	if (!IWwiseProjectDatabaseModule::ShouldInitializeProjectDatabase())
	{
		return;
	}
	auto* ProjectDatabase = FWwiseProjectDatabase::Get();
	if (!ProjectDatabase || !ProjectDatabase->IsProjectDatabaseParsed())
	{
		UE_LOG(LogAkAudio, VeryVerbose, TEXT("UAkDialogueEvent::LoadDialogueEventData: Not loading '%s' because project database is not parsed."), *GetName())
		return;
	}
	auto* ResourceCooker = IWwiseResourceCooker::GetDefault();
	if (UNLIKELY(!ResourceCooker))
	{
		return;
	}
	
	if (!ResourceCooker->PrepareCookedData(DialogueEventCookedData, this, GetValidatedInfo(DialogueEventInfo)))
	{
		const auto* GlobalCallbacks = FWwiseGlobalCallbacks::Get();
		if( GlobalCallbacks && GlobalCallbacks->IsWwiseProfilerConnected())
		{
			UE_LOG(LogAkAudio, Verbose, TEXT("Could not fetch CookedData for DialogueEvent %s, but Wwise profiler is connected. Previous errors can be ignored."),
			*GetName());
		}
		else
		{
			return;
		}
	}
	
	FillMetadata(ResourceCooker->GetProjectDatabase());
#endif

	UE_LOG(LogAkAudio, Verbose, TEXT("%s - LoadDialogueEventData"), *GetName());
	
	const auto NewlyLoadedEvent = ResourceLoader->LoadDialogueEvent(DialogueEventCookedData);
	UE_CLOG(UNLIKELY(!NewlyLoadedEvent), LogAkAudio, Log,
		TEXT("UAkDialogueEvent::LoadDialogueEventData(%s): Could not LoadDialogueEvent"), *GetName());
	auto PreviouslyLoadedEvent = LoadedDialogueEvent.exchange(NewlyLoadedEvent);
	if (UNLIKELY(PreviouslyLoadedEvent))
	{
		ResourceLoader->UnloadDialogueEvent(MoveTemp(PreviouslyLoadedEvent));
	}

	// Load the AudioNodes that are "AlwaysLoad".
	{
		if (UNLIKELY(!LoadedDialogueEvent.load())) return;
		const auto& LoadedData{ LoadedDialogueEvent.load()->GetValue() };
		const auto& CurrentCookedData{ LoadedData.LocalizedDialogueEventCookedData.DialogueEventLanguageMap[LoadedData.LanguageRef] };
		const auto& CurrentAudioNodes{ CurrentCookedData.AudioNodes };

		for (const auto& AudioNodeTuple : CurrentAudioNodes)
		{
			const auto& AudioNode{ AudioNodeTuple.Value };
			
			if (AudioNode.AudioNodeLoading == EWwiseAudioNodeLoading::AlwaysLoad)
			{
				if (auto* Object { FetchAudioNodeObject(AudioNode.AudioNodeId) }; LIKELY(Object))
				{
					if (!Object->IsLoaded())
					{
						Object->LoadData();
					}
				}
			}
		}
	}
}

#if WITH_EDITOR
void UAkDialogueEvent::LoadDialogueEventDataForContentBrowserPreview()
{
	if(!bAutoLoad)
	{
		OnBeginPIEDelegateHandle = FEditorDelegates::BeginPIE.AddUObject(this, &UAkDialogueEvent::OnBeginPIE);
	}

	LoadDialogueEventData();
}

void UAkDialogueEvent::OnBeginPIE(const bool bIsSimulating)
{
	FEditorDelegates::BeginPIE.Remove(OnBeginPIEDelegateHandle);
	OnBeginPIEDelegateHandle.Reset();
	UnloadDialogueEventData(false);
}

#endif

void UAkDialogueEvent::BeginDestroy()
{
	Super::BeginDestroy();

#if WITH_EDITOR
	if (OnBeginPIEDelegateHandle.IsValid())
	{
		FEditorDelegates::BeginPIE.Remove(OnBeginPIEDelegateHandle);
		OnBeginPIEDelegateHandle.Reset();
	}
#endif
}

void UAkDialogueEvent::UnloadDialogueEventData(bool bAsync)
{
	auto PreviouslyLoadedEvent = LoadedDialogueEvent.exchange(nullptr);
	if (PreviouslyLoadedEvent)
	{
		FWwiseResourceLoaderPtr ResourceLoader = FWwiseResourceLoader::Get();
		if (UNLIKELY(!ResourceLoader))
		{
			return;
		}
		UE_LOG(LogAkAudio, Verbose, TEXT("%s - UnloadDialogueEventData"), *GetName());
		if (bAsync)
		{
			FWwiseLoadedDialogueEventPromise Promise;
			Promise.EmplaceValue(MoveTemp(PreviouslyLoadedEvent));
			ResourceUnload = ResourceLoader->UnloadDialogueEventAsync(Promise.GetFuture());
		}
		else
		{
			ResourceLoader->UnloadDialogueEvent(MoveTemp(PreviouslyLoadedEvent));
		}
	}
}

bool UAkDialogueEvent::IsDataFullyLoaded() const
{
	auto* GlobalCallbacks = FWwiseGlobalCallbacks::Get();
	if(GlobalCallbacks && GlobalCallbacks->IsWwiseProfilerConnected())
	{
		// Always assume data is loaded when profiler is connected, live edit takes care of everything
		return true;
	}

	auto CurrentLoadedEvent = LoadedDialogueEvent.load();
	if (!CurrentLoadedEvent)
	{
		return false;
	}

	return CurrentLoadedEvent->GetValue().LoadedData.IsLoaded();
}

bool UAkDialogueEvent::IsLoaded() const
{
	auto* GlobalCallbacks = FWwiseGlobalCallbacks::Get();
	if(GlobalCallbacks && GlobalCallbacks->IsWwiseProfilerConnected())
	{
		// Always assume data is loaded when profiler is connected, live edit takes care of everything
		return true;
	}
	return LoadedDialogueEvent.load() != nullptr;
}

#if WITH_EDITORONLY_DATA
bool UAkDialogueEvent::ObjectIsInSoundBanks()
{
	auto* ResourceCooker = IWwiseResourceCooker::GetDefault();
	if (UNLIKELY(!ResourceCooker))
	{
		UE_LOG(LogAkAudio, Error, TEXT("UAkAudioEvent::GetWwiseRef: ResourceCooker not initialized"));
		return false;
	}

	auto ProjectDatabase = ResourceCooker->GetProjectDatabase();
	if (UNLIKELY(!ProjectDatabase))
	{
		UE_LOG(LogAkAudio, Error, TEXT("UAkAudioEvent::GetWwiseRef: ProjectDatabase not initialized"));
		return false;
	}

	const WwiseRefDialogueEvent DialogueEventRef = WwiseDataStructureScopeLock(*ProjectDatabase).GetDialogueEvent(GetValidatedInfo(DialogueEventInfo));
	return DialogueEventRef.IsValid();
}
#endif

TArray<FWwiseExternalSourceCookedData> UAkDialogueEvent::GetAllExternalSources() const
{
	auto CurrentLoadedEvent = LoadedDialogueEvent.load();
	if (!CurrentLoadedEvent)
	{
		return {};
	}

	const auto& EventData = CurrentLoadedEvent->GetValue();
	if (!EventData.LoadedData.IsLoaded())
	{
		return {};
	}

	const auto& LoadedLanguage = EventData.LanguageRef;
	const FWwiseDialogueEventCookedData* CookedData = DialogueEventCookedData.DialogueEventLanguageMap.Find(LoadedLanguage);
	if (UNLIKELY(!CookedData))
	{
		return {};
	}

	TArray<FWwiseExternalSourceCookedData> Result;
	for (const auto& AudioNode : CookedData->AudioNodes)
	{
		Result.Append(AudioNode.Value.ExternalSources);
	}
	return Result;
}

#if WITH_EDITORONLY_DATA
void UAkDialogueEvent::CookAdditionalFilesOverride(const TCHAR* PackageFilename, const ITargetPlatform* TargetPlatform,
	TFunctionRef<void(const TCHAR* Filename, void* Data, int64 Size)> WriteAdditionalFile)
{
	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

	EnsureResourceCookerCreated(TargetPlatform);
	IWwiseResourceCooker* ResourceCooker = IWwiseResourceCooker::GetForPlatform(TargetPlatform);
	if (!ResourceCooker)
	{
		return;
	}
	ResourceCooker->CookDialogueEvent(GetValidatedInfo(DialogueEventInfo), this, PackageFilename, WriteAdditionalFile);
}
#endif
