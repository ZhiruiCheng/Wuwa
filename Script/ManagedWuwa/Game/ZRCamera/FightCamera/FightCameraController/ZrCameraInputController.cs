using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.ZRCamera;

/// <summary>
/// 输入控制器：把这一帧玩家转镜头、滚滚轮的输入加到 Desired 上。
/// - 转镜头：灵敏度按输入大小在最小值和最大值之间取；再做缓冲（保留上一帧的一部分），
///   以 Current 的角度为起点改 Desired 的角度，俯仰夹在 ±89.9 以内；
/// - 滚轮：改 Desired 的缩放倍率，换算成距离后夹在上下限内；
/// - 可以整个被锁住（镜头修改勾了"锁输入"），也可以只锁某几项（镜头修改勾了"覆盖输入"：它改哪项就锁哪项）；
/// - 距离这一帧已经被别的控制器改过，滚轮就不再动它。
/// </summary>
internal sealed class ZrCameraInputController(ZRCFightCameraLogicComponent camera) : ZrCameraControllerBase(camera)
{
    // 灵敏度按 60 帧换算；一帧最多按 1/30 秒算，卡顿时不会一下转太多
    private const double DefaultFps = 60.0;
    private const double MaxDeltaTime = 1.0 / 30.0;
    // 转动量换成角度的倍数（UE 玩家控制器的默认值）
    private const double InputYawScale = 2.5;
    private const double InputPitchScale = -2.5;
    // 把增强输入给的量换成配置数值所用的单位：
    // 鼠标这一帧的移动量 ÷ 帧时间 × 3 ≈ 每秒的转镜头量；滚轮这一帧的格数 × 7 ≈ 这一帧的缩放量
    private const double LookInputScale = 3.0;
    private const double ZoomInputScale = 7.0;

    // ===== 配置（"镜头输入"那一组） =====
    // 滚轮缩放的速度
    private double _zoomSpeed;
    // 缓冲系数（上一帧的输入保留多少）：按输入大小在最小值和最大值之间取，输入达到"极值"时取到最大值
    private double _smoothFactorMin;
    private double _smoothFactorMax;
    private double _smoothFactorRange;
    private ZrCameraCurve _smoothFactorCurve = ZrCameraCurve.Linear;
    // 缓冲后的输入不超过最小值就当没动；超过最大值就夹住
    private double _inputSpeedMin;
    private double _inputSpeedMax;
    // 偏航 / 俯仰的灵敏度：按输入大小在最小值和最大值之间取，输入达到"极值"时取到最大值
    private double _sensitivityYawMin;
    private double _sensitivityYawMax;
    private double _sensitivityYawRange;
    private ZrCameraCurve _sensitivityYawCurve = ZrCameraCurve.Linear;
    private double _sensitivityPitchMin;
    private double _sensitivityPitchMax;
    private double _sensitivityPitchRange;
    private ZrCameraCurve _sensitivityPitchCurve = ZrCameraCurve.Linear;

    // ===== 按项锁住输入的人 =====
    private readonly HashSet<object> _yawLockers = new();
    private readonly HashSet<object> _pitchLockers = new();
    private readonly HashSet<object> _armLengthLockers = new();

    // ===== 运行中 =====
    // 缓冲后的偏航 / 俯仰输入
    private double _yawInput;
    private double _pitchInput;
    // 上次夹缩放倍率时用的距离和上下限。配置把它们改了，就按新的范围重新夹一遍
    private double _cachedArmLength;
    private double _cachedMinArmLength;
    private double _cachedMaxArmLength;

