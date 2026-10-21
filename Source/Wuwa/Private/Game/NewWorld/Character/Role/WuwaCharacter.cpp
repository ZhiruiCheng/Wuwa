// Fill out your copyright notice in the Description page of Project Settings.

#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Game/NewWorld/Combat/WuwaLockOnComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaRoleAttributeInit.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaInputIntentComponent.h"
#include "Net/UnrealNetwork.h"
#include "Animation/AnimInstance.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateBridgeComponent.h"
#include "Game/NewWorld/Character/Role/Component/WuwaRoleGaitBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Combat/WuwaFightStateBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillBridgeComponent.h"
#include "AbilitySystemComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaGameplayAbilityBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameplayTagContainer.h"
#include "Game/Common/WuwaGameTags.h"


#pragma region LifeCycle

// Sets default values
AWuwaCharacter::AWuwaCharacter(const FObjectInitializer& ObjectInitializer) :
    Super(ObjectInitializer.SetDefaultSubobjectClass<UWuwaMovementComponent>(
        ACharacter::CharacterMovementComponentName))
{
    // UnrealSharp 当前生成类的实际包是 /Script/UnrealSharp，不是 C# namespace Blueprint 包。
    UnifiedStateClass = TSoftClassPtr<UWuwaUnifiedStateBridgeComponent>(FSoftObjectPath(TEXT("/Script/UnrealSharp.WuwaUnifiedStateComponent_C")));
    RoleGaitClass = TSoftClassPtr<UWuwaRoleGaitBridgeComponent>(FSoftObjectPath(TEXT("/Script/UnrealSharp.WuwaRoleGaitComponent_C")));
    FightStateClass = TSoftClassPtr<UWuwaFightStateBridgeComponent>(FSoftObjectPath(TEXT("/Script/UnrealSharp.WuwaFightStateComponent_C")));
    SkillClass = TSoftClassPtr<UWuwaSkillBridgeComponent>(FSoftObjectPath(TEXT("/Script/UnrealSharp.WuwaSkillComponent_C")));
    InputIntent = CreateDefaultSubobject<UWuwaInputIntentComponent>(TEXT("InputIntent"));
    LockOnComponent = CreateDefaultSubobject<UWuwaLockOnComponent>(TEXT("LockOn"));

    // Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
    PrimaryActorTick.bCanEverTick = true;

}

void AWuwaCharacter::PostInitializeComponents()
{
    Super::PostInitializeComponents();
    // 角色是组装者：把输入意图交给需要它的组件，组件之间互不查找，也不回头读角色。
    if (WuwaMovementComponent)
    {
        WuwaMovementComponent->BindInputIntent(InputIntent);
    }
    if (GetWorld() && GetWorld()->IsGameWorld())
    {
        EnsureMovementStateSystem();
        EnsureSkillSystem();
    }
}



// Called when the game starts or when spawned
void AWuwaCharacter::BeginPlay()
{
    Super::BeginPlay();
    // 属性由服务端初始化（基础属性 × 等级成长），客户端靠属性复制
    if (HasAuthority())
    {
        UWuwaRoleAttributeInit::Apply(GetAbilitySystemComponent(), RoleData);
    }
    EnsureMovementStateSystem();
    EnsureSkillSystem();
    
    //可能feid属性在begin之前就到了所以要在这里调用确保一下
    ApplyOnFieldState();
}


// Called every frame
void AWuwaCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    // 暂留计时按世界时间（自己的顿帧不该拖慢下场）
    if (bLingering) UpdateGoDown(GetWorld() ? GetWorld()->GetDeltaSeconds() : DeltaTime);
}

void AWuwaCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    bFightStateEnding = true;
    // FightState 在自身 EndPlay 中先拒绝新申请，再清理状态，避免提前广播重置。
    bMovementStateEnding = true;
    bMovementStateReady = false;
    if (IsValid(UnifiedStateComponent))
    {
        UnifiedStateComponent->OnMoveStateChanged.RemoveAll(WuwaMovementComponent);
        UnifiedStateComponent->OnGaitChanged.RemoveAll(WuwaMovementComponent);
    }
    // 结束阶段：CMC 不再驱动步态，RoleGait 不再读取物理事实和输入（脚本据此不再推进状态）。
    if (WuwaMovementComponent) WuwaMovementComponent->BindMovementState(UnifiedStateComponent, nullptr);
    if (IsValid(RoleGaitComponent))
    {
        RoleGaitComponent->BindDependencies(UnifiedStateComponent, nullptr, nullptr);
        RoleGaitComponent->ResetRuntime();
    }
    Super::EndPlay(EndPlayReason);
}



#pragma endregion

#pragma region 切人暂留

namespace WuwaGoDown
{
    const FGameplayTag& SwitchNoHideTag() { static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("State.Formation.SwitchNoHide")); return Tag; }
    const FGameplayTag& StiffTag() { static const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(TEXT("State.Combat.Stiff")); return Tag; }
}

bool AWuwaCharacter::TryApplyGoDown()
{
    UAbilitySystemComponent* Abilities = GetAbilitySystemComponent();
    if (!HasAuthority() || !Abilities || !IsValid(SkillComponent) || bGoDownBuff) return false;
    // 原作 OnChangeRole：有当前技能、还没到 ReadyEnd、有技能目标（原作没有技能目标时战斗中会再找一次软锁定，项目还没有战斗状态，先不做）
    const FWuwaSkillData Skill = SkillComponent->GetCurrentSkillData();
    if (!IsValid(Skill.ActiveAbility) || Skill.bMainSkillReadyEnd || !SkillComponent->GetSkillTarget()) return false;
    Abilities->AddLooseGameplayTag(WuwaGoDown::SwitchNoHideTag());
    bGoDownBuff = true;
    GoDownSkillSerial = Skill.SkillStartSerial;
    return true;
}

bool AWuwaCharacter::ShouldStayOnGoDown() const
{
    const UAbilitySystemComponent* Abilities = GetAbilitySystemComponent();
    return Abilities && (Abilities->HasMatchingGameplayTag(WuwaGoDown::SwitchNoHideTag()) || Abilities->HasMatchingGameplayTag(WuwaGoDown::StiffTag()));
}

void AWuwaCharacter::GoDown(bool bStay)
{
    if (!HasAuthority()) return;
    if (!bStay)
    {
        RemoveGoDownBuff();
        SetOnField(false);
        return;
    }
    // 原作 SetTeamTag(1) + RestoreDefaultController：不再受玩家控制，碰撞、显示、蒙太奇（根运动）都照常
    bLingering = true;
    GoDownQuitRemaining = -1.f;
}

void AWuwaCharacter::CancelGoDown()
{
    RemoveGoDownBuff();
    bLingering = false;
    GoDownQuitRemaining = -1.f;
}

void AWuwaCharacter::RemoveGoDownBuff()
{
    if (!bGoDownBuff) return;
    bGoDownBuff = false;
    if (UAbilitySystemComponent* Abilities = GetAbilitySystemComponent()) Abilities->RemoveLooseGameplayTag(WuwaGoDown::SwitchNoHideTag());
}

void AWuwaCharacter::UpdateGoDown(float WorldDeltaSeconds)
{
    // GoDown buff 的移除条件（原作：技能 Tag 都没了，或 TsAnimNotifyEndSkill 主动移除）
    if (bGoDownBuff)
    {
        const FWuwaSkillData Skill = IsValid(SkillComponent) ? SkillComponent->GetCurrentSkillData() : FWuwaSkillData();
        if (!IsValid(Skill.ActiveAbility) || Skill.SkillStartSerial != GoDownSkillSerial || Skill.bMainSkillReadyEnd) RemoveGoDownBuff();
    }
    if (GoDownQuitRemaining < 0.f)
    {
        // 原作 Arn 监听两个 Tag 的移除，都没了才走（qrn → DisableRoleWithEffect）
        if (!ShouldStayOnGoDown()) GoDownQuitRemaining = GoDownQuitDelay;
        return;
    }
    GoDownQuitRemaining -= WorldDeltaSeconds;
    if (GoDownQuitRemaining <= 0.f) FinishGoDown();
}

