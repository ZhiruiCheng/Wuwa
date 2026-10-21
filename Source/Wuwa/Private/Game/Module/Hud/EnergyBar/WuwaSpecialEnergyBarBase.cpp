#include "Game/Module/Hud/EnergyBar/WuwaSpecialEnergyBarBase.h"

#include "Game/Module/Formation/WuwaFormationEvents.h"
#include "Game/Module/Formation/WuwaFormationModel.h"
#include "StructUtils/InstancedStruct.h"

namespace WuwaSpecialEnergyBar
{
	// 原作 SpecialEnergyBarPercentMachine 的 DURATION = 100（毫秒）：涨的时候 0.1 秒追到目标
	constexpr float RiseSeconds = 0.1f;
}

void FWuwaEnergyPercentMachine::SetTarget(float Percent)
{
	if (Percent == Target)
	{
		return;
	}
	if (Percent > Target)
	{
		Target = Percent;
		Current = FMath::Min(Current, Percent);
		Speed = (Target - Current) / WuwaSpecialEnergyBar::RiseSeconds;
	}
	else
	{
		Target = Current = Percent;
		Speed = 0.f;
	}
}

bool FWuwaEnergyPercentMachine::Update(float DeltaSeconds)
{
	if (Speed == 0.f)
	{
		return false;
	}
	Current += Speed * DeltaSeconds;
	if (Current >= Target)
	{
		Current = Target;
		Speed = 0.f;
	}
	return true;
}

void UWuwaSpecialEnergyBarBase::InitData(int32 InMemberIndex, const FWuwaSpecialEnergyBarInfo& InConfig)
{
	MemberIndex = InMemberIndex;
	Config = InConfig;
	OnInitData();
}

void UWuwaSpecialEnergyBarBase::SetMemberIndex(int32 InMemberIndex)
{
	MemberIndex = InMemberIndex;
	if (GetState() == EWuwaUiState::Shown)
	{
		if (const FWuwaTeamMemberUiData* Member = GetMember())
		{
			OnMemberData(*Member, true);
		}
	}
}

bool UWuwaSpecialEnergyBarBase::GetKeyEnable() const
{
	// 原作读表时 DisableKeyOnPercent / 100
	return PercentMachine.GetCurrent() >= Config.DisableKeyOnPercent / 100.f - KINDA_SMALL_NUMBER;
}

const FWuwaTeamMemberUiData* UWuwaSpecialEnergyBarBase::GetMember() const
{
	const UWuwaFormationModel* Model = GetModel<UWuwaFormationModel>();
	return Model ? Model->GetMember(MemberIndex) : nullptr;
}

float UWuwaSpecialEnergyBarBase::GetTargetPercent(const FWuwaTeamMemberUiData& Member) const
{
	return Member.GetSpecialEnergyPercent(Config.SpecialEnergyIndex);
}

void UWuwaSpecialEnergyBarBase::OnMemberData(const FWuwaTeamMemberUiData& Member, bool bInitial)
{
	const float Percent = GetTargetPercent(Member);
	if (bInitial)
	{
		PercentMachine.Init(Percent);
	}
	else
	{
		PercentMachine.SetTarget(Percent);
	}
	OnBarPercentChanged();
	ApplyKeyEnable(bInitial);
}

void UWuwaSpecialEnergyBarBase::OnAfterShow()
{
	// 隐藏期间收不到事件，显示时从 Model 读最新值
	if (const FWuwaTeamMemberUiData* Member = GetMember())
	{
		OnMemberData(*Member, true);
	}
	StartTicking();
}

void UWuwaSpecialEnergyBarBase::OnBeforeHide()
{
	StopTicking();
}

void UWuwaSpecialEnergyBarBase::OnBeforeDestroy()
{
	StopTicking();
}

void UWuwaSpecialEnergyBarBase::OnAddEventListener()
{
	ListenEvent(WuwaFormationEvents::MemberAttributes, [this](const FInstancedStruct& Payload)
	{
		const FWuwaFormationMemberAttributesEvent* Event = Payload.GetPtr<FWuwaFormationMemberAttributesEvent>();
		if (Event && Event->Member.Index == MemberIndex)
		{
			OnMemberData(Event->Member, false);
		}
	});
}

void UWuwaSpecialEnergyBarBase::TickBar(float DeltaSeconds)
{
	if (PercentMachine.Update(DeltaSeconds))
	{
		OnBarPercentChanged();
		ApplyKeyEnable(false);
	}
}

void UWuwaSpecialEnergyBarBase::ApplyKeyEnable(bool bForce)
{
	const bool bNow = GetKeyEnable();
	if (bForce || bNow != bLastKeyEnable)
	{
		bLastKeyEnable = bNow;
		OnKeyEnableChanged();
	}
}

void UWuwaSpecialEnergyBarBase::StartTicking()
{
	if (TickHandle.IsValid())
	{
		return;
	}
	TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateWeakLambda(this, [this](float DeltaSeconds)
	{
		TickBar(DeltaSeconds);
		return true;
	}));
}

void UWuwaSpecialEnergyBarBase::StopTicking()
{
	if (TickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
		TickHandle.Reset();
	}
}
