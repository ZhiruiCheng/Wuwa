#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Game/NewWorld/Bullet/WuwaBulletTypes.h"
#include "WuwaBulletSubsystem.generated.h"

class AActor;
class UAbilitySystemComponent;
class UAnimSequenceBase;
class USceneComponent;
struct FAbilityEndedData;

/**
 * 一次"生成子弹"的请求（≈ 原作 BulletUtil.CreateBulletFromAN 的参数）。
 * 由动画通知 UWuwaAnimNotify_SpawnBullet 发出，交给 UWuwaBulletSubsystem。
 */
struct FWuwaBulletSpawnRequest
{
	/** 发射者（原作 Attacker：放这段动画的角色）。 */
	TWeakObjectPtr<AActor> Attacker;

	/** 子弹 ID = 子弹表行名（原作 子弹数据名 / 子弹id数组 里的一项）。 */
	FName BulletId;

	/** 发射时所属的技能 ID（原作 GetCurrentMontageCorrespondingSkillId）；不在技能里时为 0。 */
	int32 SkillId = 0;

	/** 排查用：哪段动画发出的。 */
	TWeakObjectPtr<const UAnimSequenceBase> SourceAnimation;

	/** 子子弹：父子弹当时的变换（出生基准 = 父子弹时用）；动画通知发的为空。 */
	TOptional<FTransform> Parent;
};

/** 子弹为什么没了。 */
enum class EWuwaBulletEndReason : uint8
{
	/** 活够了持续时间（原作 IsTimeNotEnough）。 */
	TimeOut,
	/** 发射者或者跟随的组件没了（原作 AttackerHandle 为空）。 */
	AttackerGone,
	/** 所属技能结束，且表里勾了 技能结束是否销毁子弹（原作 IsDestroyByCharSkillEnd）。 */
	SkillEnd,
	/** 外部调用 DestroyBullet，或者子弹系统关闭。 */
	Destroyed,
};

/** 一颗活着的子弹（≈ 原作 BulletEntity 的 BulletInfo 里与出生、跟随、存活时间有关的部分）。 */
struct FWuwaBullet
{
	/** 子弹系统里的编号，从 1 开始，同一个世界里不重复；0 = 无效。 */
	int32 Handle = 0;

	/** 子弹 ID = 子弹表行名。 */
	FName BulletId;

	/** 生成时的表行拷贝（之后改表不影响已经生成的子弹）。 */
	FWuwaBulletInfo Info;

	TWeakObjectPtr<AActor> Attacker;
	int32 SkillId = 0;

	/** 出生到现在的秒数（受世界时间缩放影响）。 */
	float LiveTime = 0.f;

	/** 子弹原点当前的世界变换；判定体中心 = WuwaBullet::ComputeShapeTransform(Info, Transform)。 */
	FTransform Transform;

	/**
	 * FollowBone / SnapToBone：跟随发射者的网格（FollowSocket 为空时跟网格本身），FollowRelative 是出生时相对它的变换
	 * （≈ 原作 KeepWorld 挂接到骨骼）。Fixed 时 bFollow 为 false，Transform 一直是出生位置。
	 */
	bool bFollow = false;
	TWeakObjectPtr<USceneComponent> FollowComponent;
	FName FollowSocket;
	FTransform FollowRelative;

	bool IsCollisionActive() const { return WuwaBullet::IsCollisionActive(Info, LiveTime); }

	// ---- 命中计数（原作 BulletInfo.EntityHitSet / EntityHitCount / HitNumberAll / ObjectsHitCurrent / LastStageInterval） ----

	/** 每个单位被这颗子弹命中了几次；有记录的就是已命中过的单位。 */
	TMap<TWeakObjectPtr<AActor>, int32> VictimHits;
	/** 所有单位加起来命中了几次。 */
	int32 TotalHits = 0;
	/** "作用间隔基于个体"时，每个单位上次被命中的 LiveTime。 */
	TMap<TWeakObjectPtr<AActor>, float> VictimLastHitTime;
	/** 整颗子弹共用间隔时，已经处理过的阶段号（判定窗口打开后每过一个间隔加一）。 */
	int32 LastStage = 0;

	/** Info.Children 每一项已经生成了几颗。 */
	TArray<int32> ChildrenSpawned;
	/** Info.Effects 已经播放到第几项。 */
	int32 EffectsPlayed = 0;
};

/** 一次命中（≈ 原作命中后构造的 HitInformation 里与判定有关的部分）。伤害、受击表现在后面的步骤里接。 */
struct FWuwaBulletHit
{
	int32 BulletHandle = 0;
	FName BulletId;
	TWeakObjectPtr<AActor> Attacker;
	TWeakObjectPtr<AActor> Victim;
	int32 SkillId = 0;
	/** 子弹表的伤害 ID（0 = 不造成伤害）。 */
	int64 DamageId = 0;
	/** 命中点：受击者胶囊上离判定体中心最近的点（≈ 原作 GetImpactPointCharacter）。 */
	FVector HitLocation = FVector::ZeroVector;
	/** 这是这颗子弹第几次命中这个单位（从 1 开始）。 */
	int32 VictimHitIndex = 0;
	/** 这是这颗子弹的第一次命中（所有单位算在一起）：能量恢复类 buff 只在这次加。 */
	bool bFirstBulletHit = false;
	/** 子弹表这一行（受击表现要用：受击效果、顿帧、受击方向）。 */
	FWuwaBulletInfo Info;
	/** 命中时判定体的变换（受击方向 1 / 2 / 4 用）。 */
	FTransform BulletTransform = FTransform::Identity;
};

