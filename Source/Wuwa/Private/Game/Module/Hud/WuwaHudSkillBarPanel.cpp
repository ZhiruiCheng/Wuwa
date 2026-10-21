#include "Game/Module/Hud/WuwaHudSkillBarPanel.h"

#include "Core/ActorComponent/UISprite.h"
#include "Core/ActorComponent/UITexture.h"
#include "Engine/StreamableManager.h"
#include "Engine/Texture2D.h"
#include "Game/Common/Resource/WuwaResourceSubsystem.h"
#include "Game/LGUI/Component/WuwaUINiagara.h"
#include "Game/LGUI/Component/WuwaUiComponentsRegistry.h"
#include "Game/LGUI/Core/WuwaUiLog.h"
#include "Game/LGUI/Core/WuwaUISettings.h"
#include "Game/Module/Hud/WuwaHudEvents.h"
#include "LTweenManager.h"
#include "LTweener.h"
#include "Game/Module/Formation/WuwaFormationEvents.h"
#include "Game/Module/Formation/WuwaFormationModel.h"
#include "Game/NewWorld/Character/Role/DataAsset/WuwaRoleDataAsset.h"
#include "PrefabSystem/LGUIPrefab.h"
#include "StructUtils/InstancedStruct.h"

namespace WuwaHudSkillBar
{
	constexpr int32 ResonanceSkillIcon = 0;
	constexpr int32 LiberationIcon = 1;

	// 注册表下标 → 按钮类型
	constexpr EWuwaSkillButtonType Buttons[] = {EWuwaSkillButtonType::ResonanceSkill, EWuwaSkillButtonType::Liberation};

	// 点击圈：注册表 2~5 依次是 T、E、Q、R
	constexpr int32 FirstClickRing = 2;
	constexpr EWuwaSkillButtonType ClickButtons[] = {
		EWuwaSkillButtonType::ExploreTool, EWuwaSkillButtonType::ResonanceSkill, EWuwaSkillButtonType::Echo, EWuwaSkillButtonType::Liberation};

	// 原作 NS_Fx_LGUI_FightSkill：寿命 0.35 秒；缩放曲线 0.98→1.2（线性）；透明度 = 颜色曲线的 A × 1.2
	constexpr float ClickDuration = 0.35f;
	// 颜色曲线的 A（原作 ShaderLUT，55 个采样，时间范围 -0.00375 ~ 1.03705）
	constexpr float ClickAlphaLut[] = {
		0.0000f, 0.0579f, 0.1158f, 0.1737f, 0.2316f, 0.2895f, 0.3474f, 0.4053f, 0.4632f, 0.5211f, 0.5790f, 0.6369f, 0.6948f, 0.7037f,
		0.6943f, 0.6728f, 0.6416f, 0.6029f, 0.5590f, 0.5121f, 0.4645f, 0.4184f, 0.3761f, 0.3399f, 0.3120f, 0.2947f, 0.2881f, 0.2777f,
		0.2674f, 0.2570f, 0.2466f, 0.2362f, 0.2258f, 0.2155f, 0.2051f, 0.1947f, 0.1843f, 0.1739f, 0.1636f, 0.1532f, 0.1428f, 0.1324f,
		0.1220f, 0.1117f, 0.1013f, 0.0909f, 0.0805f, 0.0702f, 0.0598f, 0.0494f, 0.0390f, 0.0286f, 0.0183f, 0.0079f, 0.0000f};
	constexpr float ClickAlphaLutMinTime = -0.0037483312f;
	constexpr float ClickAlphaLutInvRange = 0.9607983f;

	float ClickAlpha(float NormalizedAge)
	{
		constexpr int32 Last = UE_ARRAY_COUNT(ClickAlphaLut) - 1;
		const float Position = FMath::Clamp((NormalizedAge - ClickAlphaLutMinTime) * ClickAlphaLutInvRange, 0.f, 1.f) * Last;
		const int32 Index = FMath::Min(FMath::FloorToInt(Position), Last - 1);
		return FMath::Lerp(ClickAlphaLut[Index], ClickAlphaLut[Index + 1], Position - Index);
	}
}

void UWuwaHudSkillBarPanel::OnRegisterComponent()
{
	using namespace WuwaHudSkillBar;
	ComponentRegisterInfos = {
		{ResonanceSkillIcon, UUITexture::StaticClass()},
		{LiberationIcon, UUITexture::StaticClass()},
	};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(ClickButtons); ++Index)
	{
		ComponentRegisterInfos.Add({FirstClickRing + Index, UUIItem::StaticClass()});
	}
}

