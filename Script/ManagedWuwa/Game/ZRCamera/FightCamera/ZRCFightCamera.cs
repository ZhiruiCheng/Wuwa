namespace ManagedWuwa.Game.ZRCamera.FightCamera;

public sealed class ZRCFightCamera
{
    //上层引用
    public ZRCameraModelInstance CameraModel { get; }
    
    //下层引用
    public ZRCFightCameraDisplayComponent DisplayComponent { get; }
    public ZRCFightCameraLogicComponent LogicComponent { get; }

    public ZRCFightCamera(ZRCameraModelInstance cameraModel)
    {
        CameraModel = cameraModel;
       //This
        DisplayComponent = new ZRCFightCameraDisplayComponent(cameraModel);
        LogicComponent = new ZRCFightCameraLogicComponent(cameraModel, DisplayComponent);
    }

    public void Init()
    {
        DisplayComponent.Init();
    }

    public void Tick(float deltaSeconds, float zoomDelta)
    {
        LogicComponent.Tick(deltaSeconds, zoomDelta);
    }
    
    
    public void Clear()
    {
        // 和创建顺序相反：先停逻辑（不再去摆相机），再把相机还回对象池
        LogicComponent.Clear();
        DisplayComponent.Clear();
    }
}