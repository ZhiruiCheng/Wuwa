#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "WuwaBulletTypes.generated.h"

/**
 * 子弹判定体的形状（原作 EBulletShape，数值与原作一致，方便对照原表）。
 * 原作还有 自定义模型(4)/激光(5) 和 大范围版本(6~9)；大范围版本几何含义与对应的小形状相同，转换时并入。
 */
UENUM(BlueprintType)
enum class EWuwaBulletShape : uint8
{
	/** Size = 半长 (X, Y, Z)。原作 BoxComponent.SetBoxExtent(Size)。 */
	Box = 0,
	/** Size.X = 半径。 */
	Sphere = 1,
	/** Size.X = 半径，Size.Y = 张角（度），Size.Z = 半高。 */
	Sector = 2,
	/** Size.X = 外半径，Size.Y = 内半径（0 = 实心），Size.Z = 半高（原作按环形区域判定）。 */
	Cylinder = 3,
};

/** 出生位置基准（原作 EPositionStandard，只收录目前用到的，数值与原作一致）。 */
UENUM(BlueprintType)
enum class EWuwaBulletBornStandard : uint8
{
	/** 发射者：偏移按发射者朝向旋转，Z 从脚底（胶囊底部）算起；配了骨骼时从骨骼位置算起。 */
	Attacker = 0,
	/**
	 * 技能目标：偏移的原点是目标身上的位置（按 BornDistLimit 限制离发射者的距离），朝向用发射者的；
	 * 没有目标时原点是发射者胶囊中心，配了 BornDistLimit.Z 再往前推这么远。都不从脚底算。
	 */
	SkillTarget = 1,
	/** 世界原点。 */
	World = 2,
	/** 父子弹或外部传入的位置。 */
	Parent = 3,
};

/** 子弹出生后怎么动（原作 EBulletFollowType，只收录目前用到的，数值与原作一致）。 */
UENUM(BlueprintType)
enum class EWuwaBulletFollowType : uint8
{
	/** 挂到发射者的骨骼（BoneName）上，随角色一起移动、旋转。 */
	FollowBone = 0,
	/** 停在出生时的位置。 */
	Fixed = 1,
	/**
	 * 吸附到发射者的骨骼（BoneName）：出生位置偏移按骨骼空间算，朝向跟骨骼一致，之后随骨骼移动、旋转。
	 * 原作 BulletBlueprintFunctionLibrary.AttachToBone：3 以 KeepRelative 挂上，再把相对位置设为出生位置偏移、相对旋转清零
	 * （0 是 KeepWorld 挂上，保持出生时的世界位置）。名字是项目起的。
	 */
	SnapToBone = 3,
};

/**
 * 子弹能命中哪些单位（位掩码，位值与原作 BulletActionInitHit 一致：SELF 1 / FRIEND 2 / ENEMY 4，"对队伍" = 3，"对小队" = 11）。
 * 原作 BulletUtil.AttackedCampCondition：目标是发射者自己看 Self 位；否则按阵营关系，敌对看 Enemy 位、友好看 Friend 位。
 */
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EWuwaBulletCamp : uint8
{
	None = 0 UMETA(Hidden),
	/** 发射者自己（例如开枪时给自己的顿帧子弹）。 */
	Self = 1,
	/** 友方单位。 */
	Friend = 2,
	/** 敌对单位。 */
	Enemy = 4,
	/** "对小队"（原作值 11 = 8 + 2 + 1）：只命中发射者一方当前操控的角色。 */
	PlayerGroup = 8,
};
ENUM_CLASS_FLAGS(EWuwaBulletCamp);

/**
 * 子弹表的一行，对应原作 DT_ReBulletDataMain（SReBulletDataMain）中与"判定体 + 命中次数 + 伤害"有关的字段。
 * 行名就是子弹 ID（沿用原作编号：一般是 技能 ID × 1000 + 序号，例如长离普攻1 的第一颗 = 1205001000）。
 * 子弹本身不是伤害：它是一段时间内存在的命中判定体，命中后用 DamageId 去伤害表查数值。
 * 由 UWuwaBulletSubsystem 按行生成、计时；检测、伤害在后面的步骤里接上。
 */
