using UnrealSharp;
using UnrealSharp.Core;
using UnrealSharp.CoreUObject;
using UnrealSharp.Engine;
using UnrealSharp.LevelSequence;
using UnrealSharp.MovieScene;
using UnrealSharp.Wuwa;

namespace ManagedWuwa.Game.ZRCamera;

/// <summary>
/// 序列镜头的播放组件：播技能特写镜头。
/// 开始（PlayCameraSequence）：
///   从对象池取 LevelSequenceActor 装上序列；序列里 Tag 为 Role 的绑定换成放技能的角色；
///   播放原点 = 挂在角色网格插槽上的原点 Actor（没填插槽就用开始那一刻角色 Root 骨骼的位置，之后不跟着动）；
///   播放器停住，由这里每帧按角色的时间缩放手动往前推（顿帧时特写也跟着慢）；
///   用序列自己生成的相机：跳到开头那一帧时就生成了，所以马上能切过去；
///   进入序列镜头模式，屏幕按淡入时间混到序列的相机；要隐藏 HUD 就藏到 HideHudTime（为 0 时藏到结束）。
/// 结束（StopSequence：播到 min(序列结束, 指定时长)、角色没了、同一个角色又放了一次）：
///   战斗镜头转到特写相机最后的朝向 → 恢复 HUD → 退出序列镜头模式（混回战斗镜头，混合期间画面定在特写最后一帧）→
///   战斗镜头按新朝向马上摆一次 → 停止序列、还回对象池。
///   播放器一直是暂停状态，播过结尾也不会自己停，所以结束时相机还在，能读到它最后的朝向。
/// </summary>
internal sealed class ZRCSequenceCameraPlayerComponent(ZRCameraModelInstance cameraModel, ZRCSequenceCameraDisplayComponent display)
{

    #region 关卡序列绑定名字

    // 序列里相机、角色绑定的 Tag
    private static readonly FName SequenceCameraTag = new("SequenceCamera");
    private static readonly FName RoleTag = new("Role");
    // 角色网格的根骨骼
    private static readonly FName RootSocket = new("Root");

    #endregion
    

    // 放技能的角色（不在播时 = 当前控制的角色）
    private ACharacter? _character;
    // 正在播的 LevelSequenceActor
    private ALevelSequenceActor? _sequenceActor;
    // 播放原点：挂在角色插槽上的空 Actor（第一次用时从对象池取，之后一直复用）
    private AActor? _originActor;
    
    
    // 播到第几秒 / 到第几秒结束
    private double _time;
    private double _endTime;
    // 结束时混回其他镜头的时间
    private float _blendOutTime;
    
    
    // HUD 藏着 / 藏到第几秒
    private bool _hudHidden;
    private double _hideHudTime;

    /// <summary>正在播特写镜头。</summary>
    public bool IsInCinematic { get; private set; }

    /// <summary>记下控制的角色。播放中不换。</summary>
    public void SetPawn(APawn? pawn)
    {
        if (!IsInCinematic && pawn is ACharacter character)
            _character = character;
    }

