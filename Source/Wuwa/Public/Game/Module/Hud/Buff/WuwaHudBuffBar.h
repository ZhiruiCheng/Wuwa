#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "UObject/Object.h"
#include "Game/NewWorld/Buff/WuwaBuffTypes.h"
#include "WuwaHudBuffBar.generated.h"

class UUIItem;
class UWuwaHudBuffItem;

/**
 * 一排 buff 图标（≈ 原作 BuffItemContainer，RoleUniqueBuffView / RoleBuffView 各有一个）。
 * - 数据：SetBuffs 给当前角色的全部 buff 图标（来自 UWuwaFormationModel），按 CueTypes 过滤；
 *   原作分工：专属 buff 栏（血条上方 PnlBuffTop）只收 CueType 24，普通 buff 栏（PnlBuff）收 2 / 14 / 37。
 * - 排序：优先级大的在前，同优先级新加的在前（原作 BuffItemInfo.Compare）；只显示前 MaxCount 个。
 * - 新加的播出现动画、去掉的播关闭动画（切人整体刷新时不播，原作 RefreshBuff）。
 * - 摆放：在容器里从左到右排成一行、整体居中，底部留 BottomPadding（原作容器上的 UIHorizontalLayout：WidthFitToChildren、轴心居中）。
 * 图标实例来自 UWuwaBattleUiSettings::BuffItemPrefab，收起后回收复用。
 */
UCLASS()
class WUWA_API UWuwaHudBuffBar : public UObject
{
	GENERATED_BODY()

public:
	void Init(UUIItem* InContainer, TSet<int32> InCueTypes, int32 InMaxCount, float InBottomPadding);
	/** 换成这组 buff；bAnimate：新加 / 去掉的播动画。 */
	void SetBuffs(const TArray<FWuwaBuffUiData>& Buffs, bool bAnimate);
	/** 全部收起并销毁（界面销毁时）。 */
	void Clear();

	/** 正在显示的图标（排好序的）。测试用。 */
	TArray<UWuwaHudBuffItem*> GetShownItems() const;

	virtual void BeginDestroy() override;

private:
	bool Tick(float DeltaSeconds);
	void StartTicking();
	void StopTicking();
	UWuwaHudBuffItem* AcquireItem();
	void Layout();

	UPROPERTY() TObjectPtr<UUIItem> Container;
	/** 和显示顺序一致：前 MaxCount 个有图标实例。 */
	struct FEntry
	{
		FWuwaBuffUiData Data;
		TObjectPtr<UWuwaHudBuffItem> Item;
	};
	TArray<FEntry> Entries;
	UPROPERTY() TArray<TObjectPtr<UWuwaHudBuffItem>> Hiding;
	UPROPERTY() TArray<TObjectPtr<UWuwaHudBuffItem>> Pool;
	/** Entries 里的图标实例（GC 用）。 */
	UPROPERTY() TArray<TObjectPtr<UWuwaHudBuffItem>> ActiveItems;
	TSet<int32> CueTypes;
	int32 MaxCount = 10;
	float BottomPadding = 0.f;
	FTSTicker::FDelegateHandle TickerHandle;
};
