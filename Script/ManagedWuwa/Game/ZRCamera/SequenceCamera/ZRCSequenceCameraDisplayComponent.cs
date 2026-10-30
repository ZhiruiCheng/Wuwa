using UnrealSharp.Engine;

namespace ManagedWuwa.Game.ZRCamera;

/// <summary>
/// 序列镜头的显示组件：记着正在播的那段序列生成的相机，序列镜头模式下屏幕看的就是它；序列停了就清空。
/// </summary>
internal sealed class ZRCSequenceCameraDisplayComponent
{
    public ACameraActor? CineCamera { get; private set; }

    /// <summary>记下这段序列生成的相机。</summary>
    public void SetCineCamera(ACameraActor? camera) => CineCamera = camera;

    public void Clear() => CineCamera = null;
}
