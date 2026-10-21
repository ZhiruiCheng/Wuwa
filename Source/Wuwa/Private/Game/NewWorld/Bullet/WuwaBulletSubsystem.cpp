#include "Game/NewWorld/Bullet/WuwaBulletSubsystem.h"
#include "Game/NewWorld/Combat/WuwaTimeStopSubsystem.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Character.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAbilitySystemComponent.h"
#include "Game/NewWorld/Character/Common/WuwaCharactorBase.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillBridgeComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Engine/OverlapResult.h"
#include "Game/NewWorld/Combat/WuwaCamp.h"
#include "Game/Effect/System/WuwaEffectSystem.h"
#include "Game/Effect/Types/WuwaEffectSpawnRequest.h"
#include "Game/EffectModel/Base/WuwaEffectModelBase.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaBullet, Log, All);

namespace WuwaBulletDebug
{
	static int32 Draw = 0;
	static FAutoConsoleVariableRef CVarDraw(TEXT("wuwa.Bullet.Debug"), Draw,
		TEXT("1 = 画出活着的子弹：判定窗口内红色，窗口外灰色。"));

	/** 角度区间 [A0, A1] 内、内外半径之间、上下半高之间的线框（扇形和环形柱共用）。 */
	void DrawSweep(const UWorld* World, const FTransform& Shape, double Outer, double Inner, double HalfHeight, double A0, double A1, const FColor& Color)
	{
		const int32 Segments = FMath::Clamp(FMath::CeilToInt((A1 - A0) / FMath::DegreesToRadians(10.0)), 4, 36);
		auto Point = [&](double Radius, double Angle, double Z)
		{
			return Shape.TransformPosition(FVector(Radius * FMath::Cos(Angle), Radius * FMath::Sin(Angle), Z));
		};
		for (const double Z : {HalfHeight, -HalfHeight})
		{
			for (int32 I = 0; I < Segments; ++I)
			{
				const double From = FMath::Lerp(A0, A1, double(I) / Segments), To = FMath::Lerp(A0, A1, double(I + 1) / Segments);
				DrawDebugLine(World, Point(Outer, From, Z), Point(Outer, To, Z), Color);
				if (Inner > 0.0) DrawDebugLine(World, Point(Inner, From, Z), Point(Inner, To, Z), Color);
			}
		}
		const bool bClosed = A1 - A0 >= 2.0 * PI - KINDA_SMALL_NUMBER;
		for (const double Angle : bClosed ? TArray<double>{0.0, 0.5 * PI, PI, 1.5 * PI} : TArray<double>{A0, A1})
		{
			DrawDebugLine(World, Point(Outer, Angle, HalfHeight), Point(Outer, Angle, -HalfHeight), Color);
			if (Inner > 0.0) DrawDebugLine(World, Point(Inner, Angle, HalfHeight), Point(Inner, Angle, -HalfHeight), Color);
			if (!bClosed)
			{
				// 扇形两条边连回圆心
				for (const double Z : {HalfHeight, -HalfHeight}) DrawDebugLine(World, Point(Inner, Angle, Z), Point(Outer, Angle, Z), Color);
			}
		}
		if (!bClosed && Inner <= 0.0) DrawDebugLine(World, Point(0.0, 0.0, HalfHeight), Point(0.0, 0.0, -HalfHeight), Color);
	}
}

UWuwaBulletSubsystem* UWuwaBulletSubsystem::Get(const UObject* WorldContext)
{
	const UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
	return World ? World->GetSubsystem<UWuwaBulletSubsystem>() : nullptr;
}