void AWuwaCharacter::FinishGoDown()
{
    // 原作 BCl：停掉所有技能、隐藏
    bLingering = false;
    GoDownQuitRemaining = -1.f;
    if (UAbilitySystemComponent* Abilities = GetAbilitySystemComponent()) Abilities->CancelAllAbilities();
    if (UAnimInstance* Anim = GetMesh() ? GetMesh()->GetAnimInstance() : nullptr) Anim->StopAllMontages(0.f);
    SetOnField(false);
}

#pragma endregion

#pragma region ensure each component
//保证当前的角色身上有这个组件
bool AWuwaCharacter::EnsureFightStateSystem()
{
    if (bFightStateEnding || bInitializingFightState || HasAnyFlags(RF_ClassDefaultObject)|| !GetWorld() || !GetWorld()->IsGameWorld()) return false;
    if (IsValid(FightStateComponent) && FightStateComponent->IsRegistered()) return true;

    TGuardValue<bool> InitializingGuard(bInitializingFightState, true);
    FightStateComponent = FindComponentByClass<UWuwaFightStateBridgeComponent>();
    if (!IsValid(FightStateComponent))
    {
        UClass* ManagedClass = FightStateClass.LoadSynchronous();
        if (!ManagedClass || ManagedClass->HasAnyClassFlags(CLASS_Abstract))
        {
            UE_LOG(LogTemp, Error, TEXT("[Wuwa.Combat] Managed FightState class unavailable. Build/publish ManagedWuwa before playing. Class=%s"),
                *FightStateClass.ToString());
            return false;
        }

        FightStateComponent = NewObject<UWuwaFightStateBridgeComponent>(this, ManagedClass,
            MakeUniqueObjectName(this, ManagedClass, TEXT("FightState")), RF_Transient);
        AddInstanceComponent(FightStateComponent);
    }
    //这里的Register是让组件正式生效
    if (!FightStateComponent->IsRegistered()) FightStateComponent->RegisterComponent();
    return IsValid(FightStateComponent) && FightStateComponent->IsRegistered();
}

//确保一定有SkillSystem
bool AWuwaCharacter::EnsureSkillSystem()
{
    if (bFightStateEnding || bInitializingSkill || !EnsureFightStateSystem()) return false;
    if (IsValid(SkillComponent) && SkillComponent->IsRegistered()) return true;

    TGuardValue<bool> InitializingGuard(bInitializingSkill, true);
    SkillComponent = FindComponentByClass<UWuwaSkillBridgeComponent>();
    if (!IsValid(SkillComponent))
    {
        UClass* ManagedClass = SkillClass.LoadSynchronous();
        if (!ManagedClass || ManagedClass->HasAnyClassFlags(CLASS_Abstract))
        {
            UE_LOG(LogTemp, Error, TEXT("[Wuwa.Combat] Managed Skill class unavailable. Build/publish ManagedWuwa before playing. Class=%s"),
                *SkillClass.ToString());
            return false;
        }
        SkillComponent = NewObject<UWuwaSkillBridgeComponent>(this, ManagedClass,
            MakeUniqueObjectName(this, ManagedClass, TEXT("Skill")), RF_Transient);
        AddInstanceComponent(SkillComponent);
    }
    if (!SkillComponent->IsRegistered()) SkillComponent->RegisterComponent();
    BindSkillDependencies();
    return IsValid(SkillComponent) && SkillComponent->IsRegistered();
}


