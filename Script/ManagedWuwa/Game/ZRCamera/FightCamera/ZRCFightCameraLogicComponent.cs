using ManagedWuwa.Game.ZRCamera.FightCamera;
using UnrealSharp.CoreUObject;
using UnrealSharp.Engine;
using UnrealSharp.GameplayTags;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.ZRCamera;

/// <summary>
/// 战斗镜头的逻辑：每帧算出战斗相机该摆在哪、朝哪看、视野多大。
/// - 三份镜头参数：Desired（这一帧想要的）、Current（实际用来摆相机的）、Last（切换配置那一刻的快照）。
/// - 三个子控制器只改 Desired：配置控制器（按角色身上的标签切换配置）、镜头修改（技能动画里的镜头动作）、输入控制器（玩家转镜头和滚轮）。
/// - Current 每帧跟上 Desired（切换配置时慢慢追），再按 Current 摆相机，最后把镜头朝向写进控制旋转。
/// 每帧具体做什么、按什么顺序，看 Tick。
/// </summary>
internal sealed class ZRCFightCameraLogicComponent
{
    // 玩家设置里的"镜头距离"：0 = 用基础距离，1 = 用最远距离。项目还没有这个设置，固定为 0
    private const double ArmLengthSettingPercentage = 0.0;

    //同级组件
    private readonly ZRCFightCameraDisplayComponent _display;
    
    //工具组件
    private readonly ZRCCameraCollision _collision = new();
    
    private readonly ZrCameraConfigMap _configMap = new(nameof(ZRCFightCameraLogicComponent));
    private readonly ZrFightCameraSettings _settings = new();
    //过渡专用
    private readonly ZrCameraFade _fade = new();
    //玩家复制体里面包含了镜头关键信息
    private readonly ZrCameraFollowTarget _target = new();
    
    // 每帧先更新配置控制器，再按这个顺序更新其余控制器：镜头修改在输入前面
    private readonly ZrCameraControllerBase[] _configControllers;
    private readonly ZrCameraControllerBase[] _controllers;
    // 第一次有角色时，镜头朝向要从出生朝向开始
    private bool _rotationInitialized;
    // 这一帧实际用的镜头距离
    private double _armLengthThisFrame;

    public ZRCFightCameraLogicComponent(ZRCameraModelInstance cameraModel, ZRCFightCameraDisplayComponent display)
    {
        CameraModel = cameraModel;
        _display = display;
        _settings.Register(_configMap);
        ConfigController = new ZrCameraConfigController(this);
        ModifyController = new ZrCameraModifyController(this);
        InputController = new ZrCameraInputController(this);
        _configControllers = [ConfigController];
        _controllers = [ModifyController, InputController];
    }

    public ZRCameraModelInstance CameraModel { get; }

    // ===== 镜头参数 =====

    public ZrVirtualCamera DesiredCamera { get; } = new();

    public ZrVirtualCamera CurrentCamera { get; } = new();

    public ZrVirtualCamera LastCamera { get; } = new();

    /// <summary>这一帧哪些项已经被控制器改过。</summary>
    public ZrCameraModifiedFlags Modified { get; } = new();

    // ===== 子控制器 =====

    public ZrCameraConfigController ConfigController { get; }

    public ZrCameraModifyController ModifyController { get; }

    public ZrCameraInputController InputController { get; }

    // ===== 配置对外接口 =====
    public ZrFightCameraSettings  Settings => _settings;
    
    /// <summary>正在做切换配置的过渡。</summary>
    ///
    public ZrCameraFade Fade => _fade;
    
    /// <summary>已经套过一次配置。第一次套配置时 Current 直接等于 Desired，不过渡。</summary>
    public bool Initialized { get; private set; }

    // ===== 跟随的角色 =====

    /// <summary>跟随的角色，没有时为空。</summary>
    public APawn? Character => _target.Pawn;

    /// <summary>这一帧角色的朝向。</summary>
    public FRotator PlayerRotator { get; private set; }

    /// <summary>这一帧镜头跟随的点。</summary>
    public FVector PlayerLocation { get; private set; }

