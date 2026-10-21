using UnrealSharp.Attributes;
using UnrealSharp.CoreUObject;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.AnimNotifyState;

/// <summary>开放同级技能接招；窗口身份来自原生蒙太奇播放实例。</summary>
[UClass]
public partial class UAnimNotifyState_SkillAcceptInput : UWuwaAnimNotifyState_SkillAcceptInputBridge
{
    public override string GetNotifyName() => "技能同级接招窗口";

    protected override void BeginSkillWindow_Implementation(UWuwaSkillBridgeComponent skills, int skillHandle, int montageInstanceId, int notifyEventId)
    {
        if (skills.IsValid() && skills.BeginSkillAcceptInputWindow(skillHandle, montageInstanceId, notifyEventId))
            skills.CallAnimBreakPoint(skillHandle);
    }

    protected override void EndSkillWindow_Implementation(UWuwaSkillBridgeComponent skills, int montageInstanceId, int notifyEventId)
    {
        if (skills.IsValid()) skills.EndSkillAcceptInputWindow(montageInstanceId, notifyEventId);
    }
}
