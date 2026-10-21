using UnrealSharp;
using UnrealSharp.Attributes;
using UnrealSharp.CoreUObject;
using UnrealSharp.GameplayAbilities;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.NewWorld.Character.Common.Component.Input.Conditions;

/// <summary>
/// 属性已满：当前值 ≥ 上限属性（原作 BattleUiRoleData.CanUseUltraSkill：共鸣能量 Energy ≥ EnergyMax 才能放大招）。
/// </summary>
[UClass]
public partial class UInputCondition_AttributeFull : UWuwaInputCondition
{
    [UProperty(PropertyFlags.EditAnywhere)]
    public partial FGameplayAttribute Attribute { get; set; }

    [UProperty(PropertyFlags.EditAnywhere)]
    public partial FGameplayAttribute MaxAttribute { get; set; }

    protected override bool Evaluate_Implementation(FWuwaInputCommandContext context)
    {
        if (!context.ASC.IsValid()) return false;
        float value = context.ASC.GetGameplayAttributeValue(Attribute, out bool found);
        float max = context.ASC.GetGameplayAttributeValue(MaxAttribute, out bool foundMax);
        return found && foundMax && float.IsFinite(value) && max > 0f && value >= max;
    }
}
