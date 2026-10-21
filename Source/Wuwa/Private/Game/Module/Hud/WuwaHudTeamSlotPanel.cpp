#include "Game/Module/Hud/WuwaHudTeamSlotPanel.h"

#include "Core/ActorComponent/UIText.h"
#include "Core/ActorComponent/UITexture.h"
#include "Engine/Texture2D.h"
#include "Game/Module/Formation/WuwaFormationEvents.h"
#include "Game/Module/Formation/WuwaFormationModel.h"
#include "Game/Module/Formation/WuwaFormationUiController.h"
#include "Game/NewWorld/Character/Role/DataAsset/WuwaRoleDataAsset.h"
#include "Interaction/UIButtonComponent.h"
#include "StructUtils/InstancedStruct.h"

namespace WuwaHudTeamSlot
{
	constexpr int32 Button = 0;
	constexpr int32 Head = 1;
	constexpr int32 HpFill = 2;
	constexpr int32 Number = 3;
	constexpr int32 Frame = 4;
}

void UWuwaHudTeamSlotPanel::OnRegisterComponent()
{
	using namespace WuwaHudTeamSlot;
	ComponentRegisterInfos = {
		{Button, UUIButtonComponent::StaticClass()},
		{Head, UUITexture::StaticClass()},
		{HpFill, UUITexture::StaticClass()},
		{Number, UUIText::StaticClass()},
		{Frame, UUITexture::StaticClass()},
	};
	BtnBindInfo = {
		{Button, [this]()
		{
			// 点头像只是"请求切人"，能不能切（冷却、同一个人）由队伍组件裁决，结果通过 Switched 事件回来。
			if (UWuwaFormationUiController* Controller = GetController<UWuwaFormationUiController>())
			{
				Controller->RequestSwitchTo(SlotIndex);
			}
		}},
	};
}

void UWuwaHudTeamSlotPanel::OnCreate()
{
	// 血条按比例改宽度：把锚点挪到左端并保持原位置不动，这样缩短时只从右边收。
	if (UUITexture* Fill = GetComponentAt<UUITexture>(WuwaHudTeamSlot::HpFill))
	{
		HpFullWidth = Fill->GetWidth();
		const FVector2D Pivot = Fill->GetPivot();
		FVector2D Position = Fill->GetAnchoredPosition();
		Position.X -= HpFullWidth * Pivot.X;
		Fill->SetPivot(FVector2D(0.f, Pivot.Y));
		Fill->SetAnchoredPosition(Position);
	}
}

void UWuwaHudTeamSlotPanel::OnAfterShow()
{
	// 放在 OnAfterShow：框架在 OnBeforeShow 之后才把根节点设为显示，空槽位要在那之后再隐藏。
	// 被盖住、隐藏期间收不到事件，重新显示时从 Model 读最新数据。
	Refresh();
}

void UWuwaHudTeamSlotPanel::OnAddEventListener()
{
	ListenEvent(WuwaFormationEvents::Members, [this](const FInstancedStruct&) { Refresh(); });
	ListenEvent(WuwaFormationEvents::Switched, [this](const FInstancedStruct&) { Refresh(); });
	ListenEvent(WuwaFormationEvents::MemberHealth, [this](const FInstancedStruct& Payload)
	{
		const FWuwaFormationMemberHealthEvent* Event = Payload.GetPtr<FWuwaFormationMemberHealthEvent>();
		if (Event && Event->Index == SlotIndex)
		{
			ApplyHealthPercent(Event->MaxHealth > 0.f ? FMath::Clamp(Event->Health / Event->MaxHealth, 0.f, 1.f) : 0.f);
		}
	});
}

void UWuwaHudTeamSlotPanel::Refresh()
{
	const UWuwaFormationModel* Model = GetModel<UWuwaFormationModel>();
	const FWuwaTeamMemberUiData* Member = Model ? Model->GetMember(SlotIndex) : nullptr;
	ApplyMember(Member && Member->RoleData ? Member : nullptr);
}

void UWuwaHudTeamSlotPanel::ApplyMember(const FWuwaTeamMemberUiData* Member)
{
	// 队伍不满三人时，多出来的槽位整个隐藏。
	bSlotVisible = Member != nullptr;
	if (UUIItem* Root = GetRootItem())
	{
		Root->SetIsUIActive(bSlotVisible);
	}
	if (!Member)
	{
		bHighlighted = false;
		return;
	}

	if (UUIText* NumberText = GetText(WuwaHudTeamSlot::Number))
	{
		NumberText->SetText(FText::AsNumber(SlotIndex + 1));
	}

	// 头像没配置就保留预制体里的默认图。
	UTexture2D* Icon = Member->RoleData->HeadIcon.LoadSynchronous();
	if (Icon && Icon != ShownHeadIcon)
	{
		if (UUITexture* Head = GetComponentAt<UUITexture>(WuwaHudTeamSlot::Head))
		{
			Head->SetTexture(Icon);
			ShownHeadIcon = Icon;
		}
	}

	// 和原作一样：选中效果（Frame 所在的整组：圆环、光环、辉光）只在在场时显示；在场角色不显示切人按键（键帽是 Number 的父节点）。
	// 父节点就是槽位根节点时只改 Frame / Number 自己，不能把整个槽位藏掉。
	bHighlighted = Member->bOnField;
	UUIItem* Root = GetRootItem();
	if (UUITexture* FrameTexture = GetComponentAt<UUITexture>(WuwaHudTeamSlot::Frame))
	{
		UUIItem* Group = FrameTexture->GetParentUIItem();
		(Group && Group != Root ? Group : FrameTexture)->SetIsUIActive(bHighlighted);
	}
	if (UUIText* NumberText = GetText(WuwaHudTeamSlot::Number))
	{
		UUIItem* Key = NumberText->GetParentUIItem();
		(Key && Key != Root ? Key : NumberText)->SetIsUIActive(!bHighlighted);
	}
	ApplyHealthPercent(Member->GetHealthPercent());
}

void UWuwaHudTeamSlotPanel::ApplyHealthPercent(float Percent)
{
	DisplayedHealthPercent = Percent;
	UUITexture* Fill = GetComponentAt<UUITexture>(WuwaHudTeamSlot::HpFill);
	if (!Fill || HpFullWidth <= 0.f)
	{
		return;
	}
	Fill->SetWidth(HpFullWidth * Percent);
	// 宽度为 0 时有的材质仍会画出一条细线，干脆隐藏。
	Fill->SetIsUIActive(Percent > 0.f);
}
