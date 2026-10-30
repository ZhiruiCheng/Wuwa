using UnrealSharp.CoreUObject;
using UnrealSharp.Engine;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.ZRCamera;

/// <summary>
/// 一段正在进行的镜头修改（放在镜头修改控制器的槽位里）。记着这段修改的设置和时间线，每帧算出这一帧的样子写进 Desired。
/// - 分三段走：淡入（从开始时的快照插到目标）→ 保持 → 淡出（从目标插到"结束后的值"）；
/// - 只改它管着的项，改了哪项就给哪项打"这一帧改过"的标记；
/// - 角度的目标 = 第一次更新时角色的朝向 + 设定角度。玩家这期间转了镜头、而这段修改又没锁输入，就不再管角度。
/// </summary>
internal sealed class ZrCameraModifySession
{
    private enum Phase { BlendIn, Hold, BlendOut }

    private readonly ZRCFightCameraLogicComponent _camera;
    private readonly FWuwaCameraModify _settings;
    // 最早那段修改开始前的样子（控制器保管，连着几段修改共用）："回去"的终点
    private readonly ZrVirtualCamera _origin;
    // 这段修改开始时的样子：淡入的起点
    private readonly ZrVirtualCamera _start = new();

    // ===== 时间线 =====
    // 已过时间、淡入、保持、淡出
    private double _elapsed;
    private readonly double _blendIn;
    private readonly double _duration;
    private readonly double _blendOut;
    // 淡入的快慢曲线（通知里还没导这一项，都是匀速）
    private readonly ZrCameraCurve _blendInCurve = ZrCameraCurve.Linear;

    // ===== 除了勾选的项，还要管的项：设了加量的项，或者从被它顶掉的上一段接手的项 =====
    private readonly bool _extraArmLength;
    private readonly bool _extraCameraOffset;
    private readonly bool _extraPitch;
    private readonly bool _extraYaw;

    // ===== 角度、偏移的目标 =====
    private readonly AxisTarget _pitch = new();
    private readonly AxisTarget _yaw = new();
    // 进入淡出那一刻的偏移目标，淡出从它开始
    private bool _hasBlendOutOffset;
    private FVector _blendOutOffset;

    // ===== 盯着的蒙太奇（勾了"蒙太奇停了就结束"、而且是蒙太奇触发的才有） =====
    private readonly UAnimMontage? _montage;
    private readonly UAnimInstance? _animInstance;

    /// <summary>
    /// 开始一段修改：拍下现在的样子当淡入起点。
    /// inheritArmLength / inheritCameraOffset / inheritFov：被它顶掉的上一段管着这几项，由这一段接手（结束时一起带回去）。
    /// </summary>
    public ZrCameraModifySession(ZRCFightCameraLogicComponent camera, ZrVirtualCamera origin, FWuwaCameraModify settings,
        float duration, float blendInTime, float blendOutTime, float breakBlendOutTime, UAnimSequenceBase? animation, AActor? owner,
        bool inheritArmLength, bool inheritCameraOffset, bool inheritFov)
    {
        _camera = camera;
        _origin = origin;
        _settings = settings;
        Animation = animation;
        _blendIn = blendInTime;
        _duration = duration;
        _blendOut = blendOutTime;
        BreakBlendOutTime = breakBlendOutTime;
        _start.CopyFrom(camera.CurrentCamera);
        _extraArmLength = inheritArmLength || Math.Abs(settings.ArmLengthAdditional) > ZrCameraMath.KindaSmallNumber;
        _extraCameraOffset = inheritCameraOffset || !IsNearlyZero(settings.CameraOffsetAdditional);
        _extraPitch = Math.Abs(settings.ArmRotationAdditional.Pitch) > ZrCameraMath.KindaSmallNumber;
        _extraYaw = Math.Abs(settings.ArmRotationAdditional.Yaw) > ZrCameraMath.KindaSmallNumber;
        ControlsFov = inheritFov || settings.ModifyFov;
        if (settings.StopOnMontageEnd && animation is UAnimMontage montage)
        {
            _montage = montage;
            var character = owner as ACharacter ?? camera.Character as ACharacter;
            _animInstance = character?.Mesh?.AnimInstance;
        }
    }

    /// <summary>这段修改的设置。</summary>
    public FWuwaCameraModify Settings => _settings;

    /// <summary>触发它的动画。</summary>
    public UAnimSequenceBase? Animation { get; }

    /// <summary>被提前结束时，回去用的时间。</summary>
    public double BreakBlendOutTime { get; }

    /// <summary>淡出的快慢曲线（通知里还没导这一项，都是匀速）。被提前结束时回去也用它。</summary>
    public ZrCameraCurve BlendOutCurve { get; } = ZrCameraCurve.Linear;

    // ===== 管哪些项 =====

    public bool ControlsArmLength => _settings.ModifyArmLength || _extraArmLength;

    public bool ControlsCameraOffset => ModifiesCameraOffsetAxes || _extraCameraOffset;

    public bool ControlsPitch => _settings.ModifyPitch || _extraPitch;

