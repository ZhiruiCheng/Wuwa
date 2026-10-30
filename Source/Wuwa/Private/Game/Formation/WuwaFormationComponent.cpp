#include "Game/Formation/WuwaFormationComponent.h"
#include "Core/Tag/WuwaGameTags.h"
#include "Game/Formation/WuwaRoleSwitchSnapshot.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/CapsuleComponent.h"
#include "CollisionQueryParams.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "Game/Common/Resource/WuwaResourceSubsystem.h"
#include "GameFramework/PlayerController.h"
#include "Net/UnrealNetwork.h"

#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Game/NewWorld/Character/Role/DataAsset/WuwaRoleDataAsset.h"
UWuwaFormationComponent::UWuwaFormationComponent()
{
	//确保网络同步
	PrimaryComponentTick.bCanEverTick = false;
	
	SetIsReplicatedByDefault(true);
}

void UWuwaFormationComponent::BeginPlay()
{
	Super::BeginPlay();
	// 本地客户端：服务器正在加载、生成队员，这边同时把同一套角色资源异步加载好，
	// 角色复制过来时就不用在网络回调里同步加载（那会卡住游戏线程）。服务器在 InitFormation 里加载。
	const APlayerController* PC = Cast<APlayerController>(GetOwner());
	if (PC && PC->IsLocalController() && !PC->HasAuthority())
	{
		PreloadRoles();
	}
}

void UWuwaFormationComponent::PreloadRoles()
{
	if (bRolesLoaded || RolesLoadHandle.IsValid())
	{
		return;
	}
	TArray<FSoftObjectPath> Paths;
	for (const UWuwaRoleDataAsset* RoleData : InitialRoles)
	{
		if (RoleData)
		{
			Paths.Add(RoleData->CharacterClass.ToSoftObjectPath());
			Paths.Add(RoleData->HeadIcon.ToSoftObjectPath());
		}
	}
	UWuwaResourceSubsystem* Resources = UWuwaResourceSubsystem::Get(this);
	if (!Resources)
	{
		bRolesLoaded = true;
		return;
	}
	TWeakObjectPtr<UWuwaFormationComponent> WeakThis(this);
	TSharedPtr<FStreamableHandle> Handle = Resources->LoadAsync(Paths, TEXT("Formation"), [WeakThis]()
	{
		UWuwaFormationComponent* Self = WeakThis.Get();
		if (!Self)
		{
			return;
		}
		Self->bRolesLoaded = true;
		Self->RolesLoadHandle.Reset();
		TArray<TFunction<void()>> Callbacks = MoveTemp(Self->OnRolesLoaded);
		for (TFunction<void()>& Callback : Callbacks)
		{
			Callback();
		}
	}, UWuwaResourceSubsystem::HighPriority);
	// 全部已经在内存里时，回调在 LoadAsync 里同步执行完了，句柄不用留。
	if (!bRolesLoaded)
	{
		RolesLoadHandle = Handle;
	}
}

float UWuwaFormationComponent::GetLoadProgress() const
{
	if (bRolesLoaded)
	{
		return 1.f;
	}
	return RolesLoadHandle.IsValid() ? RolesLoadHandle->GetProgress() : 0.f;
}

bool UWuwaFormationComponent::IsFormationReady() const
{
	if (Members.IsEmpty() || !IsValid(GetCurrentRole()))
	{
		return false;
	}
	for (const TObjectPtr<AWuwaCharacter>& Member : Members)
	{
		if (!IsValid(Member))
		{
			return false;
		}
	}
	return true;
}

