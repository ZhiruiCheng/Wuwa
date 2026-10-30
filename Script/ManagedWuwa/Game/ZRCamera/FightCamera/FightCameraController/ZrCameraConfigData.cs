using UnrealSharp.CoreUObject;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.ZRCamera;

/// <summary>
/// 读战斗镜头配置资产，并把资产里"枚举 → 数值"的表换成控制器登记用的"编号 → 数值"。
/// </summary>
internal static class ZrCameraConfigData
{
    //加载总表
    public const string ConfigPath = "/Game/Game/Camera/DA_FightCameraConfig.DA_FightCameraConfig";

    public static UWuwaFightCameraConfig Load()
    {
        try
        {
            // 资产不存在时 StaticLoadObject 会抛异常
            var config = UObject.StaticLoadObject<UWuwaFightCameraConfig>(ConfigPath);
            if (config.IsValid())
                return config;
        }
        catch (InvalidOperationException)
        {
        }
        LogZrCamera.LogError($"战斗镜头配置加载失败：{ConfigPath}（运行 WuwaCameraSetup -Apply 创建）；先用类默认值");
        return UObject.GetDefault<UWuwaFightCameraConfig>();
    }

    /// <summary>"枚举 → 数值"的表换成"编号 → 数值"。</summary>
    public static IReadOnlyDictionary<int, double> ToMap<TKey>(IReadOnlyDictionary<TKey, float>? source) where TKey : struct, Enum
    {
        var result = new Dictionary<int, double>();
        if (source is null)
            return result;
        foreach (var (key, value) in source)
            result[Convert.ToInt32(key)] = value;
        return result;
    }

    /// <summary>"枚举 → 曲线"的表换成"编号 → 曲线"。</summary>
    public static IReadOnlyDictionary<int, ZrCameraCurve> ToCurveMap<TKey>(IReadOnlyDictionary<TKey, FWuwaCameraCurve>? source) where TKey : struct, Enum
    {
        var result = new Dictionary<int, ZrCameraCurve>();
        if (source is null)
            return result;
        foreach (var (key, value) in source)
            result[Convert.ToInt32(key)] = ZrCameraCurve.Create(value);
        return result;
    }
}
