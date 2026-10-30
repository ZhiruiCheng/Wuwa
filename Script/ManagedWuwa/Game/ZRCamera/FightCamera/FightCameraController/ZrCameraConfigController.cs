using UnrealSharp.CoreUObject;
using UnrealSharp.GameplayTags;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.ZRCamera;

/// <summary>
/// 配置控制器：管"现在用哪几行配置"，并把它们叠到战斗镜头和各子控制器上。
/// - 基础镜头行一直生效；子镜头行在跟随的角色带上它的标签时生效（比如瞄准），标签没了就撤掉；
/// - 生效的行按优先级从低到高排着。有变化时：先全部恢复默认配置，再从"重置默认配置"的那一行开始往上逐行叠，
///   然后把配置值写进 Desired，并按这次加入 / 撤掉的行的淡入 / 淡出时间做过渡；
/// - 某一行关掉了"启用镜头修改"，这一行生效期间锁住镜头修改控制器。
/// </summary>
internal sealed class ZrCameraConfigController(ZRCFightCameraLogicComponent camera) : ZrCameraControllerBase(camera)
{
    // 过渡时间上限，也是"这一轮还没有行要过渡"时的初值
    private const double DefaultMaxFadeTime = 10.0;

    /// <summary>配置表里的一行，数值已经换成控制器用的"编号 → 数值"。</summary>
    private sealed class ConfigRow(FWuwaFightCameraConfigRow row)
    {
        public readonly EWuwaFightCameraConfigType Type = row.Type;
        public readonly FGameplayTag Tag = row.Tag;
        public readonly int Priority = row.Priority;
        public readonly double FadeInTime = row.FadeInTime;
        public readonly ZrCameraCurve FadeInCurve = ZrCameraCurve.Create(row.FadeInCurve);
        public readonly double FadeOutTime = row.FadeOutTime;
        public readonly ZrCameraCurve FadeOutCurve = ZrCameraCurve.Create(row.FadeOutCurve);
        public readonly bool EnableModifyCamera = row.EnableModifyCamera;
        public readonly bool IsResetDefaultConfig = row.ResetDefaultConfig;
        public readonly bool IsResetCameraLock = row.ResetCameraLock;
        public readonly bool IsUniqueFade = row.UniqueFade;
        public readonly IReadOnlyDictionary<int, double> BaseConfig = ZrCameraConfigData.ToMap(row.Base);
        public readonly IReadOnlyDictionary<int, double> InputConfig = ZrCameraConfigData.ToMap(row.Input);
        public readonly IReadOnlyDictionary<int, ZrCameraCurve> InputCurveConfig = ZrCameraConfigData.ToCurveMap(row.InputCurves);
        public readonly IReadOnlyDictionary<int, double> ModifyConfig = ZrCameraConfigData.ToMap(row.ModifyCamera);
    }

    // ===== 配置行 =====
    // 基础镜头行
    private ConfigRow? _baseConfig;
    // 子镜头行，按标签查
    private readonly Dictionary<FGameplayTag, ConfigRow> _subConfigs = new();
    // 已经生效的子镜头行的标签
    private readonly HashSet<FGameplayTag> _activeSubTags = new();
    // 生效的行，按优先级从低到高
    private readonly List<ConfigRow> _activeConfigs = new();

    // ===== 这一轮的变化 =====
    // 有行加入：记下最短的淡入时间和它的曲线
    private bool _hasFadeIn;
    private double _fadeInTime = DefaultMaxFadeTime;
    private ZrCameraCurve _fadeInCurve = ZrCameraCurve.Linear;
    // 有行撤掉：记下最短的淡出时间和它的曲线
    private bool _hasFadeOut;
    private double _fadeOutTime = DefaultMaxFadeTime;
    private ZrCameraCurve _fadeOutCurve = ZrCameraCurve.Linear;
    // 这一轮的过渡用"独立过渡时间"
    private bool _isUniqueFade;
    // 有变化，要重新叠配置
    private bool _dirty;
    // 镜头修改控制器现在没被配置锁着
    private bool _modifyCameraEnabled = true;

    /// <summary>读配置列表：分出基础镜头行和子镜头行，基础镜头行先生效。</summary>
    public void LoadConfig(UWuwaFightCameraConfig? config)
    {
        _baseConfig = null;
        _subConfigs.Clear();
        _activeSubTags.Clear();
        _activeConfigs.Clear();
        var table = config?.ConfigList;
        if (table is not null && table.IsValid())
        {
            table.ForEachRow<FWuwaFightCameraConfigRow>((rowName, row) =>
            {
                var configRow = new ConfigRow(row);
                if (configRow.Type == EWuwaFightCameraConfigType.Base)
                    _baseConfig = configRow;
                else if (!configRow.Tag.IsValid)
                    LogZrCamera.LogError($"初始化镜头配置失败，子镜头 {rowName} 没有正确配置Tag");
                else if (!_subConfigs.TryAdd(configRow.Tag, configRow))
                    LogZrCamera.LogError($"子镜头配置不允许重复的Tag {configRow.Tag}");
            });
        }
        if (_baseConfig is null)
        {
            LogZrCamera.LogError("初始化镜头配置失败，基础镜头未配置（DA_FightCameraConfig.ConfigList）；先只用默认配置");
            _baseConfig = new ConfigRow(new FWuwaFightCameraConfigRow { Type = EWuwaFightCameraConfigType.Base, ResetDefaultConfig = true, EnableModifyCamera = true });
        }
        InsertConfig(_baseConfig);
        OnConfigAdded(_baseConfig);
        _dirty = true;
        UpdateConfig();
    }

    protected override void UpdateInternal(float deltaSeconds) => UpdateConfig();

