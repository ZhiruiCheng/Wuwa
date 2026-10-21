using UnrealSharp.CoreUObject;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.Camera;

/// <summary>
/// 输入为值快照，输出为镜头参数；不查找角色、组件或 CameraActor。
/// 模式切换只混合相对于目标的参数，目标世界坐标始终使用当前帧位置。
/// </summary>
internal sealed class WuwaFightCameraLogic
{
    private readonly record struct PoseParameters(
        FVector PivotOffset, FRotator RotationOffset, float ArmLength, float FieldOfView);

    private bool _initialized;
    private int _modeHandle;
    private float _blendElapsed;
    private float _blendDuration;
    private PoseParameters _blendStart;
    private PoseParameters _current;

    public FWuwaCameraView Evaluate(
        FWuwaCameraFrame frame,
        FWuwaCameraSettings settings,
        float desiredArmLength,
        int modeHandle,
        float blendOutTime = -1f)
    {
        if (!WuwaCameraSettingsUtility.IsFinite(frame.TargetLocation))
            return default;

        float deltaSeconds = Math.Max(0f, WuwaCameraSettingsUtility.Finite(frame.DeltaSeconds));
        var desired = new PoseParameters(
            settings.PivotOffset, settings.RotationOffset, desiredArmLength, settings.FieldOfView);

        if (!_initialized)
        {
            _initialized = true;
            _modeHandle = modeHandle;
            _current = desired;
        }
        else
        {
            if (_modeHandle != modeHandle)
            {
                _modeHandle = modeHandle;
                _blendStart = _current;
                _blendElapsed = 0f;
                // 被移除的模式自带淡出时间就用它，否则用新模式的 BlendTime
                _blendDuration = blendOutTime >= 0f ? blendOutTime : settings.BlendTime;
                if (_blendDuration <= 0f)
                    _current = desired;
            }

            if (_blendElapsed < _blendDuration)
            {
                _blendElapsed = Math.Min(_blendDuration, _blendElapsed + deltaSeconds);
                double alpha = _blendElapsed / _blendDuration;
                alpha = alpha * alpha * (3.0 - 2.0 * alpha);
                _current = Interpolate(_blendStart, desired, alpha);
            }
            else
            {
                // 插值系数按时间计算；高帧率与低帧率下缩放响应保持一致。
                double alpha = settings.ZoomInterpSpeed > 0f
                    ? 1.0 - Math.Exp(-(double)settings.ZoomInterpSpeed * deltaSeconds)
                    : 1.0;
                _current = desired with { ArmLength = Lerp(_current.ArmLength, desiredArmLength, alpha) };
            }
        }

        var rotation = new FRotator(
            WuwaCameraSettingsUtility.Angle(frame.ControlRotation.Pitch + _current.RotationOffset.Pitch),
            WuwaCameraSettingsUtility.Angle(frame.ControlRotation.Yaw + _current.RotationOffset.Yaw),
            WuwaCameraSettingsUtility.Angle(frame.ControlRotation.Roll + _current.RotationOffset.Roll));

        // PivotOffset 是世界轴偏移，与角色转身无关；未来肩部偏移可单独增加局部空间字段。
        var pivot = new FVector(
            frame.TargetLocation.X + _current.PivotOffset.X,
            frame.TargetLocation.Y + _current.PivotOffset.Y,
            frame.TargetLocation.Z + _current.PivotOffset.Z);

        // UE 的前向向量：X 前、Y 右、Z 上；只用数值计算，避免纯策略依赖原生调用。
        double pitch = rotation.Pitch * Math.PI / 180.0;
        double yaw = rotation.Yaw * Math.PI / 180.0;
        double horizontal = Math.Cos(pitch) * _current.ArmLength;
        var location = new FVector(
            pivot.X - horizontal * Math.Cos(yaw),
            pivot.Y - horizontal * Math.Sin(yaw),
            pivot.Z - Math.Sin(pitch) * _current.ArmLength);

        return new FWuwaCameraView
        {
            Valid = WuwaCameraSettingsUtility.IsFinite(pivot) && WuwaCameraSettingsUtility.IsFinite(location),
            Pivot = pivot,
            Location = location,
            Rotation = rotation,
            FieldOfView = _current.FieldOfView,
            ArmLength = _current.ArmLength,
            CollisionTest = settings.CollisionTest,
            ProbeRadius = settings.ProbeRadius
        };
    }

    public void Reset()
    {
        _initialized = false;
        _blendElapsed = 0f;
        _blendDuration = 0f;
    }

    private static PoseParameters Interpolate(PoseParameters from, PoseParameters to, double alpha) => new(
        new FVector(
            Lerp(from.PivotOffset.X, to.PivotOffset.X, alpha),
            Lerp(from.PivotOffset.Y, to.PivotOffset.Y, alpha),
            Lerp(from.PivotOffset.Z, to.PivotOffset.Z, alpha)),
        new FRotator(
            LerpAngle(from.RotationOffset.Pitch, to.RotationOffset.Pitch, alpha),
            LerpAngle(from.RotationOffset.Yaw, to.RotationOffset.Yaw, alpha),
            LerpAngle(from.RotationOffset.Roll, to.RotationOffset.Roll, alpha)),
        Lerp(from.ArmLength, to.ArmLength, alpha),
        Lerp(from.FieldOfView, to.FieldOfView, alpha));

    private static float Lerp(float from, float to, double alpha) => (float)(from + (to - (double)from) * alpha);

    private static double Lerp(double from, double to, double alpha) => from + (to - from) * alpha;

    private static double LerpAngle(double from, double to, double alpha) =>
        WuwaCameraSettingsUtility.Angle(from + WuwaCameraSettingsUtility.Angle(to - from) * alpha);
}
