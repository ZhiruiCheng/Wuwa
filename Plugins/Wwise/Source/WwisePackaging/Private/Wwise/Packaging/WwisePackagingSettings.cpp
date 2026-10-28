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

#include "Wwise/Packaging/WwisePackagingSettings.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Wwise/Packaging/WwisePackagingUtils.h"


#if WITH_EDITOR
#include "Engine/Engine.h"
#include "Misc/ConfigCacheIni.h"
#include "AssetToolsModule.h"
#include "FileHelpers.h"
#include "ISourceControlModule.h"
#include "ModuleDescriptor.h"
#include "SSettingsEditorCheckoutNotice.h"
#include "Interfaces/IPluginManager.h"
#include "Wwise/Packaging/WwiseAssetLibrary.h"
#include "Wwise/Packaging/WwiseAssetLibraryFilter.h"
#include "Wwise/Stats/Packaging.h"
#endif

#define LOCTEXT_NAMESPACE "WwisePackaging"

#if WITH_EDITOR

void UWwisePackagingSettings::ProcessGameplayModulesUsingDefaultGroup()
{
	GameplayModuleWithoutGroups.Empty();
	GetGameplayModuleUsingDefaultGroup(GameplayModuleWithoutGroups);
}

void UWwisePackagingSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	OnSettingsChanged.Broadcast(this);
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UWwisePackagingSettings::PostInitProperties()
{
	Super::PostInitProperties();
}

bool UWwisePackagingSettings::EnsurePostEngineInit()
{
	if (const ELoadingPhase::Type CurrentPhase{ IPluginManager::Get().GetLastCompletedLoadingPhase() };
		CurrentPhase == ELoadingPhase::None || CurrentPhase < ELoadingPhase::PostDefault || !GEngine)
	{
		return false;
	}

	return true;
}

void UWwisePackagingSettings::CreateInitialAssetLibraryGroups()
{
	bool bShouldCreateNewAssetLibraryGroup = ShouldPromptInitialAssetLibraryGroups();
	if(bPackageAsBulkData && bShouldCreateNewAssetLibraryGroup)
	{
		if (!EnsurePostEngineInit()) return;

		auto AssetRegistryModule = &FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
		TArray<FAssetData> LibraryAssets;
		FARFilter LibraryFilter;
		LibraryFilter.bRecursivePaths = true;
		LibraryFilter.ClassPaths.Add(UWwiseAssetLibrary::StaticClass()->GetClassPathName());
		AssetRegistryModule->Get().GetAssets(LibraryFilter, LibraryAssets);
		
		static const FString AssetNameSuffix{ TEXT("WwiseLibraryGroup") };

		TMultiMap<FString, FAssetData> Libraries;

		for (auto& WwiseAsset : LibraryAssets)
		{
			auto Root = WwisePackagingUtils::GetGameplayModuleRoot(WwiseAsset.GetObjectPathString());
			Libraries.Add(Root.RightChop(1), WwiseAsset);
		}

		TArray<FString> Roots;
		Libraries.GetKeys(Roots);
		auto& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		for (auto& Root : Roots)
		{
			FString GroupName = Root + AssetNameSuffix;
			FString CreationPath = "/" + Root;
			TArray<FAssetData> Assets;
			Libraries.MultiFind(Root, Assets);
			UWwiseAssetLibraryGroup* AssetLibraryGroup = Cast<UWwiseAssetLibraryGroup>(AssetToolsModule.CreateAsset(GroupName, CreationPath, UWwiseAssetLibraryGroup::StaticClass(), nullptr));
			
			for (auto& WwiseAsset : Assets)
			{
				TSoftObjectPtr<UWwiseAssetLibrary> SoftPtr(WwiseAsset.GetSoftObjectPath());
				AssetLibraryGroup->Libraries.Add(SoftPtr);
			}
			UEditorLoadingAndSavingUtils::SavePackages({ AssetLibraryGroup->GetPackage() }, true);
			UE_LOG(LogWwisePackaging, Log, TEXT("UWwisePackagingSettings::CreateInitialAssetLibraryGroups: Created Library Group %s at %s"), *GroupName, *CreationPath);
		}
		
		bMigratedToLibraryGroup= true;
		SaveConfigFile();
	}
	else if(bShouldCreateNewAssetLibraryGroup)
	{
		if (!EnsurePostEngineInit()) return;

		bMigratedToLibraryGroup = true;
		SaveConfigFile();
	}
}

