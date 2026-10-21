#include "Game/Module/Mail/WuwaMailController.h"

#include "Game/LGUI/Core/WuwaUiLog.h"
#include "Game/LGUI/Core/WuwaUiManager.h"
#include "Game/Module/Mail/WuwaMailEvents.h"
#include "Game/Module/Mail/WuwaMailModel.h"
#include "Subsystems/SubsystemCollection.h"

const FName UWuwaMailController::MailBoxViewName(TEXT("MailBoxView"));

void UWuwaMailController::Initialize(FSubsystemCollectionBase& Collection)
{
	// OnInit / OnAdd* 里要用 Model，先保证它已经初始化。
	Collection.InitializeDependency<UWuwaMailModel>();
	Super::Initialize(Collection);
}

UWuwaUiView* UWuwaMailController::OpenMailBox()
{
	UWuwaUiManager* UiManager = GetUiManager();
	return UiManager ? UiManager->OpenView(MailBoxViewName) : nullptr;
}

void UWuwaMailController::RequestReadOne()
{
	const UWuwaMailModel* Model = GetModel<UWuwaMailModel>();
	const FWuwaMail* Mail = Model ? Model->FindFirstUnread() : nullptr;
	if (!Mail || IsRequestInFlight())
	{
		return;
	}

	const int32 MailId = Mail->Id;
	UE_LOG(LogWuwaUI, Log, TEXT("Mail: request read #%d"), MailId);
	ReadRequestTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this, MailId](float)
	{
		ReadRequestTicker.Reset();
		HandleReadResponse(MailId);
		return false;   // 只执行一次
	}), FakeLatencySeconds);
}

void UWuwaMailController::SetMailEnabled(bool bEnabled)
{
	if (UWuwaMailModel* Model = GetModel<UWuwaMailModel>())
	{
		Model->SetMailEnabled(bEnabled);
	}
}

void UWuwaMailController::OnAddOpenViewCheckFunction()
{
	// ≈ 鸣潮 MailController.OnAddOpenViewCheckFunction：邮件功能关闭时邮箱打不开。
	TWeakObjectPtr<UWuwaMailController> WeakThis(this);
	AddOpenViewCheck(MailBoxViewName, [WeakThis](FName)
	{
		const UWuwaMailModel* Model = WeakThis.IsValid() ? WeakThis->GetModel<UWuwaMailModel>() : nullptr;
		return Model && Model->IsMailEnabled();
	});
}

void UWuwaMailController::OnClear()
{
	FTSTicker::GetCoreTicker().RemoveTicker(ReadRequestTicker);
	ReadRequestTicker.Reset();
}

void UWuwaMailController::HandleReadResponse(int32 MailId)
{
	UWuwaMailModel* Model = GetModel<UWuwaMailModel>();
	if (!Model || !Model->MarkRead(MailId))
	{
		return;
	}

	FWuwaMailChangedEvent Event;
	Event.MailId = MailId;
	Event.UnreadCount = Model->GetUnreadCount();
	UE_LOG(LogWuwaUI, Log, TEXT("Mail: #%d read, %d unread"), MailId, Event.UnreadCount);
	EmitEvent(WuwaMailEvents::Changed, Event);
}
