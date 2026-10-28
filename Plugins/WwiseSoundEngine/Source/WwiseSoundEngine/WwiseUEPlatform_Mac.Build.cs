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
using System;
using System.IO;
using System.Collections.Generic;
using EpicGames.Core;

public class WwiseUEPlatform_Mac : WwiseUEPlatform
{
	public WwiseUEPlatform_Mac(ReadOnlyTargetRules in_TargetRules, string in_ThirdPartyFolder) : base(in_TargetRules, in_ThirdPartyFolder)
	{
	}

	public override string GetLibraryFullPath(string LibName, string LibPath)
	{
		return Path.Combine(LibPath, "lib" + LibName + ".a");
	}

	public override bool SupportsAkAutobahn { get { return Target.Configuration != UnrealTargetConfiguration.Shipping; } }

	public override bool SupportsCommunication { get { return true; } }

	public override bool SupportsDeviceMemory { get { return false; } }

	public override string AkPlatformLibDir { get { 
		string xCodePath = UnrealBuildTool.Utils.RunLocalProcessAndReturnStdOut("/bin/sh", "-c 'xcode-select -p'");
		DirectoryReference DeveloperDir = new DirectoryReference(xCodePath);
		FileReference Plist = FileReference.Combine(DeveloperDir.ParentDirectory!, "Info.plist");
		// Find out the version number in Xcode.app/Contents/Info.plist
		string ReturnedVersion = UnrealBuildTool.Utils.RunLocalProcessAndReturnStdOut("/bin/sh",
			$"-c 'plutil -extract CFBundleShortVersionString raw {Plist}'");
		string[] Version = ReturnedVersion.Split('.');
		if (Version.Length == 2)
		{
			var Major = int.Parse(Version[0]);
			return "Mac_Xcode"+Major+"00";
		}
#if UE_5_7_OR_LATER
		return "Mac_Xcode1600"; 
#else
		return "Mac_Xcode1500"; 
#endif
		} 
	}

	public override string DynamicLibExtension { get { return "dylib"; } }

	public override List<string> GetAdditionalWwiseLibs()
	{
		return new List<string>();
	}
	
	public override List<string> GetPublicSystemLibraries()
	{
		return new List<string>();
	}

	public override List<string> GetPublicDelayLoadDLLs()
	{
		return new List<string>();
	}

	public override List<string> GetPublicDefinitions()
	{
		string MacPlatformFolderDefine = string.Format("WWISE_MAC_PLATFORM_FOLDER=\"{0}\"", AkPlatformLibDir);
		return new List<string>
		{
			MacPlatformFolderDefine
		};
	}

	public override Tuple<string, string> GetAdditionalPropertyForReceipt(string ModuleDirectory)
	{
		return null;
	}

	public override List<string> GetPublicFrameworks()
	{
		return new List<string>
		{
			"AudioUnit",
			"AudioToolbox",
			"AVFoundation",
			"CoreAudio"
		};
	}
}
