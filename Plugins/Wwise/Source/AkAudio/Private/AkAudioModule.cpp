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

#include "AkAudioModule.h"
#include "AkAudioDevice.h"
#include "AkAudioStyle.h"
#include "AkSettings.h"
#include "AkSettingsPerUser.h"
#include "WwiseUnrealDefines.h"

#include "Wwise/Packaging/WwiseAssetLibrary.h"
#include "Wwise/WwiseFileHandlerModule.h"
#include "Wwise/WwiseResourceLoader.h"
#include "Wwise/WwiseSoundEngineModule.h"
#include "WwiseInitBankLoader/WwiseInitBankLoader.h"

#include "Misc/ScopedSlowTask.h"

#include "UObject/UObjectIterator.h"
#include "Framework/Application/SlateApplication.h"

#include "Wwise/API/WwiseSoundEngineAPI.h"

#if WITH_EDITORONLY_DATA
#include "Wwise/WwiseProjectDatabase.h"
#include "Wwise/WwiseDataStructure.h"
#include "Wwise/WwiseResourceCooker.h"
#endif

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Wwise/WwisePackagingEditorModule.h"
#include "Wwise/Packaging/WwiseAssetLibraryPreCooker.h"
#endif
#include "Async/Async.h"
#include "Platforms/AkPlatformInfo.h"
#include "UObject/ICookInfo.h"
#include "Wwise/WwiseConcurrencyModule.h"
#include "Wwise/WwisePackagingModule.h"
#include "Wwise/WwiseExternalSourceManager.h"
#include "Wwise/Packaging/WwiseAssetLibraryGroup.h"
#include "Wwise/Packaging/WwisePackagingSettings.h"
#include "Wwise/Packaging/WwisePackagingUtils.h"

IMPLEMENT_MODULE(FAkAudioModule, AkAudio)
#define LOCTEXT_NAMESPACE "AkAudio"

FAkAudioModule* FAkAudioModule::AkAudioModuleInstance = nullptr;
FSimpleMulticastDelegate FAkAudioModule::OnModuleInitialized;
FSimpleMulticastDelegate FAkAudioModule::OnWwiseAssetDataReloaded;
std::atomic<bool> FAkAudioModule::bIsShuttingDown{false};
std::atomic<int32> FAkAudioModule::ActiveDatabaseOperations{0};

// WwiseUnrealHelper overrides

namespace WwiseUnrealHelper
{
	static bool IsValidClass(UClass* inClass)
	{
		return IsValid(inClass) && inClass->GetName() != "None";
	}
	
	static FString GetWwiseSoundEnginePluginDirectoryImpl()
	{
		return FAkPlatform::GetWwiseSoundEnginePluginDirectory();
	}

	static FString GetWwiseProjectPathImpl()
	{
		if(!IsValidClass(UAkSettings::StaticClass()))
		{
			return {};
		}

		FString projectPath;

		if (auto* settings = GetDefault<UAkSettings>())
		{
			projectPath = settings->WwiseProjectPath.FilePath;

			if (FPaths::IsRelative(projectPath))
			{
				projectPath = FPaths::ConvertRelativePathToFull(GetProjectDirectory(), projectPath);
			}

#if PLATFORM_WINDOWS
			projectPath.ReplaceInline(TEXT("/"), TEXT("\\"));
#endif
		}

		return projectPath;
	}