//
bool AWuwaCharacter::EnsureMovementStateSystem()
{
    if (bMovementStateReady && IsValid(UnifiedStateComponent) && IsValid(RoleGaitComponent)) return true;
    bMovementStateReady = false;
    if (bMovementStateEnding || bInitializingMovementState || HasAnyFlags(RF_ClassDefaultObject) || !GetWorld()
        || !GetWorld()->IsGameWorld() || !WuwaMovementComponent) return false;
    
    
    TGuardValue<bool> InitializingGuard(bInitializingMovementState, true);
    UClass* StateClass = UnifiedStateClass.LoadSynchronous();
    UClass* GaitClass = RoleGaitClass.LoadSynchronous();
    if (!StateClass || !GaitClass || StateClass->HasAnyClassFlags(CLASS_Abstract) || GaitClass->HasAnyClassFlags(CLASS_Abstract))
    {
        UE_LOG(LogTemp, Error, TEXT("[Wuwa.State] Managed state classes unavailable. Build/publish ManagedWuwa before playing. State=%s Gait=%s"),
            *UnifiedStateClass.ToString(), *RoleGaitClass.ToString());
        return false;
    }
    UnifiedStateComponent = FindComponentByClass<UWuwaUnifiedStateBridgeComponent>();
    RoleGaitComponent = FindComponentByClass<UWuwaRoleGaitBridgeComponent>();
    if (!IsValid(UnifiedStateComponent))
    {
        UnifiedStateComponent = NewObject<UWuwaUnifiedStateBridgeComponent>(this, StateClass,
            MakeUniqueObjectName(this, StateClass, TEXT("UnifiedState")), RF_Transient);
        AddInstanceComponent(UnifiedStateComponent);
    }
    if (!IsValid(RoleGaitComponent))
    {
        RoleGaitComponent = NewObject<UWuwaRoleGaitBridgeComponent>(this, GaitClass,
            MakeUniqueObjectName(this, GaitClass, TEXT("RoleGait")), RF_Transient);
        AddInstanceComponent(RoleGaitComponent);
    }
    // 两个引用先就绪再注册，避免动态组件 BeginPlay 时只看到半套依赖。
    if (!UnifiedStateComponent->IsRegistered()) UnifiedStateComponent->RegisterComponent();
    if (!RoleGaitComponent->IsRegistered()) RoleGaitComponent->RegisterComponent();
    
    // 角色是组装者：依赖都在这里注入，组件之间不互相查找，也不回头读角色。
    // 依赖方向：CMC -> RoleGait -> UnifiedState；RoleGait 只通过引擎基类读物理事实。
    RoleGaitComponent->BindDependencies(UnifiedStateComponent, WuwaMovementComponent, InputIntent);
    WuwaMovementComponent->BindMovementState(UnifiedStateComponent, RoleGaitComponent);
    BindSkillDependencies();

    //初始化各种组件,包括Unified组件的默认值,RoleGait组件的默认值,以及CMC组件运动的默认值
    // 与原作相同：CMC 监听运动状态的变化更新速度配置；RoleGait 不订阅状态事件，由 CMC 每次物理更新前驱动。
    UnifiedStateComponent->OnMoveStateChanged.AddUniqueDynamic(WuwaMovementComponent, &UWuwaMovementComponent::HandleMoveStateChanged);
    UnifiedStateComponent->OnGaitChanged.AddUniqueDynamic(WuwaMovementComponent, &UWuwaMovementComponent::HandleGaitChanged);
    UnifiedStateComponent->InitializeState(WuwaMovementComponent->ReadPositionState(), WuwaMovementComponent->GetInitialDesiredGait());
    RoleGaitComponent->InitializePolicy();
    bMovementStateReady = true;
    RoleGaitComponent->RefreshPolicy();
    WuwaMovementComponent->RefreshMovementSettings();
    return true;
}

