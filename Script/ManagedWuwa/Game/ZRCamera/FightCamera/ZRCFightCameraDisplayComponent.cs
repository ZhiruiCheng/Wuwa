using System.Security.Cryptography.X509Certificates;
using UnrealSharp;
using UnrealSharp.CoreUObject;
using UnrealSharp.Engine;
using UnrealSharp.Wuwa;


namespace ManagedWuwa.Game.ZRCamera;

public sealed class ZRCFightCameraDisplayComponent(ZRCameraModelInstance cameraModel)
{
    public ACameraActor? CameraActor { get; private set; }

    public void Init()
    {
        var pool = cameraModel.PlayerCameraManager.GetWorldSubsystem<UWuwaActorPoolSubsystem>();
        if (pool is null)
        {
            return;
        }

        var actorclass = new TSubclassOf<AActor>(typeof(ACameraActor));

        CameraActor = pool.GetActor(actorclass, FTransform.Identity, null) as ACameraActor;

        if (CameraActor is null)
            return;

        //关闭画面比例锁定,鸣潮应该也是关闭的
        CameraActor.CameraComponent.ConstrainAspectRatio = false;
        if (cameraModel.CameraMode == EZrCameraMode.Fight)
        {
            ZrCameraController.SetViewTarget(CameraActor);
        }
    }

    //切换关卡的时候清理,但是因为pool是一个世界一个所以不会被清理
    public void Clear()
    {
        if (CameraActor is null)
            return;

        // 相机或 PCM 已经失效（比如上一局残留的静态数据），就只丢掉引用
        if (CameraActor.IsValid() && cameraModel.PlayerCameraManager.IsValid())
        {
            var pool = cameraModel.PlayerCameraManager.GetWorldSubsystem<UWuwaActorPoolSubsystem>();
            if (pool is not null && pool.IsValid())
                pool.PutActor(CameraActor, "ZRCFightCameraDisplayComponent.Clear");   // 原作：ActorSystem.Put
            else
                CameraActor.DestroyActor();   // 池已经不在了（世界正在拆除），直接销毁
        }

        // 还回之后立刻清空引用，防止重复还回
        CameraActor = null;
    }
}