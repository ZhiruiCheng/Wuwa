using UnrealSharp.Engine;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.ZRCamera;

/// <summary>
/// 镜头修改控制器：技能动画通知触发的镜头动作，比如"拉远到 700、转到角色身后"。这里只管槽位，每一帧怎么算在 ZrCameraModifySession。
/// - 同一时间只有一段修改（一个槽位）。新来的级别不比当前低，就顶掉当前这段（不回去，新的这段从现在的样子开始淡入）；比当前低，就不生效；
/// - 正常走完：按速度回去，补齐淡出后剩下的一点点差距；
/// - 中途结束（触发它的蒙太奇停了、换人、被叫停）：按打断淡出时间，从当时的样子回去；
/// - 被配置关掉：结束这段修改，镜头停在当时的样子，不回去。
/// 回去由 ZrCameraModifyFadeOut 做。
/// </summary>
internal sealed class ZrCameraModifyController(ZRCFightCameraLogicComponent camera) : ZrCameraControllerBase(camera)
{
    public const int InvalidModifyInstance = -1;

    // ===== 配置（"Modify镜头"那一组）：按速度回去时，各项的速度 =====
    private double _armLengthLagSpeed;
    private double _cameraOffsetLagSpeed;
    private double _armRotationLagSpeed;
    private double _fovLagSpeed;

    // ===== 槽位 =====
    // 槽位里的这段修改，空 = 槽位空着
    private ZrCameraModifySession? _modify;
    // 槽位的级别。这段修改进入淡出后、或者结束后清零，之后任何新的修改都能进来
    private int _priority;
    // 最早那段修改开始前的样子："回去"的终点。中途被新的一段顶掉时不重拍，所以连着几段之后还能回到最初
    private readonly ZrVirtualCamera _origin = new();
    // 这期间玩家转过镜头（修改进行中和回去时都在记）
    private bool _playerTurned;
    // 结束后回去
    private readonly ZrCameraModifyFadeOut _fadeOut = new(camera);

    /// <summary>每开始一段修改加 1。StopCameraModify 靠它确认要停的是不是还是那一段。</summary>
    public int ModifyInstance { get; private set; }

    /// <summary>槽位里有修改。</summary>
    public bool IsModified => _modify is not null;

    /// <summary>正在回去。</summary>
    public bool IsModifyFadeOut => _fadeOut.IsActive;

    /// <summary>正在控制距离（这时战斗镜头不去夹距离，修改可以暂时超出范围）。</summary>
    public bool IsControllingArmLength => (_modify?.ControlsArmLength ?? false) || _fadeOut.IsReturningArmLength;

    /// <summary>被配置关掉时不生效；勾了"强制播放"的修改照样生效。</summary>
    public override bool IsActivate => base.IsActivate || (_modify?.Settings.ForcePlayModify ?? false);

    protected override void OnInit()
    {
        Config.SetConfigMap((int)EWuwaFightCameraModify.ArmLengthLagSpeed, value => _armLengthLagSpeed = value);
        Config.SetConfigMap((int)EWuwaFightCameraModify.CameraOffsetLagSpeed, value => _cameraOffsetLagSpeed = value);
        Config.SetConfigMap((int)EWuwaFightCameraModify.ArmRotationLagSpeed, value => _armRotationLagSpeed = value);
        Config.SetConfigMap((int)EWuwaFightCameraModify.FovLagSpeed, value => _fovLagSpeed = value);
    }

    // ===== 开始、结束 =====

    /// <summary>
    /// 开始一段修改。返回这段修改的编号；没生效（被配置关掉、级别比当前低）返回 -1。
    /// 槽位里已经有一段时，直接顶掉它，不回去；被顶掉的那段管着的距离 / 偏移 / 视野，由新的这段接手（结束时一起带回去）。
    /// </summary>
    public int ApplyCameraModify(FWuwaCameraModify settings, float duration, float blendInTime, float blendOutTime,
        float breakBlendOutTime, UAnimSequenceBase? animation, AActor? owner)
    {
        if (!base.IsActivate && !settings.ForcePlayModify)
            return InvalidModifyInstance;
        if (settings.Priority < _priority)
            return InvalidModifyInstance;
        ModifyInstance++;

        bool wasModifying = IsModified || IsModifyFadeOut;
        bool inheritArmLength = _modify?.ControlsArmLength ?? false;
        bool inheritCameraOffset = _modify?.ControlsCameraOffset ?? false;
        bool inheritFov = _modify?.ControlsFov ?? false;
        EndModify(withFadeOut: false, useTimeLerp: false);

        _priority = settings.Priority;
        if (!wasModifying)
            _origin.CopyFrom(Camera.CurrentCamera);
        _modify = new ZrCameraModifySession(Camera, _origin, settings, duration, blendInTime, blendOutTime, breakBlendOutTime,
            animation, owner, inheritArmLength, inheritCameraOffset, inheritFov);
        LockPlayerInput(_modify);
        return ModifyInstance;
    }

    /// <summary>主动叫停一段修改：动画和编号都对得上才停，按打断淡出时间回去。</summary>
    public void StopCameraModify(UAnimSequenceBase? animation, int modifyInstance)
    {
        if (_modify is not null && _modify.Animation == animation && ModifyInstance == modifyInstance)
            EndModify(withFadeOut: true, useTimeLerp: true);
    }