    public bool ControlsYaw => _settings.ModifyYaw || _extraYaw;

    public bool ControlsFov { get; }

    private bool ModifiesCameraOffsetAxes => _settings.ModifyOffsetX || _settings.ModifyOffsetY || _settings.ModifyOffsetZ;

    // ===== 进度 =====

    /// <summary>已经进入淡出。</summary>
    public bool IsBlendingOut => GetPhase(out _) == Phase.BlendOut;

    /// <summary>淡入、保持、淡出都走完了。</summary>
    public bool IsFinished => _elapsed > _blendIn + _duration + _blendOut;

    /// <summary>盯着的蒙太奇已经不在播了（播完，或者被别的动画顶掉）。</summary>
    public bool IsMontageStopped =>
        _montage is not null && !(_animInstance is not null && _animInstance.IsValid() && _animInstance.CurrentActiveMontage == _montage);

    // ===== 每帧 =====

    /// <summary>往前走一帧，算出这一帧该有的样子写进 Desired。playerTurned：这期间玩家转过镜头。</summary>
    public void Update(double deltaSeconds, bool playerTurned)
    {
        _elapsed += deltaSeconds;
        var phase = GetPhase(out double alpha);
        UpdateArmLength(phase, alpha);
        UpdateCameraOffset(phase, alpha);
        UpdateRotation(phase, alpha, playerTurned);
        UpdateFov(phase, alpha);
    }

    // 现在走到哪一段，以及这一段走了多少（0 ~ 1）
    private Phase GetPhase(out double alpha)
    {
        if (_elapsed < _blendIn)
        {
            alpha = _blendIn > 0 ? _blendInCurve.GetCurrentValue(_elapsed / _blendIn) : 1;
            return Phase.BlendIn;
        }
        if (_elapsed < _blendIn + _duration)
        {
            alpha = 1;
            return Phase.Hold;
        }
        double blendOutElapsed = _elapsed - _blendIn - _duration;
        alpha = _blendOut > 0 ? BlendOutCurve.GetCurrentValue(blendOutElapsed / _blendOut) : 1;
        return Phase.BlendOut;
    }

    // 距离：写的是缩放倍率（目标距离 ÷ 当前基础距离）
    private void UpdateArmLength(Phase phase, double alpha)
    {
        if (!ControlsArmLength)
            return;
        var current = _camera.CurrentCamera;
        double baseLength = _camera.GetArmLengthWithSetting(current);
        if (baseLength <= ZrCameraMath.SmallNumber)
            return;
        double target = _settings.ModifyArmLength
            ? _settings.ArmLength / baseLength
            : _camera.GetArmLengthWithSettingAndZoom(_origin) / baseLength;
        if (_extraArmLength)
        {
            target += _settings.ArmLengthAdditional / baseLength;
            target = Math.Max(target, current.MinArmLength / baseLength);
        }
        double final = ZrCameraMath.Clamp(FinalArmLength(), current.MinArmLength, current.MaxArmLength) / baseLength;
        _camera.DesiredCamera.ZoomModifier = phase switch
        {
            Phase.BlendIn => ZrCameraMath.Lerp(_start.ZoomModifier, target, alpha),
            Phase.Hold => target,
            _ => ZrCameraMath.Lerp(target, final, alpha),
        };
        _camera.Modified.ArmLength = true;
        _camera.Modified.ZoomModifier = true;
    }

    /// <summary>结束后的距离："留下"就用修改后的距离（或指定距离），否则回到最早那段修改开始前的距离；都夹在上下限内。</summary>
    public double FinalArmLength()
    {
        var current = _camera.CurrentCamera;
        double length;
        if (_settings.ResetFinalArmLength && _settings.ModifyArmLength)
            length = _settings.ResetFinalArmLengthToSpecificValue
                ? _settings.ResetFinalSpecificArmLength
                : _settings.ArmLength + (_extraArmLength ? _settings.ArmLengthAdditional : 0);
        else
            length = _origin.ZoomModifier * _camera.GetArmLengthWithSetting(_origin);
        return ZrCameraMath.Clamp(length, current.MinArmLength, current.MaxArmLength);
    }

    // 镜头偏移：勾了的轴用设定值，没勾的轴用配置值，再加上加量；淡出时回到配置值
    private void UpdateCameraOffset(Phase phase, double alpha)
    {
        if (!ControlsCameraOffset)
            return;
        var config = _camera.Settings.CameraOffset;
        var target = ModifiesCameraOffsetAxes
            ? new FVector(_settings.ModifyOffsetX ? _settings.CameraOffset.X : config.X,
                _settings.ModifyOffsetY ? _settings.CameraOffset.Y : config.Y,
                _settings.ModifyOffsetZ ? _settings.CameraOffset.Z : config.Z)
            : config;
        if (_extraCameraOffset)
            target = ZrCameraMath.Add(target, _settings.CameraOffsetAdditional);
        var desired = _camera.DesiredCamera;
        switch (phase)
        {
            case Phase.BlendIn:
                desired.CameraOffset = ZrCameraMath.Lerp(_start.CameraOffset, target, alpha);
                break;
            case Phase.Hold:
                desired.CameraOffset = target;
                break;
            default:
                if (!_hasBlendOutOffset)
                {
                    _blendOutOffset = target;
                    _hasBlendOutOffset = true;
                }
                desired.CameraOffset = ZrCameraMath.Lerp(_blendOutOffset, config, alpha);
                break;
        }
        _camera.Modified.CameraOffset = true;
    }