/** 子弹命中时的顿帧（≈ 原作子弹表"时间膨胀"里的 攻击顿帧 / 受击顿帧）：Duration 秒内把时间缩放到 Value，有曲线时按曲线从 Value 过渡（见 UWuwaHitComponent::SetTimeScale）。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaBulletTimeScale
{
	GENERATED_BODY()

	/** 原作 时间膨胀时长（秒）；≤ 0 不顿帧。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TimeScale")
	float Duration = 0.f;

	/** 原作 时间膨胀值（0~1，越小越慢）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TimeScale")
	float Value = 0.f;

	/** 原作 优先级：同时有多个顿帧时用优先级最高的。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TimeScale")
	int32 Priority = 1;

	/** 原作 时间膨胀变化曲线：横轴按时长归一，纵轴归一后 1 = Value、0 = 不缩放。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "TimeScale")
	TSoftObjectPtr<class UCurveFloat> Curve;
};

/**
 * 子子弹（原作 子子弹设置 的一项）：父子弹出生 Delay 秒后生成 Count 颗 BulletId，相邻两颗隔 Interval 秒；
 * 子子弹的出生基准是 父子弹（Parent）时从父子弹当时的位置算起。发射者、所属技能沿用父子弹的。
 * 只收录 召唤触发 = 定时（EBulletChildrenType::NewEnumerator4）的，其余触发方式项目还没有。
 * 例：嘉贝大招起爆 0.3 秒后在落点生成持续燃烧的多段伤害子弹，燃烧子弹 1.4 秒后再生成最后一下。
 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaBulletChild
{
	GENERATED_BODY()

	/** 原作 召唤子弹ID（同一张子弹表的行名）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Children")
	FName BulletId;

	/** 原作 召唤子弹延迟：父子弹出生后多久生成第一颗。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Children", meta = (Units = "s"))
	float Delay = 0.f;

	/** 原作 召唤子弹数量。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Children")
	int32 Count = 1;

	/** 原作 召唤子弹间隔。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Children", meta = (Units = "s"))
	float Interval = 0.f;
};

/**
 * 子弹的特效（原作 表现效果设置.子弹特效DA：一个特效组，特效管线拆成一项项 Model，见 ArtSource/Bullet/prepare_bullets.py）：
 * 子弹出生 Delay 秒后在 子弹当时的位置 × (Location, Rotation, Scale) 处播放，不跟随子弹，播完由特效系统回收。
 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaBulletEffect
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	TSoftObjectPtr<class UWuwaEffectModelBase> Model;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	FVector Location = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	FRotator Rotation = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect")
	FVector Scale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effect", meta = (Units = "s"))
	float Delay = 0.f;
};

USTRUCT(BlueprintType)
struct WUWA_API FWuwaBulletInfo : public FTableRowBase
{
	GENERATED_BODY()

	/** 原作 子弹名称，例如"普攻1-1"。只用于识别。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet")
	FString BulletName;

	// ---- 判定体 ----

	/** 原作 子弹形状。Size 的含义随形状不同，见 EWuwaBulletShape。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Shape")
	EWuwaBulletShape Shape = EWuwaBulletShape::Box;

	/** 原作 初始大小（厘米）。任一用到的分量 <= 0 时不开碰撞（原作 CloseCollision），例如只用来生成子子弹的"父子弹"。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Shape")
	FVector Size = FVector::ZeroVector;

	/** 原作 中心位置偏移：判定体相对子弹自身原点的偏移（子弹本地坐标）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Shape")
	FVector CenterOffset = FVector::ZeroVector;

	// ---- 出生 ----

	/** 原作 出生位置基准。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Born")
	EWuwaBulletBornStandard BornStandard = EWuwaBulletBornStandard::Attacker;

	/** 原作 出生位置偏移（按基准的朝向旋转）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Born")
	FVector BornOffset = FVector::ZeroVector;

	/**
	 * 原作 限制生成距离（只对 技能目标 基准生效，原作 BulletActionInitMove）：
	 * X = 有目标时离发射者至少多远；Y = 最多多远（<= 0 不限）；Z = 没有目标时放在发射者正前方多远（<= 0 就在发射者身上）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Born")
	FVector BornDistLimit = FVector::ZeroVector;

	/** 原作 初始旋转：判定体相对基准朝向的旋转。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Born")
	FRotator BornRotation = FRotator::ZeroRotator;

	/** 原作 子弹跟随类型。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Born")
	EWuwaBulletFollowType FollowType = EWuwaBulletFollowType::Fixed;

	/** 原作 骨骼名字：出生位置从这个骨骼算起（Fixed 也一样），FollowBone 时挂接到它；为空时用发射者本身。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Born")
	FName BoneName;

	/**
	 * 原作 子弹允许生成Tag / 子弹禁止生成Tag：发射者身上要有全部 Required、不能有任何 Blocked 才生成。
	 * 例：嘉贝大招起爆分普通版（变身中禁止）和变身版（变身中才允许），通知两颗都发，按变身状态只生成一颗。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Born")
	FGameplayTagContainer SpawnRequiredTags;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Born")
	FGameplayTagContainer SpawnBlockedTags;

	/**
	 * 原作 是否贴地子弹 / 贴地探测距离（BulletActionInitMove）：出生位置算好后，从它上方 5 米往下打射线到它下方 StickGroundTraceLength，
	 * 落到离出生点最近的地面上（只改位置；原作没勾 不适配坡度 时还会贴合坡面法线，项目没做）。没打到地面就留在原处。
	 * 例：嘉贝大招起爆在前方 7 米、5 米高处算出，贴到地上炸开、燃烧。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Born")
	bool bStickGround = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Born", meta = (Units = "cm", EditCondition = "bStickGround"))
	float StickGroundTraceLength = 0.f;

	// ---- 时间 ----

	/** 原作 持续时间（秒）：子弹存在多久。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Time")
	float Duration = 0.f;

	/** 原作 碰撞判定延迟（秒）：出生后多久开始判定；<= 0 表示立即。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Time")
	float CollisionDelay = -1.f;

	/** 原作 碰撞判定时长（秒）：开始判定后判定多久；<= 0 表示判定到子弹消失。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Time")
	float CollisionDuration = -1.f;

	/** 原作 技能结束是否销毁子弹。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Time")
	bool bDestroyOnSkillEnd = false;

	// ---- 命中次数 ----

	/** 原作 命中个数：最多命中几个单位；-1 = 不限。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Hit")
	int32 VictimCount = -1;

	/**
	 * 原作 每个单位总作用次数：同一个单位最多被这颗子弹作用几次（多段伤害）；0 = 不作用；-1 = 不限
	 * （原作 BulletHitCountUtil.CheckHitCountPerVictim：只有 >= 0 时才按次数拦，例如芙露德莉丝的图腾扩散按间隔一直作用）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Hit")
	int32 HitsPerVictim = 1;

	/** 原作 总作用次数限制：所有单位加起来最多作用几次；-1 = 不限。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Hit")
	int32 TotalHitLimit = -1;

	/** 原作 作用间隔（秒）：两次作用之间至少隔多久；<= 0 = 没有间隔（每个单位只在进入时作用）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Hit")
	float HitInterval = -1.f;

	/** 原作 作用间隔基于个体：true = 每个单位各自计间隔；false = 整颗子弹共用一个间隔。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Hit")
	bool bIntervalPerVictim = false;

	/** 原作 命中判定类型 / 命中判定类型预设：能命中哪些单位（EWuwaBulletCamp 位掩码），默认只打敌方。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Hit", meta = (Bitmask, BitmaskEnum = "/Script/Wuwa.EWuwaBulletCamp"))
	int32 HitCamp = static_cast<int32>(EWuwaBulletCamp::Enemy);

	// ---- 命中结果 ----

	/** 原作 伤害ID：命中时去伤害表查数值；0 = 不造成伤害（只当触发器，例如"触发收束特效"）。原作有超过 int32 的 ID。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Hit")
	int64 DamageId = 0;

	/**
	 * 原作 执行逻辑.能量恢复类GE数组的Id：子弹第一次命中时给发射者加的 buff（DT_Buff 的 ID），整颗子弹只加一次
	 * （原作 BulletCollisionSystem：AllowedAddEnergyBuff 加完就关）。例：长离心眼技命中 +1 心火。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Hit")
	TArray<int64> EnergyBuffIds;

	/**
	 * 原作 执行逻辑.命中后对攻击者发射GameplayEvent标签：每次命中给发射者发这个 GameplayEvent（Instigator = 发射者，Target = 受击者）。
	 * 例：嘉贝 E 跳踢踢中 → 技能 GA 收到事件接后半段（UWuwaHitBranchAbility）。
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Hit")
	FGameplayTag HitEventToAttacker;

	// ---- 受击表现 ----

	/** 原作 被击效果：发射者受击效果表（AWuwaCharactorBase::HitEffectTable）的行名。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Hit")
	FName BeHitEffect;

	/** 原作 子弹受击方向（受击者转向）：0 面向攻击者、1 面向子弹、2 迎着子弹运动方向、3 不转、4 在子弹里背向子弹中心否则面向。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Hit")
	int32 HitDirection = 0;

	/** 原作 是否持续碰撞：持续碰撞的子弹不给受击者顿帧。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Hit")
	bool bContinuousCollision = false;

	/** 原作 攻击顿帧（给发射者）/ 受击顿帧（给受击者）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Hit")
	FWuwaBulletTimeScale AttackerTimeScale;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Hit")
	FWuwaBulletTimeScale VictimTimeScale;

	/** 原作 攻击顿帧忽略攻击者（顿帧给子弹本身，项目还没做，这类子弹不给发射者顿帧）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Hit")
	bool bTimeScaleIgnoreAttacker = false;

	// ---- 子子弹、特效 ----

	/** 原作 子子弹设置（只有定时触发的）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Children")
	TArray<FWuwaBulletChild> Children;

	/** 原作 表现效果设置.子弹特效DA（按 Delay 从小到大）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bullet|Effect")
	TArray<FWuwaBulletEffect> Effects;
};

/** 算子弹出生变换要用到的发射者信息（运行时由角色填，子弹编辑器由预览角色填）。 */
struct FWuwaBulletBornContext
{
	/** 发射者 Actor 的变换（胶囊中心）。 */
	FTransform Attacker = FTransform::Identity;
	/** 胶囊半高：不挂骨骼时出生偏移的 Z 从胶囊底部算起。 */
	float CapsuleHalfHeight = 0.f;
	/** BoneName 骨骼的世界变换；找不到骨骼时为空（按没配骨骼处理）。 */
	TOptional<FTransform> Bone;
	/** 技能目标身上的位置（BornStandard = SkillTarget，只用位置）；没有目标时为空。 */
	TOptional<FTransform> SkillTarget;
	/** 父子弹的变换（BornStandard = Parent）；为空时退回发射者。 */
	TOptional<FTransform> Parent;
};

