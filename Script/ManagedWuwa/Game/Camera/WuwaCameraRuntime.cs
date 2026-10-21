using ManagedWuwa.Game.Camera.FightCameraController;
using UnrealSharp;
using UnrealSharp.Attributes;
using UnrealSharp.CoreUObject;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.Camera;

/// <summary>
/// 每个本地玩家一份的脚本运行时；原生 PlayerCameraManager 拥有并驱动它。
/// 它负责组织模式选择、输入和镜头计算，不承载 UE 场景组件。
/// </summary>
[UClass]
public partial class UWuwaCameraRuntime : UWuwaCameraRuntimeBridge
{
    private readonly WuwaOwnedRequests<TWeakObjectPtr<UObject>, FWuwaCameraSettings> _modes = new(static owner => owner.IsValid);
    private readonly WuwaCameraInputController _input = new();
    private readonly WuwaFightCameraLogic _logic = new();
    // 正在生效的模式被移除时，混回下一个模式用它自己的 BlendOutTime（原作镜头修改的淡出时间）；< 0 = 用下一个模式的 BlendTime
    private float _pendingBlendOut = -1f;

    public override FWuwaCameraView EvaluateCamera(FWuwaCameraFrame frame, FWuwaCameraSettings baseSettings)
    {
        var settings = SelectSettings(baseSettings, out int modeHandle);
        float desiredArmLength = _input.ResolveArmLength(settings, frame.ZoomDelta);
        var view = _logic.Evaluate(frame, settings, desiredArmLength, modeHandle, _pendingBlendOut);
        _pendingBlendOut = -1f;
        return view;
    }

    public override FWuwaCameraSettings ResolveSettings(FWuwaCameraSettings baseSettings) =>
        SelectSettings(baseSettings, out _);

    public override int PushCameraMode(UObject source, FWuwaCameraSettings settings, int priority)
    {
        if (source is null || !source.IsValid())
            return 0;

        return _modes.Add(new TWeakObjectPtr<UObject>(source), WuwaCameraSettingsUtility.Normalize(settings), priority);
    }

    public override bool PopCameraMode(int handle)
    {
        if (_modes.TryGetHighest(out int active, out var activeSettings) && active == handle)
            _pendingBlendOut = activeSettings.BlendOutTime;
        return _modes.Remove(handle);
    }

    public override void ResetCamera(bool resetUserZoom)
    {
        _modes.Clear();
        if (resetUserZoom)
            _input.Reset();
        _logic.Reset();
    }

    private FWuwaCameraSettings SelectSettings(FWuwaCameraSettings baseSettings, out int modeHandle) =>
        WuwaCameraSettingsUtility.Normalize(
            _modes.TryGetHighest(out modeHandle, out var settings) ? settings : baseSettings);
}
