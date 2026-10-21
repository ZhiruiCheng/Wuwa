#include "Game/Module/Mail/WuwaMailBoxView.h"

#include "Core/ActorComponent/UIText.h"
#include "Game/Module/Mail/WuwaMailController.h"
#include "Game/Module/Mail/WuwaMailEvents.h"
#include "Game/Module/Mail/WuwaMailModel.h"
#include "Interaction/UIButtonComponent.h"

FString UWuwaMailBoxView::GetUnreadText() const
{
	const UUIText* Text = GetText(2);
	return Text ? Text->GetText().ToString() : FString();
}

void UWuwaMailBoxView::OnRegisterComponent()
{
	ComponentRegisterInfos = {
		{0, UUIButtonComponent::StaticClass()},
		{1, UUIButtonComponent::StaticClass()},
		{2, UUIText::StaticClass()},
	};
	BtnBindInfo = {
		{0, [this]()
		{
			// 点击只是"发起请求"，结果通过事件回来。
			if (UWuwaMailController* Controller = GetController<UWuwaMailController>())
			{
				Controller->RequestReadOne();
			}
		}},
		{1, [this]() { CloseMe(); }},
	};
}

void UWuwaMailBoxView::OnBeforeShow()
{
	// 被盖住期间收不到事件，重新显示时从 Model 读最新数据。
	Refresh();
}

void UWuwaMailBoxView::OnAddEventListener()
{
	ListenEvent(WuwaMailEvents::Changed, [this](const FInstancedStruct&)
	{
		Refresh();
	});
}

void UWuwaMailBoxView::Refresh()
{
	++RefreshCount;
	const UWuwaMailModel* Model = GetModel<UWuwaMailModel>();
	if (UUIText* Text = GetText(2))
	{
		Text->SetText(FText::FromString(FString::Printf(TEXT("Unread: %d"), Model ? Model->GetUnreadCount() : 0)));
	}
}
