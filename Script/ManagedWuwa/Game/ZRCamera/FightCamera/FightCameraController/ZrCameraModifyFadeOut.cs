using UnrealSharp.CoreUObject;

namespace ManagedWuwa.Game.ZRCamera;

/// <summary>
/// 一段镜头修改结束后的"回去"。结束那一刻记下每一项从哪回到哪，之后每帧往回走一步，写进 Desired。
/// 两种走法：
/// - 按时间：在打断淡出时间内，按进度从起点插到终点，时间到了就结束（被打断、切人、被叫停时用）；
/// - 按速度：每帧把还差的部分缩掉一截，差得很小就算到了（正常播完时用，补齐淡出后剩下的一点点差距）。
/// 某一项这一帧已经被别的控制器改过，或者玩家转了镜头（只针对角度），这一项就不再往回走，让给它。
/// </summary>
internal sealed class ZrCameraModifyFadeOut(ZRCFightCameraLogicComponent camera)
{
    // 差这么多以内就算回到了
    private const double ArrivedTolerance = 1.0;

    private double _elapsed;
    private double _duration;
    private bool _byTime;
    private ZrCameraCurve _curve = ZrCameraCurve.Linear;

    // 每一项：还要不要往回走、从哪回、回到哪
    private bool _armLength;
    private double _armLengthFrom;
    private double _armLengthTo;
    private bool _cameraOffset;
    private FVector _cameraOffsetFrom;
    private bool _pitch;
    private double _pitchFrom;
    private double _pitchTo;
    private bool _yaw;
    private double _yawFrom;
    private double _yawTo;
    private bool _fov;
    private double _fovFrom;
    private double _fovTo;

    /// <summary>还有没走完的项。</summary>
    public bool IsActive { get; private set; }

    /// <summary>距离还在往回走（这时战斗镜头不去夹距离）。</summary>
    public bool IsReturningArmLength => IsActive && _armLength;

    /// <summary>
    /// 记下回去的计划。不需要回去的项传空。
    /// 偏移只给起点：终点是当前配置的偏移，每帧现读（配置中途变了也跟着变）。
    /// </summary>
    public void Start(double duration, ZrCameraCurve curve, bool byTime,
        (double From, double To)? armLength, FVector? cameraOffsetFrom,
        (double From, double To)? pitch, (double From, double To)? yaw, (double From, double To)? fov)
    {
        _elapsed = 0;
        _duration = duration;
        _curve = curve;
        _byTime = byTime;
        _armLength = armLength.HasValue;
        if (armLength is { } armLengthRange)
            (_armLengthFrom, _armLengthTo) = armLengthRange;
        _cameraOffset = cameraOffsetFrom.HasValue;
        if (cameraOffsetFrom is { } offsetFrom)
            _cameraOffsetFrom = offsetFrom;
        _pitch = pitch.HasValue;
        if (pitch is { } pitchRange)
            (_pitchFrom, _pitchTo) = pitchRange;
        _yaw = yaw.HasValue;
        if (yaw is { } yawRange)
            (_yawFrom, _yawTo) = yawRange;
        _fov = fov.HasValue;
        if (fov is { } fovRange)
            (_fovFrom, _fovTo) = fovRange;
        IsActive = _armLength || _cameraOffset || _pitch || _yaw || _fov;
    }

    /// <summary>停止回去，各项停在当前的样子。</summary>
    public void Stop()
    {
        IsActive = false;
        _armLength = false;
        _cameraOffset = false;
        _pitch = false;
        _yaw = false;
        _fov = false;
    }

