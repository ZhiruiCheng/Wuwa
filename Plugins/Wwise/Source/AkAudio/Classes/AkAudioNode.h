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
#include "AkDynamicSequenceTransition.h"
#include "AkGameplayTypes.h"
#include "Wwise/Loaded/WwiseLoadedAudioNode.h"
#include "AkAudioNode.generated.h"

/**
 * Wwise Audio Node.
 *
 * This is typically provided by UAkDialogueEvent::Resolve for Dialogue Events, and are not loaded by traditional serialization.
 *
 * These can be enqueued into a UAkDynamicSequence through the UAkDynamicSequencePlaylist object.
 */
UCLASS(BlueprintType)
class AKAUDIO_API UAkAudioNode : public UAkAudioType
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "AkAudioNode")
	UAkDynamicSequence* PostAudioNode(
		UAkGameObject* GameObject,
		bool bNewInstance = false,
		bool bPlayImmediately = true,
		const FAkDynamicSequenceTransition Transition = FAkDynamicSequenceTransition()
	);

	UFUNCTION(BlueprintCallable, Category = "AkAudioNode")
	UAkDynamicSequence* PostAmbientAudioNode(
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

	virtual void Serialize(FArchive& Ar) override;
	virtual void BeginDestroy() override;

	virtual void LoadData()   override {LoadAudioNodeData();}
	virtual void UnloadData(bool bAsync = false) override {UnloadAudioNodeData(bAsync);}
	virtual AkUInt32 GetShortID() const override {return AudioNodeCookedData.AudioNodeId;}
	bool IsDataFullyLoaded() const;
	bool IsLoaded() const;

#if WITH_EDITOR
	// Allow for content browser preview to work, even if asset is not auto-loaded.
	// This method will load the content, and register to BeginPIE. When a PIE session
	// begins, data will be unloaded, allowing to replicate in-game behaviour more
	// closely.
	void LoadAudioNodeDataForContentBrowserPreview();

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
	virtual void CheckWwiseObjectInfo() override;
	virtual void CookAdditionalFilesOverride(const TCHAR* PackageFilename, const ITargetPlatform* TargetPlatform,
		TFunctionRef<void(const TCHAR* Filename, void* Data, int64 Size)> WriteAdditionalFile) override;
	virtual FWwiseObjectInfo* GetInfoMutable() override {return &AudioNodeInfo;}
	virtual FWwiseObjectInfo GetInfo() const override{return AudioNodeInfo;}
	virtual bool ObjectIsInSoundBanks() override;
#endif

	TArray<FWwiseExternalSourceCookedData> GetAllExternalSources() const;

	void LoadAudioNodeData();
	void UnloadAudioNodeData(bool bAsync);
	
protected:
	FWwiseLoadedAudioNodePtrAtomic LoadedAudioNode{nullptr};

public:
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "AkAudioNode")
	FWwiseObjectInfo AudioNodeInfo;

	UPROPERTY(EditAnywhere, Category = "AkAudioNode")
	TArray<int> HardCodedSoundBanks;

	UPROPERTY(EditAnywhere, Category = "AkAudioNode")
	TArray<int> HardCodedMedia;
#endif

	UPROPERTY(Transient, VisibleAnywhere, Category = "AkAudioNode")
	FWwiseAudioNodeCookedData AudioNodeCookedData;
};
