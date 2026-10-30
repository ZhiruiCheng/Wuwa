// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Wuwa : ModuleRules
{
	public Wuwa(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[]
    {
            "Core",
            "CoreUObject",
            "Engine",
            "DeveloperSettings",
            "InputCore",
            "EnhancedInput",
			// GAS三大核心模块
			"GameplayAbilities",
            "GameplayTags",
            "GameplayTasks",
            "ControlRig",
            "RigVM",
            "AnimationCore",
            "Niagara",
            "LGUI"
    });


        // LTween：LGUI 自带的补间库，界面打开/关闭动画用（Game/LGUI/Component/WuwaUiViewAnimation）。
        PrivateDependencyModuleNames.AddRange(new string[] { "LGUI", "LTween" });
        // Wwise：音频通知与特效音频播放原作同名事件（Game/Audio/WuwaWwiseAudio）。WwiseSoundEngine 提供 AkAudioDevice.h 用到的 SDK 头。
        PrivateDependencyModuleNames.AddRange(new string[] { "AkAudio", "WwiseSoundEngine" });
        // 技能特写镜头（大招）：C# 序列镜头（ZRCamera/SequenceCamera）用 LevelSequenceActor 播原作重建的镜头序列；C++ 这边只剩配置结构体
        PrivateDependencyModuleNames.AddRange(new string[] { "LevelSequence", "MovieScene", "CinematicCamera" });
        if (Target.bBuildEditor)
        {
            // RHI、ImageCore：特效截图测试 EffectVisual.* 判断 NullRHI、保存视口像素。
            // NiagaraEditor、AssetRegistry：Editor/Effects 下按参数重建 / 新建参考 Niagara 系统。
            // ImageWrapper、MeshDescription、StaticMeshDescription、RenderCore：把配方里的 PNG / 网格导入成资产并编译重建材质。
            PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "BlueprintGraph", "AnimGraph", "KismetCompiler", "Json", "RHI", "ImageCore", "NiagaraEditor", "AssetRegistry",
                "ImageWrapper", "MeshDescription", "StaticMeshDescription", "RenderCore",
                "ControlRig", "ControlRigDeveloper", "AnimGraphRuntime" });
            // Landscape：WuwaTerrainImport commandlet 用原作高度图建地形
            PrivateDependencyModuleNames.Add("Landscape");
            // Interchange、AssetTools：WuwaPropImport commandlet 导入原作网格（glb）并移到沿用原作目录的路径
            PrivateDependencyModuleNames.AddRange(new string[] { "InterchangeCore", "InterchangeEngine", "InterchangePipelines", "AssetTools" });
        }

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });
		
		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
