using UnrealSharp.CoreUObject;

namespace ManagedWuwa.Game.ZRCamera;

/// <summary>
/// 镜头用到的数学小工具。
/// 都是纯数值计算，不调引擎：每帧调很多次，省掉跨语言调用。
/// </summary>
internal static class ZrCameraMath
{
    public const double KindaSmallNumber = 1e-4;
    public const double SmallNumber = 1e-8;
    /// <summary>俯仰最大到 ±89.9，再大镜头就翻过去了。</summary>
    public const double PitchLimitValue = 89.9;

    public static double Lerp(double from, double to, double alpha) => from + (to - from) * alpha;

    public static FVector Lerp(FVector from, FVector to, double alpha) =>
        new(Lerp(from.X, to.X, alpha), Lerp(from.Y, to.Y, alpha), Lerp(from.Z, to.Z, alpha));

    public static double Clamp(double value, double min, double max) => value < min ? min : value > max ? max : value;

    /// <summary>角度换到 (-180, 180]。</summary>
    public static double WrapAngle(double angle)
    {
        angle %= 360.0;
        if (angle > 180.0) angle -= 360.0;
        else if (angle <= -180.0) angle += 360.0;
        return angle;
    }

    /// <summary>沿最短方向从 from 转到 to。</summary>
    public static double AxisLerp(double from, double to, double alpha) => WrapAngle(from + WrapAngle(to - from) * alpha);

    /// <summary>两个角度相差不超过 tolerance（按最短方向算）。</summary>
    public static bool IsAngleNearEqual(double a, double b, double tolerance) => Math.Abs(WrapAngle(a - b)) <= tolerance;

    /// <summary>有效的俯仰角：配置里写 ±90 这种整数倍时换成 ±89.9；其余夹在 ±89.9 以内。</summary>
    public static double GetValidPitchAngle(double pitch, bool isMin)
    {
        if (Math.Abs(pitch) > KindaSmallNumber && Math.Abs(Math.Abs(pitch) % 90.0) < KindaSmallNumber)
            return pitch > 0 ? PitchLimitValue : -PitchLimitValue;
        return isMin ? Math.Max(pitch, -PitchLimitValue) : Math.Min(pitch, PitchLimitValue);
    }

    /// <summary>按速度往目标靠，越近越慢（同 UE 的 FMath::FInterpTo）。</summary>
    public static double InterpTo(double current, double target, double deltaTime, double speed)
    {
        if (speed <= 0) return target;
        double distance = target - current;
        if (distance * distance < SmallNumber) return target;
        return current + distance * Clamp(deltaTime * speed, 0, 1);
    }

    /// <summary>角度版的 InterpTo（沿最短方向）。</summary>
    public static double AxisInterpTo(double current, double target, double deltaTime, double speed)
    {
        if (speed <= 0) return target;
        double delta = WrapAngle(target - current);
        if (Math.Abs(delta) < KindaSmallNumber) return target;
        return WrapAngle(current + delta * Clamp(deltaTime * speed, 0, 1));
    }

    /// <summary>向量版的 InterpTo（同 UE 的 FMath::VInterpTo）。</summary>
    public static FVector InterpTo(FVector current, FVector target, double deltaTime, double speed)
    {
        if (speed <= 0) return target;
        var distance = new FVector(target.X - current.X, target.Y - current.Y, target.Z - current.Z);
        if (distance.X * distance.X + distance.Y * distance.Y + distance.Z * distance.Z < KindaSmallNumber) return target;
        double alpha = Clamp(deltaTime * speed, 0, 1);
        return new FVector(current.X + distance.X * alpha, current.Y + distance.Y * alpha, current.Z + distance.Z * alpha);
    }

    /// <summary>
    /// 按朝向转一个向量（= UE FRotationMatrix(rotation).TransformVector）：
    /// 把"镜头空间 / 角色空间"里的偏移（X 前、Y 右、Z 上）换成世界坐标里的偏移。
    /// </summary>
    public static FVector RotateVector(FRotator rotation, FVector v)
    {
        double pitch = rotation.Pitch * Math.PI / 180.0, yaw = rotation.Yaw * Math.PI / 180.0, roll = rotation.Roll * Math.PI / 180.0;
        double sp = Math.Sin(pitch), cp = Math.Cos(pitch), sy = Math.Sin(yaw), cy = Math.Cos(yaw), sr = Math.Sin(roll), cr = Math.Cos(roll);
        // 三行 = 旋转后的 X / Y / Z 轴
        double xx = cp * cy, xy = cp * sy, xz = sp;
        double yx = sr * sp * cy - cr * sy, yy = sr * sp * sy + cr * cy, yz = -sr * cp;
        double zx = -(cr * sp * cy + sr * sy), zy = cy * sr - cr * sp * sy, zz = cr * cp;
        return new FVector(
            v.X * xx + v.Y * yx + v.Z * zx,
            v.X * xy + v.Y * yy + v.Z * zy,
            v.X * xz + v.Y * yz + v.Z * zz);
    }

    public static FVector Add(FVector a, FVector b) => new(a.X + b.X, a.Y + b.Y, a.Z + b.Z);

    public static FVector Subtract(FVector a, FVector b) => new(a.X - b.X, a.Y - b.Y, a.Z - b.Z);

    public static FVector Scale(FVector v, double scale) => new(v.X * scale, v.Y * scale, v.Z * scale);

    public static double AbsMax(FVector v) => Math.Max(Math.Abs(v.X), Math.Max(Math.Abs(v.Y), Math.Abs(v.Z)));

    public static bool IsFinite(FVector v) => double.IsFinite(v.X) && double.IsFinite(v.Y) && double.IsFinite(v.Z);

    public static bool IsFinite(FRotator r) => double.IsFinite(r.Pitch) && double.IsFinite(r.Yaw) && double.IsFinite(r.Roll);
}
