namespace ManagedWuwa.Game.ZRCamera;

/// <summary>
/// 一张"配置编号 → 字段"的登记表，战斗镜头逻辑和每个子控制器各有一张。
/// - 启动时登记"几号配置写进哪个字段"（SetConfigMap / SetCurveConfigMap）；
/// - 存一份默认配置（SetDefaultConfigs）；切配置时先 ResetDefaultConfig，再按行叠 SetConfigs；
/// - 表里出现没登记的编号就跳过；默认配置里缺了某条曲线，就报错并用线性曲线。
/// </summary>
internal sealed class ZrCameraConfigMap(string ownerName)
{
    private readonly Dictionary<int, Action<double>> _setters = new();
    private readonly Dictionary<int, Action<ZrCameraCurve>> _curveSetters = new();
    private IReadOnlyDictionary<int, double> _defaults = new Dictionary<int, double>();
    private IReadOnlyDictionary<int, ZrCameraCurve> _defaultCurves = new Dictionary<int, ZrCameraCurve>();
    // 缺的曲线只报一次（每次切配置都会重置一遍默认配置）
    private readonly HashSet<int> _reportedMissingCurves = new();

    /// <summary>登记：几号配置写进哪个数值字段。</summary>
    public void SetConfigMap(int key, Action<double> setter)
    {
        if (!_setters.TryAdd(key, setter))
            LogZrCamera.LogError($"{ownerName}: 重复注册了Key {key}");
    }

    /// <summary>登记：几号配置写进哪个曲线字段。</summary>
    public void SetCurveConfigMap(int key, Action<ZrCameraCurve> setter)
    {
        if (!_curveSetters.TryAdd(key, setter))
            LogZrCamera.LogError($"{ownerName}: 重复注册了曲线Key {key}");
    }

    /// <summary>存一份默认配置。</summary>
    public void SetDefaultConfigs(IReadOnlyDictionary<int, double> values, IReadOnlyDictionary<int, ZrCameraCurve> curves)
    {
        _defaults = values;
        _defaultCurves = curves;
    }

    /// <summary>把这些编号的值写进登记过的字段，没登记的编号跳过。</summary>
    public void SetConfigs(IReadOnlyDictionary<int, double>? values, IReadOnlyDictionary<int, ZrCameraCurve>? curves)
    {
        if (values is not null)
        {
            foreach (var (key, value) in values)
            {
                if (_setters.TryGetValue(key, out var setter))
                    setter(value);
            }
        }
        if (curves is not null)
        {
            foreach (var (key, curve) in curves)
            {
                if (_curveSetters.TryGetValue(key, out var setter))
                    setter(curve);
            }
        }
    }

    /// <summary>所有字段恢复成默认配置；默认配置里缺的曲线报错并用线性曲线。</summary>
    public void ResetDefaultConfig()
    {
        SetConfigs(_defaults, _defaultCurves);
        foreach (var (key, setter) in _curveSetters)
        {
            if (_defaultCurves.ContainsKey(key))
                continue;
            if (_reportedMissingCurves.Add(key))
                LogZrCamera.LogError($"{ownerName}: 缺少曲线配置 {key}，使用线性曲线");
            setter(ZrCameraCurve.Linear);
        }
    }
}