    // 对照一遍角色身上的标签，有变化就重新叠配置
    private void UpdateConfig()
    {
        if (Camera.Character is null)
            return;
        SyncSelfTags();
        if (!_dirty)
            return;
        ApplyConfigs();
        _dirty = false;
    }

    // 角色身上新带上的标签，对应的子镜头行加进来；没了的标签，对应的行撤掉
    private void SyncSelfTags()
    {
        foreach (var (tag, row) in _subConfigs)
        {
            bool hasTag = Camera.ContainsTag(tag);
            if (hasTag && _activeSubTags.Add(tag))
            {
                InsertConfig(row);
                OnConfigAdded(row);
            }
            else if (!hasTag && _activeSubTags.Remove(tag))
            {
                _activeConfigs.Remove(row);
                OnConfigRemoved(row);
            }
        }
    }

    // 插入[ 基础(0), 游泳(5), 冲刺(10) ]按照这样的顺序,方便后面做覆盖
    private void InsertConfig(ConfigRow row)
    {
        int index = _activeConfigs.FindLastIndex(existing => existing.Priority <= row.Priority) + 1;
        _activeConfigs.Insert(index, row);
    }

    // 记下这一轮加入的行里最短的淡入。这一轮或正在进行的过渡用的是独立过渡时间时，普通的行不参与
    private void OnConfigAdded(ConfigRow row)
    {
        double time = Math.Min(row.FadeInTime, DefaultMaxFadeTime);
        if (time > _fadeInTime)
            return;
        if ((_isUniqueFade || (Camera.Fade.IsFading && Camera.Fade.IsUniqueFade)) && !row.IsUniqueFade)
            return;
        _hasFadeIn = true;
        _dirty = true;
        _isUniqueFade = row.IsUniqueFade;
        _fadeInTime = time;
        _fadeInCurve = row.FadeInCurve;
    }

    // 记下这一轮撤掉的行里最短的淡出。规则同上
    private void OnConfigRemoved(ConfigRow row)
    {
        double time = Math.Min(row.FadeOutTime, DefaultMaxFadeTime);
        if (time > _fadeOutTime)
            return;
        if ((_isUniqueFade || (Camera.Fade.IsFading && Camera.Fade.IsUniqueFade)) && !row.IsUniqueFade)
            return;
        _hasFadeOut = true;
        _dirty = true;
        _isUniqueFade = row.IsUniqueFade;
        _fadeOutTime = time;
        _fadeOutCurve = row.FadeOutCurve;
    }

    // 恢复默认 → 逐行叠配置和锁 → 写进 Desired → 开始过渡
    private void ApplyConfigs()
    {
        UnlockControllers();
        ResetDefaultConfigs();
        // 从优先级最高的往下找：第一行"重置默认配置"和第一行"重置镜头锁定"
        int resetDefaultIndex = 0, resetLockIndex = 0;
        bool foundResetDefault = false, foundResetLock = false;
        for (int index = _activeConfigs.Count - 1; index >= 0; index--)
        {
            var row = _activeConfigs[index];
            if (!foundResetDefault && row.IsResetDefaultConfig)
            {
                resetDefaultIndex = index;
                foundResetDefault = true;
            }
            if (!foundResetLock && row.IsResetCameraLock)
            {
                resetLockIndex = index;
                foundResetLock = true;
            }
            if (foundResetDefault && foundResetLock)
                break;
        }
        // 配置从"重置默认配置"那行往上叠，锁从"重置镜头锁定"那行往上算
        for (int index = Math.Min(resetDefaultIndex, resetLockIndex); index < _activeConfigs.Count; index++)
        {
            var row = _activeConfigs[index];
            if (index >= resetDefaultIndex)
                ApplyRowConfigs(row);
            if (index >= resetLockIndex)
                ApplyRowLocks(row);
        }
        Camera.ApplyConfig();
        if (Camera.Initialized)
        {
            // 同一轮里既有加入又有撤掉时，用短的那个过渡
            bool useFadeIn = _hasFadeIn && _hasFadeOut ? _fadeInTime <= _fadeOutTime : _hasFadeIn;
            Camera.StartFade(useFadeIn ? _fadeInTime : _fadeOutTime, useFadeIn ? _fadeInCurve : _fadeOutCurve,
                fadeArmLength: true, fadeArmOffset: true, fadeCameraOffset: true, fadeFov: true, _isUniqueFade);
        }
        _isUniqueFade = false;
        _hasFadeIn = false;
        _hasFadeOut = false;
        _fadeInTime = DefaultMaxFadeTime;
        _fadeOutTime = DefaultMaxFadeTime;
    }

    // 上一轮被配置锁住的控制器先解锁
    private void UnlockControllers()
    {
        if (!_modifyCameraEnabled)
            Camera.ModifyController.Unlock(this);
        _modifyCameraEnabled = true;
    }

    // 战斗镜头和各子控制器都回到默认配置
    private void ResetDefaultConfigs()
    {
        Camera.ResetDefaultConfig();
        Camera.InputController.ResetDefaultConfig();
        Camera.ModifyController.ResetDefaultConfig();
    }

    // 把这一行写了的编号叠上去
    private void ApplyRowConfigs(ConfigRow row)
    {
        Camera.SetConfigs(row.BaseConfig);
        Camera.InputController.SetConfigs(row.InputConfig, row.InputCurveConfig);
        Camera.ModifyController.SetConfigs(row.ModifyConfig, null);
    }

    // 这一行关掉了镜头修改，就锁住镜头修改控制器
    private void ApplyRowLocks(ConfigRow row)
    {
        if (row.EnableModifyCamera)
            return;
        Camera.ModifyController.Lock(this);
        _modifyCameraEnabled = false;
    }
}
