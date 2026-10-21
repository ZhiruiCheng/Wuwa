#include "Game/Module/Mail/WuwaMailModel.h"

int32 UWuwaMailModel::GetUnreadCount() const
{
	int32 Count = 0;
	for (const FWuwaMail& Mail : Mails)
	{
		Count += Mail.bRead ? 0 : 1;
	}
	return Count;
}

const FWuwaMail* UWuwaMailModel::FindFirstUnread() const
{
	return Mails.FindByPredicate([](const FWuwaMail& Mail) { return !Mail.bRead; });
}

bool UWuwaMailModel::MarkRead(int32 MailId)
{
	FWuwaMail* Mail = Mails.FindByPredicate([MailId](const FWuwaMail& Item) { return Item.Id == MailId; });
	if (!Mail || Mail->bRead)
	{
		return false;
	}
	Mail->bRead = true;
	return true;
}

void UWuwaMailModel::OnInit()
{
	// 示例数据：真实项目里由登录后服务器下发，经 MailController 写进来。
	const TCHAR* Titles[] = {TEXT("Welcome"), TEXT("Maintenance compensation"), TEXT("Event reward")};
	Mails.Reset();
	for (int32 i = 0; i < UE_ARRAY_COUNT(Titles); ++i)
	{
		FWuwaMail& Mail = Mails.AddDefaulted_GetRef();
		Mail.Id = i + 1;
		Mail.Title = Titles[i];
	}
}
