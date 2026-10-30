namespace ManagedWuwa.Game.ZRCamera;

/// <summary>
/// 镜头模式：同一时间屏幕只看其中一种镜头的相机。
/// 优先级：序列 > 战斗。战斗镜头是兜底，不主动进出，别的模式都退出后自动回到它。
/// </summary>
internal enum EZrCameraMode
{
    Fight = 0, //战斗镜头
    Sequence = 1 //序列镜头
}