    /// <summary>
    /// 开始播一段特写镜头。config：特写配置；character：放技能的角色；attachSocket：播放原点挂的插槽（不填就用角色 Root 骨骼的位置）。
    /// 播不了返回 false。
    /// </summary>
    public bool PlayCameraSequence(FWuwaSequenceCameraConfig config, ACharacter? character, FName attachSocket)
    {
        //没有角色挂接自动跳过
        if (character is null || !character.IsValid()) return false;
        
        //是否正在播放,如果是的话就立刻停止当前的然后
        if (IsInCinematic)
        {
            // 正在播：同一个角色可以重新开始，别的角色不能打断
            if (_character is not null && _character.IsValid() && _character != character)
                return false;
            StopSequence();
        }
        var sequence = config.CameraSequence.LoadSynchronous();
        var pool = cameraModel.PlayerCameraManager.GetWorldSubsystem<UWuwaActorPoolSubsystem>();
        if (sequence is null || pool is null)
        {
            LogZrCamera.LogWarning($"特写镜头没播：{config.CameraSequence}（角色 {character}）");
            return false;
        }
        _character = character;
        // 填了插槽：原点 Actor 吸附到插槽上，跟着它走
        if (!attachSocket.IsNone && !AttachOrigin(pool, character, attachSocket))
            return false;

        // 从对象池取 LevelSequenceActor、装序列、算起止时间、换角色绑定
        if (pool.GetActor(new TSubclassOf<AActor>(typeof(ALevelSequenceActor)), FTransform.Identity) is not ALevelSequenceActor actor)
            return false;
        actor.Sequence = sequence;
        _sequenceActor = actor;
        var player = actor.SequencePlayer;
        // 视角由镜头模式切换来切，序列自己的镜头切换轨道关掉。
        // 直接改播放器：Actor 上的 PlaybackSettings 只在生成时交给播放器一次，之后改了不生效
        player.DisableCameraCuts = true;
        _time = ToSeconds(player.GetStartTime());
        _endTime = ToSeconds(player.EndTime);
        if (config.SpecificSequenceTime > 0)
            _endTime = Math.Min(_endTime, config.SpecificSequenceTime);
        _blendOutTime = config.BlendOutTime;
        actor.SetBindingByTag(RoleTag, [character]);

        // 设播放原点,不开启会用默认世界原点
        actor.OverrideInstanceData = true;
        if (actor.DefaultInstanceData is UDefaultLevelSequenceInstanceData instanceData)
        {
            if (attachSocket.IsNone)
                instanceData.TransformOrigin = character.Mesh.GetSocketTransform(RootSocket);
            else
                instanceData.TransformOriginActor = _originActor!;
        }
        //让我们自己控制时间调整缩放
        player.Play();
        player.PlayRate = 0;
        player.Pause();
        SetPlaybackPosition(player, _time, EUpdatePositionMethod.Jump);

        var camera = FindSequenceCamera(actor);
        if (camera is null)
        {
            LogZrCamera.LogError($"{sequence} 里没有 Tag 为 SequenceCamera 的相机");
            StopSequence();
            return false;
        }
        // 关掉画面比例锁定（不然屏幕和胶片比例不同时有黑边）
        camera.CameraComponent.ConstrainAspectRatio = false;
        display.SetCineCamera(camera);

        ZrCameraController.EnterCameraMode(EZrCameraMode.Sequence, config.BlendInTime);
        IsInCinematic = true;
        if (config.HideHud)
        {
            _hideHudTime = config.HideHudTime;
            SetHudHidden(true);
        }
        return true;
    }

    /// <summary>每帧：按角色的时间缩放往前推，播到结束时间就停；HUD 到点恢复。</summary>
    public void Tick(float deltaSeconds)
    {
        if (_sequenceActor is null)
            return;
        if (_character is null || !_character.IsValid() || !_sequenceActor.IsValid())
        {
            StopSequence();
            return;
        }
        // 顿帧 / 时停都写在角色的 CustomTimeDilation 上，特写跟着它变慢
        _time += deltaSeconds * _character.CustomTimeDilation;
        SetPlaybackPosition(_sequenceActor.SequencePlayer, _time, EUpdatePositionMethod.Play);
        if (_time >= _endTime)
        {
            StopSequence();
            return;
        }
        if (_hudHidden && _hideHudTime > 0 && _time >= _hideHudTime)
            SetHudHidden(false);
    }

    /// <summary>结束特写：战斗镜头接过特写相机最后的朝向，恢复 HUD，退出序列镜头模式，战斗镜头马上摆一次，序列停掉还回对象池。</summary>
    public void StopSequence()
    {
        var fightLogic = cameraModel.FightCamera.LogicComponent;
        // 战斗镜头接着特写相机最后的朝向
        var camera = display.CineCamera;
        if (camera is not null && camera.IsValid())
            fightLogic.SetRotation(camera.ActorRotation);
        IsInCinematic = false;
        if (_hudHidden)
            SetHudHidden(false);
        ZrCameraController.ExitCameraMode(EZrCameraMode.Sequence, _blendOutTime);
        // 马上按新朝向摆一次战斗镜头：混合的终点就是它，不然这一帧还是进特写前的旧位置
        fightLogic.ForceTickOutSide();
        ReleaseSequenceActor();
        // 原点 Actor 摘下来，留着下次用
        if (_originActor is not null && _originActor.IsValid())
            _originActor.DetachFromActor(EDetachmentRule.KeepWorld, EDetachmentRule.KeepWorld, EDetachmentRule.KeepWorld);
        display.Clear();
    }