    // 俯仰和偏航。这一帧已经被别的控制器改过的轴、或者要让给玩家的轴，不动
    private void UpdateRotation(Phase phase, double alpha, bool playerTurned)
    {
        var characterRotation = _camera.PlayerRotator;
        var final = FinalRotation();
        bool yieldToPlayer = !_settings.LockInput && playerTurned;
        var desired = _camera.DesiredCamera;
        var modified = _camera.Modified;
        if (!modified.Pitch && ControlsPitch && !yieldToPlayer)
        {
            modified.Pitch = true;
            desired.ArmRotation.Pitch = _pitch.Evaluate(phase, alpha, _start.ArmRotation.Pitch,
                _settings.ModifyPitch, _settings.ArmRotation.Pitch, _extraPitch, _settings.ArmRotationAdditional.Pitch,
                characterRotation.Pitch, final.Pitch);
        }
        if (!modified.Yaw && ControlsYaw && !yieldToPlayer)
        {
            modified.Yaw = true;
            desired.ArmRotation.Yaw = _yaw.Evaluate(phase, alpha, _start.ArmRotation.Yaw,
                _settings.ModifyYaw, _settings.ArmRotation.Yaw, _extraYaw, _settings.ArmRotationAdditional.Yaw,
                characterRotation.Yaw, final.Yaw);
        }
    }

    /// <summary>
    /// 结束后的角度："留下"就停在这一帧的角度（俯仰可以指定，偏航可以设成角色朝向 + 指定值），
    /// 否则回到最早那段修改开始前的角度。
    /// </summary>
    public FRotator FinalRotation()
    {
        if (!_settings.ResetFinalArmRotation)
            return _origin.ArmRotation;
        var desired = _camera.DesiredCamera.ArmRotation;
        double pitch = _settings.ResetFinalArmRotationToSpecificPitch ? _settings.ResetFinalSpecificPitch : desired.Pitch;
        double yaw = _settings.ResetFinalArmRotationToSpecificYaw ? _camera.PlayerRotator.Yaw + _settings.ResetFinalSpecificYaw : desired.Yaw;
        return new FRotator(pitch, yaw, 0);
    }

    // 视野：淡出时回到配置值
    private void UpdateFov(Phase phase, double alpha)
    {
        if (!ControlsFov)
            return;
        double target = _settings.ModifyFov ? _settings.Fov : _origin.Fov;
        _camera.DesiredCamera.Fov = phase switch
        {
            Phase.BlendIn => ZrCameraMath.Lerp(_start.Fov, target, alpha),
            Phase.Hold => target,
            _ => ZrCameraMath.Lerp(target, _camera.Settings.Fov, alpha),
        };
        _camera.Modified.Fov = true;
    }

    private static bool IsNearlyZero(FVector v) =>
        Math.Abs(v.X) <= ZrCameraMath.KindaSmallNumber && Math.Abs(v.Y) <= ZrCameraMath.KindaSmallNumber && Math.Abs(v.Z) <= ZrCameraMath.KindaSmallNumber;

    /// <summary>
    /// 俯仰或偏航其中一个轴的目标。
    /// 目标 = 第一次更新时记下的角色朝向 + 设定角度，再加上加量（没勾这个轴时，从开始时的角度加加量）。
    /// 进入淡出的那一刻记下当时的目标，淡出从它插到"结束后的角度"。
    /// </summary>
    private sealed class AxisTarget
    {
        private bool _characterCaptured;
        private double _character;
        private bool _blendOutCaptured;
        private double _blendOutFrom;

        /// <summary>算这一帧这个轴该写的角度。</summary>
        public double Evaluate(Phase phase, double alpha, double start, bool modifyAxis, double settingAngle,
            bool addAdditional, double additionalAngle, double characterAngle, double finalAngle)
        {
            double target = start;
            if (modifyAxis)
            {
                if (!_characterCaptured)
                {
                    _character = characterAngle;
                    _characterCaptured = true;
                }
                target = _character + settingAngle;
            }
            if (addAdditional)
                target += additionalAngle;
            switch (phase)
            {
                case Phase.BlendIn:
                    return ZrCameraMath.AxisLerp(start, target, alpha);
                case Phase.Hold:
                    return target;
                default:
                    if (!_blendOutCaptured)
                    {
                        _blendOutFrom = target;
                        _blendOutCaptured = true;
                    }
                    return ZrCameraMath.AxisLerp(_blendOutFrom, finalAngle, alpha);
            }
        }
    }
}