    /// <summary>
    /// 往回走一步。playerTurned：这期间玩家转过镜头，角度就不再往回走。
    /// 后面四个是按速度走时，距离、偏移、角度、视野各自的速度。
    /// </summary>
    public void Update(double deltaSeconds, bool playerTurned,
        double armLengthSpeed, double cameraOffsetSpeed, double rotationSpeed, double fovSpeed)
    {
        if (!IsActive)
            return;
        _elapsed += deltaSeconds;
        double alpha = _duration <= 0 ? 1 : _curve.GetCurrentValue(_elapsed / _duration);
        var current = camera.CurrentCamera;
        var desired = camera.DesiredCamera;
        var modified = camera.Modified;

        if (_armLength)
        {
            if (modified.ZoomModifier)
            {
                _armLength = false;
            }
            else
            {
                _armLengthTo = ZrCameraMath.Clamp(_armLengthTo, current.MinArmLength, current.MaxArmLength);
                (_armLength, double length) = _byTime
                    ? FloatLerp(_armLengthFrom, _armLengthTo, alpha, ZrCameraMath.KindaSmallNumber)
                    : FloatInterpTo(camera.GetArmLengthWithSettingAndZoom(current, clamp: false), _armLengthTo, deltaSeconds, armLengthSpeed);
                double baseLength = camera.GetArmLengthWithSetting(current);
                if (baseLength > ZrCameraMath.SmallNumber)
                    desired.ZoomModifier = length / baseLength;
                modified.ZoomModifier = true;
                modified.ArmLength = true;
            }
        }
        if (_cameraOffset)
        {
            if (modified.CameraOffset)
            {
                _cameraOffset = false;
            }
            else
            {
                var target = camera.Settings.CameraOffset;
                (_cameraOffset, desired.CameraOffset) = _byTime
                    ? VectorLerp(_cameraOffsetFrom, target, alpha)
                    : VectorInterpTo(current.CameraOffset, target, deltaSeconds, cameraOffsetSpeed);
                modified.CameraOffset = true;
            }
        }
        if (_pitch)
        {
            if (modified.Pitch || playerTurned)
                _pitch = false;
            else
                (_pitch, desired.ArmRotation.Pitch) = _byTime
                    ? AxisLerp(_pitchFrom, _pitchTo, alpha)
                    : AxisInterpTo(current.ArmRotation.Pitch, _pitchTo, deltaSeconds, rotationSpeed);
        }
        if (_yaw)
        {
            if (modified.Yaw || playerTurned)
                _yaw = false;
            else
                (_yaw, desired.ArmRotation.Yaw) = _byTime
                    ? AxisLerp(_yawFrom, _yawTo, alpha)
                    : AxisInterpTo(current.ArmRotation.Yaw, _yawTo, deltaSeconds, rotationSpeed);
        }
        if (_fov)
        {
            if (modified.Fov)
            {
                _fov = false;
            }
            else
            {
                (_fov, desired.Fov) = _byTime
                    ? FloatLerp(_fovFrom, _fovTo, alpha, ArrivedTolerance)
                    : FloatInterpTo(current.Fov, _fovTo, deltaSeconds, fovSpeed);
                modified.Fov = true;
            }
        }
        if (_byTime && _elapsed > _duration)
            Stop();
        else
            IsActive = _armLength || _cameraOffset || _pitch || _yaw || _fov;
    }

    // 下面几个都返回（还没到吗，这一帧的值）。按时间走的看起点和终点差多少，按速度走的看当前和终点差多少，差得够小就算到了

    private static (bool, double) FloatLerp(double from, double to, double alpha, double tolerance) =>
        Math.Abs(from - to) < tolerance ? (false, to) : (true, ZrCameraMath.Lerp(from, to, alpha));

    private static (bool, FVector) VectorLerp(FVector from, FVector to, double alpha) =>
        ZrCameraMath.AbsMax(ZrCameraMath.Subtract(from, to)) <= ArrivedTolerance ? (false, to) : (true, ZrCameraMath.Lerp(from, to, alpha));

    private static (bool, double) AxisLerp(double from, double to, double alpha) =>
        (!ZrCameraMath.IsAngleNearEqual(from, to, ArrivedTolerance), ZrCameraMath.AxisLerp(from, to, alpha));

    private static (bool, double) FloatInterpTo(double current, double target, double deltaTime, double speed) =>
        Math.Abs(current - target) < ArrivedTolerance ? (false, target) : (true, ZrCameraMath.InterpTo(current, target, deltaTime, speed));

    private static (bool, FVector) VectorInterpTo(FVector current, FVector target, double deltaTime, double speed) =>
        ZrCameraMath.AbsMax(ZrCameraMath.Subtract(current, target)) < ArrivedTolerance ? (false, target) : (true, ZrCameraMath.InterpTo(current, target, deltaTime, speed));

    private static (bool, double) AxisInterpTo(double current, double target, double deltaTime, double speed) =>
        (!ZrCameraMath.IsAngleNearEqual(current, target, ArrivedTolerance), ZrCameraMath.AxisInterpTo(current, target, deltaTime, speed));
}
