using UnrealBuildTool;

/** 战斗相关的编辑器工具（子弹编辑器等）。只在编辑器里加载。 */
public class WuwaCombatEditor : ModuleRules
{
    public WuwaCombatEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine", "InputCore", "Slate", "SlateCore", "RenderCore", "RHI",
            "UnrealEd", "EditorFramework", "PropertyEditor", "AdvancedPreviewScene", "ToolMenus", "ContentBrowser",
            "WorkspaceMenuStructure", "AssetRegistry",
            "Wuwa"
        });
    }
}
