using UnrealSharp.Attributes;
using UnrealSharp.CoreUObject;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.AnimNotify;

/// <summary>进入让位阶段；当前 GA 和蒙太奇继续播放，直到被替换或自行结束。</summary>
[UClass]
public partial class UAnimNotify_SkillReadyEnd : UWuwaAnimNotify_SkillReadyEndBridge
{
    public override string GetNotifyName() => "技能进入让位阶段";

    protected override void ReachSkillReadyEnd_Implementation(UWuwaSkillBridgeComponent skills, int skillHandle)
    {
        if (!skills.IsValid()) return;

        bool readyEndWritten = skills.SetMainSkillReadyEnd(skillHandle, true);
        // 保留原来的短路顺序：ReadyEnd 写入失败时不尝试改等级或发出断点。
        bool priorityWritten = readyEndWritten && skills.SetSkillInterruptLevel(skillHandle, 0);
        if (priorityWritten) skills.CallAnimBreakPoint(skillHandle);
    }
}
