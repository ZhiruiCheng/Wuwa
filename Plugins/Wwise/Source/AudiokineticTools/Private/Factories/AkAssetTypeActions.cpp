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

#include "AkAssetTypeActions.h"
#include "AkAudioDevice.h"
#include "AkAudioEvent.h"
#include "AkAudioNode.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "IAssetTools.h"
#include "Interfaces/IMainFrameModule.h"
#include "Misc/ScopeLock.h"
#include "Toolkits/SimpleAssetEditor.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "AkAssetTypeActions"

namespace FAkAssetTypeActions_Helpers
{
	FCriticalSection CriticalSection;
	TMap<FString, AkPlayingID> PlayingAkEvents;

#if WWISE_2025_1_OR_LATER
	void AkEventPreviewCallback(AkCallbackType in_eType, AkEventCallbackInfo * EventInfo, void * in_pCallbackInfo, void* in_pCookie)
	{
#else
	void AkEventPreviewCallback(AkCallbackType in_eType, AkCallbackInfo* in_pCallbackInfo)
	{
		auto EventInfo = static_cast<AkEventCallbackInfo*>(in_pCallbackInfo);
#endif
		if (!EventInfo)
			return;

		FScopeLock Lock(&CriticalSection);
		for (auto& PlayingEvent : PlayingAkEvents)
		{
			if (PlayingEvent.Value == EventInfo->playingID)
			{
				PlayingAkEvents.Remove(PlayingEvent.Key);
				return;
			}
		}
	}

	void PlayAudioNodes(const TArray<TWeakObjectPtr<UAkAudioNode>>& InObjects)
	{
		auto AudioDevice = FAkAudioDevice::Get();
		if (!AudioDevice)
			return;

		for (auto& Obj : InObjects)
		{
			auto Node = Obj.Get();
			if (!Node)
				continue;

			if(!Node->bAutoLoad)
			{
				Node->LoadAudioNodeDataForContentBrowserPreview();
			}
			Node->TogglePreview();
		}
	}

	void PlayDialogueEvents(const TArray<TWeakObjectPtr<UAkDialogueEvent>>& InObjects)
	{
		auto AudioDevice = FAkAudioDevice::Get();
		if (!AudioDevice)
			return;

		for (auto& Obj : InObjects)
		{
			auto Event = Obj.Get();
			if (!Event)
				continue;

			Event->TogglePreview();
		}
	}

	template<bool PlayOne>
	void PlayEvents(const TArray<TWeakObjectPtr<UAkAudioEvent>>& InObjects)
	{
		auto AudioDevice = FAkAudioDevice::Get();
		if (!AudioDevice)
			return;

		for (auto& Obj : InObjects)
		{
			auto Event = Obj.Get();
			if (!Event)
				continue;

			AkPlayingID* foundID;
			{
				FScopeLock Lock(&CriticalSection);
				foundID = PlayingAkEvents.Find(Event->GetName());
			}

			if (foundID)
			{
				AudioDevice->StopPlayingID(*foundID);
			}
			else
			{
				if(!Event->bAutoLoad)
				{
					Event->LoadEventDataForContentBrowserPreview();
				}
				const auto CurrentPlayingID = Event->PostAmbient(nullptr, &AkEventPreviewCallback, nullptr, AK_EndOfEvent, nullptr, EAkAudioContext::EditorAudio);
				if (CurrentPlayingID != AK_INVALID_PLAYING_ID)
				{
					FScopeLock Lock(&CriticalSection);
					PlayingAkEvents.FindOrAdd(Event->GetName()) = CurrentPlayingID;
				}
			}

			if (PlayOne)
				break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FAssetTypeActions_AkAcousticTexture

void FAssetTypeActions_AkAcousticTexture::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, EditWithinLevelEditor, InObjects);
}


//////////////////////////////////////////////////////////////////////////
// FAssetTypeActions_AkAudioEvent

void FAssetTypeActions_AkAudioEvent::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	auto Events = GetTypedWeakObjectPtrs<UAkAudioEvent>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AkAudioEvent_PlayEvent", "Play Event"),
		LOCTEXT("AkAudioEvent_PlayEventTooltip", "Plays the selected event."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_AkAudioEvent::PlayEvent, Events),
			FCanExecuteAction()
		)
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AkAudioEvent_StopEvent", "Stop Event"),
		LOCTEXT("AkAudioEvent_StopEventTooltip", "Stops the selected event."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_AkAudioEvent::StopEvent, Events),
			FCanExecuteAction()
		)
	);
}