    protected override void OnInit()
    {
        Config.SetConfigMap((int)EWuwaFightCameraInput.ZoomSpeed, value => _zoomSpeed = value);
        Config.SetConfigMap((int)EWuwaFightCameraInput.InputSpeedMin, value => _inputSpeedMin = value);
        Config.SetConfigMap((int)EWuwaFightCameraInput.InputSpeedMax, value => _inputSpeedMax = value);
        Config.SetConfigMap((int)EWuwaFightCameraInput.SmoothFactorMin, value => _smoothFactorMin = value);
        Config.SetConfigMap((int)EWuwaFightCameraInput.SmoothFactorMax, value => _smoothFactorMax = value);
        Config.SetConfigMap((int)EWuwaFightCameraInput.SmoothFactorRange, value => _smoothFactorRange = value);
        Config.SetCurveConfigMap((int)EWuwaFightCameraInput.SmoothFactorRange, curve => _smoothFactorCurve = curve);
        Config.SetConfigMap((int)EWuwaFightCameraInput.SensitivityYawMin, value => _sensitivityYawMin = value);
        Config.SetConfigMap((int)EWuwaFightCameraInput.SensitivityYawMax, value => _sensitivityYawMax = value);
        Config.SetConfigMap((int)EWuwaFightCameraInput.SensitivityYawRange, value => _sensitivityYawRange = value);
        Config.SetCurveConfigMap((int)EWuwaFightCameraInput.SensitivityYawRange, curve => _sensitivityYawCurve = curve);
        Config.SetConfigMap((int)EWuwaFightCameraInput.SensitivityPitchMin, value => _sensitivityPitchMin = value);
        Config.SetConfigMap((int)EWuwaFightCameraInput.SensitivityPitchMax, value => _sensitivityPitchMax = value);
        Config.SetConfigMap((int)EWuwaFightCameraInput.SensitivityPitchRange, value => _sensitivityPitchRange = value);
        Config.SetCurveConfigMap((int)EWuwaFightCameraInput.SensitivityPitchRange, curve => _sensitivityPitchCurve = curve);
    }

    // ===== 按项锁住 / 解锁 =====

    public void LockArmRotationYaw(object locker) => _yawLockers.Add(locker);

    public void LockArmRotationPitch(object locker) => _pitchLockers.Add(locker);

    public void UnlockArmRotationYaw(object locker) => _yawLockers.Remove(locker);

    public void UnlockArmRotationPitch(object locker) => _pitchLockers.Remove(locker);

    public void LockArmLength(object locker) => _armLengthLockers.Add(locker);

    public void UnlockArmLength(object locker) => _armLengthLockers.Remove(locker);

    /// <summary>清掉缓冲里攒着的转镜头输入。</summary>
    public void ResetCameraInput()
    {
        _yawInput = 0;
        _pitchInput = 0;
    }

    // 被锁住时清掉缓冲，解锁后不会接着之前攒的量继续转
    protected override void OnDisable() => ResetCameraInput();

    // ===== 每帧 =====

    protected override void UpdateInternal(float deltaSeconds)
    {
        UpdateRotation(deltaSeconds);
        UpdateZoom(deltaSeconds);
        RefreshZoomRange();
    }

