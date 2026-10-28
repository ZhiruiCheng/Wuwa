using UnrealSharp.CoreUObject;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.Camera;

/// <summary>在配置入口集中处理非法数值；后续控制器只处理已归一化的数据。</summary>
internal static class ZrCameraSettingsUtility
{
    public static FWuwaCameraSettings Normalize(FWuwaCameraSettings settings)
    {
        settings.Arm.MinArmLength = Math.Max(0f, Finite(settings.Arm.MinArmLength, 150f));
        settings.Arm.MaxArmLength = Math.Max(settings.Arm.MinArmLength, Finite(settings.Arm.MaxArmLength, 800f));
        settings.Arm.ArmLength = Math.Clamp(Finite(settings.Arm.ArmLength, 400f), settings.Arm.MinArmLength, settings.Arm.MaxArmLength);
        settings.Zoom.ZoomStep = Math.Max(0f, Finite(settings.Zoom.ZoomStep, 50f));
        settings.Zoom.ZoomInterpSpeed = Math.Max(0f, Finite(settings.Zoom.ZoomInterpSpeed, 12f));
        settings.Blend.BlendTime = Math.Max(0f, Finite(settings.Blend.BlendTime, 0.2f));
        settings.Offset.FieldOfView = Math.Clamp(Finite(settings.Offset.FieldOfView, 90f), 5f, 170f);
        settings.Rotation.PitchMin = Math.Clamp(Finite(settings.Rotation.PitchMin, -89.9f), -89.9f, 89.9f);
        settings.Rotation.PitchMax = Math.Clamp(Finite(settings.Rotation.PitchMax, 89.9f), settings.Rotation.PitchMin, 89.9f);
        settings.Offset.PivotOffset = FiniteVector(settings.Offset.PivotOffset);
        settings.Offset.RotationOffset = new FRotator(
            Angle(settings.Offset.RotationOffset.Pitch),
            Angle(settings.Offset.RotationOffset.Yaw),
            Angle(settings.Offset.RotationOffset.Roll));
        settings.Arm.ProbeRadius = Math.Max(0f, Finite(settings.Arm.ProbeRadius, 12f));
        return settings;
    }

    public static float Finite(float value, float fallback = 0f) => float.IsFinite(value) ? value : fallback;

    public static double Angle(double value) => double.IsFinite(value) ? Math.IEEERemainder(value, 360.0) : 0.0;

    public static bool IsFinite(FVector value) =>
        double.IsFinite(value.X) && double.IsFinite(value.Y) && double.IsFinite(value.Z);

    private static FVector FiniteVector(FVector value) => new(
        double.IsFinite(value.X) ? value.X : 0.0,
        double.IsFinite(value.Y) ? value.Y : 0.0,
        double.IsFinite(value.Z) ? value.Z : 0.0);
}