    // ===== 这一帧的输入和结果 =====

    /// <summary>这一帧的转镜头输入（相机管理器攒好交过来的鼠标量）。</summary>
    public FVector2D LookInput { get; private set; }

    /// <summary>这一帧的滚轮输入（格数）。</summary>
    public double ZoomInput { get; private set; }

    /// <summary>最后摆出来的相机位置。</summary>
    public FVector CameraLocation { get; private set; }

    /// <summary>最后摆出来的相机朝向。</summary>
    public FRotator CameraRotation { get; private set; }

    // ===== 开始、结束、换角色 =====

    /// <summary>读配置资产，把默认配置交给自己和各子控制器，然后让各子控制器开始工作。</summary>
    public void Init()
    {
        var config = ZrCameraConfigData.Load();
        _configMap.SetDefaultConfigs(ZrCameraConfigData.ToMap(config.Base), new Dictionary<int, ZrCameraCurve>());
        InputController.SetDefaultConfigs(ZrCameraConfigData.ToMap(config.Input), ZrCameraConfigData.ToCurveMap(config.InputCurves));
        ModifyController.SetDefaultConfigs(ZrCameraConfigData.ToMap(config.ModifyCamera), new Dictionary<int, ZrCameraCurve>());
        
        ConfigController.LoadConfig(config);
        foreach (var controller in _configControllers) controller.OnStart();
        foreach (var controller in _controllers) controller.OnStart();
    }

    /// <summary>让各子控制器结束工作，并忘掉跟随的角色。</summary>
    public void Clear()
    {
        foreach (var controller in _configControllers) controller.OnEnd();
        foreach (var controller in _controllers) controller.OnEnd();
        _target.Set(null, null);
    }

    /// <summary>
    /// 换跟随的角色。第一次有角色时，镜头朝向从控制旋转（出生朝向）开始。
    /// 从一个角色换到另一个角色时，通知各子控制器（正在进行的镜头修改会在这时结束）；镜头角度和缩放保持不变。
    /// </summary>
    public void SetPawn(APawn? pawn)
    {
        if (pawn is not null && !pawn.IsValid())
            pawn = null;
        var oldPawn = _target.Pawn;
        _target.Set(pawn, CameraModel.PlayerCameraManager.OwningPlayerController);
        if (pawn is null)
            return;
        if (!_rotationInitialized)
        {
            SetRotation(_target.Controller?.ControlRotation ?? pawn.ActorRotation);
            _rotationInitialized = true;
        }
        if (oldPawn is not null && oldPawn != pawn)
        {
            foreach (var controller in _configControllers) controller.OnChangeRole(pawn, oldPawn);
            foreach (var controller in _controllers) controller.OnChangeRole(pawn, oldPawn);
        }
    }

    /// <summary>跟随的角色身上有没有这个标签。</summary>
    public bool ContainsTag(FGameplayTag tag) => _target.ContainsTag(tag);

    // ===== 每帧 =====

    /// <summary>每帧由相机系统调用，算出这一帧的战斗镜头。当前不是战斗镜头模式时（比如在放特写），只做第一步。</summary>
    public void Tick(float deltaSeconds, FVector2D lookInput, float zoomInput)
    {
        LookInput = lookInput;
        ZoomInput = zoomInput;
        // 触发镜头修改的动画被顶掉了，要马上结束那段修改。这一步不管当前是什么镜头模式都要做
        ModifyController.CheckMontageEnd();
        var cameraActor = _display.CameraActor;
        if (!_target.IsValid || cameraActor is null || !cameraActor.IsValid())
            return;
        if (CameraModel.CameraMode != EZrCameraMode.Fight)
            return;

        Modified.Clear();
        UpdateControllers(_configControllers, deltaSeconds);
        _fade.UpdateArmLength(deltaSeconds, CurrentCamera, LastCamera, DesiredCamera, Modified.ArmLength);
        PlayerRotator = _target.Pawn!.ActorRotation;
        PlayerLocation = _target.GetFollowLocation();
        UpdateControllers(_controllers, deltaSeconds);
        ClampArmLength();
        FollowCharacter();
        _armLengthThisFrame = GetArmLengthThisFrame();
        ClampPitch();
        ClearRoll();
        _fade.UpdateCurrent(CurrentCamera, LastCamera, DesiredCamera, Modified.Fov);
        PlaceCamera(cameraActor);
        ApplyFov(cameraActor);
        _target.SetControlRotation(CurrentCamera.ArmRotation);
    }

