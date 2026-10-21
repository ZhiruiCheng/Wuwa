#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "ActiveGameplayEffectHandle.h"
#include "Game/NewWorld/Buff/WuwaBuffTypes.h"
#include "Game/LGUI/Mvc/WuwaControllerBase.h"
#include "WuwaFormationUiController.generated.h"

class AWuwaCharacter;
class APlayerController;
class UAbilitySystemComponent;
class UWuwaFormationComponent;
struct FOnAttributeChangeData;

/**
 * 队伍 UI 的业务（≈ 鸣潮 FormationController 里给界面服务的部分）：
 * 把 PlayerController 上的 UWuwaFormationComponent 和每个队员 ASC 的属性（血量、等级、元素、能量），翻译成 UWuwaFormationModel + Event.Formation.* 事件。
 * 界面不直接找 PC、角色、ASC。
 *
 * 绑定分三层，因为各自出现的时间不同：
 * 本地玩家拿到 PC（ULocalPlayer::OnPlayerControllerChanged）→ PC 上的队伍组件（OnMembersChanged / OnRoleSwitched）→ 每个队员的 ASC（Health / MaxHealth）。
 *
 * buff 图标（≈ 原作 CharOnBuffAddUITexture → RoleBuffView / RoleUniqueBuffView）：每个队员的 ASC 上监听 GE 加上 / 移除（客户端也会触发），
 * 把带 UWuwaBuffInfoComponent、cue 是 UI 类型的活动 GE 整理成 FWuwaBuffUiData 写进 Model，发 Event.Formation.MemberBuffs。
 * buff 存在各自角色身上，所以切回某个角色时它没过期的 buff 和剩余时间都还在，界面重新读一次就行。
 */
UCLASS()
class WUWA_API UWuwaFormationUiController : public UWuwaControllerBase
{
	GENERATED_BODY()

public:
	//上层初始化,初始化依赖Subsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	
	/** 界面请求切到第 Index 个队员（比如点了队伍栏头像）；能不能切由队伍组件决定。 */
	void RequestSwitchTo(int32 Index);

protected:
	//初始化,绑定PC变化函数到HandlePlayerControllerChanged
	virtual void OnInit() override;
	//解绑PC更新函数和ASC函数以及Formation组件的更新函数
	virtual void OnClear() override;
	//就比上面的那个少了个PC解绑
	virtual void OnLeaveLevel() override;

private:
	
	void HandlePlayerControllerChanged(APlayerController* NewController);
	void BindFormation(UWuwaFormationComponent* NewFormation);
	void UnbindFormation();

	void HandleMembersChanged(const TArray<TObjectPtr<AWuwaCharacter>>& NewMembers);
	void HandleRoleSwitched(AWuwaCharacter* OldRole, AWuwaCharacter* NewRole, int32 NewIndex);
	void HandleHealthChanged(int32 Index);
	void HandleAttributesChanged(int32 Index);
	/** 重新整理第 Index 个队员的 buff 图标（Removed：正在被移除、还没从 ASC 里删掉的那个 GE）。 */
	void HandleBuffsChanged(int32 Index, FActiveGameplayEffectHandle Removed = FActiveGameplayEffectHandle());
public:
	/** 一个 ASC 上全部带 UI cue 的 buff 图标（测试也用）。 */
	static TArray<FWuwaBuffUiData> CollectBuffs(const UAbilitySystemComponent* ASC, FActiveGameplayEffectHandle Excluded = FActiveGameplayEffectHandle());
private:
	void UnbindMemberHealth();
	
	/** 从队伍组件重新生成整份快照、重新绑定血量，并发 Members 事件。 */
	void RebuildMembers();

	FDelegateHandle PlayerControllerChangedHandle;

	TWeakObjectPtr<UWuwaFormationComponent> Formation;
	FDelegateHandle MembersChangedHandle;
	FDelegateHandle RoleSwitchedHandle;

	struct FHealthBinding
	{
		TWeakObjectPtr<UAbilitySystemComponent> ASC;
		FDelegateHandle Health;
		FDelegateHandle MaxHealth;
		/** 血量以外的显示属性（WuwaFormationUi::DisplayAttributes）各自的监听。 */
		TArray<TPair<FGameplayAttribute, FDelegateHandle>> Attributes;
		/** GE 加上 / 移除（buff 图标）。 */
		FDelegateHandle EffectAdded;
		FDelegateHandle EffectRemoved;
	};
	TArray<FHealthBinding> HealthBindings;
};
