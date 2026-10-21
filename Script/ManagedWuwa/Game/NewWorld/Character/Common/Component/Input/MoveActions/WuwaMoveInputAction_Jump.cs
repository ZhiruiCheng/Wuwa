using UnrealSharp;
using UnrealSharp.Attributes;
using UnrealSharp.CoreUObject;
using UnrealSharp.GameplayTags;
using ManagedWuwa.Game.NewWorld.Character.Common.Component.Input;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.NewWorld.Character.Common.Component.Input.MoveActions;

/// <summary>
/// 地面跳跃的输入入口，对应原作 CharacterMoveComponent.JumpPress（地面分支）。
/// 配置中把同一个跳跃 Tag 的 Pressed / Released / Canceled 三行都指向此类。
/// 起跳没有初速度，整段跳跃来自起跳动画的根运动（UWuwaMovementComponent.BeginRootMotionJump），所以松开按键没有作用（没有长按跳高）。
/// </summary>
[UClass(ClassFlags.EditInlineNew | ClassFlags.DefaultToInstanced)]
public partial class UWuwaMoveInputAction_Jump : UWuwaMoveInputAction
{
    // 原作 CommonParam jump_priority：跳跃打断技能时用的等级（技能按 DoCheckInterrupt 规则让位）。
    private const int JumpInterruptLevel = 8;

    public override bool Execute(FWuwaMoveInputContext context)
    {
        var character = context.Character;
        if (!character.IsValid())
        {
            return false;
        }

        if (context.InputEvent.Phase is EWuwaInputPhase.Released or EWuwaInputPhase.Canceled)
        {
            return true;
        }

        // 不在每帧 Triggered 或长按 Held 时重复请求起跳。
        if (context.InputEvent.Phase != EWuwaInputPhase.Pressed)
        {
            return false;
        }

        var movement = character.CharacterMovement as UWuwaMovementComponent;
        var state = character.UnifiedStateComponent;
        if (movement is null || !movement.IsValid() || !state.IsValid() || !state.CanDriveState()
            || state.StateData.PositionState != EWuwaPositionState.Ground
            || movement.ReadPositionState() != EWuwaPositionState.Ground
            || movement.IsJumpRootMotionActive())
        {
            return false;
        }

        // 原作 BP_Input_<角色> 跳跃按下：角色可以覆盖通用跳跃（长离有心眼 → 1205106 心眼·跃）。
        // 角色输入规则里跳跃输入的行命中就放那个技能（或吞掉输入），不再起跳；没有行命中才走通用跳跃。
        switch (TryRoleJumpCommand(character, context.InputEvent.InputTag))
        {
            case EWuwaCombatInputResult.ActivationRequested:
            case EWuwaCombatInputResult.AlreadyActive:
            case EWuwaCombatInputResult.Rejected:
            case EWuwaCombatInputResult.Ignored:
            case EWuwaCombatInputResult.InvalidRule:
                return true;
        }

        // 原作 JumpPress：受击中不能跳（CheckInHit）；有当前技能时按跳跃等级判断能否打断，能打断就结束它（StopGroup1Skill），再限制速度（LimitMaxSpeed）。
        // 本项目的受击、技能都占用 FightState：打断技能后 FightState 仍不是 None 的（受击等）不能跳。
        var fight = character.FightStateComponent;
        if (!fight.IsValid())
        {
            return false;
        }

        var stoppedSkill = false;
        if (fight.StateData.State != EWuwaFightState.None)
        {
            // 没有主技能时 TryInterruptForAction 直接返回 true，FightState 仍然不是 None（受击等），照样不能跳。
            var skills = character.SkillComponent;
            if (skills is null || !skills.IsValid() || !skills.TryInterruptForAction(JumpInterruptLevel)
                || fight.StateData.State != EWuwaFightState.None)
            {
                return false;
            }

            stoppedSkill = true;
        }

        // true 表示已经起跳（切到下落、开始用起跳根运动）。
        return movement.BeginRootMotionJump(stoppedSkill);
    }

    // 按角色输入规则解释这次跳跃：命中技能行就请求激活（能否打断当前技能由技能系统按打断等级判定，不走跳跃的等级 8）
    private static EWuwaCombatInputResult TryRoleJumpCommand(AWuwaCharacter character, FGameplayTag jumpTag)
    {
        var asc = character.AbilitySystemComponent as UWuwaAbilitySystemComponent;
        var skills = character.SkillComponent;
        if (asc is null || !asc.IsValid() || skills is null || !skills.IsValid() || !jumpTag.IsValid) return EWuwaCombatInputResult.NoCandidate;
        var input = new FWuwaInputCommandContext { ASC = asc, Avatar = character, CurrentSkill = skills.GetCurrentSkillData(), InputTag = jumpTag };
        var resolution = WuwaInputCommandResolver.SelectSkillId(input, out var skillId);
        if (resolution != EWuwaCombatInputResult.ActivationRequested) return resolution;
        var handles = asc.FindSkillHandles(skillId);
        if (handles.Count == 0) return EWuwaCombatInputResult.InvalidRule;
        return asc.RequestAbilityActivation(handles[0]) switch
        {
            EWuwaAbilityRequestResult.ActivationRequested => EWuwaCombatInputResult.ActivationRequested,
            EWuwaAbilityRequestResult.AlreadyActive => EWuwaCombatInputResult.AlreadyActive,
            _ => EWuwaCombatInputResult.Rejected
        };
    }
}
