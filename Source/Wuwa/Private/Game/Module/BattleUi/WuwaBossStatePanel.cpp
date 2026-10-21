#include "Game/Module/BattleUi/WuwaBossStatePanel.h"

#include "AbilitySystemComponent.h"
#include "Core/ActorComponent/UIItem.h"
#include "Core/ActorComponent/UIText.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Game/LGUI/Component/WuwaUiSlicedFill.h"
#include "Game/LGUI/Component/WuwaUiTweenGroup.h"
#include "Game/Module/BattleUi/WuwaBattleUiSettings.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAttributeSet.h"
#include "Game/NewWorld/Character/Common/WuwaCharactorBase.h"
#include "Game/NewWorld/Combat/WuwaCreatureSubsystem.h"
#include "PrefabSystem/LGUIPrefab.h"

namespace WuwaBossState
{
	enum : int32 { Level = 0, Name, HpBar, BufferItem, BufferBar, ShieldBar, ToughArea, ToughBar, SmallHit, BigHit };
}

// ---------------------------------------------------------------- FWuwaHpBufferStateMachine

void FWuwaHpBufferStateMachine::GetHit(float NewPercent, float OldPercent)
{
	if (OldPercent < NewPercent) return;
	if (State == 0)
	{
		State = 1;
		HitCount = 1;
		DelayElapsed = 0.f;
		Current = OldPercent;
		Target = NewPercent;
		Remaining = DisappearTime;
	}
	else if (State == 1)
	{
		if (++HitCount > RefreshCount) State = 2;
		DelayElapsed = 0.f;
		Target = NewPercent;
	}
	else
	{
		Target = NewPercent;
		Remaining = DisappearTime;
	}
}

float FWuwaHpBufferStateMachine::Update(float DeltaSeconds)
{
	if (State == 2)
	{
		const float Rate = Remaining > 0.f ? (Current - Target) / Remaining : 0.f;
		Current -= DeltaSeconds * Rate;
		Remaining -= DeltaSeconds;
		if (Remaining <= 0.f)
		{
			State = 0;
			return Target;
		}
		return Current;
	}
	if (State == 1)
	{
		DelayElapsed += DeltaSeconds;
		if (DelayElapsed > DelayTime) State = 2;
		return Current;
	}
	return -1.f;
}

// ---------------------------------------------------------------- UWuwaCommonBossStateView

void UWuwaCommonBossStateView::OnRegisterComponent()
{
	ComponentRegisterInfos.Add({WuwaBossState::Level, UUIText::StaticClass()});
	ComponentRegisterInfos.Add({WuwaBossState::Name, UUIText::StaticClass()});
	for (int32 Index = WuwaBossState::HpBar; Index <= WuwaBossState::BigHit; ++Index)
	{
		ComponentRegisterInfos.Add({Index, UUIItem::StaticClass()});
	}
}

void UWuwaCommonBossStateView::OnCreate()
{
	const UWuwaBattleUiSettings* Settings = GetDefault<UWuwaBattleUiSettings>();
	Buffer.DelayTime = Settings->HitDelayTime;
	Buffer.DisappearTime = Settings->HitBufferDisappearTime;
	Buffer.RefreshCount = Settings->HitRefreshTime;
	// 残影宽度按血条底板算（原作 pnt = 残影节点父节点的宽度）
	if (UUIItem* BufferItem = GetItem(WuwaBossState::BufferItem))
	{
		if (UUIItem* Parent = BufferItem->GetParentUIItem()) BarWidth = Parent->GetWidth();
	}
	HideBuffer();
	// 韧性 / 狂暴属性还没有：韧性条满格
	if (UUIItem* Tough = GetItem(WuwaBossState::ToughBar)) UWuwaUiSlicedFill::SetFillAmountOn(Tough, 1.f);
}

