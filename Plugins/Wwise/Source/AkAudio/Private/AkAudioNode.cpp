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

#include "AkAudioNode.h"

#include "AkAudioDevice.h"
#include "AkDynamicSequenceBlueprintFunctionLibrary.h"
#include "AkDynamicSequencePlaylist.h"
#include "AkGameObject.h"
#include "Wwise/WwiseResourceLoader.h"
#include "Audio/AudioDebug.h"
#include "Wwise/Stats/AkAudio.h"

#include <inttypes.h>

#include "UObject/FastReferenceCollector.h"
#include "Wwise/WwiseGlobalCallbacks.h"

#if WITH_EDITORONLY_DATA && UE_5_5_OR_LATER
#include "UObject/ObjectSaveContext.h"
#include "Serialization/CompactBinaryWriter.h"
#endif

#if WITH_EDITORONLY_DATA
#include "Wwise/WwiseProjectDatabase.h"
#include "Wwise/WwiseResourceCooker.h"
#endif

UAkDynamicSequence* UAkAudioNode::PostAudioNode(UAkGameObject* GameObject, bool bNewInstance, bool bPlayImmediately, const FAkDynamicSequenceTransition Transition)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkAudioNode::PostAudioNode"));
	if (!IsValid(GameObject))
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkAudioNode::PostAudioNode %s %" PRIu32 " No GameObject"),
			*GetName(), GetShortID());
		return nullptr;
	}

	auto* DynamicSequence{GameObject->OpenDynamicSequence(0, {}, Transition, false, bNewInstance)};
	if (UNLIKELY(!DynamicSequence))
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkAudioNode::PostAudioNode %s %" PRIu32 " to Go %s: Could not open Dynamic Sequence"),
			*GetName(), GetShortID(), *GameObject->GetName());
		return nullptr;
	}

	return DynamicSequence->PostAudioNode(this, bPlayImmediately) ? DynamicSequence : nullptr;
}

UAkDynamicSequence* UAkAudioNode::PostAmbientAudioNode(bool bNewInstance, bool bPlayImmediately, const FAkDynamicSequenceTransition Transition)
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkAudioNode::PostAmbientAudioNode"));
	auto* DynamicSequence{UAkDynamicSequenceBlueprintFunctionLibrary::OpenDynamicSequence(DUMMY_GAMEOBJ, GetName(), 0, {}, Transition, false, bNewInstance)};
	if (UNLIKELY(!DynamicSequence))
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkAudioNode::PostAmbientAudioNode %s %" PRIu32 " Could not open Dynamic Sequence"),
			*GetName(), GetShortID());
		return nullptr;
	}

	return DynamicSequence->PostAudioNode(this, bPlayImmediately) ? DynamicSequence : nullptr;
}

#if WITH_EDITORONLY_DATA
void UAkAudioNode::PlayPreview()
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkAudioNode::PlayPreview"));
	UE_LOG(LogAkAudio, Log, TEXT("UAkAudioNode::PlayPreview %s %" PRIu32), *GetName(), GetShortID());
	if (PlayingPreview)
	{
		PlayingPreview->Stop();
	}
	PlayingPreview = PostAmbientAudioNode(true);
}

void UAkAudioNode::StopPreview()
{
	SCOPED_AKAUDIO_EVENT(TEXT("UAkAudioNode::StopPreview"));
	UE_LOG(LogAkAudio, Log, TEXT("UAkAudioNode::StopPreview %s %" PRIu32), *GetName(), GetShortID());
	if (PlayingPreview)
	{
		PlayingPreview->Stop();
	}
	PlayingPreview = nullptr;
}

void UAkAudioNode::TogglePreview()
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

void UAkAudioNode::Serialize(FArchive& Ar)
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
		FWwiseAudioNodeCookedData CookedDataToArchive;
		if (auto* ResourceCooker = IWwiseResourceCooker::GetForArchive(Ar))
		{
			ResourceCooker->PrepareCookedData(CookedDataToArchive, this, GetValidatedInfo(AudioNodeInfo));

			for (const auto& Media: HardCodedMedia)
			{
				FWwiseMediaCookedData CookedData;
				FWwiseObjectInfo Info;
				Info.WwiseShortId = Media;
				if (ResourceCooker->PrepareCookedData(CookedData, this, Info))
				{
					CookedDataToArchive.Media.Add(CookedData);
				}
			}
			for (const auto& SoundBank: HardCodedSoundBanks)
			{
				FWwiseLocalizedSoundBankCookedData CookedData;
				FWwiseObjectInfo Info;
				Info.WwiseShortId = SoundBank;
				if (ResourceCooker->PrepareCookedData(CookedData, this, Info))
				{
					for (const auto& Language: CookedData.SoundBankLanguageMap)
					{
						CookedDataToArchive.SoundBanks.Add(Language.Value);
					}
				}
			}

			FillMetadata(ResourceCooker->GetProjectDatabase());
		}
		CookedDataToArchive.Serialize(Ar);
		CookedDataToArchive.SerializeBulkData(Ar, Options);
	}
