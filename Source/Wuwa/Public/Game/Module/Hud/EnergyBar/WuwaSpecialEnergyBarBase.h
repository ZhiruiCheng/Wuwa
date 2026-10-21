#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Game/LGUI/View/WuwaUiPanel.h"
#include "Game/Module/Hud/WuwaHudTypes.h"
#include "WuwaSpecialEnergyBarBase.generated.h"

struct FWuwaTeamMemberUiData;

/**
 * 能量条比例的平滑（≈ 原作 SpecialEnergyBarPercentMachine）：涨的时候 0.1 秒内追到目标，降的时候立刻跳过去。
 */
struct WUWA_API FWuwaEnergyPercentMachine
{
	void Init(float Percent) { Current = Target = Percent; Speed = 0.f; }
	void SetTarget(float Percent);
	/** 返回这一帧比例有没有变。 */
	bool Update(float DeltaSeconds);
	float GetCurrent() const { return Current; }
	float GetTarget() const { return Target; }

private:
	float Current = 0.f;
	float Target = 0.f;
	float Speed = 0.f;
};

/**
 * 角色特殊能量条的基类（≈ 原作 SpecialEnergyBarBase）：
 * - 数据：DT_SpecialEnergyBarInfo 的一行（FWuwaSpecialEnergyBarInfo）+ 跟着哪个队员（序号）；
 * - 数值：队员的特殊能量属性（Config.SpecialEnergyIndex 那一对），经 UWuwaFormationModel / Event.Formation.MemberAttributes 拿到，界面不碰 ASC；
 * - 平滑：FWuwaEnergyPercentMachine，显示期间每帧更新；
 * - 子类重写 OnBarPercentChanged / OnKeyEnableChanged 刷新自己的样子；"按键可用" = 比例 ≥ DisableKeyOnPercent%。
 * 由 UWuwaHudRoleStatePanel 在能量条预制体生成后创建（InitData → CreateByActor → Start → Show），换角色时整个销毁重建。
 */
UCLASS(Abstract)
class WUWA_API UWuwaSpecialEnergyBarBase : public UWuwaUiPanel
{
	GENERATED_BODY()

public:
	/** 在 CreateByActor 之前调用（子类的 OnRegisterComponent 会用到 Config，比如格数）。 */
	void InitData(int32 InMemberIndex, const FWuwaSpecialEnergyBarInfo& InConfig);
	/** 同一种能量条换给另一个队员（两个角色用同一行时）。 */
	virtual void SetMemberIndex(int32 InMemberIndex);

	const FWuwaSpecialEnergyBarInfo& GetConfig() const { return Config; }
	int32 GetMemberIndex() const { return MemberIndex; }
	float GetCurrentPercent() const { return PercentMachine.GetCurrent(); }
	/** ≈ 原作 GetKeyEnable：比例达到 DisableKeyOnPercent%（满）才可用。 */
	bool GetKeyEnable() const;

protected:
	virtual void OnAfterShow() override;
	virtual void OnBeforeHide() override;
	virtual void OnBeforeDestroy() override;
	virtual void OnAddEventListener() override;

	// —— 子类钩子（和原作同名）——
	virtual void OnInitData() {}
	virtual void OnBarPercentChanged() {}
	virtual void OnKeyEnableChanged() {}
	/** 这个条要显示的比例；默认是 Config.SpecialEnergyIndex 那一对特殊能量。 */
	virtual float GetTargetPercent(const FWuwaTeamMemberUiData& Member) const;
	/** 队员数据变了（显示时和每次 MemberAttributes 事件）；默认更新主比例。专属条可以顺便读别的属性。 */
	virtual void OnMemberData(const FWuwaTeamMemberUiData& Member, bool bInitial);
	/** 显示期间每帧调用。 */
	virtual void TickBar(float DeltaSeconds);

	const FWuwaTeamMemberUiData* GetMember() const;

	FWuwaSpecialEnergyBarInfo Config;
	int32 MemberIndex = INDEX_NONE;
	FWuwaEnergyPercentMachine PercentMachine;

private:
	void StartTicking();
	void StopTicking();
	void ApplyKeyEnable(bool bForce);

	FTSTicker::FDelegateHandle TickHandle;
	bool bLastKeyEnable = false;
};