void UWuwaCommonBossStateView::Activate(AWuwaCharactorBase* InBoss)
{
	Deactivate();
	Boss = InBoss;
	UAbilitySystemComponent* Abilities = InBoss ? InBoss->GetAbilitySystemComponent() : nullptr;
	BossAbilities = Abilities;
	if (Abilities)
	{
		HealthHandle = Abilities->GetGameplayAttributeValueChangeDelegate(UWuwaAttributeSet::GetHealthAttribute())
			.AddUObject(this, &UWuwaCommonBossStateView::HandleHealthChanged);
		MaxHealthHandle = Abilities->GetGameplayAttributeValueChangeDelegate(UWuwaAttributeSet::GetMaxHealthAttribute())
			.AddUObject(this, &UWuwaCommonBossStateView::HandleHealthChanged);
		// 原作 OnLevelChanged：等级变了刷新等级文字和威胁色
		LevelHandle = Abilities->GetGameplayAttributeValueChangeDelegate(UWuwaAttributeSet::GetLevelAttribute())
			.AddUObject(this, &UWuwaCommonBossStateView::HandleLevelChanged);
	}
	RefreshNameAndLevel();
	CurrentBarPercent = 1.f;
	RefreshHpAndShield(false);
}

void UWuwaCommonBossStateView::Deactivate()
{
	if (UAbilitySystemComponent* Abilities = BossAbilities.Get())
	{
		Abilities->GetGameplayAttributeValueChangeDelegate(UWuwaAttributeSet::GetHealthAttribute()).Remove(HealthHandle);
		Abilities->GetGameplayAttributeValueChangeDelegate(UWuwaAttributeSet::GetMaxHealthAttribute()).Remove(MaxHealthHandle);
		Abilities->GetGameplayAttributeValueChangeDelegate(UWuwaAttributeSet::GetLevelAttribute()).Remove(LevelHandle);
	}
	BossAbilities.Reset();
	Boss.Reset();
	HideBuffer();
	LastHitAnimation = -1;
}

float UWuwaCommonBossStateView::GetAttribute(const FGameplayAttribute& Attribute) const
{
	const UAbilitySystemComponent* Abilities = BossAbilities.Get();
	return Abilities ? Abilities->GetNumericAttribute(Attribute) : 0.f;
}

void UWuwaCommonBossStateView::HandleHealthChanged(const FOnAttributeChangeData& Data)
{
	RefreshHpAndShield(Data.Attribute == UWuwaAttributeSet::GetHealthAttribute());
}

void UWuwaCommonBossStateView::HandleLevelChanged(const FOnAttributeChangeData& Data)
{
	RefreshNameAndLevel();
}

void UWuwaCommonBossStateView::RefreshNameAndLevel()
{
	const AWuwaCharactorBase* Target = Boss.Get();
	if (!Target) return;
	const UWuwaBattleUiSettings* Settings = GetDefault<UWuwaBattleUiSettings>();
	const int32 BossLevel = FMath::RoundToInt(GetAttribute(UWuwaAttributeSet::GetLevelAttribute()));
	// 威胁色（原作 GetThreadColor(等级, 1)）：队伍里最高等级 − 怪物等级
	int32 Highest = 0;
	if (const UWuwaCreatureSubsystem* Creatures = UWuwaCreatureSubsystem::Get(Target))
	{
		for (const TWeakObjectPtr<AWuwaCharactorBase>& Entry : Creatures->GetCreatures())
		{
			const AWuwaCharactorBase* Creature = Entry.Get();
			if (!Creature || Creature->Camp != FName(TEXT("Player")) || !Creature->GetAbilitySystemComponent()) continue;
			Highest = FMath::Max(Highest, FMath::RoundToInt(Creature->GetAbilitySystemComponent()->GetNumericAttribute(UWuwaAttributeSet::GetLevelAttribute())));
		}
	}
	const int32 Gap = Highest - BossLevel;
	const FColor Color = Gap >= Settings->ThreatLevel1 ? Settings->ThreatLevelColor3
		: Gap >= Settings->ThreatLevel3 ? Settings->ThreatLevelColor2 : Settings->ThreatLevelColor1;
	if (UUIText* LevelText = GetText(WuwaBossState::Level))
	{
		LevelText->SetText(FText::FromString(Settings->LevelFormat.Replace(TEXT("{0}"), *FString::FromInt(BossLevel))));
		LevelText->SetColor(Color);
	}
	if (UUIText* NameText = GetText(WuwaBossState::Name))
	{
		NameText->SetText(Target->DisplayName.IsEmpty() ? FText::FromString(Target->GetName()) : Target->DisplayName);
		NameText->SetColor(Color);
	}
}

