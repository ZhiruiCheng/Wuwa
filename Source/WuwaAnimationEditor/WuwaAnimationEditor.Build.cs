using UnrealBuildTool;

public class WuwaAnimationEditor : ModuleRules
{
    public WuwaAnimationEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "AnimationModifiers", "AnimationBlueprintLibrary" });
        PrivateDependencyModuleNames.AddRange(new[] { "Wuwa", "UnrealEd" });
        // WuwaRoleSkillSetupCommandlet：建 GA 蓝图、技能分类 Tag
        PrivateDependencyModuleNames.AddRange(new[] { "GameplayAbilities", "GameplayTags", "Kismet", "KismetCompiler" });
        // Json：WuwaBossSetupCommandlet 读 prepare_boss.py 的配方
        PrivateDependencyModuleNames.AddRange(new[] { "Json" });
        // Interchange：WuwaAnimFbxImportCommandlet 用它导入 Blender 导出的角色 FBX
        PrivateDependencyModuleNames.AddRange(new[] { "InterchangeCore", "InterchangeEngine", "InterchangePipelines", "AssetRegistry" });
        // WuwaSequenceCameraSetupCommandlet：按原作特写镜头重建 LevelSequence
        PrivateDependencyModuleNames.AddRange(new[] { "LevelSequence", "MovieScene", "MovieSceneTracks", "CinematicCamera" });
    }
}