bool UWwisePackagingSettings::ShouldPromptInitialAssetLibraryGroups() const
{
	if (bMigratedToLibraryGroup)
	{
		return false;
	}
	auto AssetRegistryModule = &FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> LibraryGroupAssets;
	FARFilter GroupFilter;
	GroupFilter.bRecursivePaths = true;
	GroupFilter.ClassPaths.Add(UWwiseAssetLibraryGroup::StaticClass()->GetClassPathName());
	AssetRegistryModule->Get().GetAssets(GroupFilter, LibraryGroupAssets);
	if (LibraryGroupAssets.Num() != 0)
	{
		return false;
	}

	TArray<FAssetData> LibraryAssets;
	FARFilter LibraryFilter;
	LibraryFilter.bRecursivePaths = true;
	LibraryFilter.ClassPaths.Add(UWwiseAssetLibrary::StaticClass()->GetClassPathName());
	AssetRegistryModule->Get().GetAssets(LibraryFilter, LibraryAssets);
	return LibraryAssets.Num() > 0;
}

bool UWwisePackagingSettings::SaveConfigFile()
{
	const FString ConfigFilename = GetDefaultConfigFilename();
	if(ISourceControlModule::Get().IsEnabled())
	{
		if (!SettingsHelpers::IsCheckedOut(ConfigFilename, true))
		{
			if (!SettingsHelpers::CheckOutOrAddFile(ConfigFilename, true))
			{
				return false;
			}
		}
	}

	return TryUpdateDefaultConfigFile();
}

void UWwisePackagingSettings::GetGameplayModuleUsingDefaultGroup(TArray<FString>& OutGameplayModuleUsedInDefaultGroup)
{
	if (TemplateAssetLibraryGroup.GetLongPackageFName().IsNone())
	{
		return;	
	}
	auto AssetRegistryModule = &FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> LibraryGroupAssets;
	FARFilter GroupFilter;
	GroupFilter.bRecursivePaths = true;
	GroupFilter.ClassPaths.Add(UWwiseAssetLibraryGroup::StaticClass()->GetClassPathName());
	AssetRegistryModule->Get().GetAssets(GroupFilter, LibraryGroupAssets);

	TSet<FString> ModulesNotUsingDefaultGroup;
	const auto DefaultRoot = WwisePackagingUtils::GetGameplayModuleRoot(TemplateAssetLibraryGroup.GetLongPackageName());
	OutGameplayModuleUsedInDefaultGroup.Add(DefaultRoot.RightChop(1));
	for (auto& LibraryGroupPath : LibraryGroupAssets)
	{
		auto GroupRoot = WwisePackagingUtils::GetGameplayModuleRoot(LibraryGroupPath.GetObjectPathString());
		if (DefaultRoot != GroupRoot)
		{
			ModulesNotUsingDefaultGroup.Add(GroupRoot);
		}
	}
	
	TArray<FAssetData> WwiseAssets;
	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.TagsAndValues.Add("WwiseGuid");
	AssetRegistryModule->Get().GetAssets(Filter, WwiseAssets);

	for (auto& WwiseAsset : WwiseAssets)
	{
		auto Root = WwisePackagingUtils::GetGameplayModuleRoot(WwiseAsset.GetObjectPathString());
		if (ModulesNotUsingDefaultGroup.Contains(Root))
		{
			continue;
		}
		OutGameplayModuleUsedInDefaultGroup.AddUnique(Root.RightChop(1));
	}
}
#endif

#undef LOCTEXT_NAMESPACE