void UWuwaCommonBossStateView::RefreshHpAndShield(bool bAnimate)
{
	const float MaxHealth = GetAttribute(UWuwaAttributeSet::GetMaxHealthAttribute());
	const float Percent = MaxHealth > 0.f ? FMath::Clamp(GetAttribute(UWuwaAttributeSet::GetHealthAttribute()) / MaxHealth, 0.f, 1.f) : 0.f;
	SetHpBarPercent(Percent);
	// 护盾（项目还没有护盾）：0 就隐藏
	if (UUIItem* Shield = GetItem(WuwaBossState::ShieldBar)) Shield->SetIsUIActive(false);
	if (bAnimate) PlayBarAnimation(Percent);
	else HideBuffer();
	CurrentBarPercent = Percent;
}

void UWuwaCommonBossStateView::SetHpBarPercent(float Percent)
{
	UWuwaUiSlicedFill::SetFillAmountOn(GetItem(WuwaBossState::HpBar), Percent);
}

void UWuwaCommonBossStateView::PlayBarAnimation(float Percent)
{
	if (Percent >= CurrentBarPercent) return;
	const bool bWasOrigin = Buffer.IsOriginState();
	Buffer.GetHit(Percent, CurrentBarPercent);
	if (bWasOrigin && !Buffer.IsOriginState()) SetBarBufferPercent(CurrentBarPercent, CurrentBarPercent);
	bBufferAnimating = true;
	// 原作：一次掉的比例小于 HitLargeBufferPercent 播小受击，否则大受击
	LastHitAnimation = CurrentBarPercent - Percent < GetDefault<UWuwaBattleUiSettings>()->HitLargeBufferPercent ? WuwaBossState::SmallHit : WuwaBossState::BigHit;
	if (const AActor* AnimActor = GetRegisteredActor(LastHitAnimation))
	{
		if (UWuwaUiTweenGroup* Group = AnimActor->FindComponentByClass<UWuwaUiTweenGroup>()) Group->Play();
	}
}

void UWuwaCommonBossStateView::SetBarBufferPercent(float From, float To)
{
	// 原作 SetBarBufferPercent：残影节点覆盖 [To, From] 这一段（从左算），里面的残影条反向偏移保持贴图不动
	const float FromWidth = BarWidth * From;
	const float Width = FromWidth - BarWidth * To;
	const float Offset = FromWidth - (BarWidth + Width) / 2.f;
	UUIItem* BufferItem = GetItem(WuwaBossState::BufferItem);
	UUIItem* BufferBar = GetItem(WuwaBossState::BufferBar);
	if (!BufferItem) return;
	BufferItem->SetAnchoredPosition(FVector2D(Offset, BufferItem->GetAnchoredPosition().Y));
	BufferItem->SetWidth(Width);
	BufferItem->SetIsUIActive(true);
	if (BufferBar) BufferBar->SetAnchoredPosition(FVector2D(-Offset, BufferBar->GetAnchoredPosition().Y));
}

void UWuwaCommonBossStateView::HideBuffer()
{
	if (UUIItem* BufferItem = GetItem(WuwaBossState::BufferItem)) BufferItem->SetIsUIActive(false);
	Buffer.Reset();
	bBufferAnimating = false;
}

FString UWuwaCommonBossStateView::GetLevelText() const
{
	const UUIText* Text = GetText(WuwaBossState::Level);
	return Text ? Text->GetText().ToString() : FString();
}

FString UWuwaCommonBossStateView::GetNameText() const
{
	const UUIText* Text = GetText(WuwaBossState::Name);
	return Text ? Text->GetText().ToString() : FString();
}

bool UWuwaCommonBossStateView::IsBufferVisible() const
{
	const UUIItem* BufferItem = GetItem(WuwaBossState::BufferItem);
	return BufferItem && BufferItem->GetIsUIActiveSelf();
}

void UWuwaCommonBossStateView::Tick(float DeltaSeconds)
{
	if (!bBufferAnimating) return;
	const float Value = Buffer.Update(DeltaSeconds);
	if (Value < 0.f) HideBuffer();
	else if (Value <= 1.f) SetBarBufferPercent(Value, CurrentBarPercent);
}

// ---------------------------------------------------------------- UWuwaBossStatePanel