namespace WuwaBullet
{
	/** 行名 → 子弹 ID；不是正整数的行名返回 0（无效）。 */
	WUWA_API int64 ParseBulletId(FName RowName);

	/** 子弹 ID → 所属技能 ID（原作编号 技能 ID × 1000 + 序号）；无效时 0。 */
	WUWA_API int32 GetSkillId(int64 BulletId);

	/**
	 * 出生偏移所在的坐标系（原作 BulletActionInitMove 的 SVo / MVo / IVo）：
	 * - 发射者、没配骨骼：发射者朝向，原点在脚底（胶囊中心往下半高）；
	 * - 发射者、配了骨骼：发射者朝向，原点在骨骼；SnapToBone 时整个用骨骼的变换；
	 * - 技能目标：发射者朝向，原点在目标位置（离发射者超过 BornDistLimit.Y 拉回、近于 .X 推开）；
	 *   没有目标时原点在发射者胶囊中心，BornDistLimit.Z > 0 时再往正前方推 Z；
	 * - 父子弹：父子弹的变换（没有时按发射者）；世界：世界原点。
	 */
	WUWA_API FTransform ComputeBornBase(const FWuwaBulletInfo& Info, const FWuwaBulletBornContext& Context);

	/** 子弹出生时自身原点的世界变换 = 基准坐标系里的 BornOffset，朝向 = 基准朝向 × BornRotation（原作还有随机偏移等，这里不含）。 */
	WUWA_API FTransform ComputeBornTransform(const FWuwaBulletInfo& Info, const FWuwaBulletBornContext& Context);

