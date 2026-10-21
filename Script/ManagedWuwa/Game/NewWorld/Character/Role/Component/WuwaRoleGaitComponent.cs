using UnrealSharp;
using UnrealSharp.Attributes;
using UnrealSharp.CoreUObject;
using UnrealSharp.Engine;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.NewWorld.Character.Role.Component;

/// <summary>
/// 步态决策，对应原作 RoleGaitComponent。CMC 每次物理更新前调用 RefreshPolicy（原作 OnTick → $in），
/// 按是否有移动输入分为 UpdateMovePressing / UpdateMoveReleasing，结果写入运动状态组件。
/// 不订阅运动状态事件；位置与走跑偏好读自运动状态组件，冲刺需求和步态禁用来源由本组件保存。
/// </summary>
[UClass]
public partial class UWuwaRoleGaitComponent : UWuwaRoleGaitBridgeComponent
{
    // 地面速度不超过此值视为停稳。原作 STOP_SPEED 为 5，本项目沿用原有的 2。
    private const float StopSpeed = 2f;

    private sealed class SprintWindow
    {
        public double BeganAt;
        public EWuwaSprintDesire Desire = EWuwaSprintDesire.Temporary;
    }

    private readonly Dictionary<TWeakObjectPtr<UObject>, SprintWindow> _windows = new();
    //原作 RoleGaitUnEnableState：每个步态被哪些来源禁用
    private readonly Dictionary<EWuwaGait, HashSet<TWeakObjectPtr<UObject>>> _blocked = new()
    {
        [EWuwaGait.Walk] = new(),
        [EWuwaGait.Run] = new(),
        [EWuwaGait.Sprint] = new()
    };

    private bool _refreshing;
    private bool _endingPlay;
    private EWuwaGait _lastMovingGait = EWuwaGait.Run;
    private EWuwaPositionState _lastPosition = EWuwaPositionState.None;
    private EWuwaSprintDesire _retained = EWuwaSprintDesire.None;
    private double _temporaryExpiresAt;

    protected override void InitializePolicy_Implementation()
    {
        _endingPlay = false;
        _lastPosition = ReadMovementContext().PositionState;
        _lastMovingGait = PreferredGait(GetUnifiedState());
        RefreshPolicy();
    }

    //原作 OnTick → $in()：只由主控端执行；有移动输入走 UpdateMovePressing，否则走 UpdateMoveReleasing
    //读取引擎的信息,清算残留的Sprint和一些!valid的对象然后通过输入判断当前处于哪种状态
    protected override void RefreshPolicy_Implementation()
    {
        if (_refreshing || _endingPlay)
        {
            return;
        }

        _refreshing = true;
        try
        {
            var state = GetUnifiedState();
            //以后一定要改,太刁钻了,基本不会出现这种情况的
            if (state is null)
            {
                ClearSprintRuntime();
                PublishGait(EWuwaSprintDesire.None, _lastMovingGait);
                return;
            }

            var context = ReadMovementContext();
            PruneRuntime(context);

            (EWuwaMoveState Move, EWuwaGait Gait)? next = null;
            if (context.CanDriveState)
            {
                var current = state.StateData;
                var preferred = PreferredGait(state);
                next = context.HasMoveInput
                    ? UpdateMovePressing(context, current, preferred)
                    : UpdateMoveReleasing(context, current, preferred);
            }

            // 先发布冲刺需求和 StopGait，再提交状态：状态事件的消费者看到的是同一次决策。
            PublishGait(EvaluateSprintDesire(context), _lastMovingGait);
            if (next is { } decided)
            {
                state.SetMoveState(decided.Move, decided.Gait);
            }
        }
        finally
        {
            _refreshing = false;
        }
    }

    #region Pressing / Releasing

    //原作 UpdateMovePressing
    private (EWuwaMoveState, EWuwaGait)? UpdateMovePressing(FWuwaMovementStateContext context,
        FWuwaUnifiedStateData current, EWuwaGait preferred)
    {
        switch (context.PositionState)
        {
            case EWuwaPositionState.Ground:
                return context.IsCrouching ? (EWuwaMoveState.Other, preferred) : ResolveGroundMove(context, preferred);
            case EWuwaPositionState.Water:
                return current.MoveState is EWuwaMoveState.NormalSwim or EWuwaMoveState.FastSwim
                    ? null : (EWuwaMoveState.NormalSwim, preferred);
            case EWuwaPositionState.Climb:
                return current.MoveState is EWuwaMoveState.NormalClimb or EWuwaMoveState.FastClimb
                    ? null : (EWuwaMoveState.NormalClimb, preferred);
            case EWuwaPositionState.Air:
                return UpdateAirborne(context, current, preferred);
            default:
                return null;
        }
    }

