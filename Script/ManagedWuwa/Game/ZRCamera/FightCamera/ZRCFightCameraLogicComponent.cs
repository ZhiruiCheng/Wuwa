using ManagedWuwa.Game.Camera;
using ManagedWuwa.Game.Camera.FightCameraController;
using ManagedWuwa.Game.ZRCamera.FightCamera;
using UnrealSharp;
using UnrealSharp.CoreUObject;
using UnrealSharp.Engine;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.ZRCamera;

public class ZRCFightCameraLogicComponent(ZRCameraModelInstance cameraModel, ZRCFightCameraDisplayComponent display)
{
    // 相机模式栈：谁压入的（来源对象）→ 一整套设置；来源对象失效后自动作废
    private readonly ZrOwnedRequests<TWeakObjectPtr<UObject>, FWuwaCameraSettings> _modes = new(static owner => owner.IsValid);
    // 玩家滚轮缩放（记住玩家的缩放偏好）
    private readonly ZrCameraInputController _input = new();
    // 模式切换时的平滑过渡 + 由参数算出镜头位置和朝向
    private readonly ZrCameraPoseSolver _poseSolver = new();
    private readonly ZRCCameraCollision _collision = new();
    // 正在生效的模式被移除时，混回下一个模式用它自己的 BlendOutTime；< 0 = 用下一个模式的 BlendTime
    private float _pendingBlendOut = -1f;
    // 镜头跟随的角色
    private APawn? _pawn;
    
    /// <summary>换了跟随的角色（≈ 原作 SetPawn）。</summary>
    public void SetPawn(APawn? pawn)
    {
        // 清掉模式前，按当前生效的设置决定要不要重置玩家缩放（原 C++ NotifyPawnChanged 的逻辑）
        bool resetUserZoom = ResolveSettings().Zoom.ResetZoomOnPawnChange;
        _modes.Clear();
        if (resetUserZoom)
            _input.Reset();
        _poseSolver.Reset();
        _pawn = pawn;
    }
    
    public void Clear()
    {
        _modes.Clear();
        _pawn = null;
    }
    
    /// <summary>每帧计算并摆放相机（≈ 原作 OnAfterTick）。zoomDelta = 本帧滚轮量（由 C++ 累积后传入）。</summary>
    public void Tick(float deltaSeconds, float zoomDelta)
    {
        //如果当前的相机管理实例的模式是这个Fight模式就执行这个的Tick
        if (cameraModel.CameraMode != EZrCameraMode.Fight)
            return;

        //获取当前的相机Actor和对应的PlayerController
        ACameraActor? cameraActor = display.CameraActor;
        APlayerController? PC = cameraModel.PlayerCameraManager.OwningPlayerController;
        if (_pawn is null || !_pawn.IsValid() || cameraActor is null || PC is null) return;

        // 1. 组装本帧输入：角色位置、玩家视角、帧间隔、滚轮量
        var frame = new FWuwaCameraFrame
        {
            TargetLocation = _pawn.ActorLocation,
            ControlRotation = PC.ControlRotation,
            DeltaSeconds = deltaSeconds,
            ZoomDelta = zoomDelta,
        };

        // 2. 选出当前生效的设置 → 算出臂长（含玩家缩放） → 过渡与计算
        var settings = SelectSettings(out int modeHandle);
        float desiredArmLength = _input.ResolveArmLength(settings, frame.ZoomDelta);
        FWuwaCameraView view = _poseSolver.Evaluate(frame, settings, desiredArmLength, modeHandle, _pendingBlendOut);
        _pendingBlendOut = -1f;
        if (!view.Valid)
            return;

        // 3. 碰撞修正：从焦点推球到镜头位置
        FVector location = _collision.CheckCollision(_pawn, view.Pivot, view.Location, view.ProbeRadius, view.CollisionTest);

        // 4. 摆相机（原作：CameraActor.SetActorLocationAndRotation）
        cameraActor.SetActorLocationAndRotation(location, view.Rotation, false, out _, false);
        cameraActor.CameraComponent.FieldOfView = view.FieldOfView;
    }
    
    public int PushCameraMode(UObject source, FWuwaCameraSettings settings, int priority)
    {
        if (source is null || !source.IsValid())
            return 0;
        return _modes.Add(new TWeakObjectPtr<UObject>(source), ZrCameraSettingsUtility.Normalize(settings), priority);
    }

    public bool PopCameraMode(int handle)
    {
        // 弹出的正是当前生效的模式，就记下它的淡出时间，下一帧过渡时用
        if (_modes.TryGetHighest(out int active, out var activeSettings) && active == handle)
            _pendingBlendOut = activeSettings.Blend.BlendOutTime;
        return _modes.Remove(handle);
    }
    
    public FWuwaCameraSettings ResolveSettings() => SelectSettings(out _);
    
    // 模式栈里有就取优先级最高的，没有就用基础设置；最后统一清洗非法数值
    private FWuwaCameraSettings SelectSettings(out int modeHandle) =>
        ZrCameraSettingsUtility.Normalize(_modes.TryGetHighest(out modeHandle, out var settings) ? settings : GetBaseSettings());
    
    // 基础设置：PCM 上配置的默认相机模式资产，没有就用它的默认设置（原 C++ GetBaseSettings）
    private FWuwaCameraSettings GetBaseSettings()
    {
        if (cameraModel.PlayerCameraManager is not AWuwaPlayerCameraManager pcm)
            return default;
        return pcm.DefaultMode is { } mode && mode.IsValid() ? mode.Settings : pcm.DefaultSettings;
    }
}