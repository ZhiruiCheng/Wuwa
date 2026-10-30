using UnrealSharp.Engine;

namespace ManagedWuwa.Game.ZRCamera;

/// <summary>
/// 战斗镜头子控制器的基类。每个子控制器只管一件事（切换配置、玩家输入、镜头修改……），
/// 每帧由战斗镜头逻辑按固定顺序调 Update，只去改 Desired。
/// - 生效 = 自定义条件为真，并且没人锁着；生效状态一变就调 OnEnable / OnDisable；
/// - Update：生效时跑 UpdateInternal，不生效时跑 UpdateDeactivateInternal；
/// - 配置：子类在 OnInit 里登记"几号配置写进哪个字段"，之后按默认配置、按配置行叠加（见 ZrCameraConfigMap）。
/// </summary>
internal abstract class ZrCameraControllerBase
{
    // 锁住这个控制器的人，有人锁着就不生效
    private readonly HashSet<object> _lockers = new();
    // 子类自定义的生效条件，每帧更新一次
    private bool _customEnabled = true;

    protected ZrCameraControllerBase(ZRCFightCameraLogicComponent camera)
    {
        Camera = camera;
        Config = new ZrCameraConfigMap(GetType().Name);
        OnInit();
    }

    /// <summary>所属的战斗镜头逻辑。</summary>
    public ZRCFightCameraLogicComponent Camera { get; }

    public ZRCameraModelInstance CameraModel => Camera.CameraModel;

    /// <summary>这个控制器自己的配置（编号 → 数值）。</summary>
    protected ZrCameraConfigMap Config { get; }

    /// <summary>生效：自定义条件为真，并且没人锁着。</summary>
    public virtual bool IsActivate => _customEnabled && _lockers.Count == 0;

    /// <summary>锁住这个控制器。因此从生效变成不生效时，调 OnDisable。</summary>
    public void Lock(object locker)
    {
        bool wasActive = IsActivate;
        _lockers.Add(locker);
        if (!IsActivate && wasActive)
            OnDisable();
    }

    /// <summary>解开这个人加的锁。因此从不生效变成生效时，调 OnEnable。</summary>
    public void Unlock(object locker)
    {
        bool wasActive = IsActivate;
        _lockers.Remove(locker);
        if (IsActivate && !wasActive)
            OnEnable();
    }

    /// <summary>每帧调用：先更新自定义条件，生效状态变了就通知；然后生效时跑 UpdateInternal，不生效时跑 UpdateDeactivateInternal。</summary>
    public void Update(float deltaSeconds)
    {
        bool wasActive = IsActivate;
        _customEnabled = UpdateCustomEnableCondition();
        if (IsActivate != wasActive)
        {
            if (IsActivate) OnEnable();
            else OnDisable();
        }
        if (IsActivate) UpdateInternal(deltaSeconds);
        else UpdateDeactivateInternal(deltaSeconds);
    }

    /// <summary>设默认配置：配置行没写的编号都用它。</summary>
    public void SetDefaultConfigs(IReadOnlyDictionary<int, double> values, IReadOnlyDictionary<int, ZrCameraCurve> curves) =>
        Config.SetDefaultConfigs(values, curves);

    /// <summary>把一行配置里写了的编号叠上来。</summary>
    public void SetConfigs(IReadOnlyDictionary<int, double>? values, IReadOnlyDictionary<int, ZrCameraCurve>? curves) =>
        Config.SetConfigs(values, curves);

    /// <summary>所有编号恢复成默认配置。</summary>
    public void ResetDefaultConfig() => Config.ResetDefaultConfig();

    /// <summary>战斗镜头开始工作时调用。</summary>
    public virtual void OnStart() { }

    /// <summary>战斗镜头结束工作时调用。</summary>
    public virtual void OnEnd() { }

    /// <summary>跟随的角色从一个换成另一个时调用。</summary>
    public virtual void OnChangeRole(APawn? newPawn, APawn? oldPawn) { }

    /// <summary>构造时调用一次，子类在这里登记配置编号。</summary>
    protected virtual void OnInit() { }

    /// <summary>从不生效变成生效时调用。</summary>
    protected virtual void OnEnable() { }

    /// <summary>从生效变成不生效时调用。</summary>
    protected virtual void OnDisable() { }

    /// <summary>子类自定义的生效条件，每帧问一次。</summary>
    protected virtual bool UpdateCustomEnableCondition() => true;

    /// <summary>生效时每帧做的事。</summary>
    protected virtual void UpdateInternal(float deltaSeconds) { }

    /// <summary>不生效时每帧做的事。</summary>
    protected virtual void UpdateDeactivateInternal(float deltaSeconds) { }
}