	static FString GetSoundBankDirectoryImpl()
	{
		if(!IsValidClass(UAkSettingsPerUser::StaticClass()) || !IsValidClass(UAkSettings::StaticClass()))
		{
			return {};
		}

		const UAkSettingsPerUser* UserSettings = GetDefault<UAkSettingsPerUser>();
		FString SoundBankDirectory;
		if (UserSettings && !UserSettings->RootOutputPathOverride.Path.IsEmpty())
		{
			SoundBankDirectory = UserSettings->RootOutputPathOverride.Path;
			if(FPaths::IsRelative(UserSettings->RootOutputPathOverride.Path))
			{
				SoundBankDirectory = FPaths::Combine(GetContentDirectory(), UserSettings->RootOutputPathOverride.Path);
			}
		}
		else if (const UAkSettings* AkSettings = GetDefault<UAkSettings>())
		{
			if(AkSettings->RootOutputPath.Path.IsEmpty())
			{
				return {};
			}
			SoundBankDirectory = AkSettings->RootOutputPath.Path;
			if(FPaths::IsRelative(AkSettings->RootOutputPath.Path))
			{
				SoundBankDirectory = FPaths::Combine(GetContentDirectory(), AkSettings->RootOutputPath.Path);	
			}
		}
		else
		{
			UE_LOG(LogAkAudio, Warning, TEXT("WwiseUnrealHelper::GetSoundBankDirectory : Please set the Generated Soundbanks Folder in Wwise settings. Otherwise, sound will not function."));
			return {};
		}
		FPaths::CollapseRelativeDirectories(SoundBankDirectory);
		if(!SoundBankDirectory.EndsWith(TEXT("/")))
		{
			SoundBankDirectory.AppendChar('/');
		}

		return SoundBankDirectory;
	}

	static FString GetStagePathFromSettings()
	{
		if(!IsValidClass(UAkSettings::StaticClass()))
		{
			return {};
		}

		// In !WITH_EDITORONLY_DATA, you must prepend `FPaths::ProjectContentDir() /` to the defined Stage Path
		const UAkSettings* Settings = GetDefault<UAkSettings>();
		if (Settings && !Settings->WwiseStagingDirectory.Path.IsEmpty())
		{
			return Settings->WwiseStagingDirectory.Path;
		}
		return TEXT("WwiseAudio");
	}
}

void FAkAudioModule::StartupModule()
{
	bIsShuttingDown = false;
	
	IWwiseConcurrencyModule::GetModule();
	IWwiseFileHandlerModule::GetModule();
	IWwiseResourceLoaderModule::GetModule();
	IWwiseSoundEngineModule::ForceLoadModule();

#if WITH_EDITORONLY_DATA
	IWwiseProjectDatabaseModule::GetModule();
	IWwiseResourceLoaderModule::GetModule();
#endif

	WwiseUnrealHelper::SetHelperFunctions(
		WwiseUnrealHelper::GetWwiseSoundEnginePluginDirectoryImpl,
		WwiseUnrealHelper::GetWwiseProjectPathImpl,
		WwiseUnrealHelper::GetSoundBankDirectoryImpl);

#if WITH_EDITOR
	// It is not wanted to initialize the SoundEngine while running the GenerateSoundBanks commandlet.
	if (IsRunningCommandlet())
	{
		// We COULD use GetRunningCommandletClass(), but unfortunately it is set to nullptr in OnPostEngineInit.
		// We need to parse the command line.
		FString CmdLine(FCommandLine::Get());
		if (CmdLine.Contains(TEXT("run=GenerateSoundBanks")))
		{
			UE_LOG(LogAkAudio, Log, TEXT("FAkAudioModule::StartupModule: Detected GenerateSoundBanks commandlet is running. AkAudioModule will not be initialized."));
			return;
		}

#if WITH_EDITORONLY_DATA
		if(!IWwiseProjectDatabaseModule::ShouldInitializeProjectDatabase())
		{
			// Initialize the Rersource Cooker
			IWwiseResourceCookerModule::GetModule();
		}
#endif
	}
#endif

	if (AkAudioModuleInstance == this)
	{
		UE_LOG(LogAkAudio, Log, TEXT("FAkAudioModule::StartupModule: AkAudioModuleInstance already exists."));
		return;
	}
	UE_CLOG(AkAudioModuleInstance, LogAkAudio, Warning, TEXT("FAkAudioModule::StartupModule: Updating AkAudioModuleInstance from (%p) to (%p)! Previous Module instance was improperly shut down!"), AkAudioModuleInstance, this);

	AkAudioModuleInstance = this;

	FScopedSlowTask SlowTask(0, LOCTEXT("InitWwisePlugin", "Initializing Wwise Plug-in AkAudioModule..."));

#if WITH_EDITORONLY_DATA
	EWwiseProjectDatabaseLoadMode LoadMode = EWwiseProjectDatabaseLoadMode::Synchronous;
	if (const auto* UserSettings = GetDefault<UAkSettingsPerUser>())
	{
		LoadMode = UserSettings->ProjectDatabaseLoadMode;
	}

	switch (LoadMode)
	{
	case EWwiseProjectDatabaseLoadMode::Synchronous:
		{
			UE_LOG(LogAkAudio, Log, TEXT("FAkAudioModule::StartupModule: Using synchronous Project Database loading."));

			if (auto* AkSettings = GetDefault<UAkSettings>())
			{
				if (AkSettings->AreSoundBanksGenerated())
				{
					ParseGeneratedSoundBankData();
					InitializeAkAudioDevice();
				}
			}
#if !UE_SERVER
			UpdateWwiseResourceCookerSettings();
#endif
		}
		break;
	case EWwiseProjectDatabaseLoadMode::Asynchronous:
	default:
		UE_LOG(LogAkAudio, Log, TEXT("FAkAudioModule::StartupModule: Using asynchronous Project Database loading."));
		Async(EAsyncExecution::ThreadPool, [this]()
		{
			if (auto* AkSettings = GetDefault<UAkSettings>())
			{
				if (AkSettings->AreSoundBanksGenerated())
				{
					++ActiveDatabaseOperations;
					ParseGeneratedSoundBankData();
					--ActiveDatabaseOperations;	

					// Marshal back to game thread for the remaining of the initialization sequence
					AsyncTask(ENamedThreads::GameThread, [this]()
					{
						if (bIsShuttingDown)
						{
							UE_LOG(LogAkAudio, Log, TEXT("LogAkAudio: Async initialization aborted - module is shutting down."));
							return;
						}
						InitializeAkAudioDevice();
#if !UE_SERVER
						UpdateWwiseResourceCookerSettings();
#endif
					});
				}
			}
		});
	}
	
	// Loading the File Handler Module, in case it loads a different module with UStructs, so it gets packaged (Ex.: Simple External Source Manager)
	IWwiseFileHandlerModule::GetModule();

	// Loading the AssetLibrary module to set the cooking initialization function
	if (auto* PackagingModule = IWwisePackagingModule::GetModule())
	{
		PackagingModule->SetCreateResourceCookerForPlatformFct([](const ITargetPlatform* TargetPlatform)
		{
			CreateResourceCookerForPlatform(TargetPlatform);
		});
	}
#else
	InitializeAkAudioDevice();
#endif
}

