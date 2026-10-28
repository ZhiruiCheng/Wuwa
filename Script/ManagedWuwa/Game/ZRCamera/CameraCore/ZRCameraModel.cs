using UnrealSharp.Engine;

namespace ManagedWuwa.Game.ZRCamera;

internal static class ZRCameraModel
{
    public static ZRCameraModelInstance? MainModel { get; private set; }

    public static ZRCameraModelInstance CreateMainModel(APlayerCameraManager playerCameraManager)
    {
        // 上一局没清理干净（比如中途报错没走到结束流程），就先清掉旧的
        DestroyMainModel();

        // 先赋值，再 Init：Init 里显示组件会通过 ZrCameraController 访问 MainModel 去切换视角，
        // 如果 Init 完才赋值，那时 MainModel 还是空的，切换视角就会失败
        MainModel = new ZRCameraModelInstance(playerCameraManager);
        MainModel.Init();
        return MainModel;
    }

    public static void DestroyMainModel()
    {
        MainModel?.Clear();
        MainModel = null;
    }
}