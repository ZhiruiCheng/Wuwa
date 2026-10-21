using UnrealSharp;
using UnrealSharp.Attributes;
using UnrealSharp.CoreUObject;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.NewWorld.Character.Common.Component.Input.Conditions;

/// <summary>
/// 有 / 没有移动输入（原作 BP_InputComponent 闪避按下：有方向输入出前闪 100001 / 空中前闪 100003，否则后闪 100002 / 空中后闪 100004）。
/// </summary>
[UClass]
public partial class UInputCondition_MoveInput : UWuwaInputCondition
{
    /// <summary>真 = 要求有移动输入；假 = 要求没有。</summary>
    [UProperty(PropertyFlags.EditAnywhere)]
    public partial bool RequireInput { get; set; }

    protected override bool Evaluate_Implementation(FWuwaInputCommandContext context)
    {
        if (!context.Avatar.IsValid()) return false;
        return context.Avatar.PlayerInputState.HasMoveInput == RequireInput;
    }
}