void FAkAudioModule::ShutdownModule()
{
	UE_CLOG(AkAudioModuleInstance && AkAudioModuleInstance != this, LogAkAudio, Warning, TEXT("FAkAudioModule::ShutdownModule: Shutting down a different instance (%p) that was initially instantiated (%p)!"), this, AkAudioModuleInstance);

	bIsShuttingDown = true;
	
	// Wait for any active database operations to complete to prevent use-after-free crashes
	if (ActiveDatabaseOperations > 0)
	{
		UE_LOG(LogAkAudio, Log, TEXT("FAkAudioModule::ShutdownModule: Waiting for %d active database operation(s) to complete..."), ActiveDatabaseOperations.load());
		
		while (ActiveDatabaseOperations > 0)
		{
			FPlatformProcess::Sleep(0.01f);
		}
		
		if (ActiveDatabaseOperations == 0)
		{
			UE_LOG(LogAkAudio, Log, TEXT("FAkAudioModule::ShutdownModule: All database operations completed. Shutting down."));
		}
	}
	
	FCoreTickerType::GetCoreTicker().RemoveTicker(TickDelegateHandle);

	if (AkAudioDevice)
	{
		AkAudioDevice->Teardown();
		delete AkAudioDevice;
		AkAudioDevice = nullptr;
	}

	if (IWwiseSoundEngineModule::IsAvailable())
	{
		WwiseUnrealHelper::SetHelperFunctions(nullptr, nullptr, nullptr);
	}

	AkAudioModuleInstance = nullptr;

#if WITH_EDITOR
	if (PIEEndedHandle.IsValid())
	{
		FEditorDelegates::EndPIE.Remove(PIEEndedHandle);
	}
#endif
}

void FAkAudioModule::OnPreExit()
{
	FCoreDelegates::OnEnginePreExit.Remove(OnPreExitHandle);
	if (AkAudioDevice)
	{
		AkAudioDevice->Teardown();
	}
	if (OnApplicationDeactivatedHandle.IsValid())
	{
		FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(OnApplicationDeactivatedHandle);
	}
}

