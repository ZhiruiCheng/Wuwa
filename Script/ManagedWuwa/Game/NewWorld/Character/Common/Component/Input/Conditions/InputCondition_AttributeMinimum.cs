using UnrealSharp;
using UnrealSharp.Attributes;
using UnrealSharp.CoreUObject;
using UnrealSharp.GameplayAbilities;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.NewWorld.Character.Common.Component.Input.Conditions;

/// <summary>示例派生条件：ASC 当前属性达到阈值。对象只有配置，不保存任何角色的运行数据。</summary>
[UClass]
public partial class UInputCondition_AttributeMinimum : UWuwaInputCondition
{
    [UProperty(PropertyFlags.EditAnywhere)]
    public partial FGameplayAttribute Attribute { get; set; }

    [UProperty(PropertyFlags.EditAnywhere)]
    public partial float Minimum { get; set; }

    protected override bool Evaluate_Implementation(FWuwaInputCommandContext context)
    {
        if (!context.ASC.IsValid() || !float.IsFinite(Minimum))
        {
            return false;
        }
        // 引擎 ASC 自带的查询：属性无效或角色没有对应的 AttributeSet 时 found 为 false。
        float value = context.ASC.GetGameplayAttributeValue(Attribute, out bool found);
        return found && float.IsFinite(value) && value >= Minimum;
    }
}
