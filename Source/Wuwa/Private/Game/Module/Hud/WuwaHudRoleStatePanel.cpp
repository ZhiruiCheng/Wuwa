#include "Game/Module/Hud/WuwaHudRoleStatePanel.h"

#include "Core/ActorComponent/UISprite.h"
#include "Core/ActorComponent/UIText.h"
#include "Core/ActorComponent/UITexture.h"
#include "Engine/DataTable.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "Game/Common/Resource/WuwaResourceSubsystem.h"
#include "Game/LGUI/Core/WuwaUiLog.h"
#include "Game/LGUI/Core/WuwaUISettings.h"
#include "Game/Module/Formation/WuwaFormationEvents.h"
#include "Game/Module/Formation/WuwaFormationModel.h"
#include "Game/Module/Hud/WuwaHudTypes.h"
#include "Game/Module/BattleUi/WuwaBattleUiSettings.h"
#include "Game/Module/Hud/Buff/WuwaHudBuffBar.h"
#include "Game/Module/Hud/EnergyBar/WuwaSpecialEnergyBarMorph.h"
#include "Game/Module/Hud/EnergyBar/WuwaSpecialEnergyBarSlot.h"
#include "Game/NewWorld/Character/Role/DataAsset/WuwaRoleDataAsset.h"
#include "LGUIBPLibrary.h"
#include "PrefabSystem/LGUIPrefab.h"
#include "StructUtils/InstancedStruct.h"

namespace WuwaHudRoleState
{
	constexpr int32 ElementIcon = 0;
	constexpr int32 EnergyBar = 1;
	constexpr int32 Level = 2;
	constexpr int32 HpFill = 3;
	constexpr int32 HpText = 4;
	constexpr int32 HpTextMask = 5;
	constexpr int32 ConcertoBar = 6;
	constexpr int32 BuffTop = 7;
	constexpr int32 BuffList = 8;
	/** 原作 PnlBuffTop 的 UIHorizontalLayout Padding.Bottom = 10（设计分辨率 1440 → 1080）。 */
	constexpr float BuffTopBottomPadding = 10.f * 1080.f / 1440.f;

	UDataTable* LoadTable(const UObject* WorldContext, const TSoftObjectPtr<UDataTable>& Table)
	{
		UWuwaResourceSubsystem* Resources = UWuwaResourceSubsystem::Get(WorldContext);
		return Resources && !Table.IsNull() ? Resources->LoadSync(Table, TEXT("UI:RoleState")) : nullptr;
	}
}

void UWuwaHudRoleStatePanel::OnRegisterComponent()
{
	using namespace WuwaHudRoleState;
	ComponentRegisterInfos = {
		{ElementIcon, UUITexture::StaticClass()},
		{EnergyBar, UUIItem::StaticClass()},
		{Level, UUIText::StaticClass()},
		{HpFill, UUITexture::StaticClass()},
		{HpText, UUIText::StaticClass()},
		{HpTextMask, UUIText::StaticClass()},
		{ConcertoBar, UUISprite::StaticClass()},
		{BuffTop, UUIItem::StaticClass()},
		{BuffList, UUIItem::StaticClass()},
	};
}

void UWuwaHudRoleStatePanel::OnCreate()
{
	// buff 栏（原作 RoleUniqueBuffView / RoleBuffView 各一个 BuffItemContainer）
	const UWuwaBattleUiSettings* BattleUi = GetDefault<UWuwaBattleUiSettings>();
	if (UUIItem* Top = GetItem(WuwaHudRoleState::BuffTop))
	{
		UniqueBuffBar = NewObject<UWuwaHudBuffBar>(this);
		UniqueBuffBar->Init(Top, {24}, BattleUi->RoleUniqueBuffItemCount, WuwaHudRoleState::BuffTopBottomPadding);
	}
	if (UUIItem* List = GetItem(WuwaHudRoleState::BuffList))
	{
		BuffBar = NewObject<UWuwaHudBuffBar>(this);
		BuffBar->Init(List, {2, 14, 37}, BattleUi->RoleUniqueBuffItemCount, 0.f);
	}
	// 血条在预制体里是横向拉伸的：先换成固定锚点（大小、位置不变），再把轴心挪到左端，缩短时只从右边收。
	if (UUITexture* Fill = GetComponentAt<UUITexture>(WuwaHudRoleState::HpFill))
	{
		Fill->SetHorizontalAnchorMinMax(FVector2D(Fill->GetAnchorMin().X), true, true);
		HpFullWidth = Fill->GetWidth();
		const FVector2D Pivot = Fill->GetPivot();
		FVector2D Position = Fill->GetAnchoredPosition();
		Position.X -= HpFullWidth * Pivot.X;
		Fill->SetPivot(FVector2D(0.f, Pivot.Y));
		Fill->SetAnchoredPosition(Position);
	}
	if (const UUIText* LevelText = GetText(WuwaHudRoleState::Level))
	{
		// 预制体里是 "<size=12>Lv.</size>90"：去掉末尾的数字，剩下的当前缀
		const FString Text = LevelText->GetText().ToString();
		int32 End = Text.Len();
		while (End > 0 && FChar::IsDigit(Text[End - 1])) --End;
		LevelPrefix = Text.Left(End);
	}
}