#else
	AudioNodeCookedData.Serialize(Ar);
	AudioNodeCookedData.SerializeBulkData(Ar, Options);
#endif
#endif
}

#if WITH_EDITORONLY_DATA && UE_5_5_OR_LATER
UE_COOK_DEPENDENCY_FUNCTION(HashWwiseAudioNodeDependenciesForCook, UAkAudioType::HashDependenciesForCook);

#if UE_5_6_OR_LATER
void UAkAudioNode::OnCookEvent(UE::Cook::ECookEvent CookEvent, UE::Cook::FCookEventContext& Context)
{
	ON_SCOPE_EXIT
	{
		Super::OnCookEvent(CookEvent, Context);
	};
#else
void UAkAudioNode::PreSave(FObjectPreSaveContext Context)
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

	FWwiseAudioNodeCookedData CookedDataToArchive;
	ResourceCooker->PrepareCookedData(CookedDataToArchive, this, GetValidatedInfo(AudioNodeInfo));
	FillMetadata(ResourceCooker->GetProjectDatabase());

	FCbWriter Writer;
	Writer.BeginObject();
	CookedDataToArchive.GetPlatformCookDependencies(Context, Writer);
	Writer.EndObject();
	
	WwiseCookEventContext::AddLoadBuildDependency(Context,
		UE::Cook::FCookDependency::Function(
			UE_COOK_DEPENDENCY_FUNCTION_CALL(HashWwiseAudioNodeDependenciesForCook), Writer.Save()));
}
#endif


#if WITH_EDITORONLY_DATA
void UAkAudioNode::FillInfo()
{
}

void UAkAudioNode::CheckWwiseObjectInfo()
{
}
#endif

void UAkAudioNode::LoadAudioNodeData()
{
	SCOPED_AKAUDIO_EVENT_2(TEXT("LoadAudioNodeData"));
	FWwiseResourceLoaderPtr ResourceLoader = FWwiseResourceLoader::Get();
	if (UNLIKELY(!ResourceLoader))
	{
		return;
	}

	UnloadAudioNodeData(false);
	
#if WITH_EDITORONLY_DATA
	if (!IWwiseProjectDatabaseModule::ShouldInitializeProjectDatabase())
	{
		return;
	}
	auto* ProjectDatabase = FWwiseProjectDatabase::Get();
	if (!ProjectDatabase || !ProjectDatabase->IsProjectDatabaseParsed())
	{
		UE_LOG(LogAkAudio, VeryVerbose, TEXT("UAkAudioNode::LoadAudioNodeData: Not loading '%s' because project database is not parsed."), *GetName())
		return;
	}
	auto* ResourceCooker = IWwiseResourceCooker::GetDefault();
	if (UNLIKELY(!ResourceCooker))
	{
		return;
	}
	
	if (!ResourceCooker->PrepareCookedData(AudioNodeCookedData, this, GetValidatedInfo(AudioNodeInfo)))
	{
		const auto* GlobalCallbacks = FWwiseGlobalCallbacks::Get();
		if( GlobalCallbacks && GlobalCallbacks->IsWwiseProfilerConnected())
		{
			UE_LOG(LogAkAudio, Verbose, TEXT("Could not fetch CookedData for AudioNode %s, but Wwise profiler is connected. Previous errors can be ignored."),
			*GetName());
		}
		else
		{
			return;
		}
	}

	for (const auto& Media: HardCodedMedia)
	{
		FWwiseMediaCookedData CookedData;
		FWwiseObjectInfo Info;
		Info.WwiseShortId = Media;
		if (ResourceCooker->PrepareCookedData(CookedData, this, Info))
		{
			AudioNodeCookedData.Media.Add(CookedData);
		}
	}
	for (const auto& SoundBank: HardCodedSoundBanks)
	{
		FWwiseLocalizedSoundBankCookedData CookedData;
		FWwiseObjectInfo Info;
		Info.WwiseShortId = SoundBank;
		if (ResourceCooker->PrepareCookedData(CookedData, this, Info))
		{
			for (const auto& Language: CookedData.SoundBankLanguageMap)
			{
				AudioNodeCookedData.SoundBanks.Add(Language.Value);
			}
		}
	}
	
	FillMetadata(ResourceCooker->GetProjectDatabase());
#endif

	UE_LOG(LogAkAudio, Verbose, TEXT("%s - LoadAudioNodeData"), *GetName());
	
	const auto NewlyLoadedAudioNode = ResourceLoader->LoadAudioNode(AudioNodeCookedData);
	UE_CLOG(UNLIKELY(!NewlyLoadedAudioNode), LogAkAudio, Log,
		TEXT("UAkAudioNode::LoadAudioNodeData(%s): Could not Load AudioNode"), *GetName());
	auto PreviouslyLoadedAudioNode = LoadedAudioNode.exchange(NewlyLoadedAudioNode);
	if (UNLIKELY(PreviouslyLoadedAudioNode))
	{
		ResourceLoader->UnloadAudioNode(MoveTemp(PreviouslyLoadedAudioNode));
	}
}

