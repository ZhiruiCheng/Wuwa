#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Game/LGUI/View/WuwaUiPanel.h"
#include "WuwaBossStatePanel.generated.h"

class AWuwaCharactorBase;
class UAbilitySystemComponent;
class UUIItem;
class UWuwaCommonBossStateView;
struct FOnAttributeChangeData;
struct FStreamableHandle;

/**
 * 受击残影（≈ 原作 HpBufferStateMachine，时间换成秒）：掉血后残影先停 DelayTime，再在 DisappearTime 内缩到当前血量；
 * 停顿期间又被打会重新计时，连续超过 RefreshCount 次直接开始缩；缩的过程中又掉血，重新计时缩到新的血量。
 */
struct WUWA_API FWuwaHpBufferStateMachine
{
	float DelayTime = 0.25f;
	float DisappearTime = 0.25f;
	int32 RefreshCount = 8;

	/** 掉血：新比例、掉之前的比例（回血不管）。 */
	void GetHit(float NewPercent, float OldPercent);
	/** 推进；返回残影当前的比例，-1 表示没有残影。 */
	float Update(float DeltaSeconds);
	void Reset() { State = 0; }
	bool IsOriginState() const { return State == 0; }

private:
	int32 State = 0;          // 0 无残影、1 停顿、2 缩
	float DelayElapsed = 0.f;
	int32 HitCount = 0;
	float Target = 0.f;
	float Current = 0.f;
	float Remaining = 0.f;
};

/**
 * 普通 Boss 血条（≈ 原作 CommonBossStateView，预制体 /Game/LGUI/HUD/BossState/UiItem_BossState）：等级、名字、血条、受击残影、护盾、韧性条。
 * 血量变化时更新血条；掉血时残影跟着状态机缩，并播小 / 大受击动画（掉血超过上限的 HitLargeBufferPercent 算大）。
 * 受击 shader（RGBSplit）、Buff 图标、弱点、倒地条、韧性 / 狂暴属性还没做（韧性条先满格）。
 *
 * 根节点上的 WuwaUiComponentsRegistry：0 等级、1 名字、2 血条、3 残影节点（裁剪）、4 残影条、5 护盾条、6 韧性区、7 韧性条、8 小受击动画、9 大受击动画。
 */
UCLASS()
class WUWA_API UWuwaCommonBossStateView : public UWuwaUiPanel
{
	GENERATED_BODY()

public:
	void Activate(AWuwaCharactorBase* InBoss);
	void Deactivate();
	void Tick(float DeltaSeconds);

	AWuwaCharactorBase* GetBoss() const { return Boss.Get(); }
	float GetCurrentBarPercent() const { return CurrentBarPercent; }
	/** 残影节点正在显示（测试用）。 */
	bool IsBufferVisible() const;
	/** 等级 / 名字文字（测试用）。 */
	FString GetLevelText() const;
	FString GetNameText() const;
	/** 最近一次播的受击动画（8 小 / 9 大，-1 没播）。 */
	int32 GetLastHitAnimation() const { return LastHitAnimation; }

protected:
	virtual void OnRegisterComponent() override;
	virtual void OnCreate() override;

private:
	void RefreshHpAndShield(bool bAnimate);
	void SetHpBarPercent(float Percent);
	void SetBarBufferPercent(float From, float To);
	void HideBuffer();
	void PlayBarAnimation(float Percent);
	void RefreshNameAndLevel();
	void HandleHealthChanged(const FOnAttributeChangeData& Data);
	void HandleLevelChanged(const FOnAttributeChangeData& Data);
	float GetAttribute(const struct FGameplayAttribute& Attribute) const;

	TWeakObjectPtr<AWuwaCharactorBase> Boss;
	TWeakObjectPtr<UAbilitySystemComponent> BossAbilities;
	FDelegateHandle HealthHandle;
	FDelegateHandle MaxHealthHandle;
	FDelegateHandle LevelHandle;
	FWuwaHpBufferStateMachine Buffer;
	float CurrentBarPercent = 1.f;
	float BarWidth = 0.f;
	bool bBufferAnimating = false;
	int32 LastHitAnimation = -1;
};

/**
 * Boss 血条面板（≈ 原作 BossStatePanel，主 HUD 的子模块）：每帧在实体表里找 BossStateViewType 非 0 的怪物，
 * 离当前角色最近、在显示距离内的那个显示大血条；已显示的超出最大距离才收起。
 */
UCLASS()
class WUWA_API UWuwaBossStatePanel : public UObject
{
	GENERATED_BODY()

public:
	/** 异步加载血条预制体，挂到 Parent 下（主 HUD 根节点）。 */
	void Initialize(UUIItem* Parent);
	void Shutdown();

	/** 当前显示的 Boss；没有为空。 */
	AWuwaCharactorBase* GetShownBoss() const;
	UWuwaCommonBossStateView* GetView() const { return View; }
	/** 推进一帧（平时由 Ticker 调；测试可以手动调）。 */
	void Tick(float DeltaSeconds);

private:
	AWuwaCharactorBase* FindBoss() const;
	void OnPrefabLoaded(TWeakObjectPtr<UUIItem> Parent);

	UPROPERTY(Transient) TObjectPtr<UWuwaCommonBossStateView> View;
	TWeakObjectPtr<AWuwaCharactorBase> Shown;
	TSharedPtr<FStreamableHandle> LoadHandle;
	FTSTicker::FDelegateHandle TickHandle;
};
