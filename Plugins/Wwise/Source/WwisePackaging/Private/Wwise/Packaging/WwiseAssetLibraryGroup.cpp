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

#include "Wwise/Packaging/WwiseAssetLibraryGroup.h"

#include "Misc/MessageDialog.h"
#include "UObject/Package.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Wwise/Packaging/WwisePackagingSettings.h"
#include "Wwise/Packaging/WwisePackagingUtils.h"

#if WITH_EDITOR
void UWwiseAssetLibraryGroup::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	auto Package = WwisePackagingUtils::GetGameplayModuleRoot(GetPackage()->GetPathName());
	bool bError = false;
	for (auto& Library : Libraries)
	{
		auto PackageName = Library.GetLongPackageFName();
		if (PackageName.IsNone())
		{
			continue;
		}
		auto LibraryPath = WwisePackagingUtils::GetGameplayModuleRoot(Library.GetLongPackageName());

		if (LibraryPath != Package)
		{
			Library.Reset();
			bError = true;
		}
	}
	if (bError)
	{
		FString GameplayModule = WwisePackagingUtils::GetGameplayModuleRoot(GetPathName());
		auto Text = FString::Printf(TEXT("Wwise Asset Library Groups can only contain Libraries from the same gameplay module (%s)"), *GameplayModule);
		FMessageDialog::Open(EAppMsgType::Ok, FText::FromString(Text));
	}
}
#endif