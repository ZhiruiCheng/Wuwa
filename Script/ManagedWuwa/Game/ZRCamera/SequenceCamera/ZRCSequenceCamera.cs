namespace ManagedWuwa.Game.ZRCamera;

/// <summary>
/// 序列镜头：技能特写镜头，和战斗镜头同一层，挂在 ZRCameraModelInstance 下面。
/// 显示组件 = 这个模式下屏幕看的相机；播放组件 = 播关卡序列、进出序列镜头模式。
/// </summary>
internal sealed class ZRCSequenceCamera
{
    //上层引用
    public ZRCameraModelInstance CameraModel { get; }

    //下层引用
    public ZRCSequenceCameraDisplayComponent DisplayComponent { get; }
    public ZRCSequenceCameraPlayerComponent PlayerComponent { get; }

    public ZRCSequenceCamera(ZRCameraModelInstance cameraModel)
    {
        CameraModel = cameraModel;
        DisplayComponent = new ZRCSequenceCameraDisplayComponent();
        PlayerComponent = new ZRCSequenceCameraPlayerComponent(cameraModel, DisplayComponent);
    }

    public void Tick(float deltaSeconds)
    {
        PlayerComponent.Tick(deltaSeconds);
    }

    public void Clear()
    {
        // 先停播放（序列还回对象池），再清掉相机引用
        PlayerComponent.Clear();
        DisplayComponent.Clear();
    }
}