	/** 判定体（形状中心）的世界变换 = 出生变换 × CenterOffset。 */
	WUWA_API FTransform ComputeShapeTransform(const FWuwaBulletInfo& Info, const FTransform& Born);

	/** 有没有碰撞：形状用到的尺寸分量都 > 0（原作 CloseCollision 的反面）。 */
	WUWA_API bool HasCollision(const FWuwaBulletInfo& Info);

	/**
	 * 子弹出生 LiveTime 秒时是否在碰撞判定窗口内（原作 BulletActionInitCollision / BulletCollisionSystem）：
	 * 延迟 <= 0 按 0 算；判定时长 <= 0 表示一直判定到子弹消失；窗口两端都包含。没有碰撞的子弹永远不在窗口内。
	 */
	WUWA_API bool IsCollisionActive(const FWuwaBulletInfo& Info, float LiveTime);

	/**
	 * 粗检测用的盒子（判定体局部空间）：盒子 = 原样；球 = 外接盒；扇形 / 圆环 = 原作 BulletCollisionUtil 的近似盒子
	 * （扇形：GetSectorExtent，盒子沿前方偏移；圆环：(外半径, 外半径, 半高)）。返回盒子中心相对判定体中心的偏移，Extent 输出半长。
	 */
	WUWA_API FVector GetBroadphaseBox(const FWuwaBulletInfo& Info, FVector& OutExtent);

