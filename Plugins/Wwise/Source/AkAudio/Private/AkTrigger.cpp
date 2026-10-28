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

#include "AkTrigger.h"

#include "Wwise/WwiseGlobalCallbacks.h"
#include "Wwise/Stats/AkAudio.h"

#if WITH_EDITORONLY_DATA
#include "Wwise/WwiseProjectDatabase.h"
#include "Wwise/WwiseResourceCooker.h"
#include "AkAudioDevice.h"
#endif

#if WITH_EDITORONLY_DATA && UE_5_5_OR_LATER
#include "UObject/ObjectSaveContext.h"
#include "Serialization/CompactBinaryWriter.h"
#endif

void UAkTrigger::Serialize(FArchive& Ar)
{
	SCOPED_AKAUDIO_EVENT_3(TEXT("UAkTrigger::Serialize"));
	Super::Serialize(Ar);

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		return;
	}

#if !UE_SERVER
#if WITH_EDITORONLY_DATA
 	if (Ar.IsCooking() && Ar.IsSaving() && !Ar.CookingTarget()->IsServerOnly())
	{
		FWwiseTriggerCookedData CookedDataToArchive;
		if (auto* ResourceCooker = IWwiseResourceCooker::GetForArchive(Ar))
		{
			ResourceCooker->PrepareCookedData(CookedDataToArchive, GetValidatedInfo(TriggerInfo));
		}
		CookedDataToArchive.Serialize(Ar);
	}
#else
	TriggerCookedData.Serialize(Ar);
#endif
#endif
}

#if WITH_EDITOR
void UAkTrigger::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	GetTriggerCookedData();
}
#endif 

#if WITH_EDITORONLY_DATA
void UAkTrigger::FillInfo()
{
	SCOPED_AKAUDIO_EVENT_3(TEXT("UAkTrigger::FillInfo"));
	auto* ResourceCooker = IWwiseResourceCooker::GetDefault();
	if (UNLIKELY(!ResourceCooker))
	{
		UE_LOG(LogAkAudio, Error, TEXT("UAkTrigger::FillInfo: ResourceCooker not initialized"));
		return;
	}

	auto ProjectDatabase = ResourceCooker->GetProjectDatabase();
	if (UNLIKELY(!ProjectDatabase))
	{
		UE_LOG(LogAkAudio, Error, TEXT("UAkTrigger::FillInfo: ProjectDatabase not initialized"));
		return;
	}

	FWwiseObjectInfo* AudioTypeInfo = &TriggerInfo;
	WwiseRefTrigger TriggerRef = WwiseDataStructureScopeLock(*ProjectDatabase).GetTrigger(
		GetValidatedInfo(TriggerInfo));

	if (TriggerRef.TriggerName()->IsEmpty() || !TriggerRef.TriggerGuid().IsValid() || TriggerRef.TriggerId() == AK_INVALID_UNIQUE_ID)
	{
		UE_LOG(LogAkAudio, Warning, TEXT("UAkTrigger::FillInfo: Valid object not found in Project Database"));
		return;
	}

	int A, B, C, D;
	TriggerRef.TriggerGuid().GetGuidValues(A, B, C, D);
	AudioTypeInfo->WwiseName = FName(**TriggerRef.TriggerName());
	AudioTypeInfo->WwiseGuid = FGuid(A, B, C, D);
	AudioTypeInfo->WwiseShortId = TriggerRef.TriggerId();
}

bool UAkTrigger::ObjectIsInSoundBanks()
{
	auto* ResourceCooker = IWwiseResourceCooker::GetDefault();
	if (UNLIKELY(!ResourceCooker))
	{
		UE_LOG(LogAkAudio, Error, TEXT("UAkTrigger::GetWwiseRef: ResourceCooker not initialized"));
		return false;
	}

	auto ProjectDatabase = ResourceCooker->GetProjectDatabase();
	if (UNLIKELY(!ProjectDatabase))
	{
		UE_LOG(LogAkAudio, Error, TEXT("UAkTrigger::GetWwiseRef: ProjectDatabase not initialized"));
		return false;
	}

	FWwiseObjectInfo* AudioTypeInfo = &TriggerInfo;
	WwiseRefTrigger TriggerRef = WwiseDataStructureScopeLock(*ProjectDatabase).GetTrigger(
		GetValidatedInfo(TriggerInfo));

	return TriggerRef.IsValid();
}

void UAkTrigger::GetTriggerCookedData()
{
	SCOPED_AKAUDIO_EVENT_2(TEXT("GetTriggerCookedData"));
	if (!IWwiseProjectDatabaseModule::ShouldInitializeProjectDatabase())
	{
		return;
	}
	auto* ProjectDatabase = FWwiseProjectDatabase::Get();
	if (!ProjectDatabase || !ProjectDatabase->IsProjectDatabaseParsed())
	{
		UE_LOG(LogAkAudio, VeryVerbose, TEXT("UAkTrigger::GetTriggerCookedData: Not loading '%s' because project database is not parsed."), *GetName())
		return;
	}
	auto* ResourceCooker = IWwiseResourceCooker::GetDefault();
	if (UNLIKELY(!ResourceCooker))
	{
		return;
	}
	if(!ResourceCooker->PrepareCookedData(TriggerCookedData, GetValidatedInfo(TriggerInfo)))
	{
		const auto* GlobalCallbacks = FWwiseGlobalCallbacks::Get();
		if( GlobalCallbacks && GlobalCallbacks->IsWwiseProfilerConnected())
		{
			UE_LOG(LogAkAudio, Verbose, TEXT("Could not fetch CookedData for Trigger %s, but Wwise profiler is connected. Previous errors can be ignored."),
			*GetName());
		}
		else
		{
			return;
		}
	}
}
#endif

#if WITH_EDITORONLY_DATA && UE_5_5_OR_LATER
UE_COOK_DEPENDENCY_FUNCTION(HashWwiseTriggerDependenciesForCook, UAkAudioType::HashDependenciesForCook);

#if UE_5_6_OR_LATER
void UAkTrigger::OnCookEvent(UE::Cook::ECookEvent CookEvent, UE::Cook::FCookEventContext& Context)
{
	ON_SCOPE_EXIT
	{
		Super::OnCookEvent(CookEvent, Context);
	};
#else
void UAkTrigger::PreSave(FObjectPreSaveContext Context)
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

	FWwiseTriggerCookedData CookedDataToArchive;
	ResourceCooker->PrepareCookedData(CookedDataToArchive, GetValidatedInfo(TriggerInfo));
	FillMetadata(ResourceCooker->GetProjectDatabase());

	FCbWriter Writer;
	Writer.BeginObject();
	CookedDataToArchive.GetPlatformCookDependencies(Context, Writer);
	Writer.EndObject();
	
	WwiseCookEventContext::AddLoadBuildDependency(Context,
		UE::Cook::FCookDependency::Function(
			UE_COOK_DEPENDENCY_FUNCTION_CALL(HashWwiseTriggerDependenciesForCook), Writer.Save()));
}
#endif
