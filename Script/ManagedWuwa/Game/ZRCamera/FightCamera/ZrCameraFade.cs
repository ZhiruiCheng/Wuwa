namespace ManagedWuwa.Game.ZRCamera;

/// <summary>
/// 切换配置时的过渡。
/// 切换那一刻 Desired 已经一步换成新配置；这里让 Current 在一段时间内，从切换那一刻拍下的快照（Last）按曲线慢慢追到 Desired。
/// 参与过渡的是距离、上下限、偏移、视野、俯仰范围等。角度不参与，每帧直接跟 Desired，免得玩家转镜头跟不上手。
/// </summary>
internal sealed class ZrCameraFade
{
    private double _duration;
    private double _elapsed;
    private ZrCameraCurve _curve = ZrCameraCurve.Linear;
    // 这几项参不参与这次过渡；中途被镜头修改接管后就不再参与
    private bool _fadeArmLength;
    private bool _fadeArmOffset;
    private bool _fadeCameraOffset;
    private bool _fadeFov;

    /// <summary>正在过渡。</summary>
    public bool IsFading { get; private set; }

    /// <summary>
    /// 这次过渡用的是"独立过渡时间"。它被一次普通过渡打断时，普通过渡不用自己的时间，而是接着走完它剩下的时间。
    /// </summary>
    public bool IsUniqueFade { get; private set; }

    /// <summary>开始一次过渡。调用前先把快照拍好放进 Last。</summary>
    public void Start(double duration, ZrCameraCurve curve, bool fadeArmLength, bool fadeArmOffset, bool fadeCameraOffset, bool fadeFov, bool uniqueFade)
    {
        _fadeArmLength = fadeArmLength;
        _fadeArmOffset = fadeArmOffset;
        _fadeCameraOffset = fadeCameraOffset;
        _fadeFov = fadeFov;
        IsFading = true;
        _duration = !uniqueFade && IsUniqueFade ? Math.Max(_duration - _elapsed, 0) : duration;
        _elapsed = 0;
        _curve = curve;
        IsUniqueFade = uniqueFade;
    }

    /// <summary>
    /// 计时往前走，并先把距离和它的上下限插好。每帧在配置控制器之后、其余控制器之前做。
    /// 距离这一帧已经被改过，就不再参与过渡，直接用 Desired 的。
    /// </summary>
    public void UpdateArmLength(double deltaSeconds, ZrVirtualCamera current, ZrVirtualCamera last, ZrVirtualCamera desired, bool armLengthModified)
    {
        if (!IsFading)
            return;
        _elapsed += deltaSeconds;
        bool zeroDuration = Math.Abs(_duration) < ZrCameraMath.KindaSmallNumber;
        if (!zeroDuration && _elapsed >= _duration)
            return;
        double alpha = _curve.GetCurrentValue(zeroDuration ? 1 : _elapsed / _duration);
        if (_fadeArmLength && !armLengthModified)
        {
            current.ArmLength = ZrCameraMath.Lerp(last.ArmLength, desired.ArmLength, alpha);
        }
        else
        {
            _fadeArmLength = false;
            current.ArmLength = desired.ArmLength;
        }
        current.MinArmLength = ZrCameraMath.Lerp(last.MinArmLength, desired.MinArmLength, alpha);
        current.MaxArmLength = ZrCameraMath.Lerp(last.MaxArmLength, desired.MaxArmLength, alpha);
    }

    /// <summary>
    /// 让 Current 跟上 Desired，每帧在所有控制器之后做：
    /// 没在过渡就整份照抄；在过渡就按进度插值（角度直接抄）；时间到了就结束过渡、整份照抄。
    /// 视野这一帧已经被镜头修改改过，就不再参与过渡，直接用 Desired 的。
    /// </summary>
    public void UpdateCurrent(ZrVirtualCamera current, ZrVirtualCamera last, ZrVirtualCamera desired, bool fovModified)
    {
        if (!IsFading)
        {
            current.CopyFrom(desired);
            return;
        }
        if (_elapsed >= _duration)
        {
            IsFading = false;
            IsUniqueFade = false;
            _elapsed = 0;
            current.CopyFrom(desired);
            return;
        }
        double alpha = _curve.GetCurrentValue(_elapsed / _duration);
        current.ArmLocation = desired.ArmLocation;
        current.ArmRotation = desired.ArmRotation;
        current.ZoomModifier = desired.ZoomModifier;
        current.YawLimitMin = ZrCameraMath.Lerp(last.YawLimitMin, desired.YawLimitMin, alpha);
        current.YawLimitMax = ZrCameraMath.Lerp(last.YawLimitMax, desired.YawLimitMax, alpha);
        current.PitchLimitMin = ZrCameraMath.GetValidPitchAngle(ZrCameraMath.Lerp(last.PitchLimitMin, desired.PitchLimitMin, alpha), true);
        current.PitchLimitMax = ZrCameraMath.GetValidPitchAngle(ZrCameraMath.Lerp(last.PitchLimitMax, desired.PitchLimitMax, alpha), false);
        current.LookDownOffsetZ = ZrCameraMath.Lerp(last.LookDownOffsetZ, desired.LookDownOffsetZ, alpha);
        current.LookUpOffsetZ = ZrCameraMath.Lerp(last.LookUpOffsetZ, desired.LookUpOffsetZ, alpha);
        current.WorldYawMin = ZrCameraMath.Lerp(last.WorldYawMin, desired.WorldYawMin, alpha);
        current.WorldYawMax = ZrCameraMath.Lerp(last.WorldYawMax, desired.WorldYawMax, alpha);
        current.CameraOffsetFloatUpMin = ZrCameraMath.Lerp(last.CameraOffsetFloatUpMin, desired.CameraOffsetFloatUpMin, alpha);
        current.CameraOffsetFloatUpMax = ZrCameraMath.Lerp(last.CameraOffsetFloatUpMax, desired.CameraOffsetFloatUpMax, alpha);
        current.CharAddArmLength = ZrCameraMath.Lerp(last.CharAddArmLength, desired.CharAddArmLength, alpha);
        current.CharAddZ = ZrCameraMath.Lerp(last.CharAddZ, desired.CharAddZ, alpha);
        current.ArmOffset = _fadeArmOffset ? ZrCameraMath.Lerp(last.ArmOffset, desired.ArmOffset, alpha) : desired.ArmOffset;
        current.CameraOffset = _fadeCameraOffset ? ZrCameraMath.Lerp(last.CameraOffset, desired.CameraOffset, alpha) : desired.CameraOffset;
        if (_fadeFov && !fovModified)
        {
            current.Fov = ZrCameraMath.Lerp(last.EnableDynamicFov ? last.DynamicFov : last.Fov, desired.Fov, alpha);
        }
        else
        {
            _fadeFov = false;
            current.Fov = desired.Fov;
        }
    }
}