	/**
	 * 一个碰撞体（以它的包围盒近似：Origin 世界中心、Extent 局部半长、Rotation 朝向）在不在圆环里（原作 SpaceUtils.IsComponentInRingArea）：
	 * 包围盒 8 个角，高度上有角在 ±半高之间或上下跨过、水平距离上有角在内外半径之间或跨过，就算命中；碰撞体比外半径还大时改做平面上的盒 - 圆检测。
	 * Size = 判定体尺寸（X 外半径、Y 内半径、Z 半高），Center = 判定体中心（只用位置，原作按标准重力竖直）。
	 */
	WUWA_API bool IsBoxInRing(const FVector& Center, const FVector& Size, const FVector& Origin, const FVector& Extent, const FQuat& Rotation);

	/**
	 * 同上，扇形（原作 SpaceUtils.IsComponentInSectorArea）：Size X 半径、Y 张角（度）、Z 半高，ShapeRotation = 判定体朝向（扇形以 +X 为中线）。
	 * 碰撞体比半径还大时直接算命中；张角小于 180° 时，左右两侧的角连线跨过扇形前方也算命中。
	 */
	WUWA_API bool IsBoxInSector(const FVector& Center, const FVector& Size, const FQuat& ShapeRotation, const FVector& Origin, const FVector& Extent, const FQuat& Rotation);
}
