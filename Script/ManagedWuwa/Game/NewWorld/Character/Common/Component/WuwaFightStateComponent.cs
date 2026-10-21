using UnrealSharp.Attributes;
using UnrealSharp.Engine;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.NewWorld.Character.Common.Component;

/// <summary>
/// 对应鸣潮 FightState 的本地裁决：先比较类别，再比较该类别内的优先级。
/// 不持有 GA，不取消技能，不修改 UnifiedState；这些由后续技能/受击流程协调。
/// </summary>
[UClass]
public partial class UWuwaFightStateComponent : UWuwaFightStateBridgeComponent
{
    private int _lastIssuedHandle;
    private bool _endingPlay;

    protected override bool CheckSwitchState_Implementation(EWuwaFightState newState, int subStatePriority)
    {
        if (_endingPlay || _lastIssuedHandle == int.MaxValue || !IsKnownState(newState) || subStatePriority is < 0 or > 255)
        {
            return false;
        }

        var current = StateData;
        if (newState != current.State)
        {
            return (int)newState > (int)current.State;
        }

        if (subStatePriority != current.SubStatePriority)
        {
            return subStatePriority > current.SubStatePriority;
        }

        // 原作只有这四类允许相同类别、相同子优先级的新请求替换旧占用。
        return newState is EWuwaFightState.Skill or EWuwaFightState.Hit
            or EWuwaFightState.WeaknessBreak or EWuwaFightState.SpecialSkill;
    }

    //Try是为了防止重入,在广播的过程中被占用,但是目前一般不会占用可以直接当作SwitchState看
    protected override int TrySwitchState_Implementation(EWuwaFightState newState, int subStatePriority)
    {
        if (!CheckSwitchState(newState, subStatePriority))
        {
            return 0;
        }

        int handle = ++_lastIssuedHandle;
        PublishFightState(new FWuwaFightStateData
        {
            State = newState,
            SubStatePriority = subStatePriority,
            Handle = handle
        });

        // 广播可能同步触发另一次切换；不把已经被替换的句柄当作成功返回。
        return StateData.Handle == handle ? handle : 0;
    }

    protected override bool ExitState_Implementation(int handle)
    {
        if (handle <= 0 || StateData.Handle != handle)
        {
            return false;
        }

        PublishFightState(default);
        return true;
    }

    protected override void ResetState_Implementation()
    {
        // 不重置编号计数，否则重置前的迟到 Exit 可能匹配到新占用。
        PublishFightState(default);
    }

    public override void EndPlay(EEndPlayReason endPlayReason)
    {
        _endingPlay = true;
        ResetState();
        base.EndPlay(endPlayReason);
    }

    private static bool IsKnownState(EWuwaFightState state) => state is
        EWuwaFightState.Skill or EWuwaFightState.Hit or EWuwaFightState.SkillOverrideHit
        or EWuwaFightState.ParryHit or EWuwaFightState.SkillOverrideParry
        or EWuwaFightState.WeaknessBreak or EWuwaFightState.SkillOverrideWeaknessBreak
        or EWuwaFightState.Captured or EWuwaFightState.SpecialSkill or EWuwaFightState.StateMachine;
}
