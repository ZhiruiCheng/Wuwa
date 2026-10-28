using ManagedWuwa.Game.ZRCamera.FightCamera;
using UnrealSharp.Engine;

namespace ManagedWuwa.Game.ZRCamera;

//代表一块屏幕的相机数据,因为可能是分屏所以一块屏幕可能不止一个
public sealed class ZRCameraModelInstance(APlayerCameraManager playerCameraManager)
{

    public APlayerCameraManager PlayerCameraManager { get; } = playerCameraManager;
    
    // 当前是哪种镜头在出画面。只能在类内部修改，第 2 步加切换功能时再提供修改的方法
    public EZrCameraMode CameraMode { get; private set; } = EZrCameraMode.Fight;
    
    // 屏幕当前在看的相机 Actor，由 ZrCameraController.SetViewTarget 写入
    public AActor? CurrentCameraActor { get; set; }
    
    
    // 战斗镜头，在 Init 里创建
    public ZRCFightCamera FightCamera { get; private set; } = null!;
    //序列镜头
    //UI镜头等等

    public void Init()
    {
        // 原作 Init：创建这块屏幕的所有镜头（项目目前只有战斗镜头）
        FightCamera = new ZRCFightCamera(this);
        FightCamera.Init();
    }
    public void Clear()
    {
        FightCamera.Clear();
        CurrentCameraActor = null;
    }
    
}