int32 UWuwaBulletSubsystem::SpawnBullet(const FWuwaBulletSpawnRequest& Request)
{
	UE_LOG(LogWuwaBullet, Verbose, TEXT("Bullet request %s from %s (skill %d, anim %s)"), *Request.BulletId.ToString(),
		*GetNameSafe(Request.Attacker.Get()), Request.SkillId, *GetNameSafe(Request.SourceAnimation.Get()));
	OnBulletRequested.Broadcast(Request);

	AActor* Attacker = Request.Attacker.Get();
	// 只在有权威的一端生成（服务器 / 单机）；客户端也会播到同一个通知，这里忽略
	if (!IsValid(Attacker) || !Attacker->HasAuthority()) return 0;
	const FWuwaBulletInfo* Info = FindBulletInfo(Attacker, Request.BulletId);
	if (!Info) return 0;
	if (!HasSpawnTags(Attacker, *Info))
	{
		UE_LOG(LogWuwaBullet, Verbose, TEXT("Bullet %s skipped: attacker tags do not allow it."), *Request.BulletId.ToString());
		return 0;
	}
	if (Info->bDestroyOnSkillEnd && !IsSkillActive(Attacker, Request.SkillId))
	{
		UE_LOG(LogWuwaBullet, Verbose, TEXT("Bullet %s skipped: skill %d is no longer active."), *Request.BulletId.ToString(), Request.SkillId);
		return 0;
	}

	FWuwaBullet& Bullet = Bullets.AddDefaulted_GetRef();
	Bullet.Handle = NextHandle++;
	Bullet.BulletId = Request.BulletId;
	Bullet.Info = *Info;
	Bullet.Attacker = Attacker;
	Bullet.SkillId = Request.SkillId;
	FWuwaBulletBornContext Context = MakeBornContext(Attacker, *Info);
	Context.Parent = Request.Parent;
	Bullet.Transform = WuwaBullet::ComputeBornTransform(*Info, Context);
	if (Info->bStickGround) StickToGround(Bullet.Transform, Info->StickGroundTraceLength);
	Bullet.ChildrenSpawned.SetNumZeroed(Info->Children.Num());
	Bullet.Info.Effects.StableSort([](const FWuwaBulletEffect& A, const FWuwaBulletEffect& B) { return A.Delay < B.Delay; });

	// 跟随（原作 BulletBlueprintFunctionLibrary.AttachToBone）：挂到网格的骨骼上（骨骼名为空时挂网格本身），保持出生时的世界位置
	const ACharacter* Character = Cast<ACharacter>(Attacker);
	USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (Mesh && (Info->FollowType == EWuwaBulletFollowType::FollowBone || Info->FollowType == EWuwaBulletFollowType::SnapToBone))
	{
		Bullet.bFollow = true;
		Bullet.FollowComponent = Mesh;
		Bullet.FollowSocket = Mesh->DoesSocketExist(Info->BoneName) ? Info->BoneName : NAME_None;
		FTransform Base = Mesh->GetSocketTransform(Bullet.FollowSocket, RTS_World);
		Base.SetScale3D(FVector::OneVector);
		Bullet.FollowRelative = Bullet.Transform.GetRelativeTransform(Base);
	}
	if (Info->bDestroyOnSkillEnd) WatchSkillEnd(Attacker);
	PlayDueEffects(Bullet);

	UE_LOG(LogWuwaBullet, Verbose, TEXT("Bullet %s spawned as #%d at %s"), *Bullet.BulletId.ToString(), Bullet.Handle, *Bullet.Transform.GetLocation().ToCompactString());
	const int32 Handle = Bullet.Handle;
	OnBulletSpawned.Broadcast(Bullet);   // 广播里可能再生成子弹，Bullet 引用之后可能失效
	return Handle;
}

void UWuwaBulletSubsystem::DestroyBullet(int32 Handle)
{
	const int32 Index = Bullets.IndexOfByPredicate([Handle](const FWuwaBullet& Bullet) { return Bullet.Handle == Handle; });
	if (Index != INDEX_NONE) RemoveBulletAt(Index, EWuwaBulletEndReason::Destroyed);
}

const FWuwaBullet* UWuwaBulletSubsystem::FindBullet(int32 Handle) const
{
	return Bullets.FindByPredicate([Handle](const FWuwaBullet& Bullet) { return Bullet.Handle == Handle; });
}

void UWuwaBulletSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	// 本帧的命中先收集，子弹都更新完再广播（广播里生成 / 销毁子弹不会弄乱正在遍历的数组）
	TArray<FWuwaBulletHit> Hits;
	TArray<FWuwaBulletSpawnRequest> Children;
	const UWuwaTimeStopSubsystem* TimeStop = UWuwaTimeStopSubsystem::Get(this);
	// 倒着遍历，移除当前这颗不影响还没处理的
	for (int32 Index = Bullets.Num() - 1; Index >= 0; --Index)
	{
		FWuwaBullet& Bullet = Bullets[Index];
		// 原作 BulletUtil.FrozenCharacterBullet：大招时停冻住的单位，它的子弹不计时也不检测命中
		if (TimeStop && Bullet.Attacker.IsValid() && TimeStop->IsFrozen(Bullet.Attacker.Get())) continue;
		Bullet.LiveTime += DeltaTime;
		if (!Bullet.Attacker.IsValid() || (Bullet.bFollow && !Bullet.FollowComponent.IsValid()))
		{
			RemoveBulletAt(Index, EWuwaBulletEndReason::AttackerGone);
			continue;
		}
		if (Bullet.bFollow)
		{
			FTransform Base = Bullet.FollowComponent->GetSocketTransform(Bullet.FollowSocket, RTS_World);
			Base.SetScale3D(FVector::OneVector);
			Bullet.Transform = Bullet.FollowRelative * Base;
		}
		PlayDueEffects(Bullet);
		CollectDueChildren(Bullet, Children);
		if (Bullet.IsCollisionActive()) DetectHits(Bullet, Hits);
		if (WuwaBulletDebug::Draw > 0) DrawDebug(Bullet);
		// 原作 BulletActionUpdateLiveTime：持续时间 < 0 表示一直存在
		if (Bullet.Info.Duration >= 0.f && Bullet.LiveTime >= Bullet.Info.Duration)
		{
			RemoveBulletAt(Index, EWuwaBulletEndReason::TimeOut);
		}
	}
	for (const FWuwaBulletSpawnRequest& Request : Children) SpawnBullet(Request);
	for (const FWuwaBulletHit& Hit : Hits)
	{
		UE_LOG(LogWuwaBullet, Verbose, TEXT("Bullet %s #%d hit %s (%d) at %s"), *Hit.BulletId.ToString(), Hit.BulletHandle, *GetNameSafe(Hit.Victim.Get()),
			Hit.VictimHitIndex, *Hit.HitLocation.ToCompactString());
		if (WuwaBulletDebug::Draw > 0 && GetWorld()) DrawDebugSphere(GetWorld(), Hit.HitLocation, 15.f, 8, FColor::Green, false, 1.f);
		OnBulletHit.Broadcast(Hit);
	}
}

void UWuwaBulletSubsystem::DetectHits(FWuwaBullet& Bullet, TArray<FWuwaBulletHit>& OutHits) const
{
	UWorld* World = GetWorld();
	const FWuwaBulletInfo& Info = Bullet.Info;
	if (!World || !WuwaBullet::HasCollision(Info)) return;

	// 整颗子弹共用间隔（原作 BulletCollisionSystem.sSa）：窗口打开后每过一个间隔是一个新阶段，同一阶段只处理一次
	const float SinceOpen = Bullet.LiveTime - FMath::Max(Info.CollisionDelay, 0.f);
	const bool bSharedInterval = Info.HitInterval > 0.f && !Info.bIntervalPerVictim;
	int32 Stage = 0;
	if (bSharedInterval)
	{
		const float Clamped = Info.CollisionDuration > 0.f ? FMath::Min(SinceOpen, Info.CollisionDuration + UE_KINDA_SMALL_NUMBER) : SinceOpen;
		Stage = FMath::FloorToInt(Clamped / Info.HitInterval) + 1;
		if (Stage - Bullet.LastStage <= 0) return;
	}

	// 粗检测：判定体（扇形 / 圆环用近似盒）和 Pawn 的重叠
	const FTransform Shape = WuwaBullet::ComputeShapeTransform(Info, Bullet.Transform);
	FVector Extent;
	const FVector BoxOffset = WuwaBullet::GetBroadphaseBox(Info, Extent);
	const FCollisionShape Query = Info.Shape == EWuwaBulletShape::Sphere ? FCollisionShape::MakeSphere(Info.Size.X) : FCollisionShape::MakeBox(Extent);
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(TEXT("WuwaBullet"), false);
	World->OverlapMultiByObjectType(Overlaps, Shape.TransformPosition(BoxOffset), Shape.GetRotation(), FCollisionObjectQueryParams(ECC_Pawn), Query, Params);

	// 按单位归并；扇形 / 圆环要求单位至少有一个碰撞体通过精确检测（原作 gWo）
	TArray<AWuwaCharactorBase*> Victims;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		AWuwaCharactorBase* Victim = Cast<AWuwaCharactorBase>(Overlap.GetActor());
		const UPrimitiveComponent* Component = Overlap.GetComponent();
		if (!Victim || !Component || Victim->IsHidden() || Victims.Contains(Victim)) continue;
		if (Info.Shape == EWuwaBulletShape::Sector || Info.Shape == EWuwaBulletShape::Cylinder)
		{
			const FVector LocalExtent = Component->CalcBounds(FTransform::Identity).BoxExtent * Component->GetComponentScale().GetAbs();
			const bool bInside = Info.Shape == EWuwaBulletShape::Cylinder
				? WuwaBullet::IsBoxInRing(Shape.GetLocation(), Info.Size, Component->Bounds.Origin, LocalExtent, Component->GetComponentQuat())
				: WuwaBullet::IsBoxInSector(Shape.GetLocation(), Info.Size, Shape.GetRotation(), Component->Bounds.Origin, LocalExtent, Component->GetComponentQuat());
			if (!bInside) continue;
		}
		if (!CanHitCamp(Bullet, Victim)) continue;
		Victims.Add(Victim);
	}
	if (bSharedInterval) Bullet.LastStage = Stage;

	for (AWuwaCharactorBase* Victim : Victims)
	{
		if (!CanHitAgain(Bullet, Victim)) continue;
		// 每个单位各自计间隔（原作 IntervalAfterHit / ObjectsHitCurrent）：离上次命中不到一个间隔不算
		if (Info.HitInterval > 0.f && Info.bIntervalPerVictim)
		{
			if (const float* Last = Bullet.VictimLastHitTime.Find(Victim); Last && Bullet.LiveTime < *Last + Info.HitInterval) continue;
			Bullet.VictimLastHitTime.Add(Victim, Bullet.LiveTime);
		}
		// 原作 BulletHitCountUtil.AddHitCount
		int32& Count = Bullet.VictimHits.FindOrAdd(Victim);
		++Count;
		++Bullet.TotalHits;

		FWuwaBulletHit& Hit = OutHits.AddDefaulted_GetRef();
		Hit.BulletHandle = Bullet.Handle;
		Hit.BulletId = Bullet.BulletId;
		Hit.Attacker = Bullet.Attacker;
		Hit.Victim = Victim;
		Hit.SkillId = Bullet.SkillId;
		Hit.DamageId = Info.DamageId;
		Hit.VictimHitIndex = Count;
		Hit.bFirstBulletHit = Bullet.TotalHits == 1;
		Hit.Info = Info;
		Hit.BulletTransform = Shape;
		// 命中点：胶囊轴上离判定体中心最近的点，再朝判定体中心推出一个胶囊半径（≈ 原作 GetImpactPointCharacter）
		const UCapsuleComponent* Capsule = Victim->GetCapsuleComponent();
		const FVector Axis = Capsule->GetUpVector();
		const FVector CapsuleCenter = Capsule->GetComponentLocation();
		const double Along = FMath::Clamp(FVector::DotProduct(Shape.GetLocation() - CapsuleCenter, Axis), -Capsule->GetScaledCapsuleHalfHeight(), Capsule->GetScaledCapsuleHalfHeight());
		const FVector OnAxis = CapsuleCenter + Axis * Along;
		Hit.HitLocation = OnAxis + (Shape.GetLocation() - OnAxis).GetSafeNormal() * Capsule->GetScaledCapsuleRadius();
	}
}

bool UWuwaBulletSubsystem::CanHitCamp(const FWuwaBullet& Bullet, const AActor* Victim)
{
	const int32 Mask = Bullet.Info.HitCamp;
	const AActor* Attacker = Bullet.Attacker.Get();
	// "对小队"（原作值 11）：只命中发射者一方当前操控的角色
	if (Mask == (static_cast<int32>(EWuwaBulletCamp::PlayerGroup) | static_cast<int32>(EWuwaBulletCamp::Friend) | static_cast<int32>(EWuwaBulletCamp::Self)))
	{
		const APawn* Pawn = Cast<APawn>(Victim);
		return Pawn && Pawn->IsPlayerControlled();
	}
	if (Victim == Attacker) return (Mask & static_cast<int32>(EWuwaBulletCamp::Self)) != 0;
	// 关系值 × 2 正好是位：敌对 2 → Enemy 4，友好 1 → Friend 2
	const int32 Bit = 2 * static_cast<int32>(UWuwaCampLibrary::GetRelation(UWuwaCampLibrary::GetCamp(Attacker), UWuwaCampLibrary::GetCamp(Victim)));
	return (Mask & Bit) != 0;
}

