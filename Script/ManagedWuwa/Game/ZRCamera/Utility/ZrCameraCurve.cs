using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.ZRCamera;

/// <summary>
/// 过渡曲线：输入 0~1 的进度（超出先夹住），输出 0~1 的比例。
/// 支持线性、二阶（凹 / 凸）、三阶（凹 / 凸 / S / 反 S）、幂函数（凹 / 凸）几种公式曲线。
/// </summary>
internal sealed class ZrCameraCurve
{
    private enum EKind { Linear, Polynomial, Power }

    public static readonly ZrCameraCurve Linear = new(EKind.Linear, 0, 0, 0);

    private readonly EKind _kind;
    // 多项式 ((a * t + b) * t + c) * t；幂函数 t ^ a
    private readonly double _a;
    private readonly double _b;
    private readonly double _c;

    private ZrCameraCurve(EKind kind, double a, double b, double c)
    {
        _kind = kind;
        _a = a;
        _b = b;
        _c = c;
    }

    /// <summary>按配置造一条曲线（N 是曲线参数：二阶 / 三阶是斜率，幂函数是指数）。</summary>
    public static ZrCameraCurve Create(FWuwaCameraCurve config)
    {
        double n = config.N;
        switch (config.CurveType)
        {
            case EWuwaCameraCurveType.SquaredConcave: return n == 0 ? Linear : Squared(1 / n);
            case EWuwaCameraCurveType.SquaredConvex: return Squared(n);
            case EWuwaCameraCurveType.CubicConcave: return n == 0 ? Linear : CubicWithStartSlope(1 / n);
            case EWuwaCameraCurveType.CubicConvex: return CubicWithStartSlope(n);
            case EWuwaCameraCurveType.CubicS: return Cubic(n);
            case EWuwaCameraCurveType.CubicReverseS: return n == 0 ? Linear : Cubic(1 / n);
            case EWuwaCameraCurveType.PowerConcave: return Power(n);
            case EWuwaCameraCurveType.PowerConvex: return n == 0 ? Linear : Power(1 / n);
            default: return Linear;
        }
    }

    /// <summary>进度 t（0~1）对应的比例。</summary>
    public double GetCurrentValue(double t)
    {
        t = ZrCameraMath.Clamp(t, 0, 1);
        return _kind switch
        {
            EKind.Polynomial => ((_a * t + _b) * t + _c) * t,
            EKind.Power => Math.Pow(t, _a),
            _ => t,
        };
    }

    // 二阶曲线：起始斜率 k（0~2，越界按 1）
    private static ZrCameraCurve Squared(double slope)
    {
        if (slope < 0 || slope > 2) slope = 1;
        return new ZrCameraCurve(EKind.Polynomial, 0, 1 - slope, slope);
    }

    // 三阶曲线：起始斜率 k（≤0 按线性）
    private static ZrCameraCurve CubicWithStartSlope(double slope) =>
        slope <= 0 ? Linear : new ZrCameraCurve(EKind.Polynomial, slope + 1 / slope - 2, 3 - 2 * slope - 1 / slope, slope);

    // 三阶 S 形曲线：起点和终点的斜率都是 k
    private static ZrCameraCurve Cubic(double slope) => new(EKind.Polynomial, -2 * (1 - slope), 3 * (1 - slope), slope);

    // 幂函数：t 的 p 次方（p < 0 按 0）
    private static ZrCameraCurve Power(double exponent) => new(EKind.Power, Math.Max(0, exponent), 0, 0);
}
