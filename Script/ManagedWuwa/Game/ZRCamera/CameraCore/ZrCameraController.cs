using UnrealSharp.CoreUObject;
using UnrealSharp.Engine;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.ZRCamera;

internal  static class ZrCameraController
{
    /// <summary>创建相机系统：主屏数据 → 战斗镜头 → 从对象池取出相机并让屏幕看它。</summary>
    public static void Init(APlayerCameraManager playerCameraManager) =>
        ZRCameraModel.CreateMainModel(playerCameraManager);
    
    /// <summary>清理相机系统：战斗相机还回对象池，主屏数据清空。</summary>
    public static void Clear() => ZRCameraModel.DestroyMainModel();
    
    /// <summary>换了控制的角色（≈ 原作 OnPossess：告诉战斗镜头新的跟随对象）。</summary>
    public static void OnPossess(APawn? pawn) =>
        ZRCameraModel.MainModel?.FightCamera.LogicComponent.SetPawn(pawn);

    public static void Tick(float deltaSeconds, float zoomDelta)
    {
        //以后有很多类型的相机可以添加
        ZRCameraModel.MainModel?.FightCamera.Tick(deltaSeconds, zoomDelta);
    }
        
    
    /// <summary>≈ 原作 CameraController.SetViewTarget：记下当前相机，让屏幕（带混合）改看它。</summary>
    public static void SetViewTarget(AActor actor, float blendTime = 0f)
    {
        var model = ZRCameraModel.MainModel;
        if (model is null)
            return;
        model.CurrentCameraActor = actor;
        model.PlayerCameraManager.OwningPlayerController?.SetViewTargetWithBlend(actor, blendTime);
    }
    
    
    // ===== 下面这些都是转给战斗镜头的组件；外面只通过 Controller 访问，不直接碰组件 =====

    public static AActor? GetFightCameraActor() =>
        ZRCameraModel.MainModel?.FightCamera.DisplayComponent.CameraActor;

    public static FWuwaCameraSettings ResolveSettings() =>
        ZRCameraModel.MainModel?.FightCamera.LogicComponent.ResolveSettings() ?? default;

    public static int PushCameraMode(UObject source, FWuwaCameraSettings settings, int priority) =>
        ZRCameraModel.MainModel?.FightCamera.LogicComponent.PushCameraMode(source, settings, priority) ?? 0;

    public static bool PopCameraMode(int handle) =>
        ZRCameraModel.MainModel?.FightCamera.LogicComponent.PopCameraMode(handle) ?? false;
}