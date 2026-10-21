using UnrealSharp.Attributes;
using UnrealSharp.CoreUObject;
using UnrealSharp.Engine;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.AnimNotifyState;

[UClass]
public partial class UAnimNotifyState_DesireToKeepSprint : UAnimNotifyState
{
    public UAnimNotifyState_DesireToKeepSprint()
    {
        HoldThresholdSeconds = 0.2f;
    }

    /// <summary>冲刺输入在窗口内连续保持超过这个时长才请求长期冲刺，单位为游戏秒。</summary>
    [UProperty(PropertyFlags.EditAnywhere)]
    public partial float HoldThresholdSeconds { get; set; }

    public override string GetNotifyName() => "冲刺需求窗口";

    public override bool Received_NotifyBegin(USkeletalMeshComponent meshComp, UAnimSequenceBase animation,
        float totalDuration, FAnimNotifyEventReference eventReference)
    {
        var character = GetCharacter(meshComp);
        if (character is null || !character.IsLocallyControlled())
        {
            return true;
        }

        character.RoleGaitComponent.OpenSprintWindow(this);
        UpdateDesire(character);
        return true;
    }

    public override bool Received_NotifyTick(USkeletalMeshComponent meshComp, UAnimSequenceBase animation,
        float frameDeltaTime, FAnimNotifyEventReference eventReference)
    {
        var character = GetCharacter(meshComp);
        if (character is null)
        {
            return true;
        }

        if (!character.IsLocallyControlled())
        {
            character.RoleGaitComponent.ResetSprintRequest();
            return true;
        }

        UpdateDesire(character);
        return true;
    }

    public override bool Received_NotifyEnd(USkeletalMeshComponent meshComp, UAnimSequenceBase animation,
        FAnimNotifyEventReference eventReference)
    {
        var character = GetCharacter(meshComp);
        if (character is null)
        {
            return true;
        }

        if (!character.IsLocallyControlled())
        {
            character.RoleGaitComponent.ResetSprintRequest();
            return true;
        }

        // 补采样最后一帧，交给 RoleGait 保留；迟到回调不会重新开窗。
        UpdateDesire(character);
        character.RoleGaitComponent.CloseSprintWindow(this);
        return true;
    }

    private void UpdateDesire(AWuwaCharacter character)
    {
        // 只消费项目的语义输入快照；键盘、手柄、触屏与改键由 IA/路由层处理。
        var input = character.PlayerInputState;
        character.RoleGaitComponent.SampleSprintWindow(
            this, input.SprintHeld ? input.SprintHeldSeconds : 0f, HoldThresholdSeconds);
    }

    private static AWuwaCharacter? GetCharacter(USkeletalMeshComponent meshComp)
    {
        if (!meshComp.IsValid() || meshComp.Owner is not AWuwaCharacter character || !character.IsValid()
            || character.Mesh != meshComp || character.RoleGaitComponent is null || !character.RoleGaitComponent.IsValid())
        {
            return null;
        }

        return character;
    }
}
