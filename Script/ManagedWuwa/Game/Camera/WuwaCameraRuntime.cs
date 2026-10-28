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
    //持有者和相机设置
    private readonly ZrOwnedRequests<TWeakObjectPtr<UObject>, FWuwaCameraSettings> _modes = new(static owner => owner.IsValid);
    private readonly ZrCameraInputController _input = new();
    private readonly ZrCameraPoseSolver _poseSolver = new();
    // 正在生效的模式被移除时，混回下一个模式用它自己的 BlendOutTime（原作镜头修改的淡出时间）；< 0 = 用下一个模式的 BlendTime
    private float _pendingBlendOut = -1f;

    //被C++底层每帧调用
    public override FWuwaCameraView EvaluateCamera(FWuwaCameraFrame frame, FWuwaCameraSettings baseSettings)
    {
        //挑选出优先级最高的那个作为当前的设置
        var settings = SelectSettings(baseSettings, out int modeHandle);
        //
        float desiredArmLength = _input.ResolveArmLength(settings, frame.ZoomDelta);
        var view = _poseSolver.Evaluate(frame, settings, desiredArmLength, modeHandle, _pendingBlendOut);
        _pendingBlendOut = -1f;
        return view;
    }

    public override FWuwaCameraSettings ResolveSettings(FWuwaCameraSettings baseSettings) =>
        SelectSettings(baseSettings, out _);

    public override int PushCameraMode(UObject source, FWuwaCameraSettings settings, int priority)
    {
        if (source is null || !source.IsValid())
            return 0;

        return _modes.Add(new TWeakObjectPtr<UObject>(source), ZrCameraSettingsUtility.Normalize(settings), priority);
    }

    public override bool PopCameraMode(int handle)
    {
        if (_modes.TryGetHighest(out int active, out var activeSettings) && active == handle)
            _pendingBlendOut = activeSettings.Blend.BlendOutTime;
        return _modes.Remove(handle);
    }

    public override void ResetCamera(bool resetUserZoom)
    {
        _modes.Clear();
        if (resetUserZoom)
            _input.Reset();
        _poseSolver.Reset();
    }

    private FWuwaCameraSettings SelectSettings(FWuwaCameraSettings baseSettings, out int modeHandle) =>
        ZrCameraSettingsUtility.Normalize(_modes.TryGetHighest(out modeHandle, out var settings) ? settings : baseSettings);
}