    // 转镜头：算出这一帧转多少，以 Current 的角度为起点加上去，写进 Desired
    private void UpdateRotation(double deltaSeconds)
    {
        if (Camera.Character is null || deltaSeconds <= 0)
        {
            ResetCameraInput();
            return;
        }
        // 先换成每秒的量，再除以 60 换成"60 帧时每帧的量"
        var look = Camera.LookInput;
        double yaw = look.X / deltaSeconds * LookInputScale / DefaultFps;
        double pitch = look.Y / deltaSeconds * LookInputScale / DefaultFps;
        double magnitude = Math.Sqrt(yaw * yaw + pitch * pitch);
        double frameTime = Math.Min(deltaSeconds, MaxDeltaTime);
        yaw *= ZrCameraMath.Lerp(_sensitivityYawMin, _sensitivityYawMax, _sensitivityYawCurve.GetCurrentValue(magnitude / _sensitivityYawRange)) * frameTime;
        pitch *= ZrCameraMath.Lerp(_sensitivityPitchMin, _sensitivityPitchMax, _sensitivityPitchCurve.GetCurrentValue(magnitude / _sensitivityPitchRange)) * frameTime;

        // 缓冲：保留上一帧的一部分，再加上这一帧的
        double smooth = ZrCameraMath.Lerp(_smoothFactorMin, _smoothFactorMax, _smoothFactorCurve.GetCurrentValue(magnitude / _smoothFactorRange));
        _yawInput = ZrCameraMath.Clamp(_yawInput * smooth + yaw * (1 - smooth), -_inputSpeedMax, _inputSpeedMax);
        _pitchInput = ZrCameraMath.Clamp(_pitchInput * smooth + pitch * (1 - smooth), -_inputSpeedMax, _inputSpeedMax);

        var rotation = Camera.CurrentCamera.ArmRotation;
        bool changed = false;
        if (_yawLockers.Count == 0 && Math.Abs(_yawInput) > _inputSpeedMin)
        {
            changed = true;
            Camera.Modified.Yaw = true;
            rotation.Yaw = ZrCameraMath.WrapAngle(rotation.Yaw + _yawInput * InputYawScale);
        }
        if (_pitchLockers.Count == 0 && Math.Abs(_pitchInput) > _inputSpeedMin)
        {
            changed = true;
            Camera.Modified.Pitch = true;
            rotation.Pitch = ZrCameraMath.Clamp(ZrCameraMath.WrapAngle(rotation.Pitch) + _pitchInput * InputPitchScale,
                -ZrCameraMath.PitchLimitValue, ZrCameraMath.PitchLimitValue);
        }
        if (changed)
            Camera.DesiredCamera.ArmRotation = rotation;
    }

    // 滚轮：改缩放倍率。距离被锁住、或者这一帧已经被别的控制器改过时不动
    private void UpdateZoom(double deltaSeconds)
    {
        if (_armLengthLockers.Count > 0 || Camera.Modified.ArmLength || Camera.Modified.ZoomModifier)
            return;
        // 滚轮往前（正）是拉近，所以取负
        double input = -Camera.ZoomInput * ZoomInputScale;
        if (input == 0)
            return;
        var desired = Camera.DesiredCamera;
        double range = desired.MaxArmLength - desired.MinArmLength;
        if (range <= ZrCameraMath.SmallNumber)
            return;
        SetZoomModifier(desired.ZoomModifier + input * _zoomSpeed / range);
        Camera.Modified.ArmLength = true;
    }

    // 配置把距离或上下限改了，就按新的范围把缩放倍率重新夹一遍
    private void RefreshZoomRange()
    {
        if (Camera.Modified.ArmLength || Camera.Modified.ZoomModifier)
            return;
        var current = Camera.CurrentCamera;
        if (_cachedArmLength <= 0 || _cachedMinArmLength <= 0 || _cachedMaxArmLength <= 0)
        {
            CacheArmLengths();
            return;
        }
        if (Math.Abs(_cachedMinArmLength - current.MinArmLength) < ZrCameraMath.KindaSmallNumber
            && Math.Abs(_cachedMaxArmLength - current.MaxArmLength) < ZrCameraMath.KindaSmallNumber
            && Math.Abs(_cachedArmLength - current.ArmLength) < ZrCameraMath.KindaSmallNumber)
            return;
        SetZoomModifier(Camera.DesiredCamera.ZoomModifier);
    }

    // 设缩放倍率：先换成距离夹在上下限内，再换回倍率；并记下这次用的距离和上下限
    private void SetZoomModifier(double zoomModifier)
    {
        var current = Camera.CurrentCamera;
        double armLength = Camera.GetArmLengthWithSetting(current);
        if (armLength <= ZrCameraMath.SmallNumber)
            return;
        Camera.DesiredCamera.ZoomModifier = ZrCameraMath.Clamp(zoomModifier * armLength, current.MinArmLength, current.MaxArmLength) / armLength;
        CacheArmLengths();
    }

    private void CacheArmLengths()
    {
        var current = Camera.CurrentCamera;
        _cachedArmLength = current.ArmLength;
        _cachedMinArmLength = current.MinArmLength;
        _cachedMaxArmLength = current.MaxArmLength;
    }
}
