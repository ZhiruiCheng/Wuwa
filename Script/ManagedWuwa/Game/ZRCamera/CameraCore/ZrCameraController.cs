using UnrealSharp.Core;
using UnrealSharp.CoreUObject;
using UnrealSharp.Engine;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.ZRCamera;

/// <summary>
/// 相机系统对外的入口：创建 / 清理、每帧更新、切换镜头模式，以及转给各镜头的调用。
/// 外面只通过它访问镜头，不直接碰各镜头的组件。
/// </summary>
internal static class ZrCameraController
{
    /// <summary>创建相机系统：主屏数据 → 序列镜头 + 战斗镜头 → 战斗镜头从对象池取出相机并让屏幕看它。</summary>
    public static void Init(APlayerCameraManager playerCameraManager) =>
        ZRCameraModel.CreateMainModel(playerCameraManager);

    /// <summary>清理相机系统：相机还回对象池，主屏数据清空。</summary>
    public static void Clear() => ZRCameraModel.DestroyMainModel();

    /// <summary>换了控制的角色：战斗镜头和序列镜头都换跟随的角色。</summary>
    public static void OnPossess(APawn? pawn)
    {
        var model = ZRCameraModel.MainModel;
        if (model is null) return;
        model.FightCamera.LogicComponent.SetPawn(pawn);
        model.SequenceCamera.PlayerComponent.SetPawn(pawn);
    }

    //其中的是否弃用由相机自己判断,Controlller只知道这个相机需要Tick不知道里面在干嘛,解耦合
    // 序列镜头先更新：它结束的那一帧会让战斗镜头马上补算一次，然后战斗镜头再正常更新
    public static void Tick(float deltaSeconds, FVector2D lookInput, float zoomInput)
    {
        var model = ZRCameraModel.MainModel;
        if (model is null)
            return;
        model.SequenceCamera.Tick(deltaSeconds);
        model.FightCamera.Tick(deltaSeconds, lookInput, zoomInput);
    }


    /// <summary>
    /// 记下当前相机，让屏幕（带混合）改看它。
    /// lockOutgoing：混合期间旧相机的画面定在最后一帧，旧相机这时被销毁也没关系。
    /// </summary>
    public static void SetViewTarget(AActor? actor, float blendTime = 0f, bool lockOutgoing = false)
    {
        if (actor is null || !actor.IsValid())
        {
            LogZrCamera.LogWarning("Camera not valid");
            return;
        }
        var model = ZRCameraModel.MainModel;
        if (model is null)
            return;
        model.CurrentCameraActor = actor;
        model.PlayerCameraManager.OwningPlayerController?.SetViewTargetWithBlend(actor, blendTime, EViewTargetBlendFunction.VTBlend_Linear, 0f, lockOutgoing);
    }

    /// <summary>
    /// 打开一个镜头模式，屏幕按 blendTime 混过去。
    /// 当前模式优先级更高时只记下"开着"，等高的退出后再轮到它。战斗镜头不主动进入。
    /// </summary>
    public static bool EnterCameraMode(EZrCameraMode mode, float blendTime)
    {
        var model = ZRCameraModel.MainModel;
        if (model is null)
            return false;
        if (mode == EZrCameraMode.Fight)
        {
            LogZrCamera.LogError("[CameraMode]战斗镜头不应主动切换，应由其他镜头退出时被动切换");
            return false;
        }
        model.EnableMode(mode);
        if (model.IsInHigherMode(mode))
        {
            LogZrCamera.LogWarning($"[CameraMode]意图进入较低优先级的镜头 Current={model.CameraMode} New={mode}");
            return false;
        }
        return SwitchCameraMode(model, mode, blendTime);
    }

    /// <summary>关掉一个镜头模式，切到剩下开着的模式里优先级最高的（最后总会回到战斗镜头）。</summary>
    public static bool ExitCameraMode(EZrCameraMode mode, float blendTime)
    {
        var model = ZRCameraModel.MainModel;
        if (model is null)
            return false;
        if (mode == EZrCameraMode.Fight)
        {
            LogZrCamera.LogError("[CameraMode]战斗镜头不应主动切换，应由其他镜头退出时被动切换");
            return false;
        }
        model.DisableMode(mode);
        return SwitchCameraMode(model, model.GetNextMode(), blendTime);
    }

    // 改模式，屏幕带混合改看这个模式的相机（混合期间旧画面定格）
    private static bool SwitchCameraMode(ZRCameraModelInstance model, EZrCameraMode mode, float blendTime)
    {
        if (model.CameraMode == mode)
            return false;
        model.SetCameraMode(mode);
        AActor? camera = mode switch
        {
            EZrCameraMode.Fight => model.FightCamera.DisplayComponent.CameraActor,
            EZrCameraMode.Sequence => model.SequenceCamera.DisplayComponent.CineCamera,
            _ => null,
        };
        //用快照过渡
        SetViewTarget(camera, blendTime, lockOutgoing: true);
        return true;
    }


    // ===== 下面这些都是转给各镜头的组件；外面只通过 Controller 访问，不直接碰组件 =====

    /// <summary>战斗镜头的相机。</summary>
    public static AActor? GetFightCameraActor() =>
        ZRCameraModel.MainModel?.FightCamera.DisplayComponent.CameraActor;

    /// <summary>开始一段镜头修改（技能动画通知调用）。返回这段修改的编号，没生效返回 -1。</summary>
    public static int ApplyCameraModify(FWuwaCameraModify settings, float duration, float blendInTime, float blendOutTime,
        float breakBlendOutTime, UAnimSequenceBase? animation, AActor? owner) =>
        ZRCameraModel.MainModel?.FightCamera.LogicComponent.ApplyCameraModify(settings, duration, blendInTime, blendOutTime,
            breakBlendOutTime, animation, owner) ?? ZrCameraModifyController.InvalidModifyInstance;

    /// <summary>提前结束一段镜头修改：动画和编号都对得上才结束。</summary>
    public static void StopCameraModify(UAnimSequenceBase? animation, int modifyInstance) =>
        ZRCameraModel.MainModel?.FightCamera.LogicComponent.StopCameraModify(animation, modifyInstance);

    /// <summary>播一段技能特写镜头。</summary>
    public static bool PlayCameraSequence(FWuwaSequenceCameraConfig config, ACharacter? character, FName attachSocket) =>
        ZRCameraModel.MainModel?.SequenceCamera.PlayerComponent.PlayCameraSequence(config, character, attachSocket) ?? false;

    /// <summary>正在播技能特写镜头。</summary>
    public static bool IsPlayingCameraSequence() =>
        ZRCameraModel.MainModel?.SequenceCamera.PlayerComponent.IsInCinematic ?? false;

    /// <summary>特写镜头正在用的相机（没在播时为空）。</summary>
    public static AActor? GetSequenceCameraActor() =>
        ZRCameraModel.MainModel?.SequenceCamera.DisplayComponent.CineCamera;
}
