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

using UnrealBuildTool;

public class WwiseMotionOpenXRHelpers : ModuleRules
{
	public WwiseMotionOpenXRHelpers(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		bLegacyPublicIncludePaths = false;
		bAllowConfidentialPlatformDefines = true;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"InputDevice",
				"ApplicationCore",
				"AkAudio"
			}
		);

		bool bSupportOpenXR = false;
		PrivateDependencyModuleNames.Add("WwiseSoundEngine");
		if (Target.Platform == UnrealTargetPlatform.Win64 || (Target.Platform == UnrealTargetPlatform.Android &&
		                                                      Target.Architecture == UnrealArch.Arm64))
		{
			PublicIncludePathModuleNames.Add("OpenXRHMD");
			PublicDependencyModuleNames.Add("OpenXRHMD");
			PublicDependencyModuleNames.Add("InputCore"); //EControllerHand

			bSupportOpenXR = true;
		}

		PrivateDefinitions.Add("WWISE_OPENXR_SUPPORT=" + (bSupportOpenXR ? "1" : "0"));
	}
}