//服务端执行
void UWuwaFormationComponent::InitFormation(const FTransform& SpawnTransform, TFunction<void(bool bSuccess)> OnSpawned)
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	// 只有服务器生成队伍；已经有队员或正在加载就不重复生成。
	if (!PC || !PC->HasAuthority() || !Members.IsEmpty() || bSpawnPending)
	{
		return;
	}
	bSpawnPending = true;
	PreloadRoles();
	if (bRolesLoaded)
	{
		SpawnMembers(SpawnTransform, MoveTemp(OnSpawned));
		return;
	}
	TWeakObjectPtr<UWuwaFormationComponent> WeakThis(this);
	OnRolesLoaded.Add([WeakThis, SpawnTransform, OnSpawned = MoveTemp(OnSpawned)]() mutable
	{
		if (UWuwaFormationComponent* Self = WeakThis.Get())
		{
			Self->SpawnMembers(SpawnTransform, MoveTemp(OnSpawned));
		}
	});
}

void UWuwaFormationComponent::SpawnMembers(const FTransform& SpawnTransform, TFunction<void(bool)> OnSpawned)
{
	bSpawnPending = false;
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	//只在自己的世界生成
	UWorld* World = GetWorld();
	if (!PC || !World || !Members.IsEmpty())
	{
		if (OnSpawned) OnSpawned(false);
		return;
	}
	
	for (UWuwaRoleDataAsset* RoleData: InitialRoles)
	{
		if (!RoleData){continue;}
		// 资源已经由 PreloadRoles 异步加载好，这里只取内存里的类，不会再卡。
		UClass* CharacterClass = RoleData->CharacterClass.Get();
		if (!CharacterClass){continue;}
		AWuwaCharacter* Character = World->SpawnActorDeferred<AWuwaCharacter>(CharacterClass, SpawnTransform,PC,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Character){continue;}//没有的话就返回了
		
		//角色实例也需要复制，Members 才能在客户端解析为角色引用。
		Character->SetReplicates(true);
		
		// 控制权由队伍系统安排，避免角色自动抢占玩家或创建 AI 控制器。
		Character->AutoPossessPlayer = EAutoReceiveInput::Disabled;
		Character->AutoPossessAI = EAutoPossessAI::Disabled;
		
		Character->SetOnField(Members.IsEmpty());//将第一个人设置为可以开启的
		
		//之前，实例已经存在，但还没触发 BeginPlay，你可以提前配置所有属性。FinishSpawning 时才会把这些值带到世界，网络复制也会正确同步初始状态。
		Character->FinishSpawning(SpawnTransform);
		
		if (IsValid(Character))
			Members.Add(Character);
	}
	if (Members.IsEmpty())
	{
		if (OnSpawned) OnSpawned(false);
		return;
	}
	
	CurrentIndex = 0;
	PC->Possess(Members[CurrentIndex].Get());
	PC->ForceNetUpdate();
	// 服务器上改复制属性不会自动调 OnRep_Members，这里手动广播一次；客户端在 OnRep_Members 里广播。
	OnMembersChanged.Broadcast(Members);
	if (OnSpawned) OnSpawned(true);
}

void UWuwaFormationComponent::RequestSwitch(int32 Index)
{
	// 本地先查一遍：明显切不了的（同一个人、冷却中、序号不对）就不发 RPC。
	if (!CanSwitchTo(Index))
	{
		return;
	}
	if (const UWorld* World = GetWorld())
	{
		LastRequestTime = World->GetTimeSeconds();
	}
	// 单机/监听服务器上 Server RPC 等于直接调用；客户端上发给服务器。
	Server_RequestSwitch(Index);
}

bool UWuwaFormationComponent::CanSwitchTo(int32 Index) const
{
	const UWorld* World = GetWorld();
	const AActor* Owner = GetOwner();
	if (!World || !Owner || !Members.IsValidIndex(Index) || Index == CurrentIndex || !IsValid(Members[Index]))
	{
		return false;
	}
	// 原作大招 TsAnimNotifyStateBurst 不能切人：当前角色身上有 SwitchForbid 时拒绝
	if (Members.IsValidIndex(CurrentIndex) && IsValid(Members[CurrentIndex]))
	{
		if (const UAbilitySystemComponent* ASC = Members[CurrentIndex]->GetAbilitySystemComponent();
			ASC && ASC->HasMatchingGameplayTag(FWuwaGameTags::Get().State_Formation_SwitchForbid)) return false;
	}
	// 服务器用上次成功切人的时间裁决；客户端拿不到它，只用自己上次发请求的时间做预先检查。
	const double LastTime = Owner->HasAuthority() ? LastSwitchTime : LastRequestTime;
	return World->GetTimeSeconds() - LastTime >= SwitchCooldown;
}