    /// <summary>
    /// 触发这段修改的蒙太奇已经不在播了（被别的动画顶掉，或者播完），就提前结束这段修改，按打断淡出时间回去。
    /// 每帧都要查，而且不管当前是什么镜头模式（放特写时战斗镜头不计算，但这一步照样要做），所以由战斗镜头逻辑在每帧最开始调用。
    /// </summary>
    public void CheckMontageEnd()
    {
        if (_modify is not null && _modify.IsMontageStopped)
            EndModify(withFadeOut: true, useTimeLerp: true);
    }

    /// <summary>换人了：结束这段修改，按打断淡出时间回去。勾了"换人不结束"的修改继续。</summary>
    public override void OnChangeRole(APawn? newPawn, APawn? oldPawn)
    {
        if (!(_modify?.Settings.SwitchModifier ?? false))
            EndModify(withFadeOut: true, useTimeLerp: true);
    }

    // 被配置关掉：结束这段修改，镜头停在当时的样子，不回去
    protected override void OnDisable()
    {
        if (IsModified)
            EndModify(withFadeOut: true, useTimeLerp: false);
        if (IsModifyFadeOut)
            _fadeOut.Stop();
    }

    /// <summary>
    /// 结束槽位里的这段修改，清空槽位：级别清零、解锁玩家输入。
    /// withFadeOut 为真时还要回去：useTimeLerp 为真按打断淡出时间回去，为假按速度回去。
    /// </summary>
    private void EndModify(bool withFadeOut, bool useTimeLerp)
    {
        if (_modify is null)
            return;
        var modify = _modify;
        _priority = 0;
        UnlockAxisInput(modify);
        if (withFadeOut)
            StartFadeOut(modify, useTimeLerp);
        else
            _fadeOut.Stop();
        _playerTurned = false;
        _modify = null;
        Camera.InputController.Unlock(this);
    }

    // 记下回去的计划：每一项从这一刻的 Current 回到"结束后的值"。玩家转过镜头（又没锁输入）的话，角度就不回去了
    private void StartFadeOut(ZrCameraModifySession modify, bool useTimeLerp)
    {
        var current = Camera.CurrentCamera;
        var final = modify.FinalRotation();
        bool stillControlsRotation = modify.Settings.LockInput || !_playerTurned;
        _fadeOut.Start(modify.BreakBlendOutTime, modify.BlendOutCurve, useTimeLerp,
            armLength: modify.ControlsArmLength ? (Camera.GetArmLengthWithSettingAndZoom(current, clamp: false), modify.FinalArmLength()) : null,
            cameraOffsetFrom: modify.ControlsCameraOffset ? current.CameraOffset : null,
            pitch: modify.ControlsPitch && stillControlsRotation ? (current.ArmRotation.Pitch, final.Pitch) : null,
            yaw: modify.ControlsYaw && stillControlsRotation ? (current.ArmRotation.Yaw, final.Yaw) : null,
            fov: modify.ControlsFov ? (current.Fov, Camera.Settings.Fov ) : null);
    }

    // ===== 每帧 =====

    protected override void UpdateInternal(float deltaSeconds)
    {
        UpdateFadeOut(deltaSeconds);
        UpdateModify(deltaSeconds);
    }

    // 被配置关掉期间，只让回去接着走
    protected override void UpdateDeactivateInternal(float deltaSeconds) => UpdateFadeOut(deltaSeconds);

    private void UpdateFadeOut(double deltaSeconds)
    {
        if (!_fadeOut.IsActive)
            return;
        DetectPlayerTurn();
        _fadeOut.Update(deltaSeconds, _playerTurned, _armLengthLagSpeed, _cameraOffsetLagSpeed, _armRotationLagSpeed, _fovLagSpeed);
    }

    // 让槽位里的这段修改走一帧；进入淡出就把槽位级别清零；三段都走完就结束，按速度回去
    private void UpdateModify(double deltaSeconds)
    {
        if (_modify is null)
            return;
        DetectPlayerTurn();
        _modify.Update(deltaSeconds, _playerTurned);
        if (_modify.IsBlendingOut)
            _priority = 0;
        if (_modify.IsFinished)
            EndModify(withFadeOut: true, useTimeLerp: false);
    }

    // 这期间玩家转过镜头没有
    private void DetectPlayerTurn()
    {
        var look = Camera.LookInput;
        if (Math.Abs(look.X) > ZrCameraMath.KindaSmallNumber || Math.Abs(look.Y) > ZrCameraMath.KindaSmallNumber)
            _playerTurned = true;
    }

    // ===== 玩家输入 =====

    // 按设置锁住玩家输入："覆盖输入"只锁这段修改管着的那几项，"锁输入"把整个输入控制器锁住。要改角度时清掉输入的缓冲
    private void LockPlayerInput(ZrCameraModifySession modify)
    {
        var input = Camera.InputController;
        var settings = modify.Settings;
        if (settings.OverrideCameraInput)
        {
            if (modify.ControlsPitch) input.LockArmRotationPitch(this);
            if (modify.ControlsYaw) input.LockArmRotationYaw(this);
            if (modify.ControlsArmLength) input.LockArmLength(this);
        }
        if (settings.LockInput)
            input.Lock(this);
        if (modify.ControlsPitch || modify.ControlsYaw)
            input.ResetCameraInput();
    }

    // 解开"覆盖输入"锁住的那几项（整个输入控制器的锁在 EndModify 最后解开）
    private void UnlockAxisInput(ZrCameraModifySession modify)
    {
        if (!modify.Settings.OverrideCameraInput)
            return;
        var input = Camera.InputController;
        if (modify.ControlsPitch) input.UnlockArmRotationPitch(this);
        if (modify.ControlsYaw) input.UnlockArmRotationYaw(this);
        if (modify.ControlsArmLength) input.UnlockArmLength(this);
    }
}