bool UWuwaBulletSubsystem::CanHitAgain(const FWuwaBullet& Bullet, const AActor* Victim)
{
	const FWuwaBulletInfo& Info = Bullet.Info;
	const int32* Count = Bullet.VictimHits.Find(Victim);
	// 命中个数满了，且这个单位不是已经命中过的
	if (Info.VictimCount >= 0 && Bullet.VictimHits.Num() >= Info.VictimCount && !Count) return false;
	// 这个单位的作用次数用完了（-1 不限）
	if (Info.HitsPerVictim >= 0 && (Count ? *Count : 0) >= Info.HitsPerVictim) return false;
	// 总作用次数用完了（原作只认 > 0，0 和 -1 都不限）
	if (Info.TotalHitLimit > 0 && Bullet.TotalHits >= Info.TotalHitLimit) return false;
	return true;
}

TStatId UWuwaBulletSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWuwaBulletSubsystem, STATGROUP_Tickables);
}

void UWuwaBulletSubsystem::Deinitialize()
{
	while (Bullets.Num() > 0) RemoveBulletAt(Bullets.Num() - 1, EWuwaBulletEndReason::Destroyed);
	for (const TPair<TWeakObjectPtr<UAbilitySystemComponent>, FDelegateHandle>& Watch : SkillEndWatches)
	{
		if (UAbilitySystemComponent* AbilitySystem = Watch.Key.Get()) AbilitySystem->OnAbilityEnded.Remove(Watch.Value);
	}
	SkillEndWatches.Reset();
	Super::Deinitialize();
}

bool UWuwaBulletSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// 只在游戏 / PIE 世界里有；动画编辑器预览（EditorPreview）里通知触发时不生成子弹
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

const FWuwaBulletInfo* UWuwaBulletSubsystem::FindBulletInfo(const AActor* Attacker, FName BulletId)
{
	const AWuwaCharactorBase* Character = Cast<AWuwaCharactorBase>(Attacker);
	const UDataTable* Table = Character ? Character->BulletTable.Get() : nullptr;
	if (!Table)
	{
		UE_LOG(LogWuwaBullet, Warning, TEXT("Bullet %s: %s has no BulletTable."), *BulletId.ToString(), *GetNameSafe(Attacker));
		return nullptr;
	}
	const FWuwaBulletInfo* Info = Table->GetRowStruct() == FWuwaBulletInfo::StaticStruct()
		? reinterpret_cast<const FWuwaBulletInfo*>(Table->FindRowUnchecked(BulletId)) : nullptr;
	if (!Info)
	{
		UE_LOG(LogWuwaBullet, Warning, TEXT("Bullet %s is not in %s (attacker %s)."), *BulletId.ToString(), *Table->GetName(), *GetNameSafe(Attacker));
	}
	return Info;
}

bool UWuwaBulletSubsystem::IsSkillActive(const AActor* Attacker, int32 SkillId)
{
	const UWuwaAbilitySystemComponent* AbilitySystem = Cast<UWuwaAbilitySystemComponent>(UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Attacker));
	if (!AbilitySystem || SkillId == 0) return false;
	for (const FGameplayAbilitySpecHandle& Handle : AbilitySystem->FindSkillHandles(SkillId))
	{
		const FGameplayAbilitySpec* Spec = AbilitySystem->FindAbilitySpecFromHandle(Handle);
		if (Spec && Spec->IsActive()) return true;
	}
	return false;
}

void UWuwaBulletSubsystem::StickToGround(FTransform& Transform, float TraceLength) const
{
	// 原作 BulletActionInitMove：DEFAULT_UP_DISTANCE = 500，射线打穿多层地面时取离出生点最近的那层
	constexpr float UpDistance = 500.f;
	UWorld* World = GetWorld();
	if (!World) return;
	const FVector Born = Transform.GetLocation();
	TArray<FHitResult> Hits;
	World->LineTraceMultiByObjectType(Hits, Born + FVector(0.f, 0.f, UpDistance), Born - FVector(0.f, 0.f, TraceLength),
		FCollisionObjectQueryParams(ECC_WorldStatic), FCollisionQueryParams(SCENE_QUERY_STAT(WuwaBulletStickGround)));
	const FHitResult* Nearest = nullptr;
	for (const FHitResult& Hit : Hits)
	{
		if (!Nearest || FVector::DistSquared(Hit.Location, Born) < FVector::DistSquared(Nearest->Location, Born)) Nearest = &Hit;
	}
	if (Nearest) Transform.SetLocation(Nearest->Location);
}