void UWuwaHudSkillBarPanel::OnAfterShow()
{
	Refresh();
	LoadUltRing();
}

void UWuwaHudSkillBarPanel::OnBeforeDestroy()
{
	if (UltLoadHandle.IsValid())
	{
		UltLoadHandle->CancelHandle();
		UltLoadHandle.Reset();
	}
	Super::OnBeforeDestroy();
}

void UWuwaHudSkillBarPanel::OnAddEventListener()
{
	ListenEvent(WuwaFormationEvents::Members, [this](const FInstancedStruct&) { Refresh(); ApplyCurrentEnergy(); });
	ListenEvent(WuwaFormationEvents::Switched, [this](const FInstancedStruct&) { Refresh(); ApplyCurrentEnergy(); });
	// 共鸣能量变了（命中回能、大招清空）：只看当前角色
	ListenEvent(WuwaFormationEvents::MemberAttributes, [this](const FInstancedStruct& Payload)
	{
		const FWuwaFormationMemberAttributesEvent* Event = Payload.GetPtr<FWuwaFormationMemberAttributesEvent>();
		const UWuwaFormationModel* Model = GetModel<UWuwaFormationModel>();
		if (Event && Model && Event->Member.Index == Model->GetCurrentIndex())
		{
			ApplyEnergy(Event->Member);
		}
	});
	ListenEvent(WuwaHudEvents::SkillButtonPressed, [this](const FInstancedStruct& Payload)
	{
		if (const FWuwaSkillButtonPressedEvent* Event = Payload.GetPtr<FWuwaSkillButtonPressedEvent>())
		{
			PlayClick(Event->ButtonType);
		}
	});
}

int32 UWuwaHudSkillBarPanel::GetClickCount(EWuwaSkillButtonType ButtonType) const
{
	return ClickCounts.FindRef(ButtonType);
}

UUIItem* UWuwaHudSkillBarPanel::GetClickEffect(EWuwaSkillButtonType ButtonType) const
{
	using namespace WuwaHudSkillBar;
	for (int32 i = 0; i < UE_ARRAY_COUNT(ClickButtons); ++i)
	{
		if (ClickButtons[i] == ButtonType) return GetComponentAt<UUIItem>(FirstClickRing + i);
	}
	return nullptr;
}

void UWuwaHudSkillBarPanel::PlayClick(EWuwaSkillButtonType ButtonType)
{
	using namespace WuwaHudSkillBar;
	int32 Index = INDEX_NONE;
	for (int32 i = 0; i < UE_ARRAY_COUNT(ClickButtons); ++i)
	{
		if (ClickButtons[i] == ButtonType) Index = i;
	}
	UUIItem* RingItem = Index != INDEX_NONE ? GetComponentAt<UUIItem>(FirstClickRing + Index) : nullptr;
	// 原作的 Niagara 特效（UWuwaUINiagara 播放重建的 NS_UI_FightSkill）
	if (UWuwaUINiagara* Effect = Cast<UWuwaUINiagara>(RingItem))
	{
		ClickCounts.FindOrAdd(ButtonType)++;
		Effect->Play();
		return;
	}
	// 旧预制体：烘焙的环形贴图 + 补间
	UUITexture* Ring = Cast<UUITexture>(RingItem);
	if (!Ring)
	{
		return;
	}
	// 连按时从头再播
	if (ULTweener* Previous = ClickTweens.FindRef(ButtonType).Get())
	{
		ULTweenManager::KillIfIsTweening(this, Previous, false);
	}
	ClickCounts.FindOrAdd(ButtonType)++;
	TWeakObjectPtr<UUITexture> WeakRing(Ring);
	auto Apply = [WeakRing](float NormalizedAge)
	{
		if (UUITexture* Target = WeakRing.Get())
		{
			const float Scale = FMath::Lerp(0.98f, 1.2f, NormalizedAge);
			Target->SetRelativeScale3D(FVector(1.f, Scale, Scale));   // LGUI 的界面平面：Y 水平、Z 竖直
			Target->SetAlpha(FMath::Min(1.f, ClickAlpha(NormalizedAge) * 1.2f));
		}
	};
	Apply(0.f);
	ULTweener* Tween = ULTweenManager::VirtualTo(this, ClickDuration);
	if (Tween)
	{
		Tween->SetEase(ELTweenEase::Linear)->OnUpdate(Apply)->OnComplete([Apply]() { Apply(1.f); });
		ClickTweens.Add(ButtonType, Tween);
	}
}