    /// <summary>相机系统清理时调用：直接停掉（不再切镜头模式），恢复 HUD，原点 Actor 还回对象池。</summary>
    public void Clear()
    {
        ReleaseSequenceActor();
        if (_hudHidden)
            SetHudHidden(false);
        IsInCinematic = false;
        var origin = _originActor;
        _originActor = null;
        if (origin is not null && origin.IsValid())
            PutActor(origin, "ZRCSequenceCameraPlayerComponent.Clear");
        _character = null;
    }

    // 没有原点 Actor 就从对象池取一个空 Actor（没有根组件就加一个），再吸附到角色网格的插槽上
    private bool AttachOrigin(UWuwaActorPoolSubsystem pool, ACharacter character, FName socket)
    {
        if (_originActor is null || !_originActor.IsValid())
        {
            _originActor = pool.GetActor(new TSubclassOf<AActor>(typeof(AActor)), FTransform.Identity);
            if (_originActor is null || !_originActor.IsValid())
                return false;
            if (_originActor.RootComponent is null)
                _originActor.AddComponentByClass(new TSubclassOf<UActorComponent>(typeof(USceneComponent)), false, _originActor.ActorTransform, false);
        }
        _originActor.AttachToComponent(character.Mesh, socket, EAttachmentRule.SnapToTarget, EAttachmentRule.SnapToTarget, EAttachmentRule.SnapToTarget, false);
        return true;
    }

    // 停止序列（序列生成的相机、特效随之销毁），当场还回对象池。停止是在镜头的每帧更新里调的，不在序列求值中间，所以可以当场还
    private void ReleaseSequenceActor()
    {
        var actor = _sequenceActor;
        _sequenceActor = null;
        if (actor is null || !actor.IsValid())
            return;
        actor.SequencePlayer.Stop();
        PutActor(actor, "ZRCSequenceCameraPlayerComponent.StopSequence");
    }

    // 还回对象池；池已经不在了（世界正在拆除）就直接销毁
    private void PutActor(AActor actor, string reason)
    {
        var pool = cameraModel.PlayerCameraManager.IsValid() ? cameraModel.PlayerCameraManager.GetWorldSubsystem<UWuwaActorPoolSubsystem>() : null;
        if (pool is not null && pool.IsValid())
            pool.PutActor(actor, reason);
        else
            actor.DestroyActor();
    }

    // 隐藏 / 恢复主 HUD
    private void SetHudHidden(bool hidden)
    {
        _hudHidden = hidden;
        var player = cameraModel.PlayerCameraManager.IsValid() ? cameraModel.PlayerCameraManager.OwningPlayerController : null;
        if (player is not null && player.IsValid())
            UWuwaMainHudController.SetMainHudHidden(player, hidden);
    }

    // 序列里 Tag 为 SequenceCamera 的绑定这时对应的相机
    private static ACameraActor? FindSequenceCamera(ALevelSequenceActor actor)
    {
        var player = actor.SequencePlayer;
        foreach (var binding in actor.FindBindingsbyTag(SequenceCameraTag))
        {
            foreach (var bound in player.GetBoundObjects(binding))
            {
                if (bound is ACameraActor camera && camera.IsValid())
                    return camera;
            }
        }
        return null;
    }

    // 把播放器推到第几秒：Jump = 直接跳过去，Play = 当成正常播过去（中间的事件会触发）
    private static void SetPlaybackPosition(UMovieSceneSequencePlayer player, double seconds, EUpdatePositionMethod method) =>
        player.PlaybackPosition = new FMovieSceneSequencePlaybackParams
        {
            Time = (float)seconds,
            MarkedFrame = string.Empty,
            PositionType = EMovieScenePositionType.Time,
            UpdateMethod = method,
        };

    // 帧时间换成秒：(帧号 + 子帧) × 分母 / 分子
    private static double ToSeconds(FQualifiedFrameTime time) =>
        (time.Time.FrameNumber.Value + time.Time.SubFrame) * (double)time.Rate.Denominator / time.Rate.Numerator;
}
