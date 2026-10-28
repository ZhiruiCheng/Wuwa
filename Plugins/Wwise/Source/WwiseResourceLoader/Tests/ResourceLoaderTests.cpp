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
#include "Wwise/WwiseResourceLoaderImpl.h"

#include "Wwise/Mock/WwiseMockExternalSourceManager.h"
#include "Wwise/Mock/WwiseMockMediaManager.h"
#include "Wwise/Mock/WwiseMockSoundBankManager.h"

#include <array>
#include <atomic>
#include <memory>

WWISE_TEST_CASE(ResourceLoader_Smoke, "Wwise::ResourceLoader::ResourceLoader_Smoke", "[ApplicationContextMask][SmokeFilter]")
{
	SECTION("Static")
	{
		static_assert(std::is_constructible<FWwiseResourceLoaderImpl>::value, "Resource Loader must be constructed without parameters");
		static_assert(!std::is_copy_constructible<FWwiseResourceLoaderImpl>::value, "Cannot copy a Resource Loader");
		static_assert(!std::is_copy_assignable<FWwiseResourceLoaderImpl>::value, "Cannot reassign a Resource Loader");
		static_assert(!std::is_move_constructible<FWwiseResourceLoaderImpl>::value, "Cannot move a Resource Loader");
	}

	SECTION("Instantiation")
	{
		FWwiseResourceLoaderImpl ResourceLoaderImpl;
	}

	SECTION("Sync AudioNode")
	{
		TSharedRef ExternalSourceManager = MakeShared<FWwiseMockExternalSourceManager>();
		auto MediaManager = MakeShared<FWwiseMockMediaManager>();
		auto SoundBankManager = MakeShared<FWwiseMockSoundBankManager>();
		auto ResourceLoaderImpl = MakeShared<FWwiseResourceLoaderImpl>(ExternalSourceManager, MediaManager, SoundBankManager);

		FWwiseAudioNodeCookedData CookedData;
		CookedData.Media.Emplace(FWwiseMediaCookedData{});
		CookedData.SoundBanks.Emplace(FWwiseSoundBankCookedData{});
		CookedData.ExternalSources.Emplace(FWwiseExternalSourceCookedData{});
		
		// Creating Node
		auto* Node = ResourceLoaderImpl->CreateAudioNodeListEntry(CookedData);

		WWISE_CHECK(Node);
		if (UNLIKELY(!Node))
		{
			return;
		}

		// Loading Node
		FWwiseLoadedAudioNodePromise LoadPromise;
		auto LoadFuture = LoadPromise.GetFuture();
		ResourceLoaderImpl->LoadAudioNodeNode(MoveTemp(LoadPromise), MoveTemp(Node));
		auto LoadedNode = LoadFuture.Get();		// Synchronously
		WWISE_CHECK(LoadedNode);
		if (UNLIKELY(!LoadedNode))
		{
			return;
		}

		// Unloading Node
		FWwiseResourceUnloadPromise UnloadPromise;
		auto UnloadFuture = UnloadPromise.GetFuture();
		ResourceLoaderImpl->UnloadAudioNodeNode(MoveTemp(UnloadPromise), MoveTemp(LoadedNode));
		UnloadFuture.Get();		// Synchronously

		WWISE_CHECK(ExternalSourceManager->IsEmpty());
		WWISE_CHECK(MediaManager->IsEmpty());
		WWISE_CHECK(SoundBankManager->IsEmpty());
		WWISE_CHECK(ResourceLoaderImpl->IsEmpty());
	}

	SECTION("Sync AuxBus")
	{
		TSharedRef ExternalSourceManager = MakeShared<FWwiseMockExternalSourceManager>();
		auto MediaManager = MakeShared<FWwiseMockMediaManager>();
		auto SoundBankManager = MakeShared<FWwiseMockSoundBankManager>();
		auto ResourceLoaderImpl = MakeShared<FWwiseResourceLoaderImpl>(ExternalSourceManager, MediaManager, SoundBankManager);

		FWwiseLocalizedAuxBusCookedData CookedData;
		{
			FWwiseAuxBusCookedData Data1;
			Data1.Media.Emplace(FWwiseMediaCookedData{});
			Data1.SoundBanks.Emplace(FWwiseSoundBankCookedData{});
			CookedData.AuxBusLanguageMap.Emplace(FWwiseLanguageCookedData::Sfx, MoveTemp(Data1));
		}
		
		// Creating Node
		auto* Node = ResourceLoaderImpl->CreateAuxBusListEntry(CookedData, nullptr);

		WWISE_CHECK(Node);
		if (UNLIKELY(!Node))
		{
			return;
		}

		// Loading Node
		FWwiseLoadedAuxBusPromise LoadPromise;
		auto LoadFuture = LoadPromise.GetFuture();
		ResourceLoaderImpl->LoadAuxBusNode(MoveTemp(LoadPromise), MoveTemp(Node));
		auto LoadedNode = LoadFuture.Get();		// Synchronously
		WWISE_CHECK(LoadedNode);
		if (UNLIKELY(!LoadedNode))
		{
			return;
		}

		// Unloading Node
		FWwiseResourceUnloadPromise UnloadPromise;
		auto UnloadFuture = UnloadPromise.GetFuture();
		ResourceLoaderImpl->UnloadAuxBusNode(MoveTemp(UnloadPromise), MoveTemp(LoadedNode));
		UnloadFuture.Get();		// Synchronously

		WWISE_CHECK(ExternalSourceManager->IsEmpty());
		WWISE_CHECK(MediaManager->IsEmpty());
		WWISE_CHECK(SoundBankManager->IsEmpty());
		WWISE_CHECK(ResourceLoaderImpl->IsEmpty());
	}

	SECTION("Sync Dialogue Event")
	{
		TSharedRef ExternalSourceManager = MakeShared<FWwiseMockExternalSourceManager>();
		auto MediaManager = MakeShared<FWwiseMockMediaManager>();
		auto SoundBankManager = MakeShared<FWwiseMockSoundBankManager>();
		auto ResourceLoaderImpl = MakeShared<FWwiseResourceLoaderImpl>(ExternalSourceManager, MediaManager, SoundBankManager);

		FWwiseLocalizedDialogueEventCookedData CookedData;
		{
			FWwiseDialogueEventCookedData Data1;
			Data1.SoundBanks.Emplace(FWwiseSoundBankCookedData{});

			FWwiseGroupValueCookedDataSet GroupValueSet;
			GroupValueSet.GroupValues.Emplace(FWwiseGroupValueCookedData{});
			
			FWwiseAudioNodeCookedData AudioNode;
			AudioNode.Media.Emplace(FWwiseMediaCookedData{});
			AudioNode.SoundBanks.Emplace(FWwiseSoundBankCookedData{});
			AudioNode.ExternalSources.Emplace(FWwiseExternalSourceCookedData{});
			Data1.AudioNodes.Emplace(GroupValueSet, AudioNode);
			
			CookedData.DialogueEventLanguageMap.Emplace(FWwiseLanguageCookedData::Sfx, MoveTemp(Data1));

			FWwiseDialogueArgumentItem ArgItem;
			ArgItem.Type = EWwiseGroupType::Switch;
			FWwiseDialogueArgumentPosition ArgPosition;
			ArgPosition.Positions.Emplace(0);

			CookedData.RequiredArguments.Emplace(ArgItem, ArgPosition);
		}
		
		// Creating Node
		auto* Node = ResourceLoaderImpl->CreateDialogueEventListEntry(CookedData, nullptr);

		WWISE_CHECK(Node);
		if (UNLIKELY(!Node))
		{
			return;
		}

		// Loading Node
		FWwiseLoadedDialogueEventPromise LoadPromise;
		auto LoadFuture = LoadPromise.GetFuture();
		ResourceLoaderImpl->LoadDialogueEventNode(MoveTemp(LoadPromise), MoveTemp(Node));
		auto LoadedNode = LoadFuture.Get();		// Synchronously
		WWISE_CHECK(LoadedNode);
		if (UNLIKELY(!LoadedNode))
		{
			return;
		}

		// Unloading Node
		FWwiseResourceUnloadPromise UnloadPromise;
		auto UnloadFuture = UnloadPromise.GetFuture();
		ResourceLoaderImpl->UnloadDialogueEventNode(MoveTemp(UnloadPromise), MoveTemp(LoadedNode));
		UnloadFuture.Get();		// Synchronously

		WWISE_CHECK(ExternalSourceManager->IsEmpty());
		WWISE_CHECK(MediaManager->IsEmpty());
		WWISE_CHECK(SoundBankManager->IsEmpty());
		WWISE_CHECK(ResourceLoaderImpl->IsEmpty());
		
	}

	SECTION("Sync Event")
	{
		TSharedRef<FWwiseMockExternalSourceManager> ExternalSourceManager = MakeShared<FWwiseMockExternalSourceManager>();
		auto MediaManager = MakeShared<FWwiseMockMediaManager>();
		auto SoundBankManager = MakeShared<FWwiseMockSoundBankManager>();
		auto ResourceLoaderImpl = MakeShared<FWwiseResourceLoaderImpl>(ExternalSourceManager, MediaManager, SoundBankManager);

		FWwiseLocalizedEventCookedData CookedData;
		{
			FWwiseEventCookedData Data1;
			Data1.Media.Emplace(FWwiseMediaCookedData{});
			Data1.SoundBanks.Emplace(FWwiseSoundBankCookedData{});

			FWwiseAudioNodeCookedData AudioNode1;
			FWwiseMediaCookedData AudioNode1Media;
			AudioNode1Media.MediaId = 1;
			AudioNode1.Media.Emplace(MoveTemp(AudioNode1Media));
			FWwiseGroupValueCookedData AudioNode1GroupValue;
			AudioNode1GroupValue.Id = 1;
			AudioNode1GroupValue.GroupId = 1;
			AudioNode1GroupValue.Type = EWwiseGroupType::Switch;
			FWwiseGroupValueCookedDataSet AudioNode1GroupValueSet;
			AudioNode1GroupValueSet.GroupValues.Emplace(MoveTemp(AudioNode1GroupValue));
			Data1.AudioNodes.Emplace(MoveTemp(AudioNode1GroupValueSet), MoveTemp(AudioNode1));
			CookedData.EventLanguageMap.Emplace(FWwiseLanguageCookedData::Sfx, MoveTemp(Data1));
		}

		// Creating Node
		auto* Node = ResourceLoaderImpl->CreateEventListEntry(CookedData, nullptr);

		WWISE_CHECK(Node);
		if (UNLIKELY(!Node))
		{
			return;
		}

		// Loading Node
		FWwiseLoadedEventPromise LoadPromise;
		auto LoadFuture = LoadPromise.GetFuture();
		ResourceLoaderImpl->LoadEventNode(MoveTemp(LoadPromise), MoveTemp(Node));
		auto Loaded = LoadFuture.Get();		// Synchronously
		WWISE_CHECK(Loaded);
		if (UNLIKELY(!Loaded))
		{
			return;
		}

		// Unloading Node
		FWwiseResourceUnloadPromise UnloadPromise;
		auto UnloadFuture = UnloadPromise.GetFuture();
		ResourceLoaderImpl->UnloadEventNode(MoveTemp(UnloadPromise), MoveTemp(Loaded));
		UnloadFuture.Get();		// Synchronously

		WWISE_CHECK(ExternalSourceManager->IsEmpty());
		WWISE_CHECK(MediaManager->IsEmpty());
		WWISE_CHECK(SoundBankManager->IsEmpty());
		WWISE_CHECK(ResourceLoaderImpl->IsEmpty());
	}

	SECTION("Sync External Sources")
	{
		TSharedRef<FWwiseMockExternalSourceManager> ExternalSourceManager = MakeShared<FWwiseMockExternalSourceManager>();
		auto MediaManager = MakeShared<FWwiseMockMediaManager>();
		auto SoundBankManager = MakeShared<FWwiseMockSoundBankManager>();
		auto ResourceLoaderImpl = MakeShared<FWwiseResourceLoaderImpl>(ExternalSourceManager, MediaManager, SoundBankManager);

		FWwiseExternalSourceCookedData CookedData;

		// Creating Node
		auto* Node = ResourceLoaderImpl->CreateExternalSourceListEntry(CookedData);

		WWISE_CHECK(Node)
		if (UNLIKELY(!Node))
		{
			return;;
		}

		// Loading Node
		FWwiseLoadedExternalSourcePromise LoadPromise;
		auto LoadFuture = LoadPromise.GetFuture();
		ResourceLoaderImpl->LoadExternalSourceNode(MoveTemp(LoadPromise), MoveTemp(Node));
		auto Loaded  = LoadFuture.Get(); // Synchronously
		WWISE_CHECK(Loaded);
		if (UNLIKELY(!Loaded))
		{
			return;
		}

		// Unloading Node
		FWwiseResourceUnloadPromise UnloadPromise;
		auto UnloadFuture = UnloadPromise.GetFuture();
		ResourceLoaderImpl->UnloadExternalSourceNode(MoveTemp(UnloadPromise), MoveTemp(Loaded));
		UnloadFuture.Get();		// Synchronously

		WWISE_CHECK(ExternalSourceManager->IsEmpty());
		WWISE_CHECK(MediaManager->IsEmpty());
		WWISE_CHECK(SoundBankManager->IsEmpty());
		WWISE_CHECK(ResourceLoaderImpl->IsEmpty());
	}

	SECTION("Sync GroupValue Switch")
	{
		TSharedRef<FWwiseMockExternalSourceManager> ExternalSourceManager = MakeShared<FWwiseMockExternalSourceManager>();
		auto MediaManager = MakeShared<FWwiseMockMediaManager>();
		auto SoundBankManager = MakeShared<FWwiseMockSoundBankManager>();
		auto ResourceLoaderImpl = MakeShared<FWwiseResourceLoaderImpl>(ExternalSourceManager, MediaManager, SoundBankManager);

		FWwiseGroupValueCookedData CookedData;
		CookedData.Type = EWwiseGroupType::Switch;

		// Creating Node
		auto* Node = ResourceLoaderImpl->CreateGroupValueListEntry(CookedData);

		WWISE_CHECK(Node);
		if (UNLIKELY(!Node))
		{
			return;
		}

		// Loading Node
		FWwiseLoadedGroupValuePromise LoadPromise;
		auto LoadFuture = LoadPromise.GetFuture();
		ResourceLoaderImpl->LoadGroupValueNode(MoveTemp(LoadPromise), MoveTemp(Node));
		auto Loaded = LoadFuture.Get();		// Synchronously
		WWISE_CHECK(Loaded);
		if (UNLIKELY(!Loaded))
		{
			return;
		}

		// Unloading Node
		FWwiseResourceUnloadPromise UnloadPromise;
		auto UnloadFuture = UnloadPromise.GetFuture();
		ResourceLoaderImpl->UnloadGroupValueNode(MoveTemp(UnloadPromise), MoveTemp(Loaded));
		UnloadFuture.Get();		// Synchronously

		WWISE_CHECK(ExternalSourceManager->IsEmpty());
		WWISE_CHECK(MediaManager->IsEmpty());
		WWISE_CHECK(SoundBankManager->IsEmpty());
		WWISE_CHECK(ResourceLoaderImpl->IsEmpty());
	}

	SECTION("Sync GroupValue State")
	{
		TSharedRef<FWwiseMockExternalSourceManager> ExternalSourceManager = MakeShared<FWwiseMockExternalSourceManager>();
		auto MediaManager = MakeShared<FWwiseMockMediaManager>();
		auto SoundBankManager = MakeShared<FWwiseMockSoundBankManager>();
		auto ResourceLoaderImpl = MakeShared<FWwiseResourceLoaderImpl>(ExternalSourceManager, MediaManager, SoundBankManager);

		FWwiseGroupValueCookedData CookedData;
		CookedData.Type = EWwiseGroupType::State;

		// Creating Node
		auto* Node = ResourceLoaderImpl->CreateGroupValueListEntry(CookedData);

		WWISE_CHECK(Node);
		if (UNLIKELY(!Node))
		{
			return;
		}

		// Loading Node
		FWwiseLoadedGroupValuePromise LoadPromise;
		auto LoadFuture = LoadPromise.GetFuture();
		ResourceLoaderImpl->LoadGroupValueNode(MoveTemp(LoadPromise), MoveTemp(Node));
		auto Loaded = LoadFuture.Get();		// Synchronously
		WWISE_CHECK(Loaded);
		if (UNLIKELY(!Loaded))
		{
			return;
		}

		// Unloading Node
		FWwiseResourceUnloadPromise UnloadPromise;
		auto UnloadFuture = UnloadPromise.GetFuture();
		ResourceLoaderImpl->UnloadGroupValueNode(MoveTemp(UnloadPromise), MoveTemp(Loaded));
		UnloadFuture.Get();		// Synchronously

		WWISE_CHECK(ExternalSourceManager->IsEmpty());
		WWISE_CHECK(MediaManager->IsEmpty());
		WWISE_CHECK(SoundBankManager->IsEmpty());
		WWISE_CHECK(ResourceLoaderImpl->IsEmpty());
	}

	SECTION("Sync InitBank")
	{
		TSharedRef<FWwiseMockExternalSourceManager> ExternalSourceManager = MakeShared<FWwiseMockExternalSourceManager>();
		auto MediaManager = MakeShared<FWwiseMockMediaManager>();
		auto SoundBankManager = MakeShared<FWwiseMockSoundBankManager>();
		auto ResourceLoaderImpl = MakeShared<FWwiseResourceLoaderImpl>(ExternalSourceManager, MediaManager, SoundBankManager);

		FWwiseInitBankCookedData CookedData;

		// Creating Node
		auto* Node = ResourceLoaderImpl->CreateInitBankListEntry(CookedData);

		WWISE_CHECK(Node);
		if (UNLIKELY(!Node))
		{
			return;
		}

		// Loading Node
		FWwiseLoadedInitBankPromise LoadPromise;
		auto LoadFuture = LoadPromise.GetFuture();
		ResourceLoaderImpl->LoadInitBankNode(MoveTemp(LoadPromise), MoveTemp(Node));
		auto Loaded = LoadFuture.Get();		// Synchronously
		WWISE_CHECK(Loaded);
		if (UNLIKELY(!Loaded))
		{
			return;
		}

		// Unloading Node
		FWwiseResourceUnloadPromise UnloadPromise;
		auto UnloadFuture = UnloadPromise.GetFuture();
		ResourceLoaderImpl->UnloadInitBankNode(MoveTemp(UnloadPromise), MoveTemp(Loaded));
		UnloadFuture.Get();		// Synchronously

		WWISE_CHECK(ExternalSourceManager->IsEmpty());
		WWISE_CHECK(MediaManager->IsEmpty());
		WWISE_CHECK(SoundBankManager->IsEmpty());
		WWISE_CHECK(ResourceLoaderImpl->IsEmpty());
	}
	
	SECTION("Sync Media")
	{
		TSharedRef<FWwiseMockExternalSourceManager> ExternalSourceManager = MakeShared<FWwiseMockExternalSourceManager>();
		auto MediaManager = MakeShared<FWwiseMockMediaManager>();
		auto SoundBankManager = MakeShared<FWwiseMockSoundBankManager>();
		auto ResourceLoaderImpl = MakeShared<FWwiseResourceLoaderImpl>(ExternalSourceManager, MediaManager, SoundBankManager);

		FWwiseMediaCookedData CookedData;

		// Creating Node
		auto* Node = ResourceLoaderImpl->CreateMediaListEntry(CookedData);

		WWISE_CHECK(Node);
		if (UNLIKELY(!Node))
		{
			return;
		}

		// Loading Node
		FWwiseLoadedMediaPromise LoadPromise;
		auto LoadFuture = LoadPromise.GetFuture();
		ResourceLoaderImpl->LoadMediaNode(MoveTemp(LoadPromise), MoveTemp(Node));
		auto Loaded = LoadFuture.Get();		// Synchronously
		WWISE_CHECK(Loaded);
		if (UNLIKELY(!Loaded))
		{
			return;
		}

		// Unloading Node
		FWwiseResourceUnloadPromise UnloadPromise;
		auto UnloadFuture = UnloadPromise.GetFuture();
		ResourceLoaderImpl->UnloadMediaNode(MoveTemp(UnloadPromise), MoveTemp(Loaded));
		UnloadFuture.Get();		// Synchronously

		WWISE_CHECK(ExternalSourceManager->IsEmpty());
		WWISE_CHECK(MediaManager->IsEmpty());
		WWISE_CHECK(SoundBankManager->IsEmpty());
		WWISE_CHECK(ResourceLoaderImpl->IsEmpty());
	}
	
	SECTION("Sync ShareSet")
	{
		TSharedRef<FWwiseMockExternalSourceManager> ExternalSourceManager = MakeShared<FWwiseMockExternalSourceManager>();
		auto MediaManager = MakeShared<FWwiseMockMediaManager>();
		auto SoundBankManager = MakeShared<FWwiseMockSoundBankManager>();
		auto ResourceLoaderImpl = MakeShared<FWwiseResourceLoaderImpl>(ExternalSourceManager, MediaManager, SoundBankManager);

		FWwiseLocalizedShareSetCookedData CookedData;
		{
			FWwiseShareSetCookedData Data1;
			Data1.Media.Emplace(FWwiseMediaCookedData{});
			Data1.SoundBanks.Emplace(FWwiseSoundBankCookedData{});
			CookedData.ShareSetLanguageMap.Emplace(FWwiseLanguageCookedData::Sfx, MoveTemp(Data1));
		}

		// Creating Node
		auto* Node = ResourceLoaderImpl->CreateShareSetListEntry(CookedData, &FWwiseLanguageCookedData::Sfx);

		WWISE_CHECK(Node);
		if (UNLIKELY(!Node))
		{
			return;
		}

		// Loading Node
		FWwiseLoadedShareSetPromise LoadPromise;
		auto LoadFuture = LoadPromise.GetFuture();
		ResourceLoaderImpl->LoadShareSetNode(MoveTemp(LoadPromise), MoveTemp(Node));
		auto Loaded = LoadFuture.Get();		// Synchronously
		WWISE_CHECK(Loaded);
		if (UNLIKELY(!Loaded))
		{
			return;
		}

		// Unloading Node
		FWwiseResourceUnloadPromise UnloadPromise;
		auto UnloadFuture = UnloadPromise.GetFuture();
		ResourceLoaderImpl->UnloadShareSetNode(MoveTemp(UnloadPromise), MoveTemp(Loaded));
		UnloadFuture.Get();		// Synchronously

		WWISE_CHECK(ExternalSourceManager->IsEmpty());
		WWISE_CHECK(MediaManager->IsEmpty());
		WWISE_CHECK(SoundBankManager->IsEmpty());
		WWISE_CHECK(ResourceLoaderImpl->IsEmpty());
	}

	SECTION("Sync SoundBank")
	{
		TSharedRef<FWwiseMockExternalSourceManager> ExternalSourceManager = MakeShared<FWwiseMockExternalSourceManager>();
		auto MediaManager = MakeShared<FWwiseMockMediaManager>();
		auto SoundBankManager = MakeShared<FWwiseMockSoundBankManager>();
		auto ResourceLoaderImpl = MakeShared<FWwiseResourceLoaderImpl>(ExternalSourceManager, MediaManager, SoundBankManager);

		FWwiseLocalizedSoundBankCookedData CookedData;
		{
			FWwiseSoundBankCookedData Data1;
			CookedData.SoundBankLanguageMap.Emplace(FWwiseLanguageCookedData::Sfx, MoveTemp(Data1));
		}

		// Creating Node
		auto* Node = ResourceLoaderImpl->CreateSoundBankListEntry(CookedData, nullptr);

		WWISE_CHECK(Node);
		if (UNLIKELY(!Node))
		{
			return;
		}

		// Loading Node
		FWwiseLoadedSoundBankPromise LoadPromise;
		auto LoadFuture = LoadPromise.GetFuture();
		ResourceLoaderImpl->LoadSoundBankNode(MoveTemp(LoadPromise), MoveTemp(Node));
		auto Loaded = LoadFuture.Get();		// Synchronously
		WWISE_CHECK(Loaded);
		if (UNLIKELY(!Loaded))
		{
			return;
		}

		// Unloading Node
		FWwiseResourceUnloadPromise UnloadPromise;
		auto UnloadFuture = UnloadPromise.GetFuture();
		ResourceLoaderImpl->UnloadSoundBankNode(MoveTemp(UnloadPromise), MoveTemp(Loaded));
		UnloadFuture.Get();		// Synchronously

		WWISE_CHECK(ExternalSourceManager->IsEmpty());
		WWISE_CHECK(MediaManager->IsEmpty());
		WWISE_CHECK(SoundBankManager->IsEmpty());
		WWISE_CHECK(ResourceLoaderImpl->IsEmpty());
	}

	SECTION("Sync SwitchContainer (Event First)")
	{
		TSharedRef<FWwiseMockExternalSourceManager> ExternalSourceManager = MakeShared<FWwiseMockExternalSourceManager>();
		auto MediaManager = MakeShared<FWwiseMockMediaManager>();
		auto SoundBankManager = MakeShared<FWwiseMockSoundBankManager>();
		auto ResourceLoaderImpl = MakeShared<FWwiseResourceLoaderImpl>(ExternalSourceManager, MediaManager, SoundBankManager);

		FWwiseGroupValueCookedData GroupValue;
		GroupValue.Id = 1;
		GroupValue.GroupId = 1;
		GroupValue.Type = EWwiseGroupType::Switch;

		FWwiseLoadedEventPtr LoadedEvent{nullptr};
		FWwiseLoadedGroupValuePtr LoadedGroupValue{nullptr};

		// Load Event
		do
		{
			FWwiseLocalizedEventCookedData CookedData;
			{
				FWwiseEventCookedData Data1;
				Data1.Media.Emplace(FWwiseMediaCookedData{});
				Data1.SoundBanks.Emplace(FWwiseSoundBankCookedData{});

				FWwiseAudioNodeCookedData AudioNode1;
				FWwiseMediaCookedData AudioNode1Media;
				AudioNode1Media.MediaId = 1;
				AudioNode1.Media.Emplace(MoveTemp(AudioNode1Media));
				FWwiseGroupValueCookedDataSet AudioNode1GroupValueSet;
				AudioNode1GroupValueSet.GroupValues.Emplace(MoveTemp(GroupValue));
				Data1.AudioNodes.Emplace(MoveTemp(AudioNode1GroupValueSet), MoveTemp(AudioNode1));
				
				CookedData.EventLanguageMap.Emplace(FWwiseLanguageCookedData::Sfx, MoveTemp(Data1));
			}

			// Creating Node
			auto* Node = ResourceLoaderImpl->CreateEventListEntry(CookedData, nullptr);

			WWISE_CHECK(Node);
			if (UNLIKELY(!Node))
			{
				break;
			}

			// Loading Node
			FWwiseLoadedEventPromise LoadPromise;
			auto LoadFuture = LoadPromise.GetFuture();
			ResourceLoaderImpl->LoadEventNode(MoveTemp(LoadPromise), MoveTemp(Node));
			LoadedEvent = LoadFuture.Get();		// Synchronously
			WWISE_CHECK(LoadedEvent);
			if (UNLIKELY(!LoadedEvent))
			{
				break;
			}
		}
		while (false);

		// Load GroupValue
		do
		{
			// Creating Node
			auto* Node = ResourceLoaderImpl->CreateGroupValueListEntry(GroupValue);

			WWISE_CHECK(Node);
			if (UNLIKELY(!Node))
			{
				break;
			}

			// Loading Node
			FWwiseLoadedGroupValuePromise LoadPromise;
			auto LoadFuture = LoadPromise.GetFuture();
			ResourceLoaderImpl->LoadGroupValueNode(MoveTemp(LoadPromise), MoveTemp(Node));
			LoadedGroupValue = LoadFuture.Get();		// Synchronously
			WWISE_CHECK(LoadedGroupValue);
			if (UNLIKELY(!LoadedGroupValue))
			{
				break;
			}
		}
		while(false);

		// Unloading GroupValue
		if (LIKELY(LoadedGroupValue))
		{
			FWwiseResourceUnloadPromise UnloadPromise;
			auto UnloadFuture = UnloadPromise.GetFuture();
			ResourceLoaderImpl->UnloadGroupValueNode(MoveTemp(UnloadPromise), MoveTemp(LoadedGroupValue));
			UnloadFuture.Get();		// Synchronously
		}

		// Unloading Event
		if (LIKELY(LoadedEvent))
		{
			FWwiseResourceUnloadPromise UnloadPromise;
			auto UnloadFuture = UnloadPromise.GetFuture();
			ResourceLoaderImpl->UnloadEventNode(MoveTemp(UnloadPromise), MoveTemp(LoadedEvent));
			UnloadFuture.Get();		// Synchronously
		}

		WWISE_CHECK(ExternalSourceManager->IsEmpty());
		WWISE_CHECK(MediaManager->IsEmpty());
		WWISE_CHECK(SoundBankManager->IsEmpty());
		WWISE_CHECK(ResourceLoaderImpl->IsEmpty());
	}

	SECTION("Sync SwitchContainer (Switch First)")
	{
		TSharedRef<FWwiseMockExternalSourceManager> ExternalSourceManager = MakeShared<FWwiseMockExternalSourceManager>();
		auto MediaManager = MakeShared<FWwiseMockMediaManager>();
		auto SoundBankManager = MakeShared<FWwiseMockSoundBankManager>();
		auto ResourceLoaderImpl = MakeShared<FWwiseResourceLoaderImpl>(ExternalSourceManager, MediaManager, SoundBankManager);

		FWwiseGroupValueCookedData GroupValue;
		GroupValue.Id = 1;
		GroupValue.GroupId = 1;
		GroupValue.Type = EWwiseGroupType::Switch;

		FWwiseLoadedEventPtr LoadedEvent{nullptr};
		FWwiseLoadedGroupValuePtr LoadedGroupValue{nullptr};

		// Load GroupValue
		do
		{
			// Creating Node
			auto* Node = ResourceLoaderImpl->CreateGroupValueListEntry(GroupValue);

			WWISE_CHECK(Node);
			if (UNLIKELY(!Node))
			{
				break;
			}

			// Loading Node
			FWwiseLoadedGroupValuePromise LoadPromise;
			auto LoadFuture = LoadPromise.GetFuture();
			ResourceLoaderImpl->LoadGroupValueNode(MoveTemp(LoadPromise), MoveTemp(Node));
			LoadedGroupValue = LoadFuture.Get();		// Synchronously
			WWISE_CHECK(LoadedGroupValue);
			if (UNLIKELY(!LoadedGroupValue))
			{
				break;
			}
		}
		while(false);

		// Load Event
		do
		{
			FWwiseLocalizedEventCookedData CookedData;
			{
				FWwiseEventCookedData Data1;
				Data1.Media.Emplace(FWwiseMediaCookedData{});
				Data1.SoundBanks.Emplace(FWwiseSoundBankCookedData{});

				FWwiseAudioNodeCookedData AudioNode1;
				FWwiseMediaCookedData AudioNode1Media;
				AudioNode1Media.MediaId = 1;
				AudioNode1.Media.Emplace(MoveTemp(AudioNode1Media));
				FWwiseGroupValueCookedDataSet AudioNode1GroupValueSet;
				AudioNode1GroupValueSet.GroupValues.Emplace(MoveTemp(GroupValue));
				Data1.AudioNodes.Emplace(MoveTemp(AudioNode1GroupValueSet), MoveTemp(AudioNode1));
				
				CookedData.EventLanguageMap.Emplace(FWwiseLanguageCookedData::Sfx, MoveTemp(Data1));
			}

			// Creating Node
			auto* Node = ResourceLoaderImpl->CreateEventListEntry(CookedData, nullptr);

			WWISE_CHECK(Node);
			if (UNLIKELY(!Node))
			{
				break;
			}

			// Loading Node
			FWwiseLoadedEventPromise LoadPromise;
			auto LoadFuture = LoadPromise.GetFuture();
			ResourceLoaderImpl->LoadEventNode(MoveTemp(LoadPromise), MoveTemp(Node));
			LoadedEvent = LoadFuture.Get();		// Synchronously
			WWISE_CHECK(LoadedEvent);
			if (UNLIKELY(!LoadedEvent))
			{
				break;
			}
		}
		while (false);

		// Unloading Event
		if (LIKELY(LoadedEvent))
		{
			FWwiseResourceUnloadPromise UnloadPromise;
			auto UnloadFuture = UnloadPromise.GetFuture();
			ResourceLoaderImpl->UnloadEventNode(MoveTemp(UnloadPromise), MoveTemp(LoadedEvent));
			UnloadFuture.Get();		// Synchronously
		}

		// Unloading GroupValue
		if (LIKELY(LoadedGroupValue))
		{
			FWwiseResourceUnloadPromise UnloadPromise;
			auto UnloadFuture = UnloadPromise.GetFuture();
			ResourceLoaderImpl->UnloadGroupValueNode(MoveTemp(UnloadPromise), MoveTemp(LoadedGroupValue));
			UnloadFuture.Get();		// Synchronously
		}

		WWISE_CHECK(ExternalSourceManager->IsEmpty());
		WWISE_CHECK(MediaManager->IsEmpty());
		WWISE_CHECK(SoundBankManager->IsEmpty());
		WWISE_CHECK(ResourceLoaderImpl->IsEmpty());
	}
}