#if WITH_EDITOR
void UAkAudioNode::LoadAudioNodeDataForContentBrowserPreview()
{
	if(!bAutoLoad)
	{
		OnBeginPIEDelegateHandle = FEditorDelegates::BeginPIE.AddUObject(this, &UAkAudioNode::OnBeginPIE);
	}

	LoadAudioNodeData();
}

void UAkAudioNode::OnBeginPIE(const bool bIsSimulating)
{
	FEditorDelegates::BeginPIE.Remove(OnBeginPIEDelegateHandle);
	OnBeginPIEDelegateHandle.Reset();
	UnloadAudioNodeData(false);
}

#endif

void UAkAudioNode::BeginDestroy()
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

void UAkAudioNode::UnloadAudioNodeData(bool bAsync)
{
	auto PreviouslyLoadedAudioNode = LoadedAudioNode.exchange(nullptr);
	if (PreviouslyLoadedAudioNode)
	{
		FWwiseResourceLoaderPtr ResourceLoader = FWwiseResourceLoader::Get();
		if (UNLIKELY(!ResourceLoader))
		{
			return;
		}
		UE_LOG(LogAkAudio, Verbose, TEXT("%s - UnloadAudioNodeData"), *GetName());
		if (bAsync)
		{
			FWwiseLoadedAudioNodePromise Promise;
			Promise.EmplaceValue(MoveTemp(PreviouslyLoadedAudioNode));
			ResourceUnload = ResourceLoader->UnloadAudioNodeAsync(Promise.GetFuture());
		}
		else
		{
			ResourceLoader->UnloadAudioNode(MoveTemp(PreviouslyLoadedAudioNode));
		}
	}
}

bool UAkAudioNode::IsDataFullyLoaded() const
{
	auto* GlobalCallbacks = FWwiseGlobalCallbacks::Get();
	if(GlobalCallbacks && GlobalCallbacks->IsWwiseProfilerConnected())
	{
		// Always assume data is loaded when profiler is connected, live edit takes care of everything
		return true;
	}

	auto CurrentLoadedAudioNode = LoadedAudioNode.load();
	if (!CurrentLoadedAudioNode)
	{
		return false;
	}

	return CurrentLoadedAudioNode->GetValue().LoadedData.IsLoaded();
}

bool UAkAudioNode::IsLoaded() const
{
	auto* GlobalCallbacks = FWwiseGlobalCallbacks::Get();
	if(GlobalCallbacks && GlobalCallbacks->IsWwiseProfilerConnected())
	{
		// Always assume data is loaded when profiler is connected, live edit takes care of everything
		return true;
	}
	return LoadedAudioNode.load() != nullptr;
}

#if WITH_EDITORONLY_DATA
bool UAkAudioNode::ObjectIsInSoundBanks()
{
	return false;
}
#endif

TArray<FWwiseExternalSourceCookedData> UAkAudioNode::GetAllExternalSources() const
{
	auto CurrentLoadedAudioNode = LoadedAudioNode.load();
	if (!CurrentLoadedAudioNode)
	{
		return {};
	}

	const auto& EventData = CurrentLoadedAudioNode->GetValue();
	if (!EventData.LoadedData.IsLoaded())
	{
		return {};
	}

	const FWwiseAudioNodeCookedData* CookedData = &AudioNodeCookedData;

	TArray<FWwiseExternalSourceCookedData> Result;
	Result.Append(CookedData->ExternalSources);
	return Result;
}

#if WITH_EDITORONLY_DATA
void UAkAudioNode::CookAdditionalFilesOverride(const TCHAR* PackageFilename, const ITargetPlatform* TargetPlatform,
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
	ResourceCooker->CookAudioNode(GetValidatedInfo(AudioNodeInfo), this, PackageFilename, WriteAdditionalFile);
}
#endif
