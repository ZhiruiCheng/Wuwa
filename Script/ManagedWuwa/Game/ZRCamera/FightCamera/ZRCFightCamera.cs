using UnrealSharp.CoreUObject;

namespace ManagedWuwa.Game.ZRCamera.FightCamera;

internal sealed class ZRCFightCamera
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
        // 逻辑组件读配置，各子控制器开始工作
        LogicComponent.Init();
    }

    public void Tick(float deltaSeconds, FVector2D lookInput, float zoomInput)
    {
        LogicComponent.Tick(deltaSeconds, lookInput, zoomInput);
    }


    public void Clear()
    {
        // 和创建顺序相反：先停逻辑（不再去摆相机），再把相机还回对象池
        LogicComponent.Clear();
        DisplayComponent.Clear();
    }
}
