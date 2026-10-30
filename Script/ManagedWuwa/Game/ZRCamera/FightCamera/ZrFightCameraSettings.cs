using UnrealSharp.CoreUObject;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.ZRCamera;

/// <summary>
/// 战斗镜头自己的配置数值，对应配置资产里"基础"那一组：距离和它的上下限、视野、偏移、碰撞球半径、俯仰范围。
/// 切换配置时，配置控制器按编号把数值写进来，再由 WriteTo 写进 Desired。
/// </summary>
internal sealed class ZrFightCameraSettings
{
    private double _cameraOffsetX;
    private double _cameraOffsetY;
    private double _cameraOffsetZ;
    private double _armOffsetX;
    private double _armOffsetY;
    private double _armOffsetZ;

    public double ArmLength { get; private set; }

    public double MinArmLength { get; private set; }

    public double MaxArmLength { get; private set; }

    public double Fov { get; private set; }

    /// <summary>防穿墙检测用的小球半径。</summary>
    public double CollisionProbeSize { get; private set; }

    public double PitchLimitMin { get; private set; }

    public double PitchLimitMax { get; private set; }

    /// <summary>镜头偏移，镜头自身坐标系（X 前、Y 右、Z 上）。</summary>
    public FVector CameraOffset => new(_cameraOffsetX, _cameraOffsetY, _cameraOffsetZ);

    /// <summary>跟随点偏移，角色自身坐标系。</summary>
    public FVector ArmOffset => new(_armOffsetX, _armOffsetY, _armOffsetZ);

    /// <summary>登记"几号配置写进哪个数值"，编号就是配置资产里的枚举值。</summary>
    public void Register(ZrCameraConfigMap config)
    {
        config.SetConfigMap((int)EWuwaFightCameraDefault.ArmLength, value => ArmLength = value);
        config.SetConfigMap((int)EWuwaFightCameraDefault.MinArmLength, value => MinArmLength = value);
        config.SetConfigMap((int)EWuwaFightCameraDefault.MaxArmLength, value => MaxArmLength = value);
        config.SetConfigMap((int)EWuwaFightCameraDefault.Fov, value => Fov = value);
        config.SetConfigMap((int)EWuwaFightCameraDefault.CameraOffsetX, value => _cameraOffsetX = value);
        config.SetConfigMap((int)EWuwaFightCameraDefault.CameraOffsetY, value => _cameraOffsetY = value);
        config.SetConfigMap((int)EWuwaFightCameraDefault.CameraOffsetZ, value => _cameraOffsetZ = value);
        config.SetConfigMap((int)EWuwaFightCameraDefault.CollisionProbeSize, value => CollisionProbeSize = value);
        config.SetConfigMap((int)EWuwaFightCameraDefault.PitchLimitMin, value => PitchLimitMin = value);
        config.SetConfigMap((int)EWuwaFightCameraDefault.PitchLimitMax, value => PitchLimitMax = value);
        config.SetConfigMap((int)EWuwaFightCameraDefault.ArmOffsetX, value => _armOffsetX = value);
        config.SetConfigMap((int)EWuwaFightCameraDefault.ArmOffsetY, value => _armOffsetY = value);
        config.SetConfigMap((int)EWuwaFightCameraDefault.ArmOffsetZ, value => _armOffsetZ = value);
    }

    /// <summary>把这些数值写进一份镜头参数。角度、跟随点、缩放倍率不归配置管，不动。</summary>
    public void WriteTo(ZrVirtualCamera camera)
    {
        camera.ArmLength = ArmLength;
        camera.MinArmLength = MinArmLength;
        camera.MaxArmLength = MaxArmLength;
        camera.PitchLimitMin = ZrCameraMath.GetValidPitchAngle(PitchLimitMin, true);
        camera.PitchLimitMax = ZrCameraMath.GetValidPitchAngle(PitchLimitMax, false);
        camera.CameraOffset = CameraOffset;
        camera.ArmOffset = ArmOffset;
        camera.Fov = Fov;
    }
}
