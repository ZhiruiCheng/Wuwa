using UnrealSharp.CoreUObject;

namespace ManagedWuwa.Game.ZRCamera;

/// <summary>
/// 一份镜头参数，纯数据。
/// 战斗镜头里有三份：Desired（这一帧想要的样子）、Current（实际用来摆相机的样子）、Last（切换配置那一刻拍下的快照）；
/// 镜头修改另外还有两份自己的快照。
/// 用字段不用属性：结构体类型的字段才能直接改分量（camera.ArmRotation.Pitch = ...）。
/// </summary>
internal sealed class ZrVirtualCamera
{
    /// <summary>跟随点偏移，角色自身坐标系（随角色朝向转）。</summary>
    public FVector ArmOffset;

    /// <summary>基础距离。实际距离 = 基础距离 × ZoomModifier，再夹在上下限内。</summary>
    public double ArmLength;
    public double MinArmLength;
    public double MaxArmLength;

    /// <summary>相对角色朝向的偏航范围（度）；-180 ~ 180 表示不限制。</summary>
    public double YawLimitMin;
    public double YawLimitMax;

    /// <summary>俯仰范围（度），最大不超过 ±89.9。</summary>
    public double PitchLimitMin;
    public double PitchLimitMax;

    /// <summary>往下 / 往上看时跟随点抬高多少：俯仰从 0 到下限 / 上限，按比例加 0 到这个值。</summary>
    public double LookDownOffsetZ;
    public double LookUpOffsetZ;

    /// <summary>镜头位置偏移，镜头自身坐标系（X 前、Y 右、Z 上），镜头退到距离之后再加。</summary>
    public FVector CameraOffset;

    /// <summary>视野角度（度），最后写到相机组件。</summary>
    public double Fov;

    /// <summary>动态视野：按角色移动速度改变视野。</summary>
    public bool EnableDynamicFov;
    /// <summary>动态视野的当前值（只在 Current 上算）。</summary>
    public double DynamicFov;
    public double DynamicFovMin;
    public double DynamicFovMax;
    /// <summary>速度区间：速度在这个区间里换算成 0 ~ 1，经曲线映射到 DynamicFovMin ~ DynamicFovMax。</summary>
    public double DynamicFovParamMin;
    public double DynamicFovParamMax;
    /// <summary>动态视野每秒最多变化多少度。</summary>
    public double DynamicFovLerpSpeed;

    /// <summary>跟随点（镜头绕着转的中心），每帧跟着角色走。</summary>
    public FVector ArmLocation;

    /// <summary>镜头的朝向。输入、镜头修改等改 Desired 的这个值；每帧最后写进控制旋转。</summary>
    public FRotator ArmRotation;

    /// <summary>缩放倍率：滚轮、镜头修改改它，乘到基础距离上。</summary>
    public double ZoomModifier = 1;

    /// <summary>世界坐标下的偏航范围（相对角色的范围之外再夹一次）。</summary>
    public double WorldYawMin;
    public double WorldYawMax;

    /// <summary>横滚范围（重力玩法用）。</summary>
    public double WorldRollMin;
    public double WorldRollMax;

    /// <summary>按距离把镜头整体抬高的范围：距离越远抬得越多。</summary>
    public double CameraOffsetFloatUpMin;
    public double CameraOffsetFloatUpMax;

    /// <summary>按角色额外加的距离 / 高度（比如体型大的角色）。</summary>
    public double CharAddArmLength;
    public double CharAddZ;

    /// <summary>
    /// 把另一份镜头参数整份抄过来。
    /// 距离、视野、缩放倍率是 0 时改成 1（后面会拿它们做除数）；俯仰范围夹在 ±89.9 以内。
    /// </summary>
    public void CopyFrom(ZrVirtualCamera source)
    {
        ArmLength = OneIfZero(source.ArmLength);
        MinArmLength = OneIfZero(source.MinArmLength);
        MaxArmLength = OneIfZero(source.MaxArmLength);
        YawLimitMin = source.YawLimitMin;
        YawLimitMax = source.YawLimitMax;
        PitchLimitMin = ZrCameraMath.GetValidPitchAngle(source.PitchLimitMin, true);
        PitchLimitMax = ZrCameraMath.GetValidPitchAngle(source.PitchLimitMax, false);
        LookDownOffsetZ = source.LookDownOffsetZ;
        LookUpOffsetZ = source.LookUpOffsetZ;
        WorldYawMin = source.WorldYawMin;
        WorldYawMax = source.WorldYawMax;
        WorldRollMin = source.WorldRollMin;
        WorldRollMax = source.WorldRollMax;
        CameraOffsetFloatUpMin = source.CameraOffsetFloatUpMin;
        CameraOffsetFloatUpMax = source.CameraOffsetFloatUpMax;
        CharAddArmLength = source.CharAddArmLength;
        CharAddZ = source.CharAddZ;
        ArmOffset = source.ArmOffset;
        CameraOffset = source.CameraOffset;
        ArmLocation = source.ArmLocation;
        ArmRotation = source.ArmRotation;
        EnableDynamicFov = source.EnableDynamicFov;
        DynamicFov = source.DynamicFov;
        DynamicFovMin = source.DynamicFovMin;
        DynamicFovMax = source.DynamicFovMax;
        DynamicFovParamMin = source.DynamicFovParamMin;
        DynamicFovParamMax = source.DynamicFovParamMax;
        DynamicFovLerpSpeed = source.DynamicFovLerpSpeed;
        Fov = source.Fov;
        if (Fov < ZrCameraMath.SmallNumber)
        {
            LogZrCamera.LogError("Fov is Zero");
            Fov = 1;
        }
        ZoomModifier = OneIfZero(source.ZoomModifier);
    }

    private static double OneIfZero(double value) => value < ZrCameraMath.SmallNumber ? 1 : value;
}