void FAssetTypeActions_AkAudioEvent::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, EditWithinLevelEditor, InObjects);
}

bool FAssetTypeActions_AkAudioEvent::AssetsActivatedOverride(const TArray<UObject*>& InObjects, EAssetTypeActivationMethod::Type ActivationType)
{
	if (ActivationType == EAssetTypeActivationMethod::DoubleClicked || ActivationType == EAssetTypeActivationMethod::Opened)
	{
		if (InObjects.Num() == 1)
		{
			return GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(InObjects[0]);
		}
		else if (InObjects.Num() > 1)
		{
			return GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAssets(InObjects);
		}
	}
	else if (ActivationType == EAssetTypeActivationMethod::Previewed)
	{
		TArray<UObject*> NonNullObjects;
		for (auto& Obj : InObjects)
		{
			if (Obj != nullptr)
			{
				NonNullObjects.Add(Obj);
			}
		}
		if (!NonNullObjects.IsEmpty())
		{
			auto Events = GetTypedWeakObjectPtrs<UAkAudioEvent>(NonNullObjects);
			FAkAssetTypeActions_Helpers::PlayEvents<true>(Events);
		}
	}

	return true;
}

void FAssetTypeActions_AkAudioEvent::PlayEvent(TArray<TWeakObjectPtr<UAkAudioEvent>> Objects)
{
	FAkAssetTypeActions_Helpers::PlayEvents<false>(Objects);
}

void FAssetTypeActions_AkAudioEvent::StopEvent(TArray<TWeakObjectPtr<UAkAudioEvent>> Objects)
{
	FAkAudioDevice * AudioDevice = FAkAudioDevice::Get();
	if (AudioDevice)
	{
		AudioDevice->StopGameObject(nullptr);
	}
}

//////////////////////////////////////////////////////////////////////////
// FAssetTypeActions_AkAudioNode

void FAssetTypeActions_AkAudioNode::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	auto Nodes = GetTypedWeakObjectPtrs<UAkAudioNode>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AkAudioNode_PlayNode", "Play Audio Node"),
		LOCTEXT("AkAudioNode_PlayNodeTooltip", "Plays the selected Audio Node."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_AkAudioNode::PlayNode, Nodes),
			FCanExecuteAction()
		)
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AkAudioNode_StopNode", "Stop Audio Node"),
		LOCTEXT("AkAudioNode_StopNodeTooltip", "Stops the selected Audio Node."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_AkAudioNode::StopNode, Nodes),
			FCanExecuteAction()
		)
	);
}

void FAssetTypeActions_AkAudioNode::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, EditWithinLevelEditor, InObjects);
}

bool FAssetTypeActions_AkAudioNode::AssetsActivatedOverride(const TArray<UObject*>& InObjects, EAssetTypeActivationMethod::Type ActivationType)
{
	if (ActivationType == EAssetTypeActivationMethod::DoubleClicked || ActivationType == EAssetTypeActivationMethod::Opened)
	{
		if (InObjects.Num() == 1)
		{
			return GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(InObjects[0]);
		}
		else if (InObjects.Num() > 1)
		{
			return GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAssets(InObjects);
		}
	}
	else if (ActivationType == EAssetTypeActivationMethod::Previewed)
	{
		TArray<UObject*> NonNullObjects;
		for (auto& Obj : InObjects)
		{
			if (Obj != nullptr)
			{
				NonNullObjects.Add(Obj);
			}
		}
		if (!NonNullObjects.IsEmpty())
		{
			auto Nodes = GetTypedWeakObjectPtrs<UAkAudioNode>(InObjects);
			FAkAssetTypeActions_Helpers::PlayAudioNodes(Nodes);
		}
	}
	return true;
}