    /// <summary>马上完整算一次（时间按 0 算）。特写结束时调用，让混回战斗镜头的终点是按新朝向摆好的位置。</summary>
    public void ForceTickOutSide() => Tick(0, new FVector2D(0, 0), 0);

    private static void UpdateControllers(ZrCameraControllerBase[] controllers, float deltaSeconds)
    {
        foreach (var controller in controllers)
            controller.Update(deltaSeconds);
    }

    // 距离夹在上下限内。镜头修改控制着距离时不夹，它可以暂时超出范围
    private void ClampArmLength()
    {
        if (!ModifyController.IsControllingArmLength)
            DesiredCamera.ArmLength = ZrCameraMath.Clamp(DesiredCamera.ArmLength, DesiredCamera.MinArmLength, DesiredCamera.MaxArmLength);
    }

    // 跟随点等于角色这一帧的跟随点（这一帧没被别的控制器改过时）
    private void FollowCharacter()
    {
        if (!Modified.ArmLocation)
            DesiredCamera.ArmLocation = PlayerLocation;
    }

    // 这一帧实际用的距离：用的是上一帧的 Current。镜头修改进行中或正在回去时，不夹上下限
    private double GetArmLengthThisFrame()
    {
        bool modifying = ModifyController.IsModified || ModifyController.IsModifyFadeOut;
        return GetArmLengthWithSettingAndZoom(CurrentCamera, clamp: !modifying) + CurrentCamera.CharAddArmLength;
    }

    // 俯仰夹在 Current 的俯仰范围内
    private void ClampPitch()
    {
        double pitch = ZrCameraMath.WrapAngle(DesiredCamera.ArmRotation.Pitch);
        DesiredCamera.ArmRotation.Pitch = ZrCameraMath.Clamp(pitch, CurrentCamera.PitchLimitMin, CurrentCamera.PitchLimitMax);
    }

    // 横滚清零，战斗镜头不歪（比如特写结束时接过来的朝向可能是歪的）
    private void ClearRoll() => DesiredCamera.ArmRotation.Roll = 0;

    // 摆相机：跟随点 + 跟随点偏移（随角色朝向转）→ 沿镜头朝向往后退这一帧的距离 → 加镜头偏移（随镜头朝向转）→ 防穿墙检测
    private void PlaceCamera(ACameraActor cameraActor)
    {
        var current = CurrentCamera;
        var rotation = current.ArmRotation;
        var pawn = _target.Pawn!;
        var pivot = ZrCameraMath.Add(current.ArmLocation, ZrCameraMath.RotateVector(pawn.ActorRotation, current.ArmOffset));
        pivot = ZrCameraMath.Add(pivot, new FVector(0, 0, current.CharAddZ));
        var forward = ZrCameraMath.RotateVector(rotation, new FVector(1, 0, 0));
        var location = ZrCameraMath.Subtract(pivot, ZrCameraMath.Scale(forward, _armLengthThisFrame));
        location = ZrCameraMath.Add(location, ZrCameraMath.RotateVector(rotation, current.CameraOffset));
        location = _collision.CheckCollision(pawn, PlayerLocation, location, (float)_settings.CollisionProbeSize, true);
        ApplyRotation(rotation);
        if (!ZrCameraMath.IsFinite(location))
        {
            LogZrCamera.LogError($"CameraLocation Contains NaN: {location}");
            return;
        }
        CameraLocation = location;
        cameraActor.SetActorLocationAndRotation(location, rotation, false, out _, false);
    }

