using UnrealSharp.Attributes;
using UnrealSharp.CoreUObject;
using UnrealSharp.Engine;
using UnrealSharp.GameplayAbilities;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.AnimNotifyState;

/// <summary>
/// 放在 GA 播放的 Montage 通知轨道上，窗口内有移动输入时尝试结束该 GA。
/// 仅保存可编辑配置，不在共享的通知对象中缓存角色、GA 或窗口运行状态。
/// </summary>
[UClass]
public partial class UAnimNotifyState_MovementCancelWindow : UAnimNotifyState
{
    public UAnimNotifyState_MovementCancelWindow()
    {
        BlendOutTime = 0.1f;
    }

    /// <summary>停止蒙太奇时的姿势混出时间，单位为秒；根运动停止由 C++ 处理。</summary>
    [UProperty(PropertyFlags.EditAnywhere)]
    public partial float BlendOutTime { get; set; }

    /// <summary>
    /// 技能攻击勾选：窗口内仍须等到 ReadyEnd，避免移动先结束技能而丢失连段条件。
    /// 默认关闭，兼容没有 ReadyEnd 通知、单独用此窗口退出的 Dash。
    /// </summary>
    [UProperty(PropertyFlags.EditAnywhere)]
    public partial bool RequireSkillReadyEnd { get; set; }

    public override string GetNotifyName() => "移动取消窗口";

    public override bool Received_NotifyBegin(USkeletalMeshComponent meshComp, UAnimSequenceBase animation, float totalDuration, FAnimNotifyEventReference eventReference)
    {
        // 玩家在窗口打开前就按住方向时，立即尝试交接。
        TryCancelForMovement(meshComp, animation);
        return true;
    }

    public override bool Received_NotifyTick(USkeletalMeshComponent meshComp, UAnimSequenceBase animation, float frameDeltaTime, FAnimNotifyEventReference eventReference)
    {
        // 玩家在窗口打开后才按方向时，也能在窗口内响应。
        TryCancelForMovement(meshComp, animation);
        return true;
    }

    public override bool Received_NotifyEnd(USkeletalMeshComponent meshComp, UAnimSequenceBase animation, FAnimNotifyEventReference eventReference)
    {
        // 没有输入时，离开窗口不会主动结束 GA；本类也没有需要解除的持久状态。
        return true;
    }

    private void TryCancelForMovement(USkeletalMeshComponent meshComp, UAnimSequenceBase animation)
    {
        // 只处理主 Mesh 上由 GA 播放的 Montage，不处理预览角色或状态机里的 Sequence。
        if (!meshComp.IsValid() || animation is not UAnimMontage montage || !montage.IsValid()
            || meshComp.Owner is not AWuwaCharacter character || !character.IsValid()
            || character.Mesh != meshComp || !character.IsLocallyControlled()
            || character.AbilitySystemComponent is not UWuwaAbilitySystemComponent asc || !asc.IsValid())
        {
            return;
        }

        var ability = asc.AnimatingWuwaAbility;
        if (!ability.IsValid() || !AbilitySystemLibrary.IsActive(ability)
            || ability.CurrentMontage != montage || !ability.PlayerInputState.HasMoveInput)
        {
            return;
        }

        if (RequireSkillReadyEnd)
        {
            var skills = character.SkillComponent;
            if (!skills.IsValid()) return;
            var current = skills.GetCurrentSkillData();
            // ReadyEnd 先开放技能让位并检查缓存；没有接招时，才由移动结束同一技能。
            if (current.ActiveAbility != ability || current.FightStateHandle != ability.SkillHandle
                || !current.MainSkillReadyEnd) return;
        }

        // Stop 内还会检查播放归属和有效实例。失败时保留 GA，下一次窗口 Tick 可以重试。
        // Stop 可能同步触发播放任务回调并结束 GA，所以成功后再次确认它仍然有效且活跃。
        if (ability.StopMontageForMovement(BlendOutTime)
            && ability.IsValid() && AbilitySystemLibrary.IsActive(ability))
        {
            ability.EndAbility();
        }
    }
}