#if WITH_EDITOR
void FAkAudioModule::OnPIEExit(const bool bWasSimulating)
{
	if (auto* SoundEngine = IWwiseSoundEngineAPI::Get())
	{
		SoundEngine->ResetGlobalValues();
	}
}
#endif

void FAkAudioModule::OnApplicationDeactivated(const bool IsActive)
{
	bool RenderDuringFocusLoss = false;
	if(auto* AkSettings = GetDefault<UAkSettings>())
	{
		if (!AkSettings->SuspendAudioDuringFocusLoss)
		{
			return;
		}
		RenderDuringFocusLoss = AkSettings->RenderDuringFocusLoss;
	}
	if (auto* SoundEngine = IWwiseSoundEngineAPI::Get())
	{
		if (!IsActive)
		{
			SoundEngine->Suspend(RenderDuringFocusLoss);
		}
		else
		{
			SoundEngine->WakeupFromSuspend();
		}
	}
}

FAkAudioDevice* FAkAudioModule::GetAkAudioDevice() const
{
	return AkAudioDevice;
}

void FAkAudioModule::ReloadWwiseAssetData() const
{
	SCOPED_AKAUDIO_EVENT(TEXT("ReloadWwiseAssetData"));
	if (FAkAudioDevice::IsInitialized())
	{
		UE_LOG(LogAkAudio, Log, TEXT("FAkAudioModule::ReloadWwiseAssetData : Reloading Wwise asset data."));
		if (AkAudioDevice)
		{
			AkAudioDevice->ClearSoundBanksAndMedia();
		}
		
		auto* InitBankLoader = FWwiseInitBankLoader::Get();
		if (LIKELY(InitBankLoader))
		{
			InitBankLoader->LoadInitBank();
		}
		else
		{
			UE_LOG(LogAkAudio, Error, TEXT("LoadInitBank: WwiseInitBankLoader is not initialized."));
		}

		for (TObjectIterator<UAkAudioType> AudioAssetIt; AudioAssetIt; ++AudioAssetIt)
		{
			AudioAssetIt->LoadData();
		}
		OnWwiseAssetDataReloaded.Broadcast();
	}
	else
	{
		UE_LOG(LogAkAudio, Verbose, TEXT("FAkAudioModule::ReloadWwiseAssetData : Skipping asset data reload because the SoundEngine is not initialized."));
	}
}

void FAkAudioModule::UpdateWwiseResourceCookerSettings()
{
#if WITH_EDITORONLY_DATA
	SCOPED_AKAUDIO_EVENT(TEXT("UpdateWwiseResourceCookerSettings"));

	auto* ResourceCooker = IWwiseResourceCooker::GetDefault();
	if (!ResourceCooker)
	{
		UE_LOG(LogAkAudio, Error, TEXT("FAkAudioModule::UpdateWwiseResourceCookerSettings : No Default Resource Cooker!"));
		return;
	}
	auto* ProjectDatabase = ResourceCooker->GetProjectDatabase();
	if (!ProjectDatabase)
	{
		UE_LOG(LogAkAudio, Error, TEXT("FAkAudioModule::UpdateWwiseResourceCookerSettings : No Project Database!"));
		return;
	}
	auto ExternalSourceManager = IWwiseExternalSourceManager::Get();
	if (!ExternalSourceManager)
	{
		UE_LOG(LogAkAudio, Error, TEXT("FAkAudioModule::UpdateWwiseResourceCookerSettings : No External Source Manager!"))
		return;
	}

	const auto StagePath = WwiseUnrealHelper::GetStagePathFromSettings();
	ResourceCooker->SetWwiseStagePath(StagePath);

	const auto SoundBankDirectory = WwiseUnrealHelper::GetSoundBankDirectory();
	ProjectDatabase->SetGeneratedSoundBanksPath(FDirectoryPath{ SoundBankDirectory });

	const auto& Platform = ProjectDatabase->GetCurrentPlatform().Platform.Get();
	if (!Platform.ExternalSourceRootPath.IsNone())
	{
		auto ExternalSourcePath = Platform.PathRelativeToGeneratedSoundBanks.ToString() / Platform.ExternalSourceRootPath.ToString();
		if (FPaths::IsRelative(ExternalSourcePath))
		{
			ExternalSourcePath = SoundBankDirectory / ExternalSourcePath;
		}
		ExternalSourceManager->SetExternalSourcePath(FDirectoryPath{ExternalSourcePath });
	}
#endif
}