WWISE_TEST_CASE(ResourceLoader_Async, "Wwise::ResourceLoader::ResourceLoader_Async", "[ApplicationContextMask][ProductFilter]")
{
	// Keeping these in prime numbers so we get a higher set of possibilities
	static constexpr const auto GroupValueCount = 3;
	static constexpr const auto FileCount = 7;
	static constexpr const auto CookedDataCount = 11;
	static constexpr const auto NodeCount = 13;
	
	SECTION("Async AudioNode")
	{
		TSharedRef<FWwiseMockExternalSourceManager> ExternalSourceManager = MakeShared<FWwiseMockExternalSourceManager>();
		auto MediaManager = MakeShared<FWwiseMockMediaManager>();
		auto SoundBankManager = MakeShared<FWwiseMockSoundBankManager>();
		auto ResourceLoaderImpl = MakeShared<FWwiseResourceLoaderImpl>(ExternalSourceManager, MediaManager, SoundBankManager);

		FWwiseAudioNodeCookedData CookedDatas[CookedDataCount];
		for (auto i = 0; i < CookedDataCount; ++i)
		{
			FWwiseAudioNodeCookedData Data;
			Data.AudioNodeId = i % CookedDataCount; 
			Data.Media.Emplace(FWwiseMediaCookedData{});
			Data.Media[0].MediaId = i % FileCount;
			Data.SoundBanks.Emplace(FWwiseSoundBankCookedData{});
			CookedDatas[i] = MoveTemp(Data);
		}
		
		FWwiseResourceUnloadFuture UnloadFutures[NodeCount];
		for (auto i = 0; i < NodeCount; ++i)
		{
			// Creating Node
			auto* Node = ResourceLoaderImpl->CreateAudioNodeListEntry(CookedDatas[i%CookedDataCount]);

			WWISE_CHECK(Node);
			if (UNLIKELY(!Node))
			{
				continue;
			}

			// Loading Node
			FWwiseLoadedAudioNodePromise LoadPromise;
			auto NodeFuture = LoadPromise.GetFuture();
			ResourceLoaderImpl->LoadAudioNodeNode(MoveTemp(LoadPromise), MoveTemp(Node));

			// Unloading Node
			FWwiseResourceUnloadPromise UnloadPromise;
			UnloadFutures[i] = UnloadPromise.GetFuture();

			NodeFuture.Next([&ResourceLoaderImpl, UnloadPromise = MoveTemp(UnloadPromise)](FWwiseLoadedAudioNodePtr LoadedNode) mutable
			{
				WWISE_CHECK(LoadedNode);
				if (UNLIKELY(!LoadedNode))
				{
					UnloadPromise.EmplaceValue();
					return;
				}

				ResourceLoaderImpl->UnloadAudioNodeNode(MoveTemp(UnloadPromise), MoveTemp(LoadedNode));
			});
		}

		for (auto i = 0; i < NodeCount; ++i)
		{
			UnloadFutures[i].Get();
		}

		WWISE_CHECK(ExternalSourceManager->IsEmpty());
		WWISE_CHECK(MediaManager->IsEmpty());
		WWISE_CHECK(SoundBankManager->IsEmpty());
		WWISE_CHECK(ResourceLoaderImpl->IsEmpty());
	}

	SECTION("Async AuxBus")
	{
		TSharedRef<FWwiseMockExternalSourceManager> ExternalSourceManager = MakeShared<FWwiseMockExternalSourceManager>();
		auto MediaManager = MakeShared<FWwiseMockMediaManager>();
		auto SoundBankManager = MakeShared<FWwiseMockSoundBankManager>();
		auto ResourceLoaderImpl = MakeShared<FWwiseResourceLoaderImpl>(ExternalSourceManager, MediaManager, SoundBankManager);

		FWwiseLocalizedAuxBusCookedData CookedDatas[CookedDataCount];
		for (auto i = 0; i < CookedDataCount; ++i)
		{
			FWwiseAuxBusCookedData Data;
			Data.AuxBusId = i % CookedDataCount; 
			Data.Media.Emplace(FWwiseMediaCookedData{});
			Data.Media[0].MediaId = i % FileCount;
			Data.SoundBanks.Emplace(FWwiseSoundBankCookedData{});
			CookedDatas[i].AuxBusLanguageMap.Emplace(FWwiseLanguageCookedData::Sfx, MoveTemp(Data));
		}
		
		FWwiseResourceUnloadFuture UnloadFutures[NodeCount];
		for (auto i = 0; i < NodeCount; ++i)
		{
			// Creating Node
			auto* Node = ResourceLoaderImpl->CreateAuxBusListEntry(CookedDatas[i%CookedDataCount], nullptr);

			WWISE_CHECK(Node);
			if (UNLIKELY(!Node))
			{
				continue;
			}

			// Loading Node
			FWwiseLoadedAuxBusPromise LoadPromise;
			auto NodeFuture = LoadPromise.GetFuture();
			ResourceLoaderImpl->LoadAuxBusNode(MoveTemp(LoadPromise), MoveTemp(Node));

			// Unloading Node
			FWwiseResourceUnloadPromise UnloadPromise;
			UnloadFutures[i] = UnloadPromise.GetFuture();

			NodeFuture.Next([&ResourceLoaderImpl, UnloadPromise = MoveTemp(UnloadPromise)](FWwiseLoadedAuxBusPtr LoadedNode) mutable
			{
				WWISE_CHECK(LoadedNode);
				if (UNLIKELY(!LoadedNode))
				{
					UnloadPromise.EmplaceValue();
					return;
				}

				ResourceLoaderImpl->UnloadAuxBusNode(MoveTemp(UnloadPromise), MoveTemp(LoadedNode));
			});
		}

		for (auto i = 0; i < NodeCount; ++i)
		{
			UnloadFutures[i].Get();
		}

		WWISE_CHECK(ExternalSourceManager->IsEmpty());
		WWISE_CHECK(MediaManager->IsEmpty());
		WWISE_CHECK(SoundBankManager->IsEmpty());
		WWISE_CHECK(ResourceLoaderImpl->IsEmpty());
	}

	SECTION("Async Dialogue Event & GroupValue")
	{
		TSharedRef<FWwiseMockExternalSourceManager> ExternalSourceManager = MakeShared<FWwiseMockExternalSourceManager>();
		auto MediaManager = MakeShared<FWwiseMockMediaManager>();
		auto SoundBankManager = MakeShared<FWwiseMockSoundBankManager>();
		auto ResourceLoaderImpl = MakeShared<FWwiseResourceLoaderImpl>(ExternalSourceManager, MediaManager, SoundBankManager);

		FWwiseLocalizedDialogueEventCookedData EventCookedDatas[CookedDataCount];
		for (auto i = 0; i < CookedDataCount; ++i)
		{
			FWwiseDialogueEventCookedData Data;
			Data.DialogueEventId = i % CookedDataCount;
			Data.SoundBanks.Emplace(FWwiseSoundBankCookedData{});

			FWwiseAudioNodeCookedData AudioNode;
			FWwiseMediaCookedData AudioNodeMedia;
			AudioNodeMedia.MediaId = (i % FileCount) + 2;		// Allow overlap while allow for uniques
			AudioNode.Media.Emplace(MoveTemp(AudioNodeMedia));

			FWwiseGroupValueCookedData AudioNodeGroupValue;
			AudioNodeGroupValue.Id = i % GroupValueCount;
			AudioNodeGroupValue.GroupId = 1;
			AudioNodeGroupValue.Type = EWwiseGroupType::Switch;
			FWwiseGroupValueCookedDataSet AudioNode1GroupValueSet;
			AudioNode1GroupValueSet.GroupValues.Emplace(MoveTemp(AudioNodeGroupValue));
			Data.AudioNodes.Emplace(MoveTemp(AudioNode1GroupValueSet), MoveTemp(AudioNode));

			EventCookedDatas[i].DialogueEventLanguageMap.Emplace(FWwiseLanguageCookedData::Sfx, MoveTemp(Data));
			
			{
				FWwiseDialogueArgumentItem ArgItem;
				ArgItem.Type = EWwiseGroupType::Switch;
				ArgItem.GroupId = 1;
				FWwiseDialogueArgumentPosition ArgPosition;
				ArgPosition.Positions.Emplace(0);

				EventCookedDatas[i].RequiredArguments.Emplace(ArgItem, ArgPosition);
			}
		}

		FWwiseGroupValueCookedData GroupValueCookedDatas[CookedDataCount];
		for (auto i = 0; i < CookedDataCount; ++i)
		{
			GroupValueCookedDatas[i].Id = GroupValueCount - (i % GroupValueCount) - 1;	// Make them opposite
			GroupValueCookedDatas[i].GroupId = 1;
			GroupValueCookedDatas[i].Type = EWwiseGroupType::Switch;
		}

		constexpr const auto FuturesCount = NodeCount * 2; 
		FWwiseResourceUnloadFuture UnloadFutures[FuturesCount];

		for (auto i = 0; i < NodeCount; ++i)
		{
			auto DoEvent = [&ResourceLoaderImpl, &EventCookedDatas, i, &UnloadFutures]() mutable
			{
				// Creating Node
				auto* Node = ResourceLoaderImpl->CreateDialogueEventListEntry(EventCookedDatas[i%CookedDataCount], nullptr);

				WWISE_CHECK(Node);
				if (UNLIKELY(!Node))
				{
					return;
				}

				// Loading Node
				FWwiseLoadedDialogueEventPromise LoadPromise;
				auto NodeFuture = LoadPromise.GetFuture();
				ResourceLoaderImpl->LoadDialogueEventNode(MoveTemp(LoadPromise), MoveTemp(Node));

				// Unloading Node
				FWwiseResourceUnloadPromise UnloadPromise;
				UnloadFutures[i] = UnloadPromise.GetFuture();

				NodeFuture.Next([&ResourceLoaderImpl, UnloadPromise = MoveTemp(UnloadPromise)](FWwiseLoadedDialogueEventPtr LoadedNode) mutable
				{
					WWISE_CHECK(LoadedNode);
					if (UNLIKELY(!LoadedNode))
					{
						UnloadPromise.EmplaceValue();
						return;
					}

					ResourceLoaderImpl->UnloadDialogueEventNode(MoveTemp(UnloadPromise), MoveTemp(LoadedNode));
				});
			};

			auto DoGroupValue = [&ResourceLoaderImpl, &GroupValueCookedDatas, i, &UnloadFutures]() mutable
			{
				// Creating Node
				auto* Node = ResourceLoaderImpl->CreateGroupValueListEntry(GroupValueCookedDatas[i%CookedDataCount]);

				WWISE_CHECK(Node);
				if (UNLIKELY(!Node))
				{
					return;
				}

				// Loading Node
				FWwiseLoadedGroupValuePromise LoadPromise;
				auto NodeFuture = LoadPromise.GetFuture();
				ResourceLoaderImpl->LoadGroupValueNode(MoveTemp(LoadPromise), MoveTemp(Node));

				// Unloading Node
				FWwiseResourceUnloadPromise UnloadPromise;
				UnloadFutures[NodeCount+i] = UnloadPromise.GetFuture();

				NodeFuture.Next([&ResourceLoaderImpl, UnloadPromise = MoveTemp(UnloadPromise)](FWwiseLoadedGroupValuePtr LoadedNode) mutable
				{
					WWISE_CHECK(LoadedNode);
					if (UNLIKELY(!LoadedNode))
					{
						UnloadPromise.EmplaceValue();
						return;
					}

					ResourceLoaderImpl->UnloadGroupValueNode(MoveTemp(UnloadPromise), MoveTemp(LoadedNode));
				});
			};

			// Vary which one is done first
			if (i & 4)
			{
				DoEvent();
				DoGroupValue();
			}
			else
			{
				DoGroupValue();
				DoEvent();
			}
		}

		for (auto i = 0; i < FuturesCount; ++i)
		{
			UnloadFutures[i].Get();
		}

		WWISE_CHECK(ExternalSourceManager->IsEmpty());
		WWISE_CHECK(MediaManager->IsEmpty());
		WWISE_CHECK(SoundBankManager->IsEmpty());
		WWISE_CHECK(ResourceLoaderImpl->IsEmpty());
		UE_LOG(LogWwiseResourceLoader, Log, TEXT("Finished Resource Loader Tests"))
	}

	SECTION("Async Event & GroupValue")
	{
		TSharedRef<FWwiseMockExternalSourceManager> ExternalSourceManager = MakeShared<FWwiseMockExternalSourceManager>();
		auto MediaManager = MakeShared<FWwiseMockMediaManager>();
		auto SoundBankManager = MakeShared<FWwiseMockSoundBankManager>();
		auto ResourceLoaderImpl = MakeShared<FWwiseResourceLoaderImpl>(ExternalSourceManager, MediaManager, SoundBankManager);

		FWwiseLocalizedEventCookedData EventCookedDatas[CookedDataCount];
		for (auto i = 0; i < CookedDataCount; ++i)
		{
			FWwiseEventCookedData Data;
			Data.EventId = i % CookedDataCount;
			Data.Media.Emplace(FWwiseMediaCookedData{});
			Data.Media[0].MediaId = i % FileCount;
			Data.SoundBanks.Emplace(FWwiseSoundBankCookedData{});

			FWwiseAudioNodeCookedData AudioNode;
			FWwiseMediaCookedData AudioNodeMedia;
			AudioNodeMedia.MediaId = (i % FileCount) + 2;		// Allow overlap while allow for uniques
			AudioNode.Media.Emplace(MoveTemp(AudioNodeMedia));

			FWwiseGroupValueCookedData AudioNodeGroupValue;
			AudioNodeGroupValue.Id = i % GroupValueCount;
			AudioNodeGroupValue.GroupId = 1;
			AudioNodeGroupValue.Type = EWwiseGroupType::Switch;
			FWwiseGroupValueCookedDataSet AudioNode1GroupValueSet;
			AudioNode1GroupValueSet.GroupValues.Emplace(MoveTemp(AudioNodeGroupValue));
			Data.AudioNodes.Emplace(MoveTemp(AudioNode1GroupValueSet), MoveTemp(AudioNode));

			EventCookedDatas[i].EventLanguageMap.Emplace(FWwiseLanguageCookedData::Sfx, MoveTemp(Data));
		}

		FWwiseGroupValueCookedData GroupValueCookedDatas[CookedDataCount];
		for (auto i = 0; i < CookedDataCount; ++i)
		{
			GroupValueCookedDatas[i].Id = GroupValueCount - (i % GroupValueCount) - 1;	// Make them opposite
			GroupValueCookedDatas[i].GroupId = 1;
			GroupValueCookedDatas[i].Type = EWwiseGroupType::Switch;
		}

		constexpr const auto FuturesCount = NodeCount * 2; 
		FWwiseResourceUnloadFuture UnloadFutures[FuturesCount];

		for (auto i = 0; i < NodeCount; ++i)
		{
			auto DoEvent = [&ResourceLoaderImpl, &EventCookedDatas, i, &UnloadFutures]() mutable
			{
				// Creating Node
				auto* Node = ResourceLoaderImpl->CreateEventListEntry(EventCookedDatas[i%CookedDataCount], nullptr);

				WWISE_CHECK(Node);
				if (UNLIKELY(!Node))
				{
					return;
				}

				// Loading Node
				FWwiseLoadedEventPromise LoadPromise;
				auto NodeFuture = LoadPromise.GetFuture();
				ResourceLoaderImpl->LoadEventNode(MoveTemp(LoadPromise), MoveTemp(Node));

				// Unloading Node
				FWwiseResourceUnloadPromise UnloadPromise;
				UnloadFutures[i] = UnloadPromise.GetFuture();

				NodeFuture.Next([&ResourceLoaderImpl, UnloadPromise = MoveTemp(UnloadPromise)](FWwiseLoadedEventPtr LoadedNode) mutable
				{
					WWISE_CHECK(LoadedNode);
					if (UNLIKELY(!LoadedNode))
					{
						UnloadPromise.EmplaceValue();
						return;
					}

					ResourceLoaderImpl->UnloadEventNode(MoveTemp(UnloadPromise), MoveTemp(LoadedNode));
				});
			};

			auto DoGroupValue = [&ResourceLoaderImpl, &GroupValueCookedDatas, i, &UnloadFutures]() mutable
			{
				// Creating Node
				auto* Node = ResourceLoaderImpl->CreateGroupValueListEntry(GroupValueCookedDatas[i%CookedDataCount]);

				WWISE_CHECK(Node);
				if (UNLIKELY(!Node))
				{
					return;
				}

				// Loading Node
				FWwiseLoadedGroupValuePromise LoadPromise;
				auto NodeFuture = LoadPromise.GetFuture();
				ResourceLoaderImpl->LoadGroupValueNode(MoveTemp(LoadPromise), MoveTemp(Node));

				// Unloading Node
				FWwiseResourceUnloadPromise UnloadPromise;
				UnloadFutures[NodeCount+i] = UnloadPromise.GetFuture();

				NodeFuture.Next([&ResourceLoaderImpl, UnloadPromise = MoveTemp(UnloadPromise)](FWwiseLoadedGroupValuePtr LoadedNode) mutable
				{
					WWISE_CHECK(LoadedNode);
					if (UNLIKELY(!LoadedNode))
					{
						UnloadPromise.EmplaceValue();
						return;
					}

					ResourceLoaderImpl->UnloadGroupValueNode(MoveTemp(UnloadPromise), MoveTemp(LoadedNode));
				});
			};

			// Vary which one is done first
			if (i & 4)
			{
				DoEvent();
				DoGroupValue();
			}
			else
			{
				DoGroupValue();
				DoEvent();
			}
		}

		for (auto i = 0; i < FuturesCount; ++i)
		{
			UnloadFutures[i].Get();
		}

		WWISE_CHECK(ExternalSourceManager->IsEmpty());
		WWISE_CHECK(MediaManager->IsEmpty());
		WWISE_CHECK(SoundBankManager->IsEmpty());
		WWISE_CHECK(ResourceLoaderImpl->IsEmpty());
		UE_LOG(LogWwiseResourceLoader, Log, TEXT("Finished Resource Loader Tests"))
	}

	SECTION("Async External Source")
	{
		TSharedRef<FWwiseMockExternalSourceManager> ExternalSourceManager = MakeShared<FWwiseMockExternalSourceManager>();
		auto MediaManager = MakeShared<FWwiseMockMediaManager>();
		auto SoundBankManager = MakeShared<FWwiseMockSoundBankManager>();
		auto ResourceLoaderImpl = MakeShared<FWwiseResourceLoaderImpl>(ExternalSourceManager, MediaManager, SoundBankManager);

		FWwiseExternalSourceCookedData CookedDatas[CookedDataCount];

		for (auto i = 0; i < CookedDataCount; ++i)
		{
			CookedDatas[i].Cookie = i % FileCount;
		}

		FWwiseResourceUnloadFuture UnloadFutures[NodeCount];
		for (auto i = 0; i < NodeCount; ++i)
		{
			auto* Node = ResourceLoaderImpl->CreateExternalSourceListEntry(CookedDatas[i%CookedDataCount]);

			WWISE_CHECK(Node);

			if (UNLIKELY(!Node))
			{
				continue;
			}

			// Loading node
			FWwiseLoadedExternalSourcePromise LoadPromise;
			auto NodeFuture = LoadPromise.GetFuture();
			ResourceLoaderImpl->LoadExternalSourceNode(MoveTemp(LoadPromise), MoveTemp(Node));

			// Unloading node
			FWwiseResourceUnloadPromise UnloadPromise;
			UnloadFutures[i] = UnloadPromise.GetFuture();

			NodeFuture.Next([&ResourceLoaderImpl, UnloadPromise = MoveTemp(UnloadPromise)](FWwiseLoadedExternalSourcePtr LoadedNode) mutable
			{
				WWISE_CHECK(LoadedNode);
				if (UNLIKELY(!LoadedNode))
				{
					UnloadPromise.EmplaceValue();
					return;
				}

				ResourceLoaderImpl->UnloadExternalSourceNode(MoveTemp(UnloadPromise), MoveTemp(LoadedNode));
			});
		}

		for (auto i = 0; i < NodeCount; ++i)
		{
			UnloadFutures[i].Get();
		}

		WWISE_CHECK(ExternalSourceManager->IsEmpty());
		WWISE_CHECK(MediaManager->IsEmpty());
		WWISE_CHECK(SoundBankManager->IsEmpty());
		WWISE_CHECK(ResourceLoaderImpl->IsEmpty());
	}



	SECTION("Async InitBank")
	{
		TSharedRef<FWwiseMockExternalSourceManager> ExternalSourceManager = MakeShared<FWwiseMockExternalSourceManager>();
		auto MediaManager = MakeShared<FWwiseMockMediaManager>();
		auto SoundBankManager = MakeShared<FWwiseMockSoundBankManager>();
		auto ResourceLoaderImpl = MakeShared<FWwiseResourceLoaderImpl>(ExternalSourceManager, MediaManager, SoundBankManager);

		FWwiseInitBankCookedData CookedDatas[CookedDataCount];
		for (auto i = 0; i < CookedDataCount; ++i)
		{
			CookedDatas[i].Media.Emplace(FWwiseMediaCookedData{});
			CookedDatas[i].Media[0].MediaId = i % FileCount;
		}
		
		FWwiseResourceUnloadFuture UnloadFutures[NodeCount];
		for (auto i = 0; i < NodeCount; ++i)
		{
			// Creating Node
			auto* Node = ResourceLoaderImpl->CreateInitBankListEntry(CookedDatas[i%CookedDataCount]);

			WWISE_CHECK(Node);
			if (UNLIKELY(!Node))
			{
				continue;
			}

			// Loading Node
			FWwiseLoadedInitBankPromise LoadPromise;
			auto NodeFuture = LoadPromise.GetFuture();
			ResourceLoaderImpl->LoadInitBankNode(MoveTemp(LoadPromise), MoveTemp(Node));

			// Unloading Node
			FWwiseResourceUnloadPromise UnloadPromise;
			UnloadFutures[i] = UnloadPromise.GetFuture();

			NodeFuture.Next([&ResourceLoaderImpl, UnloadPromise = MoveTemp(UnloadPromise)](FWwiseLoadedInitBankPtr LoadedNode) mutable
			{
				WWISE_CHECK(LoadedNode);
				if (UNLIKELY(!LoadedNode))
				{
					UnloadPromise.EmplaceValue();
					return;
				}

				ResourceLoaderImpl->UnloadInitBankNode(MoveTemp(UnloadPromise), MoveTemp(LoadedNode));
			});
		}

		for (auto i = 0; i < NodeCount; ++i)
		{
			UnloadFutures[i].Get();
		}

		WWISE_CHECK(ExternalSourceManager->IsEmpty());
		WWISE_CHECK(MediaManager->IsEmpty());
		WWISE_CHECK(SoundBankManager->IsEmpty());
		WWISE_CHECK(ResourceLoaderImpl->IsEmpty());
	}
	
	SECTION("Async Media")
	{
		TSharedRef<FWwiseMockExternalSourceManager> ExternalSourceManager = MakeShared<FWwiseMockExternalSourceManager>();
		auto MediaManager = MakeShared<FWwiseMockMediaManager>();
		auto SoundBankManager = MakeShared<FWwiseMockSoundBankManager>();
		auto ResourceLoaderImpl = MakeShared<FWwiseResourceLoaderImpl>(ExternalSourceManager, MediaManager, SoundBankManager);

		FWwiseMediaCookedData CookedDatas[CookedDataCount];
		for (auto i = 0; i < CookedDataCount; ++i)
		{
			CookedDatas[i].MediaId = i % FileCount;
			CookedDatas[i].PackagedFile.bStreaming = !!(i % 2);
		}
		
		FWwiseResourceUnloadFuture UnloadFutures[NodeCount];
		for (auto i = 0; i < NodeCount; ++i)
		{
			// Creating Node
			auto* Node = ResourceLoaderImpl->CreateMediaListEntry(CookedDatas[i%CookedDataCount]);

			WWISE_CHECK(Node);
			if (UNLIKELY(!Node))
			{
				continue;
			}

			// Loading Node
			FWwiseLoadedMediaPromise LoadPromise;
			auto NodeFuture = LoadPromise.GetFuture();
			ResourceLoaderImpl->LoadMediaNode(MoveTemp(LoadPromise), MoveTemp(Node));

			// Unloading Node
			FWwiseResourceUnloadPromise UnloadPromise;
			UnloadFutures[i] = UnloadPromise.GetFuture();

			NodeFuture.Next([&ResourceLoaderImpl, UnloadPromise = MoveTemp(UnloadPromise)](FWwiseLoadedMediaPtr LoadedNode) mutable
			{
				WWISE_CHECK(LoadedNode);
				if (UNLIKELY(!LoadedNode))
				{
					UnloadPromise.EmplaceValue();
					return;
				}

				ResourceLoaderImpl->UnloadMediaNode(MoveTemp(UnloadPromise), MoveTemp(LoadedNode));
			});
		}

		for (auto i = 0; i < NodeCount; ++i)
		{
			UnloadFutures[i].Get();
		}

		WWISE_CHECK(ExternalSourceManager->IsEmpty());
		WWISE_CHECK(MediaManager->IsEmpty());
		WWISE_CHECK(SoundBankManager->IsEmpty());
		WWISE_CHECK(ResourceLoaderImpl->IsEmpty());
	}
	
	SECTION("Async ShareSet")
	{
		TSharedRef<FWwiseMockExternalSourceManager> ExternalSourceManager = MakeShared<FWwiseMockExternalSourceManager>();
		auto MediaManager = MakeShared<FWwiseMockMediaManager>();
		auto SoundBankManager = MakeShared<FWwiseMockSoundBankManager>();
		auto ResourceLoaderImpl = MakeShared<FWwiseResourceLoaderImpl>(ExternalSourceManager, MediaManager, SoundBankManager);

		FWwiseLocalizedShareSetCookedData CookedDatas[CookedDataCount];
		for (auto i = 0; i < CookedDataCount; ++i)
		{
			FWwiseShareSetCookedData Data;
			Data.ShareSetId = i % CookedDataCount; 
			Data.Media.Emplace(FWwiseMediaCookedData{});
			Data.Media[0].MediaId = i % FileCount;
			Data.SoundBanks.Emplace(FWwiseSoundBankCookedData{});
			CookedDatas[i].ShareSetLanguageMap.Emplace(FWwiseLanguageCookedData::Sfx, MoveTemp(Data));
		}
		
		FWwiseResourceUnloadFuture UnloadFutures[NodeCount];
		for (auto i = 0; i < NodeCount; ++i)
		{
			// Creating Node
			auto* Node = ResourceLoaderImpl->CreateShareSetListEntry(CookedDatas[i%CookedDataCount], &FWwiseLanguageCookedData::Sfx);

			WWISE_CHECK(Node);
			if (UNLIKELY(!Node))
			{
				continue;
			}

			// Loading Node
			FWwiseLoadedShareSetPromise LoadPromise;
			auto NodeFuture = LoadPromise.GetFuture();
			ResourceLoaderImpl->LoadShareSetNode(MoveTemp(LoadPromise), MoveTemp(Node));

			// Unloading Node
			FWwiseResourceUnloadPromise UnloadPromise;
			UnloadFutures[i] = UnloadPromise.GetFuture();

			NodeFuture.Next([&ResourceLoaderImpl, UnloadPromise = MoveTemp(UnloadPromise)](FWwiseLoadedShareSetPtr LoadedNode) mutable
			{
				WWISE_CHECK(LoadedNode);
				if (UNLIKELY(!LoadedNode))
				{
					UnloadPromise.EmplaceValue();
					return;
				}

				ResourceLoaderImpl->UnloadShareSetNode(MoveTemp(UnloadPromise), MoveTemp(LoadedNode));
			});
		}

		for (auto i = 0; i < NodeCount; ++i)
		{
			UnloadFutures[i].Get();
		}

		WWISE_CHECK(ExternalSourceManager->IsEmpty());
		WWISE_CHECK(MediaManager->IsEmpty());
		WWISE_CHECK(SoundBankManager->IsEmpty());
		WWISE_CHECK(ResourceLoaderImpl->IsEmpty());
	}

	SECTION("Async SoundBank")
	{
		TSharedRef<FWwiseMockExternalSourceManager> ExternalSourceManager = MakeShared<FWwiseMockExternalSourceManager>();
		auto MediaManager = MakeShared<FWwiseMockMediaManager>();
		auto SoundBankManager = MakeShared<FWwiseMockSoundBankManager>();
		auto ResourceLoaderImpl = MakeShared<FWwiseResourceLoaderImpl>(ExternalSourceManager, MediaManager, SoundBankManager);

		FWwiseLocalizedSoundBankCookedData CookedDatas[CookedDataCount];
		for (auto i = 0; i < CookedDataCount; ++i)
		{
			FWwiseSoundBankCookedData Data;
			Data.SoundBankId = i % FileCount;
			CookedDatas[i].SoundBankLanguageMap.Emplace(FWwiseLanguageCookedData::Sfx, MoveTemp(Data));
		}
		
		FWwiseResourceUnloadFuture UnloadFutures[NodeCount];
		for (auto i = 0; i < NodeCount; ++i)
		{
			// Creating Node
			auto* Node = ResourceLoaderImpl->CreateSoundBankListEntry(CookedDatas[i%CookedDataCount], nullptr);

			WWISE_CHECK(Node);
			if (UNLIKELY(!Node))
			{
				continue;
			}

			// Loading Node
			FWwiseLoadedSoundBankPromise LoadPromise;
			auto NodeFuture = LoadPromise.GetFuture();
			ResourceLoaderImpl->LoadSoundBankNode(MoveTemp(LoadPromise), MoveTemp(Node));

			// Unloading Node
			FWwiseResourceUnloadPromise UnloadPromise;
			UnloadFutures[i] = UnloadPromise.GetFuture();

			NodeFuture.Next([&ResourceLoaderImpl, UnloadPromise = MoveTemp(UnloadPromise)](FWwiseLoadedSoundBankPtr LoadedNode) mutable
			{
				WWISE_CHECK(LoadedNode);
				if (UNLIKELY(!LoadedNode))
				{
					UnloadPromise.EmplaceValue();
					return;
				}

				ResourceLoaderImpl->UnloadSoundBankNode(MoveTemp(UnloadPromise), MoveTemp(LoadedNode));
			});
		}

		for (auto i = 0; i < NodeCount; ++i)
		{
			UnloadFutures[i].Get();
		}

		WWISE_CHECK(ExternalSourceManager->IsEmpty());
		WWISE_CHECK(MediaManager->IsEmpty());
		WWISE_CHECK(SoundBankManager->IsEmpty());
		WWISE_CHECK(ResourceLoaderImpl->IsEmpty());
	}
}

