using System.Globalization;
using UnrealSharp.CoreUObject;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.NewWorld.Character.Common.Component.Input;

// 屏幕调试：常驻两行，显示当前主技能状态和预输入缓存。同一个 key 覆盖上一条，不刷屏。
// 开关是控制台变量 wuwa.Debug.CombatInput（默认关），和蓝图的 Wuwa Debug Print 用同一套频道。
internal static class CombatInputDebug
{
    public static bool Enabled => UWuwaDebugLibrary.IsDebugChannelEnabled(EWuwaDebugChannel.CombatInput);

    // 每帧刷新；角色或输入失效后不再刷新，短时间内自动消失。
    private const float DisplaySeconds = 0.2f;

    public static void Show(FWuwaSkillData state, string inputCache)
    {
        if (!Enabled) return;
        UObject.PrintString($"[Skill] {Describe(state)}", DisplaySeconds, default, printToScreen: true, printToConsole: false, key: "Wuwa.CombatInput.Skill");
        UObject.PrintString($"[InputCache] {inputCache}", DisplaySeconds, default, printToScreen: true, printToConsole: false, key: "Wuwa.CombatInput.Cache");
    }

    private static string Name(UObject? value) => value is not null && value.IsValid() ? value.Name.ToString() : "None";

    private static string Describe(FWuwaSkillData state)
    {
        var ability = state.ActiveAbility;
        bool canEnd = ability is not null && ability.IsValid() && ability.CanEndSkillExecutionNow();
        return FormattableString.Invariant($"skill={Name(ability)} handle={state.FightStateHandle} ready={state.MainSkillReadyEnd} accept={state.SkillAcceptInput} priority={state.InterruptLevel} canEnd={canEnd}");
    }
}