#if WITH_EDITORONLY_DATA
void FAkAudioModule::CreateResourceCookerForPlatform(const ITargetPlatform* TargetPlatform)
{
	SCOPED_AKAUDIO_EVENT(TEXT("CreateResourceCookerForPlatform"));

	FCookLoadScope UsedInGameScope(ECookLoadType::UsedInGame);

	auto* ResourceCooker = IWwiseResourceCooker::GetForPlatform(TargetPlatform);
	FWwiseProjectDatabase* ProjectDatabase;

	if (LIKELY(ResourceCooker))
	{
		ProjectDatabase = ResourceCooker->GetProjectDatabase();
		if (!ProjectDatabase)
		{
			UE_LOG(LogAkAudio, Error, TEXT("FAkAudioModule::CreateResourceCookerForPlatform : No Project Database!"));
			return;
		}
	}
	else
	{
		const auto PlatformID = UAkPlatformInfo::GetSharedPlatformInfo(TargetPlatform->IniPlatformName());
		if (UNLIKELY(!PlatformID.IsValid()))
		{
			UE_LOG(LogAkAudio, Error, TEXT("FAkAudioModule::CreateResourceCookerForPlatform : Could not get platform info for target %s!"), *TargetPlatform->IniPlatformName());
			return;
		}

		bool bPackageAsBulkData{ false };
		FWwiseAssetLibraryPreCooker::FAssetLibraryArray* AssetLibraries { new FWwiseAssetLibraryPreCooker::FAssetLibraryArray };
		if (auto* WwisePackagingSettings = GetDefault<UWwisePackagingSettings>())
		{
			bPackageAsBulkData = WwisePackagingSettings->bPackageAsBulkData;
		}

		ON_SCOPE_EXIT
		{
			if (AssetLibraries != nullptr)
			{
				delete AssetLibraries;
				AssetLibraries = nullptr;
			}
		};

		if (AssetLibraries)
		{
			UE_LOG(LogAkAudio, Display, TEXT("FAkAudioModule::CreateResourceCookerForPlatform : Loaded asset libraries count: %i"), AssetLibraries->Num());

			for (int32 Idx = 0; Idx < AssetLibraries->Num(); ++Idx)
			{
				const auto& LibrarySoftPtr = (*AssetLibraries)[Idx];
				if (LibrarySoftPtr.LoadSynchronous() != nullptr)
				{
					auto* Library = LibrarySoftPtr.Get();
					UE_LOG(LogAkAudio, Display, TEXT("  [%i] Asset Library: %s (Path: %s)"), Idx, *Library->GetName(), *LibrarySoftPtr.ToString());
				}
				else
				{
					UE_LOG(LogAkAudio, Warning, TEXT("  [%i] Failed to load asset library at path: %s"), Idx, *LibrarySoftPtr.ToString());
				}
			}
		}

		auto AssetRegistryModule = &FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		TArray<FAssetData> LibraryGroupAssets;
		if (!TargetPlatform->IsServerOnly())
		{
			FARFilter Filter;
			Filter.bRecursivePaths = true;
			Filter.ClassPaths.Add(UWwiseAssetLibraryGroup::StaticClass()->GetClassPathName());
			AssetRegistryModule->Get().GetAssets(Filter, LibraryGroupAssets);
		}
		
		TArray<FString> GameplayModulesCovered;
		for (auto& LibraryGroupAsset : LibraryGroupAssets)
		{
			auto* WwisePackagingSettings = GetDefault<UWwisePackagingSettings>();
			
			auto Group = (UWwiseAssetLibraryGroup*)LibraryGroupAsset.GetAsset();
			UE_LOG(LogAkAudio, Display, TEXT("Using Library Group: %s for GameplayModule %s"), *LibraryGroupAsset.GetFullName(), *WwisePackagingUtils::GetGameplayModuleRoot(LibraryGroupAsset.GetFullName()));
			FString Package = WwisePackagingUtils::GetGameplayModuleRoot(LibraryGroupAsset.PackagePath.ToString());
			GameplayModulesCovered.Add(Package);
			for (auto& Library : Group->Libraries)
			{
				if (Library.LoadSynchronous() == nullptr)
				{
					continue;
				}
				Library->RelevantGameplayModulePaths.Add(Package);
				if (AssetLibraries)
				{
					AssetLibraries->Add(Library);					
				}
			}
		}

		if(IsRunningCookCommandlet() && FParse::Param(FCommandLine::Get(), TEXT("DisableWwiseBulkData")))
		{
			UE_LOG(LogAkAudio, Display, TEXT("FAkAudioModule::CreateResourceCookerForPlatform : Disabling bulk data packaging because of command line."));
			bPackageAsBulkData = false;
		}
		
		EWwisePackagingStrategy TargetPackagingStrategy{ EWwisePackagingStrategy::AdditionalFile };
		if (bPackageAsBulkData)
		{
			TargetPackagingStrategy = EWwisePackagingStrategy::BulkData;
		}

		ResourceCooker = IWwiseResourceCooker::CreateForPlatform(TargetPlatform, PlatformID, TargetPackagingStrategy, EWwiseExportDebugNameRule::Name);
		if (UNLIKELY(!ResourceCooker))
		{
			UE_CLOG(!TargetPlatform->IsServerOnly(), LogAkAudio, Error, TEXT("FAkAudioModule::CreateResourceCookerForPlatform : Could not create Resource Cooker!"));
			return;
		}

		ProjectDatabase = ResourceCooker->GetProjectDatabase();
		if (!ProjectDatabase)
		{
			UE_LOG(LogAkAudio, Error, TEXT("FAkAudioModule::CreateResourceCookerForPlatform : No Project Database!"));
			return;
		}

		if (bPackageAsBulkData && AssetLibraries && AssetLibraries->Num() > 0)
		{
			auto* AssetLibraryEditorModule = IWwisePackagingEditorModule::GetModule();
			if (UNLIKELY(!AssetLibraryEditorModule))
			{
				UE_LOG(LogAkAudio, Error, TEXT("FAkAudioModule::CreateResourceCookerForPlatform : Could not get AssetLibraryEditor module!"));
				return;
			}
			const TUniquePtr<FWwiseAssetLibraryPreCooker> AssetLibraryPreCooker { AssetLibraryEditorModule ? AssetLibraryEditorModule->InstantiatePreCooker(*ProjectDatabase) : nullptr };
			if (!AssetLibraryPreCooker)
			{
				UE_LOG(LogAkAudio, Error, TEXT("FAkAudioModule::CreateResourceCookerForPlatform : Could not create AssetLibrary PreCooker!"));
				return;
			}
			UE_LOG(LogAkAudio, Display, TEXT("FAkAudioModule::CreateResourceCookerForPlatform : Processing asset libraries..."));
			auto ProcessedLibraries = AssetLibraryPreCooker->Process(*AssetLibraries);
			
			UE_LOG(LogAkAudio, Display, TEXT("FAkAudioModule::CreateResourceCookerForPlatform : PreCooker processed %i asset libraries"), ProcessedLibraries.Num());
			for (const auto& LibraryPair : ProcessedLibraries)
			{
				if (LibraryPair.Key && LibraryPair.Value.IsValid())
				{
					const auto& LibraryInfo = LibraryPair.Value;
					UE_LOG(LogAkAudio, Display, TEXT(" Library '%s': %i filtered assets"), *LibraryPair.Key->GetName(), LibraryInfo->FilteredAssets.Num());
				}
			}
			
			ResourceCooker->PreCacheAssetLibraries(ProcessedLibraries);
			for (auto& LibrarySoftPtr : *AssetLibraries)
			{
				auto* WwisePackagingSettings = GetDefault<UWwisePackagingSettings>();
				WwisePackagingSettings->AssetLibrariesKeepAlive.Add(LibrarySoftPtr.Get());
			}
		}
	}
	
	auto ExternalSourceManager = IWwiseExternalSourceManager::Get();
	if (!ExternalSourceManager)
	{
		UE_LOG(LogAkAudio, Error, TEXT("FAkAudioModule::CreateResourceCookerForPlatform : No External Source Manager!"))
		return;
	}

	const auto StagePath = WwiseUnrealHelper::GetStagePathFromSettings();
	ResourceCooker->SetWwiseStagePath(StagePath);

	const auto SoundBankDirectory = WwiseUnrealHelper::GetSoundBankDirectory();
	ProjectDatabase->SetGeneratedSoundBanksPath(FDirectoryPath{ SoundBankDirectory });

	const auto& Platform = ProjectDatabase->GetCurrentPlatform().Platform.Get();
	if (!Platform.ExternalSourceRootPath.IsNone())
	{
		auto ExternalSourcePath = Platform.PathRelativeToGeneratedSoundBanks.ToString() / Platform.ExternalSourceRootPath.ToString();
		if (FPaths::IsRelative(ExternalSourcePath))
		{
			ExternalSourcePath = SoundBankDirectory / ExternalSourcePath;
		}
		ExternalSourceManager->SetExternalSourcePath(FDirectoryPath{ExternalSourcePath });
	}
}