WWISE_TEST_CASE(ResourceLoader_EdgeCases, "Wwise::ResourceLoader::ResourceLoader_EdgeCases", "[ApplicationContextMask][ProductFilter]")
{
	SECTION("GroupValue PendingKill Reload")
	{
		static constexpr const auto Count = 16;
	
		const bool bMockSleepOnMediaLoad = FWwiseResourceLoaderImpl::Test::bMockSleepOnMediaLoad;
		FWwiseResourceLoaderImpl::Test::bMockSleepOnMediaLoad = true;
		ON_SCOPE_EXIT { FWwiseResourceLoaderImpl::Test::bMockSleepOnMediaLoad = bMockSleepOnMediaLoad; };

		TSharedRef<FWwiseMockExternalSourceManager> ExternalSourceManager = MakeShared<FWwiseMockExternalSourceManager>();
		auto MediaManager = MakeShared<FWwiseMockMediaManager>();
		auto SoundBankManager = MakeShared<FWwiseMockSoundBankManager>();
		auto ResourceLoaderImpl = MakeShared<FWwiseResourceLoaderImpl>(ExternalSourceManager, MediaManager, SoundBankManager);

		FWwiseLocalizedEventCookedData EventCookedDatas[Count];
		FWwiseGroupValueCookedData GroupValueCookedDatas[Count];

		for (auto i = 0; i < Count; ++i)
		{
			// Event
			FWwiseEventCookedData Data;
			Data.EventId = i;
			Data.Media.Emplace(FWwiseMediaCookedData{});
			Data.Media[0].MediaId = i + Count;

			FWwiseAudioNodeCookedData AudioNode;
			FWwiseMediaCookedData AudioNodeMedia;
			AudioNodeMedia.MediaId = i;
			AudioNode.Media.Emplace(MoveTemp(AudioNodeMedia));

			FWwiseGroupValueCookedData AudioNodeGroupValue;
			AudioNodeGroupValue.Id = i;
			AudioNodeGroupValue.GroupId = 1;
			AudioNodeGroupValue.Type = EWwiseGroupType::Switch;
			FWwiseGroupValueCookedDataSet AudioNode1GroupValueSet;
			AudioNode1GroupValueSet.GroupValues.Emplace(MoveTemp(AudioNodeGroupValue));
			Data.AudioNodes.Emplace(MoveTemp(AudioNode1GroupValueSet), MoveTemp(AudioNode));

			EventCookedDatas[i].EventLanguageMap.Emplace(FWwiseLanguageCookedData::Sfx, MoveTemp(Data));

			// GroupValue
			GroupValueCookedDatas[i].Id = i;
			GroupValueCookedDatas[i].GroupId = 1;
			GroupValueCookedDatas[i].Type = EWwiseGroupType::Switch;
		}

		constexpr const auto FuturesCount = Count * 2;
		FWwiseLoadedEventFuture LoadFutures[Count];
		FWwiseResourceUnloadFuture UnloadFutures[FuturesCount];

		for (auto i = 0; i < Count; ++i)
		{
			auto DoEvent = [&LoadFutures, &ResourceLoaderImpl, &EventCookedDatas, i]() mutable
			{
				// Creating Node
				auto* Node = ResourceLoaderImpl->CreateEventListEntry(EventCookedDatas[i%Count], nullptr);

				WWISE_CHECK(Node);
				if (UNLIKELY(!Node))
				{
					return;
				}

				// Loading Node 1
				FWwiseLoadedEventPromise LoadPromise;
				LoadFutures[i] = LoadPromise.GetFuture();
				ResourceLoaderImpl->LoadEventNode(MoveTemp(LoadPromise), MoveTemp(Node));
				LoadFutures[i].Wait();
			};

			auto UnloadEvent = [i, &ResourceLoaderImpl, &LoadFutures, &UnloadFutures]() mutable
			{
				// Unloading Event
				FWwiseResourceUnloadPromise UnloadPromise;
				UnloadFutures[i] = UnloadPromise.GetFuture();
				ResourceLoaderImpl->UnloadEventNode(MoveTemp(UnloadPromise), std::move(LoadFutures[i].Get()));
			};

			auto DoGroupValue = [&ResourceLoaderImpl, &GroupValueCookedDatas, i, &UnloadFutures]() mutable
			{
				// Creating Node
				auto* Node = ResourceLoaderImpl->CreateGroupValueListEntry(GroupValueCookedDatas[i%Count]);

				WWISE_CHECK(Node);
				if (UNLIKELY(!Node))
				{
					return;
				}

				// Loading Node 1
				FWwiseLoadedGroupValuePromise LoadPromise;
				auto NodeFuture = LoadPromise.GetFuture();
				ResourceLoaderImpl->LoadGroupValueNode(MoveTemp(LoadPromise), MoveTemp(Node));

				// Future for completion (Unloading Node 2)
				FWwiseResourceUnloadPromise UnloadPromise2;
				UnloadFutures[Count+i] = UnloadPromise2.GetFuture();

				NodeFuture.Next([i, &GroupValueCookedDatas, &ResourceLoaderImpl, UnloadPromise2 = MoveTemp(UnloadPromise2)](FWwiseLoadedGroupValuePtr LoadedNode) mutable
				{
					WWISE_CHECK(LoadedNode);
					if (UNLIKELY(!LoadedNode))
					{
						UnloadPromise2.EmplaceValue();
						return;
					}

					// Loading Node 2
					auto* Node = ResourceLoaderImpl->CreateGroupValueListEntry(GroupValueCookedDatas[i%Count]);
					FWwiseLoadedGroupValuePromise LoadPromise2;
					auto NodeFuture2 = LoadPromise2.GetFuture();

					if (i&8)
					{
						ResourceLoaderImpl->LoadGroupValueNode(MoveTemp(LoadPromise2), MoveTemp(Node));
					}

					// Unload Node 1
					FWwiseResourceUnloadPromise UnloadPromise;
					auto UnloadFuture = UnloadPromise.GetFuture();
					ResourceLoaderImpl->UnloadGroupValueNode(MoveTemp(UnloadPromise), MoveTemp(LoadedNode));

					if (!(i&8))
					{
						ResourceLoaderImpl->LoadGroupValueNode(MoveTemp(LoadPromise2), MoveTemp(Node));
					}

					// Wait for Loading Node 2
					NodeFuture2.Next([&ResourceLoaderImpl, UnloadFuture = MoveTemp(UnloadFuture), UnloadPromise2 = MoveTemp(UnloadPromise2)](FWwiseLoadedGroupValuePtr LoadedNode) mutable
					{
						WWISE_CHECK(LoadedNode);
						if (UNLIKELY(!LoadedNode))
						{
							UnloadPromise2.EmplaceValue();
							return;
						}

						// Wait for Unloading Node 1
						UnloadFuture.Next([&ResourceLoaderImpl, UnloadPromise2 = MoveTemp(UnloadPromise2), LoadedNode](int) mutable
						{
							// Unloading Node 2 and terminate
							ResourceLoaderImpl->UnloadGroupValueNode(MoveTemp(UnloadPromise2), MoveTemp(LoadedNode));
						});
					});
				});
			};

			DoEvent();
			DoGroupValue();
			UnloadEvent();
		}

		for (auto i = 0; i < FuturesCount; ++i)
		{
			UnloadFutures[i].Get();
		}

		WWISE_CHECK(ExternalSourceManager->IsEmpty());
		WWISE_CHECK(MediaManager->IsEmpty());
		WWISE_CHECK(SoundBankManager->IsEmpty());
		WWISE_CHECK(ResourceLoaderImpl->IsEmpty());
	}

	SECTION("Event PendingKill Reload")
	{
		static constexpr const auto Count = 16;
	
		const bool bMockSleepOnMediaLoad = FWwiseResourceLoaderImpl::Test::bMockSleepOnMediaLoad;
		FWwiseResourceLoaderImpl::Test::bMockSleepOnMediaLoad = true;
		ON_SCOPE_EXIT { FWwiseResourceLoaderImpl::Test::bMockSleepOnMediaLoad = bMockSleepOnMediaLoad; };

		TSharedRef<FWwiseMockExternalSourceManager> ExternalSourceManager = MakeShared<FWwiseMockExternalSourceManager>();
		auto MediaManager = MakeShared<FWwiseMockMediaManager>();
		auto SoundBankManager = MakeShared<FWwiseMockSoundBankManager>();
		auto ResourceLoaderImpl = MakeShared<FWwiseResourceLoaderImpl>(ExternalSourceManager, MediaManager, SoundBankManager);

		FWwiseLocalizedEventCookedData EventCookedDatas[Count];
		FWwiseGroupValueCookedData GroupValueCookedDatas[Count];

		for (auto i = 0; i < Count; ++i)
		{
			// Event
			FWwiseEventCookedData Data;
			Data.EventId = i;
			Data.Media.Emplace(FWwiseMediaCookedData{});
			Data.Media[0].MediaId = i + Count;

			FWwiseAudioNodeCookedData AudioNode;
			FWwiseMediaCookedData AudioNodeMedia;
			AudioNodeMedia.MediaId = i;
			AudioNode.Media.Emplace(MoveTemp(AudioNodeMedia));

			FWwiseGroupValueCookedData AudioNodeGroupValue;
			AudioNodeGroupValue.Id = i;
			AudioNodeGroupValue.GroupId = 1;
			AudioNodeGroupValue.Type = EWwiseGroupType::Switch;
			FWwiseGroupValueCookedDataSet AudioNode1GroupValueSet;
			AudioNode1GroupValueSet.GroupValues.Emplace(MoveTemp(AudioNodeGroupValue));
			Data.AudioNodes.Emplace(MoveTemp(AudioNode1GroupValueSet), MoveTemp(AudioNode));

			EventCookedDatas[i].EventLanguageMap.Emplace(FWwiseLanguageCookedData::Sfx, MoveTemp(Data));

			// GroupValue
			GroupValueCookedDatas[i].Id = i;
			GroupValueCookedDatas[i].GroupId = 1;
			GroupValueCookedDatas[i].Type = EWwiseGroupType::Switch;
		}

		constexpr const auto FuturesCount = Count * 2;
		FWwiseLoadedGroupValueFuture LoadFutures[Count];
		FWwiseResourceUnloadFuture UnloadFutures[FuturesCount];
		
		for (auto i = 0; i < Count; ++i)
		{
			auto DoEvent = [&ResourceLoaderImpl, &EventCookedDatas, i, &UnloadFutures]() mutable
			{
				// Creating Node
				auto* Node = ResourceLoaderImpl->CreateEventListEntry(EventCookedDatas[i%Count], nullptr);

				WWISE_CHECK(Node);
				if (UNLIKELY(!Node))
				{
					return;
				}

				// Loading Node 1
				FWwiseLoadedEventPromise LoadPromise;
				auto NodeFuture = LoadPromise.GetFuture();
				ResourceLoaderImpl->LoadEventNode(MoveTemp(LoadPromise), MoveTemp(Node));

				// Future for completion (Unloading Node 2)
				FWwiseResourceUnloadPromise UnloadPromise2;
				UnloadFutures[i] = UnloadPromise2.GetFuture();

				NodeFuture.Next([i, &EventCookedDatas, &ResourceLoaderImpl, UnloadPromise2 = MoveTemp(UnloadPromise2)](FWwiseLoadedEventPtr LoadedNode) mutable
				{
					WWISE_CHECK(LoadedNode);
					if (UNLIKELY(!LoadedNode))
					{
						UnloadPromise2.EmplaceValue();
						return;
					}

					// Creating Node 2
					auto* Node2 = ResourceLoaderImpl->CreateEventListEntry(EventCookedDatas[i%Count], nullptr);

					// Loading Node 2
                    FWwiseLoadedEventPromise LoadPromise2;
                    auto NodeFuture2 = LoadPromise2.GetFuture();
					if (i&4)
					{
						ResourceLoaderImpl->LoadEventNode(MoveTemp(LoadPromise2), MoveTemp(Node2));
					}

					// Unloading Node 1
					FWwiseResourceUnloadPromise UnloadPromise;
					auto UnloadFuture = UnloadPromise.GetFuture();
					ResourceLoaderImpl->UnloadEventNode(MoveTemp(UnloadPromise), MoveTemp(LoadedNode));

					if (!(i&4))
					{
						ResourceLoaderImpl->LoadEventNode(MoveTemp(LoadPromise2), MoveTemp(Node2));
					}

					// Wait for Loading Node 2
					NodeFuture2.Next([&ResourceLoaderImpl, UnloadFuture = MoveTemp(UnloadFuture), UnloadPromise2 = MoveTemp(UnloadPromise2)](FWwiseLoadedEventPtr LoadedNode) mutable
					{
						WWISE_CHECK(LoadedNode);
						if (UNLIKELY(!LoadedNode))
						{
							UnloadPromise2.EmplaceValue();
							return;
						}

						// Wait for Unloading Node 1
						UnloadFuture.Next([&ResourceLoaderImpl, UnloadPromise2 = MoveTemp(UnloadPromise2), LoadedNode](int) mutable
						{
							// Unloading Node 2 and terminate
							ResourceLoaderImpl->UnloadEventNode(MoveTemp(UnloadPromise2), MoveTemp(LoadedNode));
						});
					});
				});
			};

			auto DoGroupValue = [&LoadFutures, &ResourceLoaderImpl, &GroupValueCookedDatas, i]() mutable
			{
				// Creating Node
				auto* Node = ResourceLoaderImpl->CreateGroupValueListEntry(GroupValueCookedDatas[i%Count]);

				WWISE_CHECK(Node);
				if (UNLIKELY(!Node))
				{
					return;
				}

				// Loading Node 1
				FWwiseLoadedGroupValuePromise LoadPromise;
				LoadFutures[i] = LoadPromise.GetFuture();
				ResourceLoaderImpl->LoadGroupValueNode(MoveTemp(LoadPromise), MoveTemp(Node));
				LoadFutures[i].Wait();
			};

			auto UnloadGroupValue = [&LoadFutures, &ResourceLoaderImpl, i, &UnloadFutures]() mutable
			{

				// Future for completion (Unloading Node 2)
				FWwiseResourceUnloadPromise UnloadPromise2;
				UnloadFutures[Count+i] = UnloadPromise2.GetFuture();
				ResourceLoaderImpl->UnloadGroupValueNode(MoveTemp(UnloadPromise2), std::move(LoadFutures[i].Get()));
			};

			DoEvent();
			DoGroupValue();
			UnloadGroupValue();
		}

		for (auto i = 0; i < FuturesCount; ++i)
		{
			UnloadFutures[i].Get();
		}

		WWISE_CHECK(ExternalSourceManager->IsEmpty());
		WWISE_CHECK(MediaManager->IsEmpty());
		WWISE_CHECK(SoundBankManager->IsEmpty());
		WWISE_CHECK(ResourceLoaderImpl->IsEmpty());
	}
}