void UWuwaBossStatePanel::Initialize(UUIItem* Parent)
{
	const TSoftObjectPtr<ULGUIPrefab> Prefab = GetDefault<UWuwaBattleUiSettings>()->BossStatePrefab;
	if (Prefab.IsNull() || !Parent) return;
	TWeakObjectPtr<UUIItem> WeakParent = Parent;
	TWeakObjectPtr<UWuwaBossStatePanel> WeakThis = this;
	LoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(Prefab.ToSoftObjectPath(), [WeakThis, WeakParent]()
	{
		if (UWuwaBossStatePanel* Self = WeakThis.Get()) Self->OnPrefabLoaded(WeakParent);
	});
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float DeltaSeconds)
	{
		Tick(DeltaSeconds);
		return true;
	}));
}

void UWuwaBossStatePanel::OnPrefabLoaded(TWeakObjectPtr<UUIItem> Parent)
{
	ULGUIPrefab* Prefab = GetDefault<UWuwaBattleUiSettings>()->BossStatePrefab.Get();
	UUIItem* ParentItem = Parent.Get();
	if (!Prefab || !ParentItem || View) return;
	AActor* Actor = Prefab->LoadPrefab(ParentItem->GetWorld(), ParentItem, false);   // 保留预制体里按锚点算好的位置（重置变换会把它放到父节点中心）
	if (!Actor) return;
	View = NewObject<UWuwaCommonBossStateView>(this);
	View->CreateByActor(Actor);
	View->Start();
	View->Hide();
}

void UWuwaBossStatePanel::Shutdown()
{
	FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
	if (LoadHandle.IsValid()) LoadHandle->CancelHandle();
	if (View)
	{
		View->Deactivate();
		View->DestroyImmediately();
		View = nullptr;
	}
	Shown.Reset();
}

AWuwaCharactorBase* UWuwaBossStatePanel::GetShownBoss() const
{
	return Shown.Get();
}

AWuwaCharactorBase* UWuwaBossStatePanel::FindBoss() const
{
	// 原作 fnC：离当前角色最近；自带显示距离的按自己的，否则在 ShowDistance 内出现，已经有血条显示时 MaxDistance 内都算
	const UWorld* World = View ? View->GetWorld() : nullptr;
	const APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
	const APawn* Pawn = Player ? Player->GetPawn() : nullptr;
	const UWuwaCreatureSubsystem* Creatures = World ? UWuwaCreatureSubsystem::Get(World) : nullptr;
	if (!Pawn || !Creatures) return nullptr;
	const UWuwaBattleUiSettings* Settings = GetDefault<UWuwaBattleUiSettings>();
	const double Show = FMath::Square(Settings->BossStateShowDistance);
	const double Max = FMath::Square(Settings->BossStateShowMaxDistance);
	AWuwaCharactorBase* Best = nullptr;
	double BestDistance = TNumericLimits<double>::Max();
	for (const TWeakObjectPtr<AWuwaCharactorBase>& Entry : Creatures->GetCreatures())
	{
		AWuwaCharactorBase* Creature = Entry.Get();
		if (!Creature || Creature->BossViewConfig.BossStateViewType == 0 || Creature->IsHidden()) continue;
		const double Distance = FVector::DistSquared(Creature->GetActorLocation(), Pawn->GetActorLocation());
		if (Creature->BossViewConfig.ShowDistance > 0.f)
		{
			if (Distance <= FMath::Square(Creature->BossViewConfig.ShowDistance) && Distance < BestDistance) { BestDistance = Distance; Best = Creature; }
			continue;
		}
		if (Distance > Max) continue;
		if ((Distance <= Show || Shown.IsValid()) && Distance < BestDistance) { BestDistance = Distance; Best = Creature; }
	}
	return Best;
}

void UWuwaBossStatePanel::Tick(float DeltaSeconds)
{
	if (!View || View->GetState() == EWuwaUiState::None || View->IsBusy()) return;
	AWuwaCharactorBase* Boss = FindBoss();
	if (Boss != Shown.Get())
	{
		Shown = Boss;
		if (Boss)
		{
			View->Activate(Boss);
			View->Show();
		}
		else
		{
			View->Deactivate();
			View->Hide();
		}
	}
	View->Tick(DeltaSeconds);
}