    //原作 UpdateMoveReleasing
    private (EWuwaMoveState, EWuwaGait)? UpdateMoveReleasing(FWuwaMovementStateContext context,
        FWuwaUnifiedStateData current, EWuwaGait preferred)
    {
        switch (context.PositionState)
        {
            case EWuwaPositionState.Ground:
                // 原作：瞄准朝向时松开方向不改变移动状态。
                if (current.DirectionState == EWuwaDirectionState.AimDirection)
                {
                    return null;
                }

                if (context.IsCrouching || !IsAllowed(EWuwaGait.Walk, context) && !IsAllowed(EWuwaGait.Run, context))
                {
                    return (EWuwaMoveState.Other, preferred);
                }

                var stopped = !float.IsFinite(context.GroundSpeed) || context.GroundSpeed <= StopSpeed;
                switch (current.MoveState)
                {
                    case EWuwaMoveState.Walk:
                    case EWuwaMoveState.Run:
                    case EWuwaMoveState.Sprint:
                        return stopped ? (EWuwaMoveState.Stand, preferred) : SetRunStop(current.MoveState);
                    default:
                        // 原作：其他状态（Dodge、Other、各 Stop 等）只在停稳后转 Stand；Stand 同时跟随走跑偏好。
                        return stopped || current.MoveState == EWuwaMoveState.Stand
                            ? (EWuwaMoveState.Stand, preferred) : null;
                }
            case EWuwaPositionState.Water:
            case EWuwaPositionState.Climb:
                return (EWuwaMoveState.Other, preferred);
            case EWuwaPositionState.Air:
                return UpdateAirborne(context, current, preferred);
            default:
                return null;
        }
    }

    //原作 SetRunStop 按剩余速度在 RunStop/SprintStop 间选择；本项目按当前步态选择对应的 Stop
    private static (EWuwaMoveState, EWuwaGait) SetRunStop(EWuwaMoveState current) => current switch
    {
        EWuwaMoveState.Walk => (EWuwaMoveState.WalkStop, EWuwaGait.Walk),
        EWuwaMoveState.Sprint => (EWuwaMoveState.SprintStop, EWuwaGait.Sprint),
        _ => (EWuwaMoveState.RunStop, EWuwaGait.Run)
    };

    //有方向输入时的地面步态：冲刺请求且 Sprint 未禁用 → Sprint；否则按走跑偏好，偏好被禁用时改用另一个
    private (EWuwaMoveState, EWuwaGait) ResolveGroundMove(FWuwaMovementStateContext context, EWuwaGait preferred)
    {
        // 原作另有体力条件，本项目没有体力。
        var gait = preferred;
        if (EvaluateSprintDesire(context) != EWuwaSprintDesire.None && IsAllowed(EWuwaGait.Sprint, context))
        {
            gait = EWuwaGait.Sprint;
        }
        else if (!IsAllowed(gait, context))
        {
            gait = preferred == EWuwaGait.Walk ? EWuwaGait.Run : EWuwaGait.Walk;
            if (!IsAllowed(gait, context))
            {
                // Walk 与 Run 都被禁用（本项目规则）。
                return (EWuwaMoveState.Other, gait);
            }
        }

        _lastMovingGait = gait;
        return (gait switch
        {
            EWuwaGait.Walk => EWuwaMoveState.Walk,
            EWuwaGait.Sprint => EWuwaMoveState.Sprint,
            _ => EWuwaMoveState.Run
        }, gait);
    }

    //本项目扩展（原作没有 Jump/Fall）：只在普通空中状态之间切换，不覆盖 Dodge、Glide 等由动作写入的状态
    private static (EWuwaMoveState, EWuwaGait)? UpdateAirborne(FWuwaMovementStateContext context,
        FWuwaUnifiedStateData current, EWuwaGait preferred)
    {
        if (current.MoveState is not (EWuwaMoveState.Other or EWuwaMoveState.Jump
            or EWuwaMoveState.Fall or EWuwaMoveState.Flying))
        {
            return null;
        }

        var move = context.IsFlying ? EWuwaMoveState.Flying
            : context.VerticalSpeed > 0f ? EWuwaMoveState.Jump : EWuwaMoveState.Fall;
        return (move, preferred);
    }

    #endregion

