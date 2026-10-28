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

#include "Wwise/Packaging/WwisePackagingFactories.h"

#include "Wwise/Packaging/WwiseAssetLibrary.h"
#include "Wwise/Packaging/WwiseAssetLibraryGroup.h"
#include "Wwise/Packaging/WwisePackagingUtils.h"
#include "Wwise/Packaging/WwiseSharedAssetLibraryFilter.h"
#include "Wwise/WwiseEditorUtilsModule.h"
#include "Wwise/WwisePackagingEditorModule.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/MessageDialog.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

UWwisePackagingFactory::UWwisePackagingFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UWwiseAssetLibrary::StaticClass();

	bCreateNew = true;
	bEditorImport = true;
	bEditAfterNew = true;
}

UObject* UWwisePackagingFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context,
	FFeedbackContext* Warn)
{
	return NewObject<UWwiseAssetLibrary>(InParent, Name, Flags);
}

uint32 UWwisePackagingFactory::GetMenuCategories() const
{
	if (auto* EditorUtils = IWwiseEditorUtilsModule::GetModule())
	{
		return EditorUtils->GetWwiseAssetTypeCategory();
	}
	else
	{
		return EAssetTypeCategories::Sounds;
	}
}

const TArray<FText>& UWwisePackagingFactory::GetMenuCategorySubMenus() const
{
	if (auto* EditorUtils = IWwisePackagingEditorModule::GetModule())
	{
		return EditorUtils->GetAssetLibrarySubMenu();
	}
	else
	{
		static TArray<FText> Submenu{};
		return Submenu;
	}
}


UWwiseSharedAssetLibraryFilterFactory::UWwiseSharedAssetLibraryFilterFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UWwiseSharedAssetLibraryFilter::StaticClass();

	bCreateNew = true;
	bEditorImport = true;
	bEditAfterNew = true;
}

UObject* UWwiseSharedAssetLibraryFilterFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context,
	FFeedbackContext* Warn)
{
	return NewObject<UWwiseSharedAssetLibraryFilter>(InParent, Name, Flags);
}

uint32 UWwiseSharedAssetLibraryFilterFactory::GetMenuCategories() const
{
	if (auto* EditorUtils = IWwiseEditorUtilsModule::GetModule())
	{
		return EditorUtils->GetWwiseAssetTypeCategory();
	}
	else
	{
		return EAssetTypeCategories::Sounds;
	}
}

const TArray<FText>& UWwiseSharedAssetLibraryFilterFactory::GetMenuCategorySubMenus() const
{
	if (auto* EditorUtils = IWwisePackagingEditorModule::GetModule())
	{
		return EditorUtils->GetSharedAssetLibraryFilterSubMenu();
	}
	else
	{
		static TArray<FText> Submenu{};
		return Submenu;
	}
}

UWwiseAssetLibraryGroupFactory::UWwiseAssetLibraryGroupFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UWwiseAssetLibraryGroup::StaticClass();

	bCreateNew = true;
	bEditorImport = true;
	bEditAfterNew = true;
}

UObject* UWwiseAssetLibraryGroupFactory::FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context,
	FFeedbackContext* Warn)
{
	FString Package = WwisePackagingUtils::GetGameplayModuleRoot(InParent->GetPathName());
	auto AssetRegistryModule = &FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry");
	TArray<FAssetData> WwiseAssets;
	FARFilter Filter;
	Filter.bRecursivePaths = true;
	Filter.PackagePaths.Add(FName(Package));
	Filter.ClassPaths.Add(UWwiseAssetLibraryGroup::StaticClass()->GetClassPathName());
	AssetRegistryModule->Get().GetAssets(Filter, WwiseAssets);
	if (!WwiseAssets.IsEmpty())
	{
		auto Text = FString::Printf(TEXT("Only one Wwise Asset Library Group can exist per Gameplay Module. Wwise Asset Library Group was found at path:\n %s"), *WwiseAssets[0].PackageName.ToString());
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Text));
		return nullptr;
	}
	return NewObject<UWwiseAssetLibraryGroup>(InParent, Name, Flags);
}

uint32 UWwiseAssetLibraryGroupFactory::GetMenuCategories() const
{
	if (auto* EditorUtils = IWwiseEditorUtilsModule::GetModule())
	{
		return EditorUtils->GetWwiseAssetTypeCategory();
	}
	else
	{
		return EAssetTypeCategories::Sounds;
	}
}

const TArray<FText>& UWwiseAssetLibraryGroupFactory::GetMenuCategorySubMenus() const
{
	if (auto* EditorUtils = IWwisePackagingEditorModule::GetModule())
	{
		return EditorUtils->GetSharedAssetLibraryFilterSubMenu();
	}
	else
	{
		static TArray<FText> Submenu{};
		return Submenu;
	}
}