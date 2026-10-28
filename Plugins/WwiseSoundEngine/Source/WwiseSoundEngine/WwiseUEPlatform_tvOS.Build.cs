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

public class WwiseUEPlatform_TVOS : WwiseUEPlatform
{
	public WwiseUEPlatform_TVOS(ReadOnlyTargetRules in_TargetRules, string in_ThirdPartyFolder) : base(in_TargetRules, in_ThirdPartyFolder) {}

	public override string GetLibraryFullPath(string LibName, string LibPath)
	{
		return Path.Combine(LibPath, "lib" + LibName + ".a");
	}

	public override bool SupportsAkAutobahn { get { return false; } }

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
				return "tvOS_Xcode"+Major+"00";
			}
#if UE_5_7_OR_LATER
		return "tvOS_Xcode1600"; 
#else
		return "tvOS_Xcode1500"; 
#endif
		} 
	}

	public override string DynamicLibExtension { get { return "framework"; } }

	public override List<string> GetPublicLibraryPaths()
	{
		return new List<string>
		{
			Path.Combine(ThirdPartyFolder, AkPlatformLibDir, WwiseConfigurationDir + "-appletvos", "lib")
		};
	}

	public override List<string> GetAdditionalWwiseLibs()
	{
		return GetAllLibrariesInFolder(Path.Combine(ThirdPartyFolder, AkPlatformLibDir, WwiseConfigurationDir + "-appletvos", "lib"), "a", true);
	}

	public override List<string> GetRuntimeDependencies()
	{
		return new List<string>();
	}

	public override List<string> GetPublicDelayLoadDLLs()
	{
		return new List<string>();
	}

	public override List<string> GetPublicSystemLibraries()
	{
		return new List<string>();
	}

	public override List<string> GetPublicDefinitions()
	{
		string TvOsPlatformFolderDefine = string.Format("WWISE_TVOS_PLATFORM_FOLDER=\"{0}\"", AkPlatformLibDir);
		return new List<string>
		{
			TvOsPlatformFolderDefine
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
			"AudioToolbox",
			"AVFoundation",
			"CoreAudio"
		};
	}

	public override string WwiseDspDir
	{
		get { return Path.Combine(WwiseConfigurationDir + "-appletvos"); }
	}

	public override IDictionary<string, string> GetAdditionalFrameworks()
	{
		IDictionary<string, string> Result = new Dictionary<string, string>();
		var frameworkRootPath = Path.Combine(ThirdPartyFolder, AkPlatformLibDir, WwiseDspDir, "bin");
		if (!Directory.Exists(frameworkRootPath))
		{
			return Result;
		}

		// .framework are folders, not files
		var ResultPaths = Directory.GetDirectories(frameworkRootPath, "*" + DynamicLibExtension);

		foreach (var ResultPath in ResultPaths)
		{
			string ResultName = Path.GetFileNameWithoutExtension(ResultPath);
			Result.Add(ResultName, ResultPath);
		}
		return Result;
	}
}
