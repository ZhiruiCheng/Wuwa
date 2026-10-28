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
using System.IO;
using System.Linq;
using System.Collections.Generic;

#if UE_5_3_OR_LATER
using Microsoft.Extensions.Logging;
#else
using EpicGames.Core;
#endif

public class WwiseSoundEngine_2023_1 : WwiseSoundEngineVersionBase
{
	public override List<string> AkLibs 
	{ 
		get 
		{
			return new List<string> 
			{
				"AkSoundEngine",
				"AkMusicEngine",
				"AkMemoryMgr",
				"AkStreamMgr",
				"AkSpatialAudio",
				"AkAudioInputSource",
				"AkVorbisDecoder",
				"AkMeterFX", // AkMeter does not have a dedicated DLL
			};
		}
	}
	public override string VersionNumber { get { return "2023_1"; } }
}
