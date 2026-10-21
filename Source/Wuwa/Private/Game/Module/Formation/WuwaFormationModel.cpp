#include "Game/Module/Formation/WuwaFormationModel.h"

void UWuwaFormationModel::SetMembers(TArray<FWuwaTeamMemberUiData> InMembers, int32 InCurrentIndex)
{
	Members = MoveTemp(InMembers);
	MemberBuffs.SetNum(Members.Num());
	SetCurrentIndex(InCurrentIndex);
}

bool UWuwaFormationModel::SetBuffs(int32 Index, TArray<FWuwaBuffUiData> Buffs)
{
	if (!MemberBuffs.IsValidIndex(Index)) return false;
	// 已经在的图标沿用原来的 SortId（原作 BuffItemInfo.GenSortId 只在加上时取一次）
	const TArray<FWuwaBuffUiData>& Old = MemberBuffs[Index].Buffs;
	for (FWuwaBuffUiData& Buff : Buffs)
	{
		const FWuwaBuffUiData* Existing = Old.FindByPredicate([&Buff](const FWuwaBuffUiData& O) { return O.IsSameIcon(Buff); });
		Buff.SortId = Existing ? Existing->SortId : NextBuffSortId++;
	}
	MemberBuffs[Index].Buffs = MoveTemp(Buffs);
	return true;
}

void UWuwaFormationModel::SetCurrentIndex(int32 InCurrentIndex)
{
	CurrentIndex = Members.IsValidIndex(InCurrentIndex) ? InCurrentIndex : INDEX_NONE;
	for (FWuwaTeamMemberUiData& Member : Members)
	{
		Member.bOnField = Member.Index == CurrentIndex;
	}
}

bool UWuwaFormationModel::SetHealth(int32 Index, float Health, float MaxHealth)
{
	if (!Members.IsValidIndex(Index))
	{
		return false;
	}
	FWuwaTeamMemberUiData& Member = Members[Index];
	if (Member.Health == Health && Member.MaxHealth == MaxHealth)
	{
		return false;
	}
	Member.Health = Health;
	Member.MaxHealth = MaxHealth;
	return true;
}

bool UWuwaFormationModel::SetAttributes(int32 Index, const FWuwaTeamMemberUiData& Values)
{
	if (!Members.IsValidIndex(Index))
	{
		return false;
	}
	FWuwaTeamMemberUiData& Member = Members[Index];
	if (Member.Level == Values.Level && Member.Element == Values.Element && Member.Energy == Values.Energy && Member.MaxEnergy == Values.MaxEnergy
		&& Member.ConcertoEnergy == Values.ConcertoEnergy && Member.MaxConcertoEnergy == Values.MaxConcertoEnergy
		&& Member.SpecialEnergy == Values.SpecialEnergy && Member.MaxSpecialEnergy == Values.MaxSpecialEnergy)
	{
		return false;
	}
	Member.Level = Values.Level;
	Member.Element = Values.Element;
	Member.Energy = Values.Energy;
	Member.MaxEnergy = Values.MaxEnergy;
	Member.ConcertoEnergy = Values.ConcertoEnergy;
	Member.MaxConcertoEnergy = Values.MaxConcertoEnergy;
	Member.SpecialEnergy = Values.SpecialEnergy;
	Member.MaxSpecialEnergy = Values.MaxSpecialEnergy;
	return true;
}

void UWuwaFormationModel::OnClear()
{
	Members.Reset();
	MemberBuffs.Reset();
	CurrentIndex = INDEX_NONE;
}

void UWuwaFormationModel::OnLeaveLevel()
{
	// 队员是这个世界里的角色，世界没了快照也作废；新地图生成队伍后 Controller 会重新写入。
	OnClear();
}