WWISE_TEST_CASE(ResourceLoader_Stress, "Wwise::ResourceLoader::ResourceLoader_Stress", "[ApplicationContextMask][StressFilter]")
{
	SECTION("Random All")
	{
		static constexpr const auto NumOp = 10000;

		static constexpr const uint32 NumAudioNodes = 191;
		static constexpr const uint32 NumAuxBusses = 31;
		static constexpr const uint32 NumDialogueEvents = 249;
		static constexpr const uint32 NumEvents = 501;
		static constexpr const uint32 NumExternalSources = 16;
		static constexpr const uint32 NumInitBanks = 10;
		static constexpr const uint32 NumMedia = 2000;
		static constexpr const uint32 NumShareSets = 15;
		static constexpr const uint32 NumSoundBanks = 200;
		static constexpr const uint32 NumStates = 17;
		static constexpr const uint32 NumSwitches = 14;
		static constexpr const uint32 MaxLeaves = 10;

		auto AudioNodesPtr = std::make_unique<std::array<FWwiseAudioNodeCookedData, NumAudioNodes>>();
		auto AuxBussesPtr = std::make_unique<std::array<FWwiseLocalizedAuxBusCookedData, NumAuxBusses>>();
		auto DialogueEventsPtr = std::make_unique<std::array<FWwiseLocalizedDialogueEventCookedData, NumDialogueEvents>>();
		auto EventsPtr = std::make_unique<std::array<FWwiseLocalizedEventCookedData, NumEvents>>();
		auto ExternalSourcePtr = std::make_unique<std::array<FWwiseExternalSourceCookedData, NumExternalSources>>();
		auto InitBanksPtr = std::make_unique<std::array<FWwiseInitBankCookedData, NumInitBanks>>();
		auto MediaPtr = std::make_unique<std::array<FWwiseMediaCookedData, NumMedia>>();
		auto ShareSetsPtr = std::make_unique<std::array<FWwiseLocalizedShareSetCookedData, NumShareSets>>();
		auto SoundBanksPtr = std::make_unique<std::array<FWwiseLocalizedSoundBankCookedData, NumSoundBanks>>();
		auto StatesPtr = std::make_unique<std::array<FWwiseGroupValueCookedData, NumStates>>();
		auto SwitchesPtr = std::make_unique<std::array<FWwiseGroupValueCookedData, NumSwitches>>();

		auto& AudioNodes = *AudioNodesPtr;
		auto& AuxBusses = *AuxBussesPtr;
		auto& DialogueEvents = *DialogueEventsPtr;
		auto& Events = *EventsPtr;
		auto& ExternalSources = *ExternalSourcePtr;
		auto& InitBanks = *InitBanksPtr;
		auto& Media = *MediaPtr;
		auto& ShareSets = *ShareSetsPtr;
		auto& SoundBanks = *SoundBanksPtr;
		auto& States = *StatesPtr;
		auto& Switches = *SwitchesPtr;

		// Fill up data
		{
			int32 Id = 0;
			
			for (auto& Medii : Media)
			{
				const auto Rnd = WwiseHashCombineFast(GetTypeHash(Id), 54209201);
				Medii.MediaId = ++Id;
				Medii.PackagedFile.bStreaming = (Rnd % 4) == 0; 
			}

			for (auto& SoundBank : SoundBanks)
			{
				FWwiseSoundBankCookedData Data;
				Data.SoundBankId = ++Id;
				SoundBank.SoundBankLanguageMap.Add(FWwiseLanguageCookedData::Sfx, MoveTemp(Data));
			}

			for (auto& ShareSet : ShareSets)
			{
				FWwiseShareSetCookedData Data;
				Data.ShareSetId = ++Id;
				auto RequiredMediaCount = WwiseHashCombineFast(GetTypeHash(Id), 139) % (MaxLeaves*2);
				if (RequiredMediaCount > MaxLeaves) RequiredMediaCount = 0;
				
				auto RequiredSoundBankCount = WwiseHashCombineFast(GetTypeHash(Id),295) % (MaxLeaves*3);
				if (RequiredSoundBankCount > MaxLeaves) RequiredSoundBankCount = 0;

				for (uint32 i=0; i < RequiredMediaCount; ++i)
				{
					const auto MediaToAdd = WwiseHashCombineFast(GetTypeHash(Id), i ^ 9042) % NumMedia;
					Data.Media.Add(Media[MediaToAdd]);
				}

				for (uint32 i=0; i < RequiredSoundBankCount; ++i)
				{
					const auto SoundBankToAdd = WwiseHashCombineFast(GetTypeHash(Id), i ^ 5930) % NumSoundBanks;
					Data.SoundBanks.Add(SoundBanks[SoundBankToAdd].SoundBankLanguageMap[FWwiseLanguageCookedData::Sfx]);
				}
				ShareSet.ShareSetLanguageMap.Add(FWwiseLanguageCookedData::Sfx, MoveTemp(Data));
			}

			for (auto& State : States)
			{
				State.GroupId = 0;
				State.Id = ++Id;
				State.Type = EWwiseGroupType::State;
			}

			for (auto& Switch : Switches)
			{
				Switch.GroupId = 0;
				Switch.Id = ++Id;
				Switch.Type = EWwiseGroupType::Switch;
			}

			for (auto& AudioNode : AudioNodes)
			{
				AudioNode.AudioNodeId = ++Id;
				auto RequiredMediaCount = WwiseHashCombineFast(GetTypeHash(Id), 19593) % (MaxLeaves*3);
				if (RequiredMediaCount > MaxLeaves*2.5) RequiredMediaCount = 0;
				
				auto RequiredSoundBankCount = WwiseHashCombineFast(GetTypeHash(Id), 99595) % (MaxLeaves/2);
				if (RequiredSoundBankCount > MaxLeaves/2.5) RequiredSoundBankCount = 0;

				for (uint32 i=0; i < RequiredMediaCount; ++i)
				{
					const auto MediaToAdd = WwiseHashCombineFast(GetTypeHash(Id), i ^ 9385934) % NumMedia;
					AudioNode.Media.Add(Media[MediaToAdd]);
				}

				for (uint32 i=0; i < RequiredSoundBankCount; ++i)
				{
					const auto SoundBankToAdd = WwiseHashCombineFast(GetTypeHash(Id), i ^ 283985) % NumSoundBanks;
					AudioNode.SoundBanks.Add(SoundBanks[SoundBankToAdd].SoundBankLanguageMap[FWwiseLanguageCookedData::Sfx]);
				}
			}
			
			for (auto& AuxBus : AuxBusses)
			{
				FWwiseAuxBusCookedData Data;
				Data.AuxBusId = ++Id;
				auto RequiredMediaCount = WwiseHashCombineFast(GetTypeHash(Id), 57381) % (MaxLeaves*2);
				if (RequiredMediaCount > MaxLeaves) RequiredMediaCount = 0;
				
				auto RequiredSoundBankCount = WwiseHashCombineFast(GetTypeHash(Id), 414) % (MaxLeaves*3);
				if (RequiredSoundBankCount > MaxLeaves) RequiredSoundBankCount = 0;

				for (uint32 i=0; i < RequiredMediaCount; ++i)
				{
					const auto MediaToAdd = WwiseHashCombineFast(GetTypeHash(Id), i ^ 9385938) % NumMedia;
					Data.Media.Add(Media[MediaToAdd]);
				}

				for (uint32 i=0; i < RequiredSoundBankCount; ++i)
				{
					const auto SoundBankToAdd = WwiseHashCombineFast(GetTypeHash(Id), i ^ 223982) % NumSoundBanks;
					Data.SoundBanks.Add(SoundBanks[SoundBankToAdd].SoundBankLanguageMap[FWwiseLanguageCookedData::Sfx]);
				}
				AuxBus.AuxBusLanguageMap.Add(FWwiseLanguageCookedData::Sfx, MoveTemp(Data));
			}
			
			for (auto& InitBank : InitBanks)
			{
				InitBank.SoundBankId = ++Id;
				auto RequiredMediaCount = WwiseHashCombineFast(GetTypeHash(Id), 1758317) % (MaxLeaves*2);
				if (RequiredMediaCount > MaxLeaves) RequiredMediaCount = 0;

				for (uint32 i=0; i < RequiredMediaCount; ++i)
				{
					const auto MediaToAdd = WwiseHashCombineFast(GetTypeHash(Id), i ^ 190469) % NumMedia;
					InitBank.Media.Add(Media[MediaToAdd]);
				}

			}

			for (auto& Event : DialogueEvents)
			{
				FWwiseDialogueEventCookedData Data;
				Data.DialogueEventId = ++Id;
				
				auto RequiredSoundBankCount = WwiseHashCombineFast(GetTypeHash(Id), 16692402) % (MaxLeaves/2) % NumSoundBanks;
				if (RequiredSoundBankCount > MaxLeaves/2.2) RequiredSoundBankCount = 0;

				auto RequiredAudioNodeCount = WwiseHashCombineFast(GetTypeHash(Id), 853019401) % (MaxLeaves*2) % NumAudioNodes;
				if (RequiredAudioNodeCount > MaxLeaves*1.5) RequiredAudioNodeCount = 0;

				for (uint32 i=0; i < RequiredSoundBankCount; ++i)
				{
					const auto SoundBankToAdd = WwiseHashCombineFast(GetTypeHash(Id), i ^ 3123982) % NumSoundBanks;
					Data.SoundBanks.Add(SoundBanks[SoundBankToAdd].SoundBankLanguageMap[FWwiseLanguageCookedData::Sfx]);
				}

				for (uint32 AudioNodeIter=0; AudioNodeIter < RequiredAudioNodeCount; ++AudioNodeIter)
				{
					const auto AudioNodeToAdd = WwiseHashCombineFast(GetTypeHash(Id), AudioNodeIter ^ 48389941) % NumAudioNodes;

					auto GroupValueCount = WwiseHashCombineFast(GetTypeHash(Id), AudioNodeIter ^ 2151584104) % MaxLeaves;
					if (GroupValueCount > MaxLeaves / 2) GroupValueCount = 0;
					++GroupValueCount; // At least one

					FWwiseGroupValueCookedDataSet GroupValueSet;
					for (uint32 i=0; i < GroupValueCount; ++i)
					{
						const bool bIsState = WwiseHashCombineFast(GetTypeHash(Id), i ^ AudioNodeIter ^ 8484531414) % 4 == 0;		// Mostly switches
						const auto NumToAdd = WwiseHashCombineFast(GetTypeHash(Id), i ^ AudioNodeIter ^ 9671952) % (bIsState ? NumStates : NumSwitches);
						GroupValueSet.GroupValues.Add(bIsState ? States[NumToAdd] : Switches[NumToAdd]);
					}
					Data.AudioNodes.Add(MoveTemp(GroupValueSet), AudioNodes[AudioNodeToAdd]);
				}

				Event.DialogueEventLanguageMap.Add(FWwiseLanguageCookedData::Sfx, MoveTemp(Data));
			}

			for (auto& Event : Events)
			{
				FWwiseEventCookedData Data;
				Data.EventId = ++Id;
				
				auto RequiredMediaCount = WwiseHashCombineFast(GetTypeHash(Id), 57381) % (MaxLeaves*2);
				if (RequiredMediaCount > MaxLeaves) RequiredMediaCount = 0;
				
				auto RequiredSoundBankCount = WwiseHashCombineFast(GetTypeHash(Id), 414) % (MaxLeaves*3);
				if (RequiredSoundBankCount > MaxLeaves) RequiredSoundBankCount = 0;

				auto RequiredGroupValueCount = WwiseHashCombineFast(GetTypeHash(Id), 462058) % (MaxLeaves*4);
				if (RequiredGroupValueCount > MaxLeaves) RequiredGroupValueCount = 0;
				
				auto RequiredSwitchContainerAudioNodeCount = WwiseHashCombineFast(GetTypeHash(Id), 953019401) % (MaxLeaves*2);
				if (RequiredSwitchContainerAudioNodeCount > MaxLeaves) RequiredSwitchContainerAudioNodeCount = 0;

				for (uint32 i=0; i < RequiredMediaCount; ++i)
				{
					const auto MediaToAdd = WwiseHashCombineFast(GetTypeHash(Id), i ^ 9385938) % NumMedia;
					Data.Media.Add(Media[MediaToAdd]);
				}

				for (uint32 i=0; i < RequiredSoundBankCount; ++i)
				{
					const auto SoundBankToAdd = WwiseHashCombineFast(GetTypeHash(Id), i ^ 223982) % NumSoundBanks;
					Data.SoundBanks.Add(SoundBanks[SoundBankToAdd].SoundBankLanguageMap[FWwiseLanguageCookedData::Sfx]);
				}

				for (uint32 i=0; i < RequiredGroupValueCount; ++i)
				{
					const bool bIsState = WwiseHashCombineFast(GetTypeHash(Id), i ^ 190101) % 4 == 0;		// Mostly switches
					const auto NumToAdd = WwiseHashCombineFast(GetTypeHash(Id), i ^ 415938) % (bIsState ? NumStates : NumSwitches);
					Data.RequiredGroupValueSet.Add(bIsState ? States[NumToAdd] : Switches[NumToAdd]);
				}

				for (uint32 AudioNodeIter=0; AudioNodeIter < RequiredSwitchContainerAudioNodeCount; ++AudioNodeIter)
				{
					FWwiseAudioNodeCookedData AudioNode;
					auto MediaCount = WwiseHashCombineFast(GetTypeHash(Id), AudioNodeIter ^ 4863419941) % (MaxLeaves * 2);
					if (MediaCount > MaxLeaves) MediaCount = 0;
					++MediaCount; // At least one
				
					auto SoundBankCount = WwiseHashCombineFast(GetTypeHash(Id), AudioNodeIter ^ 1065842795) % (MaxLeaves*3);
					if (SoundBankCount > MaxLeaves) SoundBankCount = 0;

					auto GroupValueCount = WwiseHashCombineFast(GetTypeHash(Id), AudioNodeIter ^ 2828284104) % MaxLeaves;
					if (GroupValueCount > MaxLeaves / 2) GroupValueCount = 0;
					++GroupValueCount; // At least one

					for (uint32 i=0; i < MediaCount; ++i)
					{
						const auto MediaToAdd = WwiseHashCombineFast(GetTypeHash(Id), i ^ AudioNodeIter ^ 19891919) % NumMedia;
						AudioNode.Media.Add(Media[MediaToAdd]);
					}

					for (uint32 i=0; i < SoundBankCount; ++i)
					{
						const auto SoundBankToAdd = WwiseHashCombineFast(GetTypeHash(Id), i ^ AudioNodeIter ^ 924606208) % NumSoundBanks;
						AudioNode.SoundBanks.Add(SoundBanks[SoundBankToAdd].SoundBankLanguageMap[FWwiseLanguageCookedData::Sfx]);
					}

					FWwiseGroupValueCookedDataSet GroupValueSet;
					for (uint32 i=0; i < GroupValueCount; ++i)
					{
						const bool bIsState = WwiseHashCombineFast(GetTypeHash(Id), i ^ AudioNodeIter ^ 8484846414) % 4 == 0;		// Mostly switches
						const auto NumToAdd = WwiseHashCombineFast(GetTypeHash(Id), i ^ AudioNodeIter ^ 10671953) % (bIsState ? NumStates : NumSwitches);
						GroupValueSet.GroupValues.Add(bIsState ? States[NumToAdd] : Switches[NumToAdd]);
					}
					Data.AudioNodes.Add(MoveTemp(GroupValueSet), MoveTemp(AudioNode));
				}

				Event.EventLanguageMap.Add(FWwiseLanguageCookedData::Sfx, MoveTemp(Data));
			}
			
			for (auto& ExternalSource: ExternalSources)
			{
				FWwiseExternalSourceCookedData Data;
				Data.Cookie = ++Id;
			}
		}

		// Operations
		TSharedRef<FWwiseMockExternalSourceManager> ExternalSourceManager = MakeShared<FWwiseMockExternalSourceManager>();
		auto MediaManager = MakeShared<FWwiseMockMediaManager>();
		auto SoundBankManager = MakeShared<FWwiseMockSoundBankManager>();
		auto ResourceLoaderImpl = MakeShared<FWwiseResourceLoaderImpl>(ExternalSourceManager, MediaManager, SoundBankManager);
		
		auto UnloadFuturesPtr = std::make_unique<std::array<FWwiseResourceUnloadFuture, NumOp>>();
		auto& UnloadFutures = *UnloadFuturesPtr;
		
		for (uint32 Op = 0; Op < NumOp; ++Op)
		{
			const auto Type = WwiseHashCombineFast(GetTypeHash(Op), 939401805) % 12;
			switch(Type)
			{
			case 0:		// AudioNode
				{
					const auto AudioNode = WwiseHashCombineFast(GetTypeHash(Op), 409892689) % NumAudioNodes;

					// Creating Node
					auto* Node = ResourceLoaderImpl->CreateAudioNodeListEntry(AudioNodes[AudioNode]);

					WWISE_CHECK(Node);
					if (UNLIKELY(!Node))
					{
						continue;
					}

					// Loading Node
					FWwiseLoadedAudioNodePromise LoadPromise;
					auto NodeFuture = LoadPromise.GetFuture();
					ResourceLoaderImpl->LoadAudioNodeNode(MoveTemp(LoadPromise), MoveTemp(Node));

					// Unloading Node
					FWwiseResourceUnloadPromise UnloadPromise;
					UnloadFutures[Op] = UnloadPromise.GetFuture();

					NodeFuture.Next([&ResourceLoaderImpl, UnloadPromise = MoveTemp(UnloadPromise)](FWwiseLoadedAudioNodePtr LoadedNode) mutable
					{
						WWISE_CHECK(LoadedNode);
						if (UNLIKELY(!LoadedNode))
						{
							UnloadPromise.EmplaceValue();
							return;
						}

						ResourceLoaderImpl->UnloadAudioNodeNode(MoveTemp(UnloadPromise), MoveTemp(LoadedNode));
					});

				}
				break;
			case 1:		// AuxBus
				{
					const auto AuxBus = WwiseHashCombineFast(GetTypeHash(Op), 2076755927) % NumAuxBusses;

					// Creating Node
					auto* Node = ResourceLoaderImpl->CreateAuxBusListEntry(AuxBusses[AuxBus], nullptr);

					WWISE_CHECK(Node);
					if (UNLIKELY(!Node))
					{
						continue;
					}

					// Loading Node
					FWwiseLoadedAuxBusPromise LoadPromise;
					auto NodeFuture = LoadPromise.GetFuture();
					ResourceLoaderImpl->LoadAuxBusNode(MoveTemp(LoadPromise), MoveTemp(Node));

					// Unloading Node
					FWwiseResourceUnloadPromise UnloadPromise;
					UnloadFutures[Op] = UnloadPromise.GetFuture();

					NodeFuture.Next([&ResourceLoaderImpl, UnloadPromise = MoveTemp(UnloadPromise)](FWwiseLoadedAuxBusPtr LoadedNode) mutable
					{
						WWISE_CHECK(LoadedNode);
						if (UNLIKELY(!LoadedNode))
						{
							UnloadPromise.EmplaceValue();
							return;
						}

						ResourceLoaderImpl->UnloadAuxBusNode(MoveTemp(UnloadPromise), MoveTemp(LoadedNode));
					});

				}
				break;
			case 2:		// Dialogue Event
				{
					const auto Event = WwiseHashCombineFast(GetTypeHash(Op), 99426828) % NumDialogueEvents;

					// Creating Node
					auto* Node = ResourceLoaderImpl->CreateDialogueEventListEntry(DialogueEvents[Event], nullptr);

					WWISE_CHECK(Node);
					if (UNLIKELY(!Node))
					{
						return;
					}

					// Loading Node
					FWwiseLoadedDialogueEventPromise LoadPromise;
					auto NodeFuture = LoadPromise.GetFuture();
					ResourceLoaderImpl->LoadDialogueEventNode(MoveTemp(LoadPromise), MoveTemp(Node));

					// Unloading Node
					FWwiseResourceUnloadPromise UnloadPromise;
					UnloadFutures[Op] = UnloadPromise.GetFuture();

					NodeFuture.Next([&ResourceLoaderImpl, UnloadPromise = MoveTemp(UnloadPromise)](FWwiseLoadedDialogueEventPtr LoadedNode) mutable
					{
						WWISE_CHECK(LoadedNode);
						if (UNLIKELY(!LoadedNode))
						{
							UnloadPromise.EmplaceValue();
							return;
						}

						ResourceLoaderImpl->UnloadDialogueEventNode(MoveTemp(UnloadPromise), MoveTemp(LoadedNode));
					});
				}
				break;
			default:	// More Events than anything else! (see Type's Modulo for how many are events)
				{
					const auto Event = WwiseHashCombineFast(GetTypeHash(Op), 549205820) % NumEvents;

					// Creating Node
					auto* Node = ResourceLoaderImpl->CreateEventListEntry(Events[Event], nullptr);

					WWISE_CHECK(Node);
					if (UNLIKELY(!Node))
					{
						return;
					}

					// Loading Node
					FWwiseLoadedEventPromise LoadPromise;
					auto NodeFuture = LoadPromise.GetFuture();
					ResourceLoaderImpl->LoadEventNode(MoveTemp(LoadPromise), MoveTemp(Node));

					// Unloading Node
					FWwiseResourceUnloadPromise UnloadPromise;
					UnloadFutures[Op] = UnloadPromise.GetFuture();

					NodeFuture.Next([&ResourceLoaderImpl, UnloadPromise = MoveTemp(UnloadPromise)](FWwiseLoadedEventPtr LoadedNode) mutable
					{
						WWISE_CHECK(LoadedNode);
						if (UNLIKELY(!LoadedNode))
						{
							UnloadPromise.EmplaceValue();
							return;
						}

						ResourceLoaderImpl->UnloadEventNode(MoveTemp(UnloadPromise), MoveTemp(LoadedNode));
					});
				}
				break;
			case 3: // External source
				{
					const auto ExternalSource = WwiseHashCombineFast(GetTypeHash(Op), 64897712) % NumExternalSources;

					// Creating Node
					auto* Node = ResourceLoaderImpl->CreateExternalSourceListEntry(ExternalSources[ExternalSource]);

					WWISE_CHECK(Node);
					if (UNLIKELY(!Node))
					{
						continue;
					}

					// Loading Node
					FWwiseLoadedExternalSourcePromise LoadPromise;
					auto NodeFuture = LoadPromise.GetFuture();
					ResourceLoaderImpl->LoadExternalSourceNode(MoveTemp(LoadPromise), MoveTemp(Node));

					// Unloading Node
					FWwiseResourceUnloadPromise UnloadPromise;
					UnloadFutures[Op] = UnloadPromise.GetFuture();

					NodeFuture.Next([&ResourceLoaderImpl, UnloadPromise = MoveTemp(UnloadPromise)](FWwiseLoadedExternalSourcePtr LoadedNode) mutable
					{
						WWISE_CHECK(LoadedNode);
						if (UNLIKELY(!LoadedNode))
						{
							UnloadPromise.EmplaceValue();
							return;
						}

						ResourceLoaderImpl->UnloadExternalSourceNode(MoveTemp(UnloadPromise), MoveTemp(LoadedNode));
					});
				}
				break;
			case 4:		// Init Bank
				{
					const auto InitBank = WwiseHashCombineFast(GetTypeHash(Op), 65471010) % NumInitBanks;

					// Creating Node
					auto* Node = ResourceLoaderImpl->CreateInitBankListEntry(InitBanks[InitBank]);

					WWISE_CHECK(Node);
					if (UNLIKELY(!Node))
					{
						continue;
					}

					// Loading Node
					FWwiseLoadedInitBankPromise LoadPromise;
					auto NodeFuture = LoadPromise.GetFuture();
					ResourceLoaderImpl->LoadInitBankNode(MoveTemp(LoadPromise), MoveTemp(Node));

					// Unloading Node
					FWwiseResourceUnloadPromise UnloadPromise;
					UnloadFutures[Op] = UnloadPromise.GetFuture();

					NodeFuture.Next([&ResourceLoaderImpl, UnloadPromise = MoveTemp(UnloadPromise)](FWwiseLoadedInitBankPtr LoadedNode) mutable
					{
						WWISE_CHECK(LoadedNode);
						if (UNLIKELY(!LoadedNode))
						{
							UnloadPromise.EmplaceValue();
							return;
						}

						ResourceLoaderImpl->UnloadInitBankNode(MoveTemp(UnloadPromise), MoveTemp(LoadedNode));
					});
				}
				break;
			case 5:		// Media
				{
					const auto Medii = WwiseHashCombineFast(GetTypeHash(Op), 193119891) % NumMedia;

					// Creating Node
					auto* Node = ResourceLoaderImpl->CreateMediaListEntry(Media[Medii]);

					WWISE_CHECK(Node);
					if (UNLIKELY(!Node))
					{
						continue;
					}

					// Loading Node
					FWwiseLoadedMediaPromise LoadPromise;
					auto NodeFuture = LoadPromise.GetFuture();
					ResourceLoaderImpl->LoadMediaNode(MoveTemp(LoadPromise), MoveTemp(Node));

					// Unloading Node
					FWwiseResourceUnloadPromise UnloadPromise;
					UnloadFutures[Op] = UnloadPromise.GetFuture();

					NodeFuture.Next([&ResourceLoaderImpl, UnloadPromise = MoveTemp(UnloadPromise)](FWwiseLoadedMediaPtr LoadedNode) mutable
					{
						WWISE_CHECK(LoadedNode);
						if (UNLIKELY(!LoadedNode))
						{
							UnloadPromise.EmplaceValue();
							return;
						}

						ResourceLoaderImpl->UnloadMediaNode(MoveTemp(UnloadPromise), MoveTemp(LoadedNode));
					});
				}
				break;
			case 6:		// ShareSet
				{
					const auto ShareSet = WwiseHashCombineFast(GetTypeHash(Op), 331319104) % NumShareSets;

					// Creating Node
					auto* Node = ResourceLoaderImpl->CreateShareSetListEntry(ShareSets[ShareSet], &FWwiseLanguageCookedData::Sfx);

					WWISE_CHECK(Node);
					if (UNLIKELY(!Node))
					{
						continue;
					}

					// Loading Node
					FWwiseLoadedShareSetPromise LoadPromise;
					auto NodeFuture = LoadPromise.GetFuture();
					ResourceLoaderImpl->LoadShareSetNode(MoveTemp(LoadPromise), MoveTemp(Node));

					// Unloading Node
					FWwiseResourceUnloadPromise UnloadPromise;
					UnloadFutures[Op] = UnloadPromise.GetFuture();

					NodeFuture.Next([&ResourceLoaderImpl, UnloadPromise = MoveTemp(UnloadPromise)](FWwiseLoadedShareSetPtr LoadedNode) mutable
					{
						WWISE_CHECK(LoadedNode);
						if (UNLIKELY(!LoadedNode))
						{
							UnloadPromise.EmplaceValue();
							return;
						}

						ResourceLoaderImpl->UnloadShareSetNode(MoveTemp(UnloadPromise), MoveTemp(LoadedNode));
					});

				}
				break;
			case 7:		// SoundBank
				{
					const auto SoundBank = WwiseHashCombineFast(GetTypeHash(Op), 953952742) % NumSoundBanks;

					// Creating Node
					auto* Node = ResourceLoaderImpl->CreateSoundBankListEntry(SoundBanks[SoundBank], nullptr);

					WWISE_CHECK(Node);
					if (UNLIKELY(!Node))
					{
						continue;
					}

					// Loading Node
					FWwiseLoadedSoundBankPromise LoadPromise;
					auto NodeFuture = LoadPromise.GetFuture();
					ResourceLoaderImpl->LoadSoundBankNode(MoveTemp(LoadPromise), MoveTemp(Node));

					// Unloading Node
					FWwiseResourceUnloadPromise UnloadPromise;
					UnloadFutures[Op] = UnloadPromise.GetFuture();

					NodeFuture.Next([&ResourceLoaderImpl, UnloadPromise = MoveTemp(UnloadPromise)](FWwiseLoadedSoundBankPtr LoadedNode) mutable
					{
						WWISE_CHECK(LoadedNode);
						if (UNLIKELY(!LoadedNode))
						{
							UnloadPromise.EmplaceValue();
							return;
						}

						ResourceLoaderImpl->UnloadSoundBankNode(MoveTemp(UnloadPromise), MoveTemp(LoadedNode));
					});
				}
				break;
			case 8:		// State
				{
					const auto State = WwiseHashCombineFast(GetTypeHash(Op), 8198259) % NumStates;

					// Creating Node
					auto* Node = ResourceLoaderImpl->CreateGroupValueListEntry(States[State]);

					WWISE_CHECK(Node);
					if (UNLIKELY(!Node))
					{
						return;
					}

					// Loading Node
					FWwiseLoadedGroupValuePromise LoadPromise;
					auto NodeFuture = LoadPromise.GetFuture();
					ResourceLoaderImpl->LoadGroupValueNode(MoveTemp(LoadPromise), MoveTemp(Node));

					// Unloading Node
					FWwiseResourceUnloadPromise UnloadPromise;
					UnloadFutures[Op] = UnloadPromise.GetFuture();

					NodeFuture.Next([&ResourceLoaderImpl, UnloadPromise = MoveTemp(UnloadPromise)](FWwiseLoadedGroupValuePtr LoadedNode) mutable
					{
						WWISE_CHECK(LoadedNode);
						if (UNLIKELY(!LoadedNode))
						{
							UnloadPromise.EmplaceValue();
							return;
						}

						ResourceLoaderImpl->UnloadGroupValueNode(MoveTemp(UnloadPromise), MoveTemp(LoadedNode));
					});
				}
				break;
			case 9:		// Switch
				{
					const auto Switch = WwiseHashCombineFast(GetTypeHash(Op), 24650269) % NumSwitches;

					// Creating Node
					auto* Node = ResourceLoaderImpl->CreateGroupValueListEntry(Switches[Switch]);

					WWISE_CHECK(Node);
					if (UNLIKELY(!Node))
					{
						return;
					}

					// Loading Node
					FWwiseLoadedGroupValuePromise LoadPromise;
					auto NodeFuture = LoadPromise.GetFuture();
					ResourceLoaderImpl->LoadGroupValueNode(MoveTemp(LoadPromise), MoveTemp(Node));

					// Unloading Node
					FWwiseResourceUnloadPromise UnloadPromise;
					UnloadFutures[Op] = UnloadPromise.GetFuture();

					NodeFuture.Next([&ResourceLoaderImpl, UnloadPromise = MoveTemp(UnloadPromise)](FWwiseLoadedGroupValuePtr LoadedNode) mutable
					{
						WWISE_CHECK(LoadedNode);
						if (UNLIKELY(!LoadedNode))
						{
							UnloadPromise.EmplaceValue();
							return;
						}

						ResourceLoaderImpl->UnloadGroupValueNode(MoveTemp(UnloadPromise), MoveTemp(LoadedNode));
					});
				}
				break;
			}
			
			// Leave some leeway to execute some of the operations before starting the next batch
			if (Op % 100 == 0)
			{
				FPlatformProcess::Sleep(0.01f);
			}
		}
		
		for (auto& UnloadFuture : UnloadFutures)
		{
			UnloadFuture.Get();
		}

		WWISE_CHECK(ExternalSourceManager->IsEmpty());
		WWISE_CHECK(MediaManager->IsEmpty());
		WWISE_CHECK(SoundBankManager->IsEmpty());
		WWISE_CHECK(ResourceLoaderImpl->IsEmpty());
	}

	SECTION("Sequential AudioNode Load")
	{
		const bool bMockSleepOnMediaLoad = FWwiseResourceLoaderImpl::Test::bMockSleepOnMediaLoad;
		FWwiseResourceLoaderImpl::Test::bMockSleepOnMediaLoad = true;
		ON_SCOPE_EXIT { FWwiseResourceLoaderImpl::Test::bMockSleepOnMediaLoad = bMockSleepOnMediaLoad; };

		auto ExternalSourceManager = MakeShared<FWwiseMockExternalSourceManager>();
		auto MediaManager = MakeShared<FWwiseMockMediaManager>();
		auto SoundBankManager = MakeShared<FWwiseMockSoundBankManager>();
		auto ResourceLoaderImpl = MakeShared<FWwiseResourceLoaderImpl>(ExternalSourceManager, MediaManager, SoundBankManager);

		static constexpr const auto MediaOverlap = 1.25f;
		static constexpr const uint32 NumGroupValues = 1000;
		static constexpr const auto NumLoops = 2;
		static constexpr const auto FuturesCount = (NumGroupValues + 2) * NumLoops;
		static constexpr const auto EventsCount = 2 * NumLoops;
		
		FWwiseLocalizedEventCookedData EventCookedDatas[2];

		// Fill ordered EventCookedData (0)
		{
			FWwiseEventCookedData Data;
			Data.EventId = 1;

			FWwiseAudioNodeCookedData AudioNode;
			for (int i = 0; i < NumGroupValues; ++i)
			{
				FWwiseMediaCookedData AudioNodeMedia;
				AudioNodeMedia.MediaId = 10000 + i / MediaOverlap;
				AudioNode.Media.Emplace(MoveTemp(AudioNodeMedia));

				FWwiseGroupValueCookedData AudioNodeGroupValue;
				AudioNodeGroupValue.Id = 15000 + i;
				AudioNodeGroupValue.GroupId = 1;
				AudioNodeGroupValue.Type = EWwiseGroupType::Switch;
				FWwiseGroupValueCookedDataSet AudioNode1GroupValueSet;
				AudioNode1GroupValueSet.GroupValues.Emplace(MoveTemp(AudioNodeGroupValue));
				Data.AudioNodes.Emplace(MoveTemp(AudioNode1GroupValueSet), MoveTemp(AudioNode));
			}

			EventCookedDatas[0].EventLanguageMap.Emplace(FWwiseLanguageCookedData::Sfx, MoveTemp(Data));
		}

		// Fill unordered EventCookedData (1)
		{
			FWwiseEventCookedData Data;
			Data.EventId = 1;

			FWwiseAudioNodeCookedData AudioNode;
			for (int i = 0; i < NumGroupValues; ++i)
			{
				for (uint32 j = 0; j < 2; ++j)
				{
					FWwiseMediaCookedData AudioNodeMedia;
					AudioNodeMedia.MediaId = 10000 + (WwiseHashCombineFast(GetTypeHash(i), 142401 + j) % NumGroupValues) / MediaOverlap;
					AudioNode.Media.Emplace(MoveTemp(AudioNodeMedia));
				}

				FWwiseGroupValueCookedData AudioNodeGroupValue;
				AudioNodeGroupValue.Id = 15000 + i;
				AudioNodeGroupValue.GroupId = 1;
				AudioNodeGroupValue.Type = EWwiseGroupType::Switch;
				FWwiseGroupValueCookedDataSet AudioNode1GroupValueSet;
				AudioNode1GroupValueSet.GroupValues.Emplace(MoveTemp(AudioNodeGroupValue));
				Data.AudioNodes.Emplace(MoveTemp(AudioNode1GroupValueSet), MoveTemp(AudioNode));
			}

			EventCookedDatas[1].EventLanguageMap.Emplace(FWwiseLanguageCookedData::Sfx, MoveTemp(Data));
		}

		FWwiseGroupValueCookedData GroupValueCookedDatas[NumGroupValues];
		for (auto i = 0; i < NumGroupValues; ++i)
		{
			GroupValueCookedDatas[i].Id = 15000 + i;
			GroupValueCookedDatas[i].GroupId = 1;
			GroupValueCookedDatas[i].Type = EWwiseGroupType::Switch;
		}

		FWwiseLoadedEventFuture EventFuturesArray[EventsCount];
		FWwiseResourceUnloadFuture UnloadFuturesArray[FuturesCount];

		for (auto Loop = 0; Loop < NumLoops; ++Loop)
		{
			auto* EventFutures( &EventFuturesArray[2 * Loop] );
			auto* UnloadFutures( &UnloadFuturesArray[(NumGroupValues + 2) * Loop] );
			
			for (auto i = 0; i < NumGroupValues; ++i)
			{
				if (i == 0)
				{
					// Creating Node
					auto* Node = ResourceLoaderImpl->CreateEventListEntry(EventCookedDatas[0], nullptr);

					WWISE_CHECK(Node);
					if (UNLIKELY(!Node))
					{
						return;
					}

					// Loading Node
					FWwiseLoadedEventPromise LoadPromise;
					EventFutures[0] = LoadPromise.GetFuture();
					ResourceLoaderImpl->LoadEventNode(MoveTemp(LoadPromise), MoveTemp(Node));
				}
				else if (i == NumGroupValues / 4 * 3)
				{
					// Unloading Node
					FWwiseResourceUnloadPromise UnloadPromise;
					UnloadFutures[NumGroupValues] = UnloadPromise.GetFuture();

					EventFutures[0].Next([&ResourceLoaderImpl, UnloadPromise = MoveTemp(UnloadPromise)](FWwiseLoadedEventPtr LoadedNode) mutable
					{
						WWISE_CHECK(LoadedNode);
						if (UNLIKELY(!LoadedNode))
						{
							UnloadPromise.EmplaceValue();
							return;
						}

						ResourceLoaderImpl->UnloadEventNode(MoveTemp(UnloadPromise), MoveTemp(LoadedNode));
					});
				}

				if (i == NumGroupValues / 4 * 2)
				{
					// Creating Node
					auto* Node = ResourceLoaderImpl->CreateEventListEntry(EventCookedDatas[1], nullptr);

					WWISE_CHECK(Node);
					if (UNLIKELY(!Node))
					{
						return;
					}

					// Loading Node
					FWwiseLoadedEventPromise LoadPromise;
					EventFutures[1] = LoadPromise.GetFuture();
					ResourceLoaderImpl->LoadEventNode(MoveTemp(LoadPromise), MoveTemp(Node));
				}
				else if (i == NumGroupValues - 1)
				{
					// Unloading Node
					FWwiseResourceUnloadPromise UnloadPromise;
					UnloadFutures[NumGroupValues + 1] = UnloadPromise.GetFuture();

					EventFutures[1].Next([&ResourceLoaderImpl, UnloadPromise = MoveTemp(UnloadPromise)](FWwiseLoadedEventPtr LoadedNode) mutable
					{
						WWISE_CHECK(LoadedNode);
						if (UNLIKELY(!LoadedNode))
						{
							UnloadPromise.EmplaceValue();
							return;
						}

						ResourceLoaderImpl->UnloadEventNode(MoveTemp(UnloadPromise), MoveTemp(LoadedNode));
					});
				}

				{
					// Creating Node
					auto* Node = ResourceLoaderImpl->CreateGroupValueListEntry(GroupValueCookedDatas[i]);

					WWISE_CHECK(Node);
					if (UNLIKELY(!Node))
					{
						return;
					}

					// Loading Node
					FWwiseLoadedGroupValuePromise LoadPromise;
					auto NodeFuture = LoadPromise.GetFuture();
					ResourceLoaderImpl->LoadGroupValueNode(MoveTemp(LoadPromise), MoveTemp(Node));

					// Unloading Node
					FWwiseResourceUnloadPromise UnloadPromise;
					UnloadFutures[i] = UnloadPromise.GetFuture();

					NodeFuture.Next([&ResourceLoaderImpl, UnloadPromise = MoveTemp(UnloadPromise)](FWwiseLoadedGroupValuePtr LoadedNode) mutable
					{
						WWISE_CHECK(LoadedNode);
						if (UNLIKELY(!LoadedNode))
						{
							UnloadPromise.EmplaceValue();
							return;
						}

						ResourceLoaderImpl->UnloadGroupValueNode(MoveTemp(UnloadPromise), MoveTemp(LoadedNode));
					});
				}

				// Leave some leeway to execute some of the operations before starting the next batch
				if (i % 100 == 0)
				{
					FPlatformProcess::Sleep(0.01f);
				}
			}

			// Leave some leeway to execute some of the operations before starting the next loop
			FPlatformProcess::Sleep(0.1f);
		}

		for (auto i = 0; i < FuturesCount; ++i)
		{
			UnloadFuturesArray[i].Get();
		}

		WWISE_CHECK(ExternalSourceManager->IsEmpty());
		WWISE_CHECK(MediaManager->IsEmpty());
		WWISE_CHECK(SoundBankManager->IsEmpty());
		WWISE_CHECK(ResourceLoaderImpl->IsEmpty());
	}
}

#endif // WWISE_UNIT_TESTS