/**
 * 子弹系统（≈ 原作 BulletController + BulletModel），每个游戏世界一个。
 * 收到请求后查发射者的子弹表、算出生位置，然后每帧让子弹跟随、计时，判定窗口内做碰撞检测和命中计数，到时销毁；
 * 计时到了的特效（Info.Effects）在子弹位置播放、子子弹（Info.Children）从子弹位置生成。
 * 特效也只在权威端播放，联机时要另外同步（目前只有单机）。
 * 只在有权威的一端（服务器 / 单机）生成；客户端收到的通知直接忽略，命中结果之后通过 GAS 同步。
 * 命中只广播 OnBulletHit；伤害、受击表现在后面的步骤里加。
 */
UCLASS()
class WUWA_API UWuwaBulletSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	/** 所在世界的子弹系统；不是游戏世界（例如动画编辑器的预览）时为空。 */
	static UWuwaBulletSubsystem* Get(const UObject* WorldContext);

	/** 请求生成一颗子弹，返回子弹编号；没生成（客户端、查不到行、技能已结束等）时返回 0。 */
	int32 SpawnBullet(const FWuwaBulletSpawnRequest& Request);

	/** 立即销毁一颗子弹；编号无效时什么也不做。 */
	void DestroyBullet(int32 Handle);

	/** 按编号找活着的子弹；没有时为空。 */
	const FWuwaBullet* FindBullet(int32 Handle) const;

	/** 所有活着的子弹。 */
	const TArray<FWuwaBullet>& GetBullets() const { return Bullets; }

	DECLARE_MULTICAST_DELEGATE_OneParam(FWuwaOnBulletRequested, const FWuwaBulletSpawnRequest&);
	/** 每收到一个请求广播一次（不管最后生没生成）。 */
	FWuwaOnBulletRequested OnBulletRequested;

	DECLARE_MULTICAST_DELEGATE_OneParam(FWuwaOnBulletSpawned, const FWuwaBullet&);
	/** 子弹生成后广播。 */
	FWuwaOnBulletSpawned OnBulletSpawned;

	DECLARE_MULTICAST_DELEGATE_OneParam(FWuwaOnBulletHit, const FWuwaBulletHit&);
	/** 每次命中广播一次（同一帧的命中在本帧子弹更新完后统一广播）。 */
	FWuwaOnBulletHit OnBulletHit;

	DECLARE_MULTICAST_DELEGATE_TwoParams(FWuwaOnBulletEnded, const FWuwaBullet&, EWuwaBulletEndReason);
	/** 子弹被移除前广播。 */
	FWuwaOnBulletEnded OnBulletEnded;

	//~ UTickableWorldSubsystem
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual void Deinitialize() override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	/** 发射者的子弹表里这一行；查不到时为空并打警告。 */
	static const FWuwaBulletInfo* FindBulletInfo(const AActor* Attacker, FName BulletId);
	/** 原作 CreateBulletCustomTarget：勾了 技能结束是否销毁子弹 时，所属技能必须还在运行才生成。 */
	static bool IsSkillActive(const AActor* Attacker, int32 SkillId);
	/** 原作 子弹允许生成Tag / 子弹禁止生成Tag。 */
	static bool HasSpawnTags(const AActor* Attacker, const FWuwaBulletInfo& Info);
	static FWuwaBulletBornContext MakeBornContext(const AActor* Attacker, const FWuwaBulletInfo& Info);
	/** 原作 是否贴地子弹：出生位置往下贴到地面（WorldStatic）。 */
	void StickToGround(FTransform& Transform, float TraceLength) const;

	/** 播放到时间的子弹特效（Info.Effects）。 */
	void PlayDueEffects(FWuwaBullet& Bullet) const;
	/** 到时间的子子弹追加到 OutRequests（遍历完所有子弹再生成）。 */
	static void CollectDueChildren(FWuwaBullet& Bullet, TArray<FWuwaBulletSpawnRequest>& OutRequests);

	/** 发射者的技能结束时销毁它这个技能生成的、勾了 技能结束是否销毁子弹 的子弹。每个 ASC 只监听一次。 */
	void WatchSkillEnd(AActor* Attacker);
	void HandleAbilityEnded(const FAbilityEndedData& Data, TWeakObjectPtr<UAbilitySystemComponent> WeakAbilitySystem);

	/**
	 * 判定窗口内每帧做一次（≈ 原作 BulletCollisionSystem）：粗检测（盒 / 球 / 扇形和圆环的近似盒，查 Pawn）→ 扇形 / 圆环精确检测 →
	 * 阵营（HitCamp）→ 间隔 → 次数限制，命中的追加到 OutHits。
	 */
	void DetectHits(FWuwaBullet& Bullet, TArray<FWuwaBulletHit>& OutHits) const;
	/** 原作 BulletUtil.AttackedCampCondition。 */
	static bool CanHitCamp(const FWuwaBullet& Bullet, const AActor* Victim);
	/** 原作 BulletHitCountUtil.CheckHitCountPerVictim。 */
	static bool CanHitAgain(const FWuwaBullet& Bullet, const AActor* Victim);

	void RemoveBulletAt(int32 Index, EWuwaBulletEndReason Reason);
	void DrawDebug(const FWuwaBullet& Bullet) const;

	TArray<FWuwaBullet> Bullets;
	int32 NextHandle = 1;
	TMap<TWeakObjectPtr<UAbilitySystemComponent>, FDelegateHandle> SkillEndWatches;
};