bool UWuwaBulletSubsystem::HasSpawnTags(const AActor* Attacker, const FWuwaBulletInfo& Info)
{
	if (Info.SpawnRequiredTags.IsEmpty() && Info.SpawnBlockedTags.IsEmpty()) return true;
	const UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Attacker);
	if (!AbilitySystem) return Info.SpawnRequiredTags.IsEmpty();
	return AbilitySystem->HasAllMatchingGameplayTags(Info.SpawnRequiredTags) && !AbilitySystem->HasAnyMatchingGameplayTags(Info.SpawnBlockedTags);
}

void UWuwaBulletSubsystem::PlayDueEffects(FWuwaBullet& Bullet) const
{
	const TArray<FWuwaBulletEffect>& Effects = Bullet.Info.Effects;
	UWuwaEffectSystem* EffectSystem = nullptr;
	while (Bullet.EffectsPlayed < Effects.Num() && Bullet.LiveTime >= Effects[Bullet.EffectsPlayed].Delay)
	{
		const FWuwaBulletEffect& Effect = Effects[Bullet.EffectsPlayed++];
		// 第一次用到时同步加载（子弹表只存软引用，不让整张表把所有特效都拉进内存）
		UWuwaEffectModelBase* Model = Effect.Model.LoadSynchronous();
		EffectSystem = EffectSystem ? EffectSystem : UWuwaEffectSystem::GetEffectSystem(this);
		if (!Model || !EffectSystem) continue;
		FWuwaEffectSpawnRequest Request;
		Request.Model = Model;
		Request.Transform = FTransform(Effect.Rotation, Effect.Location, Effect.Scale) * Bullet.Transform;
		EffectSystem->SpawnEffect(Request);
	}
}

void UWuwaBulletSubsystem::CollectDueChildren(FWuwaBullet& Bullet, TArray<FWuwaBulletSpawnRequest>& OutRequests)
{
	for (int32 Index = 0; Index < Bullet.Info.Children.Num(); ++Index)
	{
		const FWuwaBulletChild& Child = Bullet.Info.Children[Index];
		int32& Spawned = Bullet.ChildrenSpawned[Index];
		while (Spawned < Child.Count && Bullet.LiveTime >= Child.Delay + Spawned * FMath::Max(Child.Interval, 0.f))
		{
			FWuwaBulletSpawnRequest& Request = OutRequests.AddDefaulted_GetRef();
			Request.Attacker = Bullet.Attacker;
			Request.BulletId = Child.BulletId;
			Request.SkillId = Bullet.SkillId;
			Request.Parent = Bullet.Transform;
			++Spawned;
		}
	}
}

FWuwaBulletBornContext UWuwaBulletSubsystem::MakeBornContext(const AActor* Attacker, const FWuwaBulletInfo& Info)
{
	FWuwaBulletBornContext Context;
	Context.Attacker = Attacker->GetActorTransform();
	Context.Attacker.SetScale3D(FVector::OneVector);
	const ACharacter* Character = Cast<ACharacter>(Attacker);
	if (const UCapsuleComponent* Capsule = Character ? Character->GetCapsuleComponent() : nullptr)
	{
		Context.CapsuleHalfHeight = Capsule->GetScaledCapsuleHalfHeight();
	}
	const USkeletalMeshComponent* Mesh = Character ? Character->GetMesh() : nullptr;
	if (Mesh && !Info.BoneName.IsNone() && Mesh->DoesSocketExist(Info.BoneName))
	{
		FTransform Bone = Mesh->GetSocketTransform(Info.BoneName, RTS_World);
		Bone.SetScale3D(FVector::OneVector);
		Context.Bone = Bone;
	}
	// 技能目标：发射者技能组件里本次技能挑出的目标（打在它的骨骼上，没有骨骼用 Actor 位置）（父子弹的变换由 SpawnBullet 按请求填）
	const AWuwaCharacter* Role = Cast<AWuwaCharacter>(Attacker);
	const UWuwaSkillBridgeComponent* Skills = Role ? Role->SkillComponent.Get() : nullptr;
	if (const AActor* Target = Skills ? Skills->GetSkillTarget() : nullptr)
	{
		const ACharacter* TargetCharacter = Cast<ACharacter>(Target);
		const USkeletalMeshComponent* TargetMesh = TargetCharacter ? TargetCharacter->GetMesh() : nullptr;
		const FName Socket = Skills->GetSkillTargetSocket();
		Context.SkillTarget = FTransform(!Socket.IsNone() && TargetMesh && TargetMesh->DoesSocketExist(Socket)
			? TargetMesh->GetSocketLocation(Socket) : Target->GetActorLocation());
	}
	return Context;
}

