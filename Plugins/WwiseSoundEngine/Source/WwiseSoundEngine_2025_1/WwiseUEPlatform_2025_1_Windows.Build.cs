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

public abstract class WwiseUEPlatform_2025_1_Windows : WwiseUEPlatform_Windows
{
	public WwiseUEPlatform_2025_1_Windows(ReadOnlyTargetRules in_TargetRules, string in_ThirdPartyFolder) : base(in_TargetRules, in_ThirdPartyFolder) {}
}

public class WwiseUEPlatform_2025_1_Win32 : WwiseUEPlatform_Win32
{
	public WwiseUEPlatform_2025_1_Win32(ReadOnlyTargetRules in_TargetRules, string in_ThirdPartyFolder) : base(in_TargetRules, in_ThirdPartyFolder) {}
	public override string PlatformPrefix { get { return "Win32"; } }
}

public class WwiseUEPlatform_2025_1_Win64 : WwiseUEPlatform_Win64
{
	public WwiseUEPlatform_2025_1_Win64(ReadOnlyTargetRules in_TargetRules, string in_ThirdPartyFolder) : base(in_TargetRules, in_ThirdPartyFolder) {}
	public override string PlatformPrefix { get { return "x64"; } }
}
