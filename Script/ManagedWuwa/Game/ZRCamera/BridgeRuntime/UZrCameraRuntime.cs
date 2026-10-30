using UnrealSharp.Attributes;
using UnrealSharp.Core;
using UnrealSharp.CoreUObject;
using UnrealSharp.Engine;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.ZRCamera;

/// <summary>
/// C++ 和 C# 相机系统之间的门（C# 这一侧）：重写 UWuwaCameraRuntimeBridge 的函数，全部转给 ZrCameraController，
/// 自己不存任何东西。镜头的拥有和计算都在 ZRCameraModel 下面。
/// 类名不要改：C++ 的 PCM 按 "/Script/UnrealSharp.ZrCameraRuntime_C" 加载它。
/// </summary>
[UClass]
public partial class UZrCameraRuntime : UWuwaCameraRuntimeBridge
{
    public override void InitCamera(APlayerCameraManager cameraManager) => ZrCameraController.Init(cameraManager);

    public override void ClearCamera() => ZrCameraController.Clear();

    public override void OnPossess(APawn pawn) => ZrCameraController.OnPossess(pawn);

    public override void TickCamera(float deltaSeconds, FVector2D lookInput, float zoomInput) =>
        ZrCameraController.Tick(deltaSeconds, lookInput, zoomInput);

    // 相机还没创建时返回空；! 只是告诉编译器"这里允许返回空"，C++ 那边会判空
    public override AActor GetFightCameraActor() => ZrCameraController.GetFightCameraActor()!;

    public override int ApplyCameraModify(FWuwaCameraModify settings, float duration, float blendInTime, float blendOutTime,
        float breakBlendOutTime, UAnimSequenceBase animation, AActor owner) =>
        ZrCameraController.ApplyCameraModify(settings, duration, blendInTime, blendOutTime, breakBlendOutTime, animation, owner);

    public override void StopCameraModify(UAnimSequenceBase animation, int modifyInstance) =>
        ZrCameraController.StopCameraModify(animation, modifyInstance);

    public override bool PlayCameraSequence(FWuwaSequenceCameraConfig config, ACharacter character, FName attachSocket) =>
        ZrCameraController.PlayCameraSequence(config, character, attachSocket);

    public override bool IsPlayingCameraSequence() => ZrCameraController.IsPlayingCameraSequence();

    public override AActor GetSequenceCameraActor() => ZrCameraController.GetSequenceCameraActor()!;
}