void UWuwaFormationComponent::Server_RequestSwitch_Implementation(int32 Index)
{
	// 客户端发来的请求不一定可信（数据可能是旧的），服务器必须再查一遍。
	if (!CanSwitchTo(Index))
	{
		return;
	}
	SwitchTo(Index);
}

void UWuwaFormationComponent::SwitchTo(int32 Index)
{
	APlayerController* PC = Cast<APlayerController>(GetOwner());
	AWuwaCharacter* Old = GetCurrentRole();
	AWuwaCharacter* New = Members.IsValidIndex(Index) ? Members[Index].Get() : nullptr;
	if (!PC || !PC->HasAuthority() || !IsValid(Old) || !IsValid(New) || Old == New)
	{
		return;
	}

	// Capture before cancellation/unpossess clears movement policy and animation.
	const FWuwaRoleSwitchSnapshot SwitchSnapshot = Old->CaptureRoleSwitch();

	// ⓪ 旧角色要不要暂留（原作 RoleTeamComponent.OnChangeRole）：技能没到可结束点且有目标 → GoDown（切人不隐藏）；
	//    有"切人不隐藏"或受击硬直就先留在场上，否则马上离场。新角色在暂留中被切回来时取消它的暂留。
	// 新角色还在场上（之前切走时暂留、还没离场）：直接接管它，原地不动、技能和动作照常，不瞬移、也不搬运旧角色的移动和动画。
	const bool bNewOnField = New->bOnField;
	New->CancelGoDown();
	Old->TryApplyGoDown();
	const bool bOldStays = Old->ShouldStayOnGoDown();

	// ① 新角色就位：旧角色马上离场时站到它的位置（先关掉旧角色碰撞，免得两个胶囊体互相挤开）；
	//    旧角色暂留时它还占着位置，按原作 SetQtePosition 在旁边找个点。只取 Yaw，地面角色不继承倾斜。
	if (!bNewOnField)
	{
		const FRotator NewRotation(0.f, Old->GetActorRotation().Yaw, 0.f);
		FVector NewLocation = Old->GetActorLocation();
		if (bOldStays)
		{
			NewLocation = FindStaySwitchLocation(Old, New);
		}
		else
		{
			Old->SetActorEnableCollision(false);
		}
		New->SetActorLocationAndRotation(NewLocation, NewRotation, false, nullptr, ETeleportType::TeleportPhysics);
	}

	// ② 新角色上场。
	New->SetOnField(true);

	// ③ 清理旧角色（原作 StopGroup1Skill）：没有"切人不隐藏"时打断技能和蒙太奇，不能在隐藏状态下继续触发通知、特效和判定。
	//    有"切人不隐藏"时技能照常播完（受击硬直暂留时技能已经被受击打断了）。
	const UAbilitySystemComponent* OldAbilities = Old->GetAbilitySystemComponent();
	if (!OldAbilities || !OldAbilities->HasMatchingGameplayTag(FGameplayTag::RequestGameplayTag(TEXT("State.Formation.SwitchNoHide"))))
	{
		if (UAbilitySystemComponent* OldASC = Old->GetAbilitySystemComponent())
		{
			OldASC->CancelAllAbilities();
		}
		if (const USkeletalMeshComponent* OldMesh = Old->GetMesh())
		{
			if (UAnimInstance* OldAnim = OldMesh->GetAnimInstance())
			{
				OldAnim->StopAllMontages(0.1f);
			}
		}
	}

	// ④ 控制新角色：引擎先让旧角色 UnPossessed（Owner 被清空），再 PossessedBy 新角色；SetPawn 里清输入意图、重置技能 Runtime、通知相机。
	// Possess 会把 ControlRotation 重置成新角色的朝向（APlayerController::OnPossess 里的 SetControlRotation），
	// 镜头会甩到角色背后、俯仰归零。切人不应该改变玩家的视角，所以保存下来、Possess 之后恢复。
	const FRotator ViewRotation = PC->GetControlRotation();
	PC->Possess(New);
	PC->SetControlRotation(ViewRotation);

	// ⑤ 旧角色立刻重新归 PC 所有，再离场（或暂留）。否则它"隐藏 + 关碰撞 + 无主"，IsNetRelevantFor 为 false，客户端会把它销毁。
	Old->SetOwner(PC);
	Old->GoDown(bOldStays);
	New->RestoreRoleSwitch(SwitchSnapshot, !bNewOnField);

	// ⑥ 记录状态、同步、广播。服务器上改复制属性不会自动调 OnRep，所以这里手动广播一次。
	CurrentIndex = Index;
	if (const UWorld* World = GetWorld())
	{
		LastSwitchTime = World->GetTimeSeconds();
	}
	PC->ForceNetUpdate();
	OnRoleSwitched.Broadcast(Old, New, Index);
}

