#include "Game/Module/Hud/Buff/WuwaHudBuffBar.h"

#include "Core/ActorComponent/UIItem.h"
#include "Engine/World.h"
#include "Game/LGUI/Core/WuwaUiLog.h"
#include "Game/Module/BattleUi/WuwaBattleUiSettings.h"
#include "Game/Module/Hud/Buff/WuwaHudBuffItem.h"
#include "PrefabSystem/LGUIPrefab.h"

void UWuwaHudBuffBar::Init(UUIItem* InContainer, TSet<int32> InCueTypes, int32 InMaxCount, float InBottomPadding)
{
	Container = InContainer;
	CueTypes = MoveTemp(InCueTypes);
	MaxCount = FMath::Max(1, InMaxCount);
	BottomPadding = InBottomPadding;
}

void UWuwaHudBuffBar::SetBuffs(const TArray<FWuwaBuffUiData>& Buffs, bool bAnimate)
{
	if (!Container) return;
	TArray<FWuwaBuffUiData> Wanted = Buffs.FilterByPredicate([this](const FWuwaBuffUiData& Buff) { return CueTypes.Contains(Buff.CueType); });
	// 原作 BuffItemInfo.Compare：优先级大的在前，同优先级新加的（SortId 大）在前
	Wanted.Sort([](const FWuwaBuffUiData& A, const FWuwaBuffUiData& B) { return A.Priority != B.Priority ? A.Priority > B.Priority : A.SortId > B.SortId; });

	// 去掉的：收起（播关闭动画的先放进 Hiding，等动画播完再回收）
	TArray<FEntry> Old = MoveTemp(Entries);
	for (FEntry& Entry : Old)
	{
		const bool bKept = Wanted.ContainsByPredicate([&Entry](const FWuwaBuffUiData& Buff) { return Buff.IsSameIcon(Entry.Data); });
		if (bKept || !Entry.Item) continue;
		Entry.Item->Deactivate(bAnimate);
		(bAnimate ? Hiding : Pool).Add(Entry.Item);
		Entry.Item = nullptr;
	}
	// 新的顺序：还在的沿用原来的图标实例
	for (const FWuwaBuffUiData& Buff : Wanted)
	{
		FEntry& Entry = Entries.AddDefaulted_GetRef();
		Entry.Data = Buff;
		if (FEntry* Previous = Old.FindByPredicate([&Buff](const FEntry& O) { return O.Item && O.Data.IsSameIcon(Buff); }))
		{
			Entry.Item = Previous->Item;
			Entry.Item->UpdateData(Buff);
		}
	}
	// 只显示前 MaxCount 个：挤出去的收起（不播动画），新进来的建图标
	ActiveItems.Reset();
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		FEntry& Entry = Entries[Index];
		if (Index >= MaxCount)
		{
			if (Entry.Item)
			{
				Entry.Item->Deactivate(false);
				Pool.Add(Entry.Item);
				Entry.Item = nullptr;
			}
			continue;
		}
		if (!Entry.Item)
		{
			const bool bNew = !Old.ContainsByPredicate([&Entry](const FEntry& O) { return O.Data.IsSameIcon(Entry.Data); });
			Entry.Item = AcquireItem();
			if (!Entry.Item) continue;
			Entry.Item->Activate(Entry.Data, bAnimate && bNew);
		}
		ActiveItems.Add(Entry.Item);
	}
	Layout();
	if (!ActiveItems.IsEmpty() || !Hiding.IsEmpty()) StartTicking();
}

void UWuwaHudBuffBar::Clear()
{
	StopTicking();
	auto Destroy = [](UWuwaHudBuffItem* Item) { if (Item && Item->GetActor()) Item->GetActor()->Destroy(); };
	for (FEntry& Entry : Entries) Destroy(Entry.Item);
	for (UWuwaHudBuffItem* Item : Hiding) Destroy(Item);
	for (UWuwaHudBuffItem* Item : Pool) Destroy(Item);
	Entries.Reset();
	Hiding.Reset();
	Pool.Reset();
	ActiveItems.Reset();
}

TArray<UWuwaHudBuffItem*> UWuwaHudBuffBar::GetShownItems() const
{
	TArray<UWuwaHudBuffItem*> Items;
	for (const FEntry& Entry : Entries) if (Entry.Item) Items.Add(Entry.Item);
	return Items;
}

void UWuwaHudBuffBar::BeginDestroy()
{
	StopTicking();
	Super::BeginDestroy();
}

UWuwaHudBuffItem* UWuwaHudBuffBar::AcquireItem()
{
	if (Pool.Num() > 0) return Pool.Pop();
	// 预制体在加载界面的预热阶段已经加载（UWuwaCombatWarmupSubsystem 扫战斗 UI 设置的软引用）
	ULGUIPrefab* Prefab = GetDefault<UWuwaBattleUiSettings>()->BuffItemPrefab.LoadSynchronous();
	UWorld* World = Container ? Container->GetWorld() : nullptr;
	AActor* Actor = Prefab && World ? Prefab->LoadPrefab(World, Container, true) : nullptr;
	UWuwaHudBuffItem* Item = NewObject<UWuwaHudBuffItem>(this);
	if (!Item->Init(Actor))
	{
		UE_LOG(LogWuwaUI, Warning, TEXT("Buff bar: buff item prefab %s has no usable registry."), *GetNameSafe(Prefab));
		if (Actor) Actor->Destroy();
		return nullptr;
	}
	return Item;
}

void UWuwaHudBuffBar::Layout()
{
	// 从左到右一行、整体居中（容器轴心在中间、宽度跟子节点走），底部留 BottomPadding
	const int32 Count = ActiveItems.Num();
	for (int32 Index = 0; Index < Count; ++Index)
	{
		UUIItem* Root = ActiveItems[Index]->GetRoot();
		FUIAnchorData Data = Root->GetAnchorData();
		const float Width = Root->GetWidth();
		Data.AnchorMin = Data.AnchorMax = FVector2D(0.5f, 0.f);
		Data.Pivot = FVector2D(0.5f, 0.f);
		Data.AnchoredPosition = FVector2D((Index - (Count - 1) * 0.5f) * Width, BottomPadding);
		Root->SetAnchorData(Data);
		Root->SetHierarchyIndex(Index);
	}
}

bool UWuwaHudBuffBar::Tick(float DeltaSeconds)
{
	const UWorld* World = Container ? Container->GetWorld() : nullptr;
	if (!World)
	{
		TickerHandle.Reset();
		return false;
	}
	const double Now = World->GetTimeSeconds();
	for (const FEntry& Entry : Entries) if (Entry.Item) Entry.Item->Tick(Now);
	for (int32 Index = Hiding.Num() - 1; Index >= 0; --Index)
	{
		if (Hiding[Index]->TickHiding(DeltaSeconds)) continue;
		Pool.Add(Hiding[Index]);
		Hiding.RemoveAt(Index);
	}
	if (ActiveItems.IsEmpty() && Hiding.IsEmpty())
	{
		TickerHandle.Reset();
		return false;
	}
	return true;
}

void UWuwaHudBuffBar::StartTicking()
{
	if (TickerHandle.IsValid()) return;
	TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float DeltaSeconds) { return Tick(DeltaSeconds); }));
}

void UWuwaHudBuffBar::StopTicking()
{
	if (!TickerHandle.IsValid()) return;
	FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
	TickerHandle.Reset();
}