    // 视野有变化才写给相机组件
    private void ApplyFov(ACameraActor cameraActor)
    {
        var component = cameraActor.CameraComponent;
        if (Math.Abs(component.FieldOfView - CurrentCamera.Fov) > ZrCameraMath.KindaSmallNumber)
            component.FieldOfView = (float)CurrentCamera.Fov;
    }

    // ===== 镜头朝向 =====

    /// <summary>直接把镜头转到这个朝向：Desired 和 Current 一起改，不经过插值。特写结束时用它接过特写最后的朝向。</summary>
    public void SetRotation(FRotator rotation)
    {
        if (!ZrCameraMath.IsFinite(rotation))
        {
            LogZrCamera.LogError($"rotation is invalid: {rotation}");
            return;
        }
        DesiredCamera.ArmRotation = rotation;
        CurrentCamera.ArmRotation = rotation;
        ApplyRotation(rotation);
    }

    // 记下镜头朝向，并写进控制旋转
    private void ApplyRotation(FRotator rotation)
    {
        CameraRotation = rotation;
        _target.SetControlRotation(rotation);
    }

    // ===== 配置 =====

    /// <summary>把一行配置里写了的编号叠上来。</summary>
    public void SetConfigs(IReadOnlyDictionary<int, double>? values) => _configMap.SetConfigs(values, null);

    /// <summary>所有编号恢复成默认配置。</summary>
    public void ResetDefaultConfig() => _configMap.ResetDefaultConfig();

    /// <summary>把当前配置的数值写进 Desired（不含角度）。第一次写时 Current 直接等于 Desired。</summary>
    public void ApplyConfig()
    {
        _settings.WriteTo(DesiredCamera);
        if (Initialized)
            return;
        CurrentCamera.CopyFrom(DesiredCamera);
        Initialized = true;
    }

    /// <summary>开始一次切换配置的过渡：先把 Current 拍成快照放进 Last，之后 duration 秒内 Current 从快照追到 Desired。</summary>
    public void StartFade(double duration, ZrCameraCurve curve, bool fadeArmLength, bool fadeArmOffset, bool fadeCameraOffset, bool fadeFov, bool uniqueFade)
    {
        CurrentCamera.ArmRotation.Pitch = ZrCameraMath.WrapAngle(CurrentCamera.ArmRotation.Pitch);
        LastCamera.CopyFrom(CurrentCamera);
        _fade.Start(duration, curve, fadeArmLength, fadeArmOffset, fadeCameraOffset, fadeFov, uniqueFade);
    }

    // ===== 镜头距离 =====

    /// <summary>按玩家设置的"镜头距离"，在基础距离和最远距离之间取一个距离。</summary>
    public double GetArmLengthWithSetting(ZrVirtualCamera camera) =>
        ZrCameraMath.Lerp(camera.ArmLength, camera.MaxArmLength, ArmLengthSettingPercentage);

    /// <summary>再乘上缩放倍率，就是实际距离；默认夹在上下限内。</summary>
    public double GetArmLengthWithSettingAndZoom(ZrVirtualCamera camera, bool clamp = true)
    {
        double length = GetArmLengthWithSetting(camera) * camera.ZoomModifier;
        return clamp ? ZrCameraMath.Clamp(length, camera.MinArmLength, camera.MaxArmLength) : length;
    }

    // ===== 镜头修改 =====

    /// <summary>开始一段镜头修改（技能动画通知调用）。返回这段修改的编号，没生效返回 -1。</summary>
    public int ApplyCameraModify(FWuwaCameraModify settings, float duration, float blendInTime, float blendOutTime,
        float breakBlendOutTime, UAnimSequenceBase? animation, AActor? owner) =>
        ModifyController.ApplyCameraModify(settings, duration, blendInTime, blendOutTime, breakBlendOutTime, animation, owner);

    /// <summary>提前结束一段镜头修改：动画和编号都对得上才结束。</summary>
    public void StopCameraModify(UAnimSequenceBase? animation, int modifyInstance) =>
        ModifyController.StopCameraModify(animation, modifyInstance);
}