// 技能组件与运动状态可能先后装配，任一方就绪时都重新注入一次。
void AWuwaCharacter::BindSkillDependencies()
{
    if (IsValid(SkillComponent))
    {
        SkillComponent->BindDependencies(FightStateComponent, UnifiedStateComponent, RoleGaitComponent);
    }
}

#pragma endregion

#pragma region ProcessInput

FWuwaPlayerInputState AWuwaCharacter::GetPlayerInputState() const
{
    return IsValid(InputIntent) ? InputIntent->GetPlayerInputState() : FWuwaPlayerInputState{};
}

void AWuwaCharacter::ResetPlayerInputState()
{
    ConsumeMovementInputVector();
    if (IsValid(InputIntent))
    {
        InputIntent->ResetInputs();
    }
    // 冲刺需求由 RoleGait 保存；角色结束阶段不再驱动它（与原来经 CMC 转发时一致）。
    if (!bMovementStateEnding && IsValid(RoleGaitComponent))
    {
        RoleGaitComponent->ResetSprintRequest();
    }
}

#pragma endregion

#pragma region 切人属性

void AWuwaCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    // 注册 bOnField
    DOREPLIFETIME(AWuwaCharacter, bOnField);
}

void AWuwaCharacter::OnRep_OnField()
{
    ApplyOnFieldState();
}

void AWuwaCharacter::SetOnField(bool bNewOnField)
{
    if (!HasAuthority() || bOnField == bNewOnField)
    {
        return;
    }

    bOnField = bNewOnField;

    // C++ 修改复制属性不会自动在服务器调用 OnRep。
    ApplyOnFieldState();
    ForceNetUpdate();
}


void AWuwaCharacter::ApplyOnFieldState()
{
    const bool bEnteringField = bOnField && !bOnFieldApplied;
    bOnFieldApplied = bOnField;

    // 原作 行为状态.逻辑状态.前台（buff 移除条件等要查）
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
    {
        ASC->SetLooseGameplayTagCount(FWuwaGameTags::Get().State_Formation_OnField, bOnField ? 1 : 0);
    }
    SetActorHiddenInGame(!bOnField);
    SetActorEnableCollision(bOnField);
    SetActorTickEnabled(bOnField);

    if (UCharacterMovementComponent* Movement = GetCharacterMovement())
    {
        if (bOnField)
        {
            Movement->SetComponentTickEnabled(true);
            // 不用 SetDefaultMovementMode：刚瞬移过来还没有站立的地面（MovementBase），它会立刻改成 Falling，
            // 角色掉几帧再落地，看起来就是"抖一下"。直接进 Walking 时引擎会当场找地面、修正高度、设置 Base；
            // 脚下真的没有地面（空中切人）时，下一帧会自己转成 Falling。
            Movement->SetMovementMode(MOVE_Walking);
        }
        else
        {
            Movement->StopMovementImmediately();
            Movement->DisableMovement();
            Movement->SetComponentTickEnabled(false);
        }
    }

    if (USkeletalMeshComponent* CharacterMesh = GetMesh())
    {
        CharacterMesh->SetComponentTickEnabled(bOnField);

        // 离场时 Mesh 停止 Tick，动画被冻结在离场那一刻（比如攻击蒙太奇正在淡出、状态机停在急停里），
        // 再上场会接着播完这些残留。上场时重新初始化动画：立刻清掉所有蒙太奇、状态机回到入口，
        // 并马上算一次姿势，第一帧就不显示冻结的旧姿势。动画实例对象不变，ASC ActorInfo 里缓存的指针仍然有效。
        UAnimInstance* AnimInstance = CharacterMesh->GetAnimInstance();
        if (bEnteringField && AnimInstance)
        {
            AnimInstance->InitializeAnimation();
            CharacterMesh->TickAnimation(0.f, false);
            CharacterMesh->RefreshBoneTransforms();
        }
    }
}


#pragma endregion

