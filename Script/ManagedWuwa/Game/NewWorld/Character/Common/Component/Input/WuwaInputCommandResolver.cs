using UnrealSharp;
using UnrealSharp.CoreUObject;
using UnrealSharp.GameplayTags;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.NewWorld.Character.Common.Component.Input;

// 对应鸣潮 InputCommandTransform：先按行顺序选招，再由 Skill/GAS 判断能否执行。
// 不持有角色状态，也不缓存目标 GA；每次输入和动画断点都重新解释原始输入。
internal static class WuwaInputCommandResolver
{
    public static EWuwaCombatInputResult SelectSkillId(FWuwaInputCommandContext context, out int skillId)
    {
        skillId = 0;
        var config = context.Avatar.InputCommandConfig;
        if (!config.IsValid()) return EWuwaCombatInputResult.NoCandidate;

        // 当前主技能的 ID：GA 已结束（记录过期）时按"没有当前技能"处理
        var current = context.CurrentSkill.ActiveAbility is { } active && active.IsValid() ? context.CurrentSkill.SkillId : 0;
        var position = ReadPosition(context.Avatar);
        bool recognized = false;
        foreach (var rule in config.Rules)
        {
            // 输入 Tag 和阶段（按下 / 长按 / 松开）都对上才算这一行管的输入
            if (!rule.InputTag.Equals(context.InputTag) || rule.Phase != context.Phase) continue;
            recognized = true;
            if (rule.Position != EWuwaInputPosition.Any && rule.Position != position) continue;
            if (rule.RequiredCurrentSkillId != 0 && current != rule.RequiredCurrentSkillId) continue;
            // 分类条件：空查询不限制；非空要求当前有主技能且它的 SkillTags 满足查询
            if (!context.ASC.SkillMatchesTagQuery(current, rule.CurrentSkillTagQuery)) continue;
            if (!context.ASC.MatchesOwnedTagQuery(rule.OwnerTagQuery)) continue;

            bool matched = true;
            foreach (var condition in rule.Conditions)
            {
                // 空对象不是“无条件”；不能因为漏配而意外落入后面的兜底技能。
                if (!condition.IsValid()) return EWuwaCombatInputResult.InvalidRule;
                if (!condition.Evaluate(context)) { matched = false; break; }
            }
            if (!matched) continue;

            // 吞掉输入的行：命中即结束，什么都不放（Ignored：不预输入；跳跃等通用动作也不再执行）
            if (rule.BlockInput) return EWuwaCombatInputResult.Ignored;
            // 首命中即结束。目标缺失/冷却/打断失败都不能继续选下一行。
            skillId = rule.TargetSkillId;
            return skillId > 0 ? EWuwaCombatInputResult.ActivationRequested : EWuwaCombatInputResult.InvalidRule;
        }

        // 有这个输入的行但条件都没命中：NoMatchingRule（可预输入，等条件满足）。
        // 规则表里根本没有这个输入：NoCandidate（不预输入；也不再拿输入 Tag 当技能身份兜底）。
        return recognized ? EWuwaCombatInputResult.NoMatchingRule : EWuwaCombatInputResult.NoCandidate;
    }

    // 原作攻击按下：CMC IsMovingOnGround 走地面链，否则（下落 / 飞行）走空中链；其他位置（攀爬、水中）两边都不算。
    private static EWuwaInputPosition ReadPosition(AWuwaCharacter avatar)
    {
        var movement = avatar.CharacterMovement as UWuwaMovementComponent;
        if (movement is null || !movement.IsValid()) return EWuwaInputPosition.Any;
        if (movement.IsMovingOnGround()) return EWuwaInputPosition.Ground;
        return movement.ReadPositionState() == EWuwaPositionState.Air ? EWuwaInputPosition.Air : EWuwaInputPosition.Any;
    }
}