    //旧 SetDesiredGait(Sprint) 的兼容入口：有移动输入时记为长期冲刺请求，不修改走跑偏好
    protected override bool RequestSprint_Implementation()
    {
        var context = ReadMovementContext();
        if (_endingPlay || !context.CanDriveState || !CanSampleSprint(context))
        {
            return false;
        }

        _retained = context.HasMoveInput ? EWuwaSprintDesire.Sustained : EWuwaSprintDesire.None;
        _temporaryExpiresAt = 0.0;
        RefreshPolicy();
        return true;
    }

    //原作禁用步态的标签变化后立即 $in()；这里每个来源只能增删自己的限制
    protected override void SetGaitBlocked_Implementation(UObject source, EWuwaGait gait, bool blocked)
    {
        if (!source.IsValid() || !_blocked.TryGetValue(gait, out var sources))
        {
            return;
        }
        var key = new TWeakObjectPtr<UObject>(source);
        if (blocked)
        {
            sources.Add(key);
        }
        else
        {
            sources.Remove(key);
        }
        RefreshPolicy();
    }

    //原作 EnableRoleGaitState
    protected override bool IsGaitAllowed_Implementation(EWuwaGait gait) => IsAllowed(gait, ReadMovementContext());

    #region Sprint Window

    protected override void OpenSprintWindow_Implementation(UObject source)
    {
        if (_endingPlay) return;
        var context = ReadMovementContext();
        if (!source.IsValid() || !CanSampleSprint(context))
        {
            return;
        }
        // Deal with position/control exits before accepting a new window.
        PruneRuntime(context);
        _windows[new TWeakObjectPtr<UObject>(source)] = new SprintWindow { BeganAt = context.GameTimeSeconds };
        RefreshPolicy();
    }

    protected override void SampleSprintWindow_Implementation(UObject source, float inputHeldSeconds, float holdThresholdSeconds)
    {
        if (_endingPlay) return;
        if (!source.IsValid())
        {
            return;
        }
        var context = ReadMovementContext();
        PruneRuntime(context);
        if (!CanSampleSprint(context) || !_windows.TryGetValue(new TWeakObjectPtr<UObject>(source), out var window))
        {
            return;
        }

        var held = float.IsFinite(inputHeldSeconds) ? Math.Max(0f, inputHeldSeconds) : 0f;
        var threshold = float.IsFinite(holdThresholdSeconds) ? Math.Max(0f, holdThresholdSeconds) : 0.2f;
        var heldInWindow = Math.Min(Math.Max(0.0, context.GameTimeSeconds - window.BeganAt), held);
        window.Desire = heldInWindow > threshold ? EWuwaSprintDesire.Sustained : EWuwaSprintDesire.Temporary;
        RefreshPolicy();
    }

    protected override void CloseSprintWindow_Implementation(UObject source)
    {
        if (_endingPlay) return;
        if (!source.IsValid())
        {
            return;
        }
        var context = ReadMovementContext();
        PruneRuntime(context);
        if (!_windows.Remove(new TWeakObjectPtr<UObject>(source), out var window))
        {
            return; // A late/duplicate End cannot recreate a request or extend its deadline.
        }

        if (_retained != EWuwaSprintDesire.Sustained)
        {
            _retained = window.Desire;
            var duration = float.IsFinite(context.TemporarySprintDuration)
                ? Math.Max(0f, context.TemporarySprintDuration) : 1f;
            _temporaryExpiresAt = _retained == EWuwaSprintDesire.Temporary
                ? context.GameTimeSeconds + duration : 0.0;
        }
        if (!context.HasMoveInput && _retained == EWuwaSprintDesire.Sustained)
        {
            _retained = EWuwaSprintDesire.None;
            _temporaryExpiresAt = 0.0;
        }
        RefreshPolicy();
    }

    protected override void ResetSprintRequest_Implementation()
    {
        ClearSprintRuntime();
        RefreshPolicy();
    }

    protected override EWuwaSprintDesire ReadSprintDesire_Implementation() => EvaluateSprintDesire(ReadMovementContext());

    protected override float ReadSprintRemainingSeconds_Implementation()
    {
        var context = ReadMovementContext();
        if (EvaluateSprintDesire(context) != EWuwaSprintDesire.Temporary) return 0f;
        // Cutting out ends the old animation window. Do not carry its UObject/source into the new role.
        if (_windows.Any(pair => pair.Key.IsValid))
            return float.IsFinite(context.TemporarySprintDuration) ? Math.Max(0f, context.TemporarySprintDuration) : 0f;
        return (float)Math.Max(0.0, _temporaryExpiresAt - context.GameTimeSeconds);
    }

