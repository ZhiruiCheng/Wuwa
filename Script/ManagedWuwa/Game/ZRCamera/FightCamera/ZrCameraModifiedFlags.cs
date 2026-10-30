namespace ManagedWuwa.Game.ZRCamera;

/// <summary>
/// "这一帧改过"的标记：某个控制器这一帧改了 Desired 的哪一项，就把哪一项的标记设上；
/// 排在后面的步骤看到标记，这一帧就不再动这一项。每帧开始时全部清掉。
/// </summary>
internal sealed class ZrCameraModifiedFlags
{
    public bool ArmLocation;
    public bool ArmLength;
    public bool ZoomModifier;
    public bool Pitch;
    public bool Yaw;
    public bool CameraOffset;
    public bool Fov;

    public void Clear()
    {
        ArmLocation = false;
        ArmLength = false;
        ZoomModifier = false;
        Pitch = false;
        Yaw = false;
        CameraOffset = false;
        Fov = false;
    }
}
