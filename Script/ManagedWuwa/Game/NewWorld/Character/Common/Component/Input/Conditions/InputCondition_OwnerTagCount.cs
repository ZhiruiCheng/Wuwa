using UnrealSharp;
using UnrealSharp.Attributes;
using UnrealSharp.CoreUObject;
using UnrealSharp.GameplayTags;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.NewWorld.Character.Common.Component.Input.Conditions;

/// <summary>
/// 角色身上某个 Tag 的层数在 [Minimum, Maximum] 里（Maximum &lt; 0 = 不限上限）。
/// 原作长离空中普攻：1205101 + TagCount(动作标识.空中普攻次数)，夹到 1205105；层数由 buff 1205000003 每加一次多一份。
/// </summary>
[UClass]
public partial class UInputCondition_OwnerTagCount : UWuwaInputCondition
{
    [UProperty(PropertyFlags.EditAnywhere)]
    public partial FGameplayTag Tag { get; set; }

    [UProperty(PropertyFlags.EditAnywhere)]
    public partial int Minimum { get; set; }

    [UProperty(PropertyFlags.EditAnywhere)]
    public partial int Maximum { get; set; }

    protected override bool Evaluate_Implementation(FWuwaInputCommandContext context)
    {
        if (!context.ASC.IsValid() || !Tag.IsValid) return false;
        int count = context.ASC.GetGameplayTagCount(Tag);
        return count >= Minimum && (Maximum < 0 || count <= Maximum);
    }
}
