using UnrealSharp.Core;
using UnrealSharp.CoreUObject;
using UnrealSharp.Engine;
using UnrealSharp.GameplayAbilities;
using UnrealSharp.GameplayTags;

namespace ManagedWuwa.Game.ZRCamera;

/// <summary>
/// 战斗镜头跟随的角色。记着三样东西：角色本身、操控它的玩家控制器（用来写控制旋转）、
/// 它的技能系统组件（用来查角色身上有没有某个标签）；并负责算出镜头每帧要跟随的点。
/// </summary>
internal sealed class ZrCameraFollowTarget
{
    // 角色网格上跟镜头有关的插槽
    private static readonly FName CameraPositionSocket = new("CameraPosition");//是角色切换之前只读一次后面就不读的因为动作变化的时候骨骼会发生变化
    private static readonly FName HitCaseSocket = new("HitCase");//这个是实时读取的
    private static readonly FName RootSocket = new("Root");//根骨
    // 网格上两个插槽都没有时，跟随点 = 角色位置 + 胶囊半高 × 这个倍数
    private const double CapsuleHeightRate = 1.0;

    private UAbilitySystemComponent? _abilitySystem;
    // 有 CameraPosition 插槽时，记下它相对角色的位置。只在换角色时记一次，之后不跟着动画晃
    private bool _hasCameraPositionSocket;
    private FVector _cameraPositionOffset;
    private bool _hasHitCaseSocket;

    /// <summary>跟随的角色，没有时为空。</summary>
    public APawn? Pawn { get; private set; }

    /// <summary>操控这个角色的玩家控制器。</summary>
    public APlayerController? Controller { get; private set; }

    public bool IsValid => Pawn is not null && Pawn.IsValid();

    /// <summary>换跟随的角色：记下角色、控制器、技能系统组件，并看一下网格上有哪些镜头插槽。传空就是不再跟随。</summary>
    public void Set(APawn? pawn, APlayerController? controller)
    {
        Pawn = pawn;
        Controller = pawn is null ? null : controller;
        _abilitySystem = pawn is null ? null : AbilitySystemLibrary.GetAbilitySystemComponent(pawn);
        CacheSockets(pawn);
    }

    /// <summary>角色身上有没有这个标签。</summary>
    public bool ContainsTag(FGameplayTag tag) =>
        _abilitySystem is not null && _abilitySystem.IsValid() && _abilitySystem.HasMatchingGameplayTag(tag);

    /// <summary>把镜头朝向写进控制旋转，角色的移动方向就跟着镜头走。</summary>
    public void SetControlRotation(FRotator rotation)
    {
        if (Controller is not null)
            Controller.ControlRotation = rotation;
    }

    /// <summary>
    /// 镜头这一帧要跟随的点：
    /// 有 CameraPosition 插槽就用它（按角色这一帧的位置和朝向换算）；没有就用 HitCase 插槽；再没有就用角色位置往上抬半个胶囊高。
    /// 动画把根骨骼带离了网格原点（比如带位移的动画），再加上这段偏移，镜头跟着根骨骼走。
    /// </summary>
    public FVector GetFollowLocation()
    {
        var pawn = Pawn!;
        if (pawn is not ACharacter character || character.Mesh is not { } mesh || !mesh.IsValid())
            return pawn.ActorLocation;
        FVector location;
        if (_hasCameraPositionSocket)
        {
            location = ZrCameraMath.Add(character.ActorLocation, ZrCameraMath.RotateVector(character.ActorRotation, _cameraPositionOffset));
        }
        else if (_hasHitCaseSocket)
        {
            location = mesh.GetSocketLocation(HitCaseSocket);
        }
        else
        {
            var up = ZrCameraMath.RotateVector(character.ActorRotation, new FVector(0, 0, 1));
            location = ZrCameraMath.Add(character.ActorLocation, ZrCameraMath.Scale(up, CapsuleHeightRate * character.CapsuleComponent.ScaledCapsuleHalfHeight));
        }
        return ZrCameraMath.Add(location, ZrCameraMath.Subtract(mesh.GetSocketLocation(RootSocket), mesh.WorldLocation));
    }

    // 看角色网格上有没有 CameraPosition / HitCase 插槽
    private void CacheSockets(APawn? pawn)
    {
        _hasCameraPositionSocket = false;
        _hasHitCaseSocket = false;
        var mesh = (pawn as ACharacter)?.Mesh;
        if (mesh is null || !mesh.IsValid())
            return;
        if (mesh.DoesSocketExist(CameraPositionSocket))
        {
            _hasCameraPositionSocket = true;
            _cameraPositionOffset = mesh.GetSocketTransform(CameraPositionSocket, ERelativeTransformSpace.RTS_Actor).Location;
        }
        else
        {
            _hasHitCaseSocket = mesh.DoesSocketExist(HitCaseSocket);
        }
    }
}
