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
#include "AkAudioType.h"
#include "AkDynamicSequence.h"
#include "Wwise/CookedData/WwiseLocalizedDialogueEventCookedData.h"
#include "Wwise/Info/WwiseDialogueEventInfo.h"
#include "Wwise/Loaded/WwiseLoadedDialogueEvent.h"
#include "AkDialogueEvent.generated.h"

class UAkDynamicSequence;
class UAkGameObject;
class UAkGroupValue;
class UAkAudioNode;

DECLARE_DELEGATE_RetVal_ThreeParams(bool, FAkDialogueEventCandidateCallback, int32 /* Event */, int32 /* Candidate */, UAkDialogueEvent* /* DialogueEvent */);

/**
 * Dialogue Event (Dynamic Dialogue).
 *
 * Allows dialogue argument paths to be resolved to an UAkAudioNode.
 */
UCLASS(BlueprintType)
class AKAUDIO_API UAkDialogueEvent : public UAkAudioType
{
	GENERATED_BODY()

public:
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "AkDialogueEvent")
	FWwiseDialogueEventInfo DialogueEventInfo;
#endif

	UPROPERTY(Transient, VisibleAnywhere, Category = "AkDialogueEvent")
	FWwiseLocalizedDialogueEventCookedData DialogueEventCookedData;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "AkDialogueEvent")
	TArray<TObjectPtr<UAkGroupValue>> PreviewGroupValues;

	UPROPERTY(EditAnywhere, Category = "AkDialogueEvent")
	bool bPreviewOrderedPath { false };
#endif
	
	/// Resolve a dialogue event into an UAkAudioNode based on the specified arguments.
	///
	/// Any number of arguments can be passed, in any order. Arguments cannot be overridden. Missing arguments are considered fallback.
	///
	/// It's possible to ask ResolveArguments in a precise order instead. This is recommended if multiple identical GroupValues are used.
	/// In this case, use ResolveOrderedArguments.
	UFUNCTION(BlueprintCallable, Category = "AkDialogueEvent")
	UAkAudioNode* ResolveArguments(
		TArray<UAkGroupValue*> Arguments);

	/// Resolve a dialogue event into an UAkAudioNode based on the specified arguments.
	///
	/// Exactly the proper list of arguments must be used, with the proper typing. If a value is fallthrough, it must have no
	/// object applied in the arguments.
	///
	/// It's possible to provide any arguments through the ResolveArguments operation. This is typically the simpler way to provide arguments.
	UFUNCTION(BlueprintCallable, Category = "AkDialogueEvent")
	UAkAudioNode* ResolveOrderedArguments(
		TArray<UAkGroupValue*> Arguments);

	
	/// Resolve a dialogue event into an UAkAudioNode based on the specified arguments.
	UAkAudioNode* Resolve(
		TArray<UAkGroupValue*> Arguments,
		bool bOrderedPath,
		FAkDialogueEventCandidateCallback CandidateCallback);
	
	/// Retrieves the AudioNode's UAkAudioNode from its id.
	UFUNCTION(BlueprintCallable, Category = "AkDialogueEvent")
	UAkAudioNode* FetchAudioNodeObject(int32 AudioNodeId);

	UFUNCTION(BlueprintCallable, Category = "AkDialogueEvent")
	UAkDynamicSequence* PostDialogueEvent(
		UAkGameObject* GameObject,
		const TArray<UAkGroupValue*>& Arguments,
		bool bOrderedPath = false,
		bool bNewInstance = false,
		bool bPlayImmediately = true,
		const FAkDynamicSequenceTransition Transition = FAkDynamicSequenceTransition()
	);

	UFUNCTION(BlueprintCallable, Category = "AkDialogueEvent")
	UAkDynamicSequence* PostAmbientDialogueEvent(
		const TArray<UAkGroupValue*>& Arguments,
		bool bOrderedPath = false,
		bool bNewInstance = false,
		bool bPlayImmediately = true,
		const FAkDynamicSequenceTransition Transition = FAkDynamicSequenceTransition()
	);

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	TObjectPtr<UAkDynamicSequence> PlayingPreview;
	void PlayPreview();
	void StopPreview();
	void TogglePreview();
#endif

public:
	virtual void Serialize(FArchive& Ar) override;
	virtual void BeginDestroy() override;

	virtual void LoadData() override {LoadDialogueEventData();}
	virtual void UnloadData(bool bAsync = false) override {UnloadDialogueEventData(bAsync);}
	virtual AkUInt32 GetShortID() const override {return DialogueEventCookedData.DialogueEventId;}
	bool IsDataFullyLoaded() const;
	bool IsLoaded() const;

#if WITH_EDITOR
	// Allow for content browser preview to work, even if asset is not auto-loaded.
	// This method will load the content, and register to BeginPIE. When a PIE session
	// begins, data will be unloaded, allowing to replicate in-game behaviour more
	// closely.
	void LoadDialogueEventDataForContentBrowserPreview();

private:
	void OnBeginPIE(const bool bIsSimulating);
	FDelegateHandle OnBeginPIEDelegateHandle;
#endif // WITH_EDITOR

#if WITH_EDITORONLY_DATA && UE_5_5_OR_LATER
public:
#if UE_5_6_OR_LATER
	virtual void OnCookEvent(UE::Cook::ECookEvent CookEvent, UE::Cook::FCookEventContext& Context) override;
#else
	virtual void PreSave(FObjectPreSaveContext Context) override;
#endif
#endif
	
#if WITH_EDITORONLY_DATA
public:
	virtual void FillInfo() override;
	virtual void CookAdditionalFilesOverride(const TCHAR* PackageFilename, const ITargetPlatform* TargetPlatform,
		TFunctionRef<void(const TCHAR* Filename, void* Data, int64 Size)> WriteAdditionalFile) override;
	virtual FWwiseObjectInfo* GetInfoMutable() override {return &DialogueEventInfo;}
	virtual FWwiseObjectInfo GetInfo() const override{return DialogueEventInfo;}
	virtual bool ObjectIsInSoundBanks() override;
#endif

	TArray<FWwiseExternalSourceCookedData> GetAllExternalSources() const;

	void LoadDialogueEventData();
	void UnloadDialogueEventData(bool bAsync);
	
protected:
	FWwiseLoadedDialogueEventPtrAtomic LoadedDialogueEvent{nullptr};

	UPROPERTY(Transient)
	TMap<int32, TObjectPtr<UAkAudioNode>> LoadedAudioNodes;

	static TMap<int32, TWeakObjectPtr<UAkAudioNode>> KnownAudioNodes;
};
