// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Game/NewWorld/Character/Common/Component/Input/WuwaPlayerInputState.h"
#include"Game/NewWorld/Character/Common/WuwaCharactorBase.h"
//所有头文件放于之上
#include "WuwaCharacter.generated.h"

#pragma region Forward Declarations
class UWuwaRoleDataAsset;
class UWuwaMovementComponent;
class UWuwaUnifiedStateBridgeComponent;
class UWuwaRoleGaitBridgeComponent;
class UWuwaFightStateBridgeComponent;
class UWuwaSkillBridgeComponent;
class UWuwaInputCommandConfig;
class UWuwaLockOnComponent;
class UWuwaInputIntentComponent;
struct FWuwaRoleSwitchSnapshot;
#pragma endregion

UCLASS()
class WUWA_API AWuwaCharacter : public AWuwaCharactorBase
{
	GENERATED_BODY()
	
#pragma region 切人属性
public:
	//服务器变量发生变化时，自动通知客户端，并调用客户端回调函数 `OnRep_OnField()`
	UPROPERTY(ReplicatedUsing = OnRep_OnField)
	bool bOnField = true;
	
	UFUNCTION()
	void OnRep_OnField();
	
	void SetOnField(bool bNewOnField);      // 只在服务器调：改变量 + 自己也应用一次
	FWuwaRoleSwitchSnapshot CaptureRoleSwitch();
	/** Apply after on-field initialization and possession, before the first rendered frame. */
	/** bMotion = false：新角色本来就在场上（暂留中被切回），只接过玩家输入（移动方向、走跑偏好），保留它自己的速度、步态和动画。 */
	void RestoreRoleSwitch(const FWuwaRoleSwitchSnapshot& Snapshot, bool bMotion = true);
	
	//告诉UE哪些属性要网络同步,需要在这里注册
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
	void ApplyOnFieldState();               // 每台机器各自执行的本地效果

	/** 上一次在这台机器上应用的在场状态；用来识别"离场 → 上场"这一刻。 */
	bool bOnFieldApplied = true;
#pragma endregion

#pragma region 切人暂留
	/*
	 * 下场时的暂留（≈ 原作 RoleTeamComponent.OnChangeRole / Arn / DisableRoleWithEffect），只在服务器跑：
	 *   1. GoDown：当前主技能还没到"可结束"（ReadyEnd 通知）且有技能目标时，给自己加 State.Formation.SwitchNoHide
	 *      （原作 GoDown buff 1101006002 授予 角色.Common.切人不隐藏）；技能结束、换了技能或到了 ReadyEnd 就去掉。
	 *   2. 下场时有 SwitchNoHide（GoDown 或蒙太奇窗口加的）或受击硬直 State.Combat.Stiff：不隐藏，交出控制后动作照常播完；
	 *      两个 Tag 都没了再过 GoDownQuitDelay 秒（原作 EFFECT_DELAY_QUIT，期间原作播下场溶解材质，项目还没有）停掉技能、离场。
	 *   3. 没有这两个 Tag：立刻离场（技能由队伍组件先打断）。
	 */
public:
	/** 尝试加 GoDown（见上）；返回是否加上了。 */
	bool TryApplyGoDown();
	/** 现在下场是否要先留在场上（有 SwitchNoHide 或受击硬直）。 */
	bool ShouldStayOnGoDown() const;
	/** 下场：bStay 时进入暂留，否则立刻离场。 */
	void GoDown(bool bStay);
	/** 暂留中又被切回来：取消暂留（技能照常继续）。 */
	void CancelGoDown();
	bool IsLingering() const { return bLingering; }

	/** 原作 SceneTeamDefine.EFFECT_DELAY_QUIT（300 毫秒）。 */
	static constexpr float GoDownQuitDelay = 0.3f;

private:
	void UpdateGoDown(float WorldDeltaSeconds);
	void RemoveGoDownBuff();
	void FinishGoDown();