void UWuwaHudRoleStatePanel::OnAfterShow()
{
	Refresh();
	// 能量条不是注册表里的子组件（运行时换的），自己跟着显示隐藏
	if (EnergyBarPanel && EnergyBarPanel->GetState() != EWuwaUiState::Shown)
	{
		EnergyBarPanel->Show();   // 排队执行：还在创建 / 开始时会等它们做完
	}
}

void UWuwaHudRoleStatePanel::OnBeforeHide()
{
	if (EnergyBarPanel && EnergyBarPanel->GetState() == EWuwaUiState::Shown)
	{
		EnergyBarPanel->Hide();
	}
}

void UWuwaHudRoleStatePanel::OnAddEventListener()
{
	ListenEvent(WuwaFormationEvents::Members, [this](const FInstancedStruct&) { Refresh(); });
	ListenEvent(WuwaFormationEvents::Switched, [this](const FInstancedStruct&) { Refresh(); });
	ListenEvent(WuwaFormationEvents::MemberHealth, [this](const FInstancedStruct& Payload)
	{
		const FWuwaFormationMemberHealthEvent* Event = Payload.GetPtr<FWuwaFormationMemberHealthEvent>();
		const UWuwaFormationModel* Model = GetModel<UWuwaFormationModel>();
		if (Event && Model && Event->Index == Model->GetCurrentIndex())
		{
			ApplyHealth(Event->Health, Event->MaxHealth);
		}
	});
	ListenEvent(WuwaFormationEvents::MemberBuffs, [this](const FInstancedStruct& Payload)
	{
		const FWuwaFormationMemberBuffsEvent* Event = Payload.GetPtr<FWuwaFormationMemberBuffsEvent>();
		const UWuwaFormationModel* Model = GetModel<UWuwaFormationModel>();
		if (Event && Model && Event->Index == Model->GetCurrentIndex())
		{
			ApplyBuffs(&Event->Buffs, true);
		}
	});
	ListenEvent(WuwaFormationEvents::MemberAttributes, [this](const FInstancedStruct& Payload)
	{
		const FWuwaFormationMemberAttributesEvent* Event = Payload.GetPtr<FWuwaFormationMemberAttributesEvent>();
		const UWuwaFormationModel* Model = GetModel<UWuwaFormationModel>();
		if (Event && Model && Event->Member.Index == Model->GetCurrentIndex())
		{
			ApplyAttributes(Event->Member);
		}
	});
}

void UWuwaHudRoleStatePanel::OnBeforeDestroy()
{
	++EnergyBarRequest;
	ClearEnergyBar();
	if (UniqueBuffBar) UniqueBuffBar->Clear();
	if (BuffBar) BuffBar->Clear();
}

void UWuwaHudRoleStatePanel::Refresh()
{
	const UWuwaFormationModel* Model = GetModel<UWuwaFormationModel>();
	const FWuwaTeamMemberUiData* Member = Model ? Model->GetMember(Model->GetCurrentIndex()) : nullptr;
	if (!Member || !Member->RoleData)
	{
		return;
	}
	ApplyRole(Member->RoleData);
	// 两个角色用同一种能量条时不重建，只换跟着的队员
	if (EnergyBarPanel && EnergyBarPanel->GetMemberIndex() != Model->GetCurrentIndex())
	{
		EnergyBarPanel->SetMemberIndex(Model->GetCurrentIndex());
	}
	ApplyAttributes(*Member);
	ApplyHealth(Member->Health, Member->MaxHealth);
	// 切人：按新角色身上现有的 buff 整体刷新，不播动画（原作 RefreshBuff）
	ApplyBuffs(Model->GetBuffs(Model->GetCurrentIndex()), false);
}

void UWuwaHudRoleStatePanel::ApplyBuffs(const TArray<FWuwaBuffUiData>* Buffs, bool bAnimate)
{
	static const TArray<FWuwaBuffUiData> None;
	if (UniqueBuffBar) UniqueBuffBar->SetBuffs(Buffs ? *Buffs : None, bAnimate);
	if (BuffBar) BuffBar->SetBuffs(Buffs ? *Buffs : None, bAnimate);
}

void UWuwaHudRoleStatePanel::ApplyRole(const UWuwaRoleDataAsset* Role)
{
	if (Role == ShownRole)
	{
		return;
	}
	ShownRole = Role;
	ApplyEnergyBar(Role->SpecialEnergyBarId);
}

void UWuwaHudRoleStatePanel::ApplyAttributes(const FWuwaTeamMemberUiData& Member)
{
	if (Member.Level != DisplayedLevel)
	{
		DisplayedLevel = Member.Level;
		if (UUIText* LevelText = GetText(WuwaHudRoleState::Level))
		{
			LevelText->SetText(FText::FromString(LevelPrefix + FString::FromInt(Member.Level)));
		}
	}
	ApplyElement(Member.Element);
	DisplayedConcertoPercent = Member.GetConcertoPercent();
	if (UUISprite* Bar = GetSprite(WuwaHudRoleState::ConcertoBar))
	{
		Bar->SetFillAmount(DisplayedConcertoPercent);
	}
}

void UWuwaHudRoleStatePanel::ApplyElement(EWuwaElement Element)
{
	if (bElementShown && Element == DisplayedElement)
	{
		return;
	}
	const UDataTable* Table = WuwaHudRoleState::LoadTable(this, GetDefault<UWuwaUISettings>()->ElementTable);
	const FGameplayTag Tag = WuwaElement::ToTag(Element);
	const FWuwaElementUiInfo* Info = Table && Tag.IsValid() ? Table->FindRow<FWuwaElementUiInfo>(Tag.GetTagName(), TEXT("RoleState"), false) : nullptr;
	if (!Info)
	{
		return;
	}
	DisplayedElement = Element;
	bElementShown = true;
	if (UUITexture* Icon = GetComponentAt<UUITexture>(WuwaHudRoleState::ElementIcon))
	{
		if (UTexture2D* Texture = Info->Icon.LoadSynchronous())
		{
			Icon->SetTexture(Texture);
		}
		Icon->SetColor(Info->Color);
	}
	if (UUISprite* Bar = GetSprite(WuwaHudRoleState::ConcertoBar))
	{
		Bar->SetColor(Info->Color);
	}
}

void UWuwaHudRoleStatePanel::ApplyEnergyBar(int32 EnergyBarId)
{
	if (EnergyBarId == ShownEnergyBarId && (EnergyBarId == 0 || EnergyBarActor || EnergyBarLoadHandle.IsValid()))
	{
		return;
	}
	ShownEnergyBarId = EnergyBarId;
	const int32 Request = ++EnergyBarRequest;
	ClearEnergyBar();
	if (EnergyBarId == 0)
	{
		return;
	}

	const UDataTable* Table = WuwaHudRoleState::LoadTable(this, GetDefault<UWuwaUISettings>()->SpecialEnergyBarTable);
	const FWuwaSpecialEnergyBarInfo* Info = Table ? Table->FindRow<FWuwaSpecialEnergyBarInfo>(*FString::FromInt(EnergyBarId), TEXT("RoleState"), false) : nullptr;
	UWuwaResourceSubsystem* Resources = UWuwaResourceSubsystem::Get(this);
	if (!Info || Info->Prefab.IsNull() || !Resources)
	{
		UE_LOG(LogWuwaUI, Warning, TEXT("RoleState: no special energy bar %d in the table."), EnergyBarId);
		return;
	}
	// LGUI 没有异步实例化：先异步加载预制体，回来再在 PnlEnergyBar 下面生成（≈ 鸣潮按 PrefabPath 加载能量条）
	const TSoftObjectPtr<ULGUIPrefab> Prefab = Info->Prefab;
	EnergyBarLoadHandle = Resources->LoadAsync({Prefab.ToSoftObjectPath()}, TEXT("UI:EnergyBar"),
		[WeakThis = TWeakObjectPtr<UWuwaHudRoleStatePanel>(this), Prefab, Request, Row = *Info]()
		{
			UWuwaHudRoleStatePanel* Self = WeakThis.Get();
			if (!Self || Request != Self->EnergyBarRequest)
			{
				return;
			}
			Self->EnergyBarLoadHandle.Reset();
			UUIItem* Parent = Self->GetItem(WuwaHudRoleState::EnergyBar);
			ULGUIPrefab* Loaded = Prefab.Get();
			if (Parent && Loaded)
			{
				Self->EnergyBarActor = Loaded->LoadPrefab(Parent->GetWorld(), Parent, true);
				Self->CreateEnergyBar(Self->EnergyBarActor, Row);
			}
		});
	// 已经在内存里时回调同步执行完，句柄也已完成，不需要保留。
	if (EnergyBarLoadHandle.IsValid() && EnergyBarLoadHandle->HasLoadCompleted())
	{
		EnergyBarLoadHandle.Reset();
	}
}

