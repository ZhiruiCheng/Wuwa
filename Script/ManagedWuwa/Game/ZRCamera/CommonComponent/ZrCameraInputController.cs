using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.Camera.FightCameraController;

/// <summary>
/// 处理语义缩放增量，不读取物理按键。旋转继续使用 UE 的 ControlRotation 输入流程。
/// 用户缩放偏好独立保存，技能模式临时覆盖距离不会覆盖这个偏好。
/// </summary>
internal sealed class ZrCameraInputController
{
    private double _userZoomModifier = 1.0;

    public float ResolveArmLength(FWuwaCameraSettings settings, float zoomDelta)
    {
        if (!settings.Zoom.UseUserZoom)
            return settings.Arm.ArmLength;

        double distance = Math.Clamp(
            settings.Arm.ArmLength * _userZoomModifier,
            settings.Arm.MinArmLength,
            settings.Arm.MaxArmLength);

        if (settings.Zoom.AllowZoomInput && float.IsFinite(zoomDelta) && zoomDelta != 0f)
        {
            // MouseWheelAxis 是本帧的增量，不能再乘 DeltaSeconds。
            distance = Math.Clamp(
                distance - (double)zoomDelta * settings.Zoom.ZoomStep,
                settings.Arm.MinArmLength,
                settings.Arm.MaxArmLength);

            //计算出用户输入的真实值
            if (settings.Arm.ArmLength > 0.001f)
                _userZoomModifier = distance / settings.Arm.ArmLength;
        }

        return (float)distance;
    }

    public void Reset() => _userZoomModifier = 1.0;
}