UTexture2D* UWuwaHudSkillBarPanel::GetShownIcon(int32 Index) const
{
	const UUITexture* Icon = GetComponentAt<UUITexture>(Index);
	return Icon ? Cast<UTexture2D>(Icon->GetTexture()) : nullptr;
}

void UWuwaHudSkillBarPanel::Refresh()
{
	const UWuwaFormationModel* Model = GetModel<UWuwaFormationModel>();
	const FWuwaTeamMemberUiData* Member = Model ? Model->GetMember(Model->GetCurrentIndex()) : nullptr;
	const UWuwaRoleDataAsset* Role = Member ? Member->RoleData.Get() : nullptr;
	if (!Role || Role == ShownRole)
	{
		return;
	}
	ShownRole = Role;
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(WuwaHudSkillBar::Buttons); ++Index)
	{
		const FWuwaRoleSkillButton* Button = Role->FindSkillButton(WuwaHudSkillBar::Buttons[Index]);
		UTexture2D* Texture = Button ? Button->Icon.LoadSynchronous() : nullptr;
		UUITexture* Icon = GetComponentAt<UUITexture>(Index);
		if (Texture && Icon)
		{
			Icon->SetTexture(Texture);
		}
	}
}

void UWuwaHudSkillBarPanel::LoadUltRing()
{
	if (UltActor || UltLoadHandle.IsValid())
	{
		return;
	}
	const TSoftObjectPtr<ULGUIPrefab> Prefab = GetDefault<UWuwaUISettings>()->SkillUltPrefab;
	UWuwaResourceSubsystem* Resources = UWuwaResourceSubsystem::Get(this);
	if (Prefab.IsNull() || !Resources)
	{
		return;
	}
	// LGUI 没有异步实例化：先异步加载预制体，回来再挂到 R 按钮下
	UltLoadHandle = Resources->LoadAsync({Prefab.ToSoftObjectPath()}, TEXT("UI:SkillUlt"),
		[WeakThis = TWeakObjectPtr<UWuwaHudSkillBarPanel>(this), Prefab]()
		{
			UWuwaHudSkillBarPanel* Self = WeakThis.Get();
			if (!Self)
			{
				return;
			}
			Self->UltLoadHandle.Reset();
			const UUITexture* Icon = Self->GetComponentAt<UUITexture>(WuwaHudSkillBar::LiberationIcon);
			UUIItem* Parent = Icon ? Icon->GetParentUIItem() : nullptr;
			ULGUIPrefab* Loaded = Prefab.Get();
			if (!Parent || !Loaded)
			{
				UE_LOG(LogWuwaUI, Warning, TEXT("SkillBar: cannot create the ultimate energy ring (prefab %s)."), *Prefab.ToString());
				return;
			}
			Self->UltActor = Loaded->LoadPrefab(Parent->GetWorld(), Parent, true);
			// 原作 FightSkillUlt 挂在 PnlContent 下（图标后面，盖在图标上）
			if (UUIItem* Root = Self->UltActor ? Cast<UUIItem>(Self->UltActor->GetRootComponent()) : nullptr)
			{
				Root->SetHierarchyIndex(Icon->GetHierarchyIndex() + 1);
			}
			const UWuwaUiComponentsRegistry* Registry = Self->UltActor ? Self->UltActor->FindComponentByClass<UWuwaUiComponentsRegistry>() : nullptr;
			const AActor* Bar = Registry ? Registry->GetActorAt(0) : nullptr;
			Self->UltBar = Bar ? Bar->FindComponentByClass<UUISprite>() : nullptr;
			Self->ApplyCurrentEnergy();
		});
	// 已经在内存里时回调同步执行完，句柄也已完成，不需要保留。
	if (UltLoadHandle.IsValid() && UltLoadHandle->HasLoadCompleted())
	{
		UltLoadHandle.Reset();
	}
}

void UWuwaHudSkillBarPanel::ApplyCurrentEnergy()
{
	const UWuwaFormationModel* Model = GetModel<UWuwaFormationModel>();
	if (const FWuwaTeamMemberUiData* Member = Model ? Model->GetMember(Model->GetCurrentIndex()) : nullptr)
	{
		ApplyEnergy(*Member);
	}
}

void UWuwaHudSkillBarPanel::ApplyEnergy(const FWuwaTeamMemberUiData& Member)
{
	if (UUISprite* Bar = UltBar.Get())
	{
		Bar->SetFillAmount(Member.GetEnergyPercent());
	}
}

float UWuwaHudSkillBarPanel::GetUltPercent() const
{
	const UUISprite* Bar = UltBar.Get();
	return Bar ? Bar->GetFillAmount() : -1.f;
}