void FAssetTypeActions_AkAudioNode::PlayNode(TArray<TWeakObjectPtr<UAkAudioNode>> Objects)
{
	for (const auto& Object : Objects)
	{
		if (auto Node = Object.Get())
		{
			Node->PlayPreview();
		}
	}
}

void FAssetTypeActions_AkAudioNode::StopNode(TArray<TWeakObjectPtr<UAkAudioNode>> Objects)
{
	for (const auto& Object : Objects)
	{
		if (auto Node = Object.Get())
		{
			Node->StopPreview();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FAssetTypeActions_AkDialogueEvent

void FAssetTypeActions_AkDialogueEvent::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	auto Events = GetTypedWeakObjectPtrs<UAkDialogueEvent>(InObjects);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AkDialogueEvent_PlayEvent", "Play Dialogue Event"),
		LOCTEXT("AkDialogueEvent_PlayEventTooltip", "Plays the selected dialogue event using its preview arguments."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_AkDialogueEvent::PlayEvent, Events),
			FCanExecuteAction()
		)
	);

	MenuBuilder.AddMenuEntry(
		LOCTEXT("AkDialogueEvent_StopEvent", "Stop Dialogue Event"),
		LOCTEXT("AkDialogueEvent_StopEventTooltip", "Stops the selected dialogue event."),
		FSlateIcon(),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_AkDialogueEvent::StopEvent, Events),
			FCanExecuteAction()
		)
	);
}

void FAssetTypeActions_AkDialogueEvent::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, EditWithinLevelEditor, InObjects);
}

bool FAssetTypeActions_AkDialogueEvent::AssetsActivatedOverride(const TArray<UObject*>& InObjects, EAssetTypeActivationMethod::Type ActivationType)
{
	if (ActivationType == EAssetTypeActivationMethod::DoubleClicked || ActivationType == EAssetTypeActivationMethod::Opened)
	{
		if (InObjects.Num() == 1)
		{
			return GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(InObjects[0]);
		}
		else if (InObjects.Num() > 1)
		{
			return GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAssets(InObjects);
		}
	}
	else if (ActivationType == EAssetTypeActivationMethod::Previewed)
	{
		TArray<UObject*> NonNullObjects;
		for (auto& Obj : InObjects)
		{
			if (Obj != nullptr)
			{
				NonNullObjects.Add(Obj);
			}
		}
		if (!NonNullObjects.IsEmpty())
		{
			auto Events = GetTypedWeakObjectPtrs<UAkDialogueEvent>(InObjects);
			FAkAssetTypeActions_Helpers::PlayDialogueEvents(Events);
		}
	}
	return true;
}

void FAssetTypeActions_AkDialogueEvent::PlayEvent(TArray<TWeakObjectPtr<UAkDialogueEvent>> Objects)
{
	for (const auto& Object : Objects)
	{
		if (auto Event = Object.Get())
		{
			Event->PlayPreview();
		}
	}
}

void FAssetTypeActions_AkDialogueEvent::StopEvent(TArray<TWeakObjectPtr<UAkDialogueEvent>> Objects)
{
	for (const auto& Object : Objects)
	{
		if (auto Event = Object.Get())
		{
			Event->StopPreview();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FAssetTypeActions_AkAuxBus

void FAssetTypeActions_AkAuxBus::OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, EditWithinLevelEditor, InObjects);
}

void FAssetTypeActions_AkAuxBus::GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder)
{
	auto AuxBusses = GetTypedWeakObjectPtrs<UAkAuxBus>(InObjects);
}

void FAssetTypeActions_AkSwitchValue::OpenAssetEditor(const TArray<UObject*>& InObjects,
	TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, EditWithinLevelEditor, InObjects);
}

void FAssetTypeActions_AkStateValue::OpenAssetEditor(const TArray<UObject*>& InObjects,
	TSharedPtr<IToolkitHost> EditWithinLevelEditor)
{
	FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, EditWithinLevelEditor, InObjects);
}
#undef LOCTEXT_NAMESPACE