    protected override void RestoreSwitchGait_Implementation(EWuwaSprintDesire desire, float remainingSeconds, EWuwaGait lastMovingGait)
    {
        if (_endingPlay) return;
        ClearSprintRuntime();
        var context = ReadMovementContext();
        _lastPosition = context.PositionState;
        _lastMovingGait = lastMovingGait;
        if (CanSampleSprint(context) && context.HasMoveInput && IsAllowed(EWuwaGait.Sprint, context))
        {
            if (desire == EWuwaSprintDesire.Sustained) _retained = desire;
            else if (desire == EWuwaSprintDesire.Temporary && float.IsFinite(remainingSeconds) && remainingSeconds > 0f)
            {
                _retained = desire;
                _temporaryExpiresAt = context.GameTimeSeconds + remainingSeconds;
            }
        }
        RefreshPolicy();
    }

    #endregion

    protected override void ResetRuntime_Implementation()
    {
        ClearSprintRuntime();
        foreach (var sources in _blocked.Values)
        {
            sources.Clear();
        }
        RefreshPolicy();
    }

    public override void EndPlay(EEndPlayReason endPlayReason)
    {
        _endingPlay = true;
        ClearSprintRuntime();
        foreach (var sources in _blocked.Values) sources.Clear();
        base.EndPlay(endPlayReason);
    }

    //运动状态组件由角色组装时注入，不从角色身上查找
    private UWuwaUnifiedStateBridgeComponent? GetUnifiedState()
    {
        var state = UnifiedState;
        return state is not null && state.IsValid() ? state : null;
    }

    //走跑偏好由运动状态组件保存（原作 IsWalkBaseMode）
    private static EWuwaGait PreferredGait(UWuwaUnifiedStateBridgeComponent? state)
    {
        return state is not null && state.IsWalkPreferred() ? EWuwaGait.Walk : EWuwaGait.Run;
    }

    //清除跑步欲望
    private void ClearSprintRuntime()
    {
        _windows.Clear();
        _retained = EWuwaSprintDesire.None;
        _temporaryExpiresAt = 0.0;
    }

    //清理失效的禁用来源，并重新判断冲刺需求是否仍然有效
    private void PruneRuntime(FWuwaMovementStateContext context)
    {
        foreach (var sources in _blocked.Values)
        {
            sources.RemoveWhere(source => !source.IsValid);
        }
        if (context.PositionState != _lastPosition || !CanSampleSprint(context))
        {
            ClearSprintRuntime();
        }
        _lastPosition = context.PositionState;
        //判断冲刺状态
        if (_retained == EWuwaSprintDesire.Sustained && !context.HasMoveInput
            || _retained == EWuwaSprintDesire.Temporary && context.GameTimeSeconds >= _temporaryExpiresAt)
        {
            _retained = EWuwaSprintDesire.None;
            _temporaryExpiresAt = 0.0;
        }
        foreach (var source in _windows.Keys.Where(source => !source.IsValid).ToArray())
        {
            _windows.Remove(source);
        }
    }

    private EWuwaSprintDesire EvaluateSprintDesire(FWuwaMovementStateContext context)
    {
        if (!CanSampleSprint(context) || context.PositionState != _lastPosition)
        {
            return EWuwaSprintDesire.None;
        }
        var result = _retained;
        if (result == EWuwaSprintDesire.Sustained && !context.HasMoveInput
            || result == EWuwaSprintDesire.Temporary && context.GameTimeSeconds >= _temporaryExpiresAt)
        {
            result = EWuwaSprintDesire.None;
        }
        if (result == EWuwaSprintDesire.Sustained)
        {
            return result;
        }
        foreach (var (source, window) in _windows)
        {
            if (!source.IsValid)
            {
                continue;
            }
            if (window.Desire == EWuwaSprintDesire.Sustained)
            {
                return EWuwaSprintDesire.Sustained;
            }
            result = EWuwaSprintDesire.Temporary;
        }
        return result;
    }

    //判断某种步态是否允许,现在一共就三种步态
    private bool IsAllowed(EWuwaGait gait, FWuwaMovementStateContext context)
    {
        return context.CanDriveState && context.PositionState == EWuwaPositionState.Ground
            && !context.IsCrouching && _blocked.TryGetValue(gait, out var sources)
            && !sources.Any(source => source.IsValid);
    }

    //
    private static bool CanSampleSprint(FWuwaMovementStateContext context)
    {
        return context.CanDriveState && context.PositionState == EWuwaPositionState.Ground
            && !context.IsCrouching && double.IsFinite(context.GameTimeSeconds) && context.GameTimeSeconds >= 0.0;
    }
}