void FAkAudioModule::ParseGeneratedSoundBankData()
{
	SCOPED_AKAUDIO_EVENT(TEXT("ParseGeneratedSoundBankData"));
	UE_LOG(LogAkAudio, Log, TEXT("FAkAudioModule::ParseGeneratedSoundBankData : Parsing Wwise project data."));
	auto* ProjectDatabase = FWwiseProjectDatabase::Get();
	if (UNLIKELY(!ProjectDatabase))
	{
		UE_LOG(LogAkAudio, Warning, TEXT("FAkAudioModule::ParseGeneratedSoundBankData : Could not get FWwiseProjectDatabase instance. Generated sound data will not be parsed."));
	}
	else
	{
		ProjectDatabase->UpdateDataStructure();
	}
}
#endif


void FAkAudioModule::InitializeAkAudioDevice()
{
#if WITH_EDITORONLY_DATA
	FWwiseInitBankLoader::Get()->UpdateInitBankInSettings();
#endif

	AkAudioDevice = new FAkAudioDevice;
	if (!AkAudioDevice)
	{
		UE_LOG(LogAkAudio, Log, TEXT("FAkAudioModule::InitializeAkAudioDevice: Couldn't create FAkAudioDevice. AkAudioModule will not be fully initialized."));
		bModuleInitialized = true;
		return;
	}

	if (!AkAudioDevice->Init())
	{
		UE_LOG(LogAkAudio, Log, TEXT("FAkAudioModule::InitializeAkAudioDevice: Couldn't initialize FAkAudioDevice. AkAudioModule will not be fully initialized."));
		bModuleInitialized = true;
		delete AkAudioDevice;
		AkAudioDevice = nullptr;
		return;
	}

	OnPreExitHandle = FCoreDelegates::OnEnginePreExit.AddRaw(this, &FAkAudioModule::OnPreExit);

	//Load init bank in Runtime
	UE_LOG(LogAkAudio, VeryVerbose, TEXT("FAkAudioModule::InitializeAkAudioDevice: Loading Init Bank."));
	FWwiseInitBankLoader::Get()->LoadInitBank();

	OnTick = FTickerDelegate::CreateRaw(AkAudioDevice, &FAkAudioDevice::Update);
	TickDelegateHandle = FCoreTickerType::GetCoreTicker().AddTicker(OnTick);

	AkAudioDevice->LoadDelayedObjects();

	UE_LOG(LogAkAudio, VeryVerbose, TEXT("FAkAudioModule::StartupModule: Module Initialized."));
	OnModuleInitialized.Broadcast();
	bModuleInitialized = true;

#if WITH_EDITOR
	PIEEndedHandle = FEditorDelegates::EndPIE.AddRaw(this, &FAkAudioModule::OnPIEExit);
#endif

	OnApplicationDeactivatedHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().Add(TDelegate<void(const bool)>::CreateRaw(this, &FAkAudioModule::OnApplicationDeactivated));
}
#undef LOCTEXT_NAMESPACE
