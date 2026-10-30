using ManagedWuwa.Game.ZRCamera.FightCamera;
using UnrealSharp.Engine;

namespace ManagedWuwa.Game.ZRCamera;

//代表一块屏幕的相机数据,因为可能是分屏所以一块屏幕可能不止一个
internal sealed class ZRCameraModelInstance(APlayerCameraManager playerCameraManager)
{
    // 镜头模式按优先级从高到低排，越靠前优先级越高
    private static readonly EZrCameraMode[] ModePriority = [EZrCameraMode.Sequence, EZrCameraMode.Fight];

    // 哪些模式开着。战斗镜头一直开着，别的模式都关掉后就回到它
    private readonly HashSet<EZrCameraMode> _enabledModes = [EZrCameraMode.Fight];

    public APlayerCameraManager PlayerCameraManager { get; } = playerCameraManager;

    // 当前是哪种镜头在出画面，由 ZrCameraController 切换模式时改
    public EZrCameraMode CameraMode { get; private set; } = EZrCameraMode.Fight;

    // 屏幕当前在看的相机 Actor，由 ZrCameraController.SetViewTarget 写入
    public AActor? CurrentCameraActor { get; set; }

    //序列镜头
    public ZRCSequenceCamera SequenceCamera { get; private set; } = null!;
    // 战斗镜头，在 Init 里创建
    public ZRCFightCamera FightCamera { get; private set; } = null!;
    //UI镜头等等

    /// <summary>创建这块屏幕的所有镜头。</summary>
    public void Init()
    {
        SequenceCamera = new ZRCSequenceCamera(this);
        FightCamera = new ZRCFightCamera(this);
        FightCamera.Init();
    }

    /// <summary>清理所有镜头，忘掉屏幕在看的相机。</summary>
    public void Clear()
    {
        FightCamera.Clear();
        SequenceCamera.Clear();
        CurrentCameraActor = null;
    }

    /// <summary>改当前镜头模式（只改记录；换相机由 ZrCameraController 做）。</summary>
    public void SetCameraMode(EZrCameraMode mode) => CameraMode = mode;

    /// <summary>记下这个模式开着。</summary>
    public void EnableMode(EZrCameraMode mode) => _enabledModes.Add(mode);

    /// <summary>记下这个模式关了。</summary>
    public void DisableMode(EZrCameraMode mode) => _enabledModes.Remove(mode);

    /// <summary>开着的模式里优先级最高的那个。</summary>
    public EZrCameraMode GetNextMode()
    {
        foreach (var mode in ModePriority)
        {
            if (_enabledModes.Contains(mode))
                return mode;
        }
        return EZrCameraMode.Fight;
    }

    /// <summary>当前模式比 mode 优先级高。</summary>
    public bool IsInHigherMode(EZrCameraMode mode) => GetPriority(CameraMode) > GetPriority(mode);

    private static int GetPriority(EZrCameraMode mode) => ModePriority.Length - Array.IndexOf(ModePriority, mode);
}
