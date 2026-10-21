#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WuwaFormationComponent.generated.h" // 必须是最后一个 include

struct FStreamableHandle;

class UWuwaRoleDataAsset;
class AWuwaCharacter;


//定义新委托
DECLARE_MULTICAST_DELEGATE_ThreeParams(FWuwaOnRoleSwitched, AWuwaCharacter* /*Old*/, AWuwaCharacter* /*New*/, int32 /*NewIndex*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FWuwaOnMembersChanged, const TArray<TObjectPtr<AWuwaCharacter>>& /*最新成员*/);

UCLASS(ClassGroup = (Wuwa), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaFormationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	
	//Core
	UWuwaFormationComponent();
	virtual void BeginPlay() override;
	
	virtual void GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	//InitRoles
	/**
	 * 服务器：先异步加载全部队员的角色资源（不卡游戏线程，加载界面能显示进度），加载完在 SpawnTransform 生成队员、控制第 0 个。
	 * OnSpawned(true) 表示队员已生成并且 PC 已控制第 0 个；失败（没有可用队员）传 false。
	 */
	void InitFormation(const FTransform& SpawnTransform, TFunction<void(bool bSuccess)> OnSpawned = nullptr);

	/** 异步预加载 InitialRoles 的角色资源；服务器在 InitFormation 里调，本地客户端在 BeginPlay 里调（角色复制过来前先把资源准备好）。重复调用无副作用。 */
	void PreloadRoles();
	/** 角色资源加载进度 0~1（加载界面用）。 */
	float GetLoadProgress() const;
	/** 队员都在、当前角色有效：本地可以开始玩了（客户端要等复制过来）。 */
	bool IsFormationReady() const;
	
	//Switch Pole
	void RequestSwitch(int32 Index);
	
	bool CanSwitchTo(int32 Index) const;
	
	FWuwaOnRoleSwitched OnRoleSwitched;         // 服务器和客户端都会广播
	FWuwaOnMembersChanged OnMembersChanged;     // 服务器生成队伍后、客户端收到 Members 复制后广播
	//Tools about Members（GameMode、UI 和测试要读，保持 public）
	AWuwaCharacter* GetCurrentRole() const;
	
	const TArray<TObjectPtr<AWuwaCharacter>>& GetMembers() const{return Members;}
	
	int32 GetCurrentIndex() const { return CurrentIndex; }
	
protected:
	UFUNCTION(Server, Reliable)
	void Server_RequestSwitch(int32 Index);

	void SwitchTo(int32 Index);                 // 只在服务器

	/** 旧角色暂留时新角色的上场位置（原作 RoleQteComponent.SetQtePosition 的地面分支）。 */
	FVector FindStaySwitchLocation(const AWuwaCharacter* Old, const AWuwaCharacter* New) const;

	/** 原作 SceneTeamDefine.SPECIAL_CHANGE_ANGLE_LAND / SPECIAL_CHANGE_DIS_LAND。 */
	static constexpr float StaySwitchAngle = 80.f;
	static constexpr float StaySwitchDistance = 200.f;
	
	UFUNCTION()
	void OnRep_CurrentIndex(int32 OldIndex);
	
	UFUNCTION()
	void OnRep_Members();
	
	UPROPERTY(EditDefaultsOnly, Category = "Wuwa|Formation", meta = (ClampMin = "0"))
	float SwitchCooldown = 1.0f;
	
protected:
	/** 初始队伍配置；以后可以改成从存档或编队界面读取。 */
	UPROPERTY(EditDefaultsOnly, Category = "Wuwa|Formation")
	TArray<TObjectPtr<UWuwaRoleDataAsset>> InitialRoles;

	//真正存放的队伍,当前读取好的队伍列表
	UPROPERTY(ReplicatedUsing = OnRep_Members)
	TArray<TObjectPtr<AWuwaCharacter>> Members;

	//当前正在运行的角色下标
	UPROPERTY(ReplicatedUsing = OnRep_CurrentIndex)
	int32 CurrentIndex = INDEX_NONE;
	
	
private:
	/** 角色资源加载完：按 SpawnTransform 生成队员并控制第 0 个。 */
	void SpawnMembers(const FTransform& SpawnTransform, TFunction<void(bool)> OnSpawned);

	TSharedPtr<FStreamableHandle> RolesLoadHandle;
	bool bRolesLoaded = false;
	/** 角色资源加载期间排队的生成请求。 */
	bool bSpawnPending = false;
	TArray<TFunction<void()>> OnRolesLoaded;

	double LastSwitchTime = -1.0e9;             // 服务器：上次成功切人的时间
	double LastRequestTime = -1.0e9;            // 本地：上次发请求的时间，只用于客户端的预先检查
};