void UWuwaHudRoleStatePanel::ApplyHealth(float Health, float MaxHealth)
{
	const float Percent = MaxHealth > 0.f ? FMath::Clamp(Health / MaxHealth, 0.f, 1.f) : 0.f;
	DisplayedHealthPercent = Percent;
	UUITexture* Fill = GetComponentAt<UUITexture>(WuwaHudRoleState::HpFill);
	if (Fill && HpFullWidth > 0.f)
	{
		Fill->SetWidth(HpFullWidth * Percent);
		// 宽度为 0 时九宫格的两头还会画出来，干脆隐藏。
		Fill->SetIsUIActive(Percent > 0.f);
	}
	const FText Text = FText::FromString(FString::Printf(TEXT("%d/%d"), FMath::CeilToInt(Health), FMath::CeilToInt(MaxHealth)));
	for (const int32 Index : {WuwaHudRoleState::HpText, WuwaHudRoleState::HpTextMask})
	{
		if (UUIText* Label = GetText(Index))
		{
			Label->SetText(Text);
		}
	}
}

void UWuwaHudRoleStatePanel::ClearEnergyBar()
{
	if (EnergyBarLoadHandle.IsValid())
	{
		EnergyBarLoadHandle->CancelHandle();
		EnergyBarLoadHandle.Reset();
	}
	if (EnergyBarPanel)
	{
		// 能量条界面连同它的 Actor 一起销毁
		EnergyBarPanel->DestroyImmediately(true);
		EnergyBarPanel = nullptr;
	}
	else if (IsValid(EnergyBarActor))
	{
		ULGUIBPLibrary::DestroyActorWithHierarchy(EnergyBarActor, true);
	}
	EnergyBarActor = nullptr;
}

void UWuwaHudRoleStatePanel::CreateEnergyBar(AActor* Actor, const FWuwaSpecialEnergyBarInfo& Info)
{
	// 表里没填类时按类型给通用的（≈ 原作按 PrefabType 取 specialEnergyBarClassList）；专属预制体没有类就只显示样子
	TSubclassOf<UWuwaSpecialEnergyBarBase> BarClass = Info.BarClass;
	if (!BarClass)
	{
		if (Info.Type == EWuwaSpecialEnergyBarType::Morph) BarClass = UWuwaSpecialEnergyBarMorph::StaticClass();
		else if (Info.Type == EWuwaSpecialEnergyBarType::Slot) BarClass = UWuwaSpecialEnergyBarSlot::StaticClass();
	}
	if (!Actor || !BarClass)
	{
		return;
	}
	const UWuwaFormationModel* Model = GetModel<UWuwaFormationModel>();
	EnergyBarPanel = NewObject<UWuwaSpecialEnergyBarBase>(this, BarClass);
	EnergyBarPanel->InitData(Model ? Model->GetCurrentIndex() : INDEX_NONE, Info);
	EnergyBarPanel->CreateByActor(Actor);
	EnergyBarPanel->Start();
	if (GetState() == EWuwaUiState::Shown)
	{
		EnergyBarPanel->Show();
	}
}