	bool bLingering = false;
	/** GoDown 加的 SwitchNoHide 还在（松散 Tag 计数里的这一份）。 */
	bool bGoDownBuff = false;
	/** 加 GoDown 时的技能（换了技能就去掉 GoDown）。 */
	int64 GoDownSkillSerial = 0;
	/** < 0：还在等 Tag 消失；≥ 0：离场倒计时。 */
	float GoDownQuitRemaining = -1.f;
#pragma endregion

#pragma region SelfIdentification
public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Role")
	TObjectPtr<UWuwaRoleDataAsset> RoleData;
	
#pragma  endregion
	
#pragma region LifeCycle
	
public: 
	
	//Before BeginPlay
	virtual void PostInitializeComponents() override;
	
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;
	
	// Called every frame
	virtual void Tick(float DeltaTime) override;
	
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	
#pragma endregion
	
#pragma region PlayerInput
public:
	/** 转发输入意图组件的组合快照（GA_Dash 等蓝图在用）。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Input")
	FWuwaPlayerInputState GetPlayerInputState() const;

	/** 输入被清空或切换控制角色时同步清理角色侧意图。 */
	void ResetPlayerInputState();

	/** 角色的输入意图：输入来源写入，玩法系统读取。角色自己不依赖任何控制器。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Input")
	TObjectPtr<UWuwaInputIntentComponent> InputIntent;

	/** 已不再写入，移动轴在 InputIntent 中。只为 BP_WuwaCharacterBase 未被调用的 bCancelAttack 函数保留，删掉该函数后可移除。 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Wuwa|PlayerInput")
	FVector2D MoveInput;

#pragma  endregion
	
	
//Component
public:

	// Sets default values for this character's properties
	AWuwaCharacter(const FObjectInitializer& ObjectInitializer);


#pragma region C sharp桥接
	
	//运动状态
	/** 从托管类创建，基类只提供反射/快照桥。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Wuwa|State")
	TObjectPtr<UWuwaUnifiedStateBridgeComponent> UnifiedStateComponent;//运动状态
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category="Wuwa|State")
	TObjectPtr<UWuwaRoleGaitBridgeComponent> RoleGaitComponent;
	UPROPERTY(EditDefaultsOnly, Category="Wuwa|State") 
	TSoftClassPtr<UWuwaUnifiedStateBridgeComponent> UnifiedStateClass;
	UPROPERTY(EditDefaultsOnly, Category="Wuwa|State") 
	TSoftClassPtr<UWuwaRoleGaitBridgeComponent> RoleGaitClass;
	
	//战斗状态
	/** 独立的战斗状态裁决；不与位置/步态状态共用占用记录。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Wuwa|Combat")
	TObjectPtr<UWuwaFightStateBridgeComponent> FightStateComponent;//战斗状态
	UPROPERTY(EditDefaultsOnly, Category = "Wuwa|Combat")
	TSoftClassPtr<UWuwaFightStateBridgeComponent> FightStateClass;
	/** 可重复调用；已存在的组件及其战斗占用不会被重置。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Combat")
	bool EnsureFightStateSystem();
	
	/** 锁定（软锁定挑技能目标），≈ 原作 CharacterLockOnComponent。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Combat")
	TObjectPtr<UWuwaLockOnComponent> LockOnComponent;

	/** 主技能管理，与 FightState 一起自动装配；不从移动状态推导当前技能。 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Wuwa|Combat")
	TObjectPtr<UWuwaSkillBridgeComponent> SkillComponent;
	UPROPERTY(EditDefaultsOnly, Category = "Wuwa|Combat")
	TSoftClassPtr<UWuwaSkillBridgeComponent> SkillClass;
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Combat")
	bool EnsureSkillSystem();
	
	//比如同一个输入会有不同的效果,典型的例子就是普攻分段
	/** 同一个输入根据角色条件转换成具体技能；策略在 C# 输入层执行。 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Wuwa|Combat|Input")
	TObjectPtr<UWuwaInputCommandConfig> InputCommandConfig;

	
#pragma endregion
	
	UFUNCTION(BlueprintCallable, Category="Wuwa|State") bool EnsureMovementStateSystem();


private:
	void BindSkillDependencies();

	//初始化保护句柄
	bool bInitializingMovementState = false;
	bool bInitializingSkill = false;
	bool bInitializingFightState = false;
	
	bool bMovementStateReady = false;
	bool bMovementStateEnding = false;
	bool bFightStateEnding = false;
};