void UWuwaBulletSubsystem::WatchSkillEnd(AActor* Attacker)
{
	UAbilitySystemComponent* AbilitySystem = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Attacker);
	if (!AbilitySystem || SkillEndWatches.Contains(AbilitySystem)) return;
	const FDelegateHandle Handle = AbilitySystem->OnAbilityEnded.AddUObject(this, &UWuwaBulletSubsystem::HandleAbilityEnded,
		TWeakObjectPtr<UAbilitySystemComponent>(AbilitySystem));
	SkillEndWatches.Add(AbilitySystem, Handle);
}

void UWuwaBulletSubsystem::HandleAbilityEnded(const FAbilityEndedData& Data, TWeakObjectPtr<UAbilitySystemComponent> WeakAbilitySystem)
{
	// 原作 BulletController.OnSkillEnd：这个发射者、这个技能生成的子弹里，勾了 技能结束是否销毁子弹 的销毁
	const UWuwaAbilitySystemComponent* AbilitySystem = Cast<UWuwaAbilitySystemComponent>(WeakAbilitySystem.Get());
	const int32 SkillId = AbilitySystem ? AbilitySystem->GetSkillIdForHandle(Data.AbilitySpecHandle) : 0;
	const AActor* Avatar = AbilitySystem ? AbilitySystem->GetAvatarActor() : nullptr;
	if (SkillId == 0 || !Avatar) return;
	for (int32 Index = Bullets.Num() - 1; Index >= 0; --Index)
	{
		const FWuwaBullet& Bullet = Bullets[Index];
		if (Bullet.Info.bDestroyOnSkillEnd && Bullet.SkillId == SkillId && Bullet.Attacker.Get() == Avatar)
		{
			RemoveBulletAt(Index, EWuwaBulletEndReason::SkillEnd);
		}
	}
}

void UWuwaBulletSubsystem::RemoveBulletAt(int32 Index, EWuwaBulletEndReason Reason)
{
	// 先从数组里拿出来再广播：广播里生成 / 销毁别的子弹不会弄乱下标
	const FWuwaBullet Bullet = MoveTemp(Bullets[Index]);
	Bullets.RemoveAt(Index);
	UE_LOG(LogWuwaBullet, Verbose, TEXT("Bullet %s #%d ended (%d) after %.2fs"), *Bullet.BulletId.ToString(), Bullet.Handle, int32(Reason), Bullet.LiveTime);
	OnBulletEnded.Broadcast(Bullet, Reason);
}

void UWuwaBulletSubsystem::DrawDebug(const FWuwaBullet& Bullet) const
{
	const UWorld* World = GetWorld();
	const FWuwaBulletInfo& Info = Bullet.Info;
	if (!World || !WuwaBullet::HasCollision(Info)) return;
	const FTransform Shape = WuwaBullet::ComputeShapeTransform(Info, Bullet.Transform);
	const FColor Color = Bullet.IsCollisionActive() ? FColor::Red : FColor(128, 128, 128);
	const FVector& S = Info.Size;
	switch (Info.Shape)
	{
	case EWuwaBulletShape::Box:
		DrawDebugBox(World, Shape.GetLocation(), S, Shape.GetRotation(), Color);
		break;
	case EWuwaBulletShape::Sphere:
		DrawDebugSphere(World, Shape.GetLocation(), S.X, 16, Color);
		break;
	case EWuwaBulletShape::Sector:
	{
		const double Half = FMath::DegreesToRadians(FMath::Clamp(S.Y, 0.0, 360.0) * 0.5);
		WuwaBulletDebug::DrawSweep(World, Shape, S.X, 0.0, S.Z, -Half, Half, Color);
		break;
	}
	case EWuwaBulletShape::Cylinder:
		WuwaBulletDebug::DrawSweep(World, Shape, S.X, FMath::Clamp(S.Y, 0.0, S.X), S.Z, 0.0, 2.0 * PI, Color);
		break;
	default:
		break;
	}
}
