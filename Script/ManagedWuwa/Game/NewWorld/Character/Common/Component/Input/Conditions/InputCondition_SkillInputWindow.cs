using UnrealSharp;
using UnrealSharp.Attributes;
using UnrealSharp.CoreUObject;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.NewWorld.Character.Common.Component.Input.Conditions;

/// <summary>
/// 当前主技能的接招状态（原作 BP_Input 里的 GetSkillAcceptInput / GetCommonSkillCanBeInterrupt）：
/// AcceptInput = 在接招窗口里；AllowReadyEnd = 已 ReadyEnd（或没有主技能，原作 IsMainSkillReadyEnd 初值为真）也算。
/// 例：长离空中长按普攻 1205105 要求 接招窗口 或 ReadyEnd；心眼·跃后接下落攻击只认接招窗口。
/// </summary>
[UClass]
public partial class UInputCondition_SkillInputWindow : UWuwaInputCondition
{
    [UProperty(PropertyFlags.EditAnywhere)]
    public partial bool AllowReadyEnd { get; set; }

    protected override bool Evaluate_Implementation(FWuwaInputCommandContext context)
    {
        var skills = context.Avatar.IsValid() ? context.Avatar.SkillComponent : null;
        if (skills is null || !skills.IsValid()) return false;
        return skills.IsSkillAcceptingInput() || AllowReadyEnd && skills.IsMainSkillReadyEnd();
    }
}