FVector UWuwaFormationComponent::FindStaySwitchLocation(const AWuwaCharacter* Old, const AWuwaCharacter* New) const
{
	// 原作 RoleQteComponent.SetQtePosition（地面：Rotate 80°、Length 200、Height 0，SceneTeamDefine.SPECIAL_CHANGE_*_LAND）：
	// 以旧角色为基准，朝向绕竖轴转 80°，距离 = 200 + 胶囊半径，胶囊扫过去；挡住了就换反方向，都挡住就退回到能站的地方。
	const FVector Origin = Old->GetActorLocation();
	const UCapsuleComponent* Capsule = New->GetCapsuleComponent();
	const float Radius = Capsule ? Capsule->GetScaledCapsuleRadius() : 34.f;
	const float HalfHeight = Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 88.f;
	const float Length = StaySwitchDistance + Radius;
	const FVector Forward = FRotator(0.f, Old->GetActorRotation().Yaw, 0.f).Vector();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WuwaStaySwitch), false);
	Params.AddIgnoredActor(Old);
	Params.AddIgnoredActor(New);
	const FCollisionShape Shape = FCollisionShape::MakeCapsule(Radius, HalfHeight);
	const UWorld* World = GetWorld();
	FVector Fallback = Origin;
	for (const float Angle : {StaySwitchAngle, StaySwitchAngle + 180.f})
	{
		const FVector Target = Origin + Forward.RotateAngleAxis(Angle, FVector::UpVector) * Length;
		FHitResult Hit;
		if (!World || !World->SweepSingleByChannel(Hit, Origin, Target, FQuat::Identity, ECC_Pawn, Shape, Params)) return Target;
		if (Fallback.Equals(Origin) && Hit.Time > 0.f) Fallback = Hit.Location;
	}
	return Fallback;
}

void UWuwaFormationComponent::OnRep_CurrentIndex(int32 OldIndex)
{
	// 客户端：CurrentIndex 和"PC 控制的 Pawn"分开复制，这里 Pawn 可能还没切过来，监听者只用参数里的角色。
	AWuwaCharacter* Old = Members.IsValidIndex(OldIndex) ? Members[OldIndex].Get() : nullptr;
	OnRoleSwitched.Broadcast(Old, GetCurrentRole(), CurrentIndex);
}

void UWuwaFormationComponent::OnRep_Members()
{
	OnMembersChanged.Broadcast(Members);
}

AWuwaCharacter* UWuwaFormationComponent::GetCurrentRole() const
{
	return Members.IsValidIndex(CurrentIndex)
		? Members[CurrentIndex].Get()
		: nullptr;
}

void UWuwaFormationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UWuwaFormationComponent, Members);
	DOREPLIFETIME(UWuwaFormationComponent, CurrentIndex);
}

