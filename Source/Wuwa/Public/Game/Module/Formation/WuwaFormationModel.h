#pragma once

#include "CoreMinimal.h"
#include "Game/LGUI/Mvc/WuwaModelBase.h"
#include "Game/Module/Formation/WuwaFormationEvents.h"
#include "WuwaFormationModel.generated.h"

/**
 * 队伍 UI 的数据（≈ 鸣潮 FormationModel 里给界面看的那部分）：队员快照 + 当前是第几个。
 * 只有 UWuwaFormationUiController 写；界面只读，显示前从这里取一次，显示期间靠事件更新。
 */
UCLASS()
class WUWA_API UWuwaFormationModel : public UWuwaModelBase
{
	GENERATED_BODY()

public:
	const TArray<FWuwaTeamMemberUiData>& GetMembers() const { return Members; }
	const FWuwaTeamMemberUiData* GetMember(int32 Index) const { return Members.IsValidIndex(Index) ? &Members[Index] : nullptr; }
	int32 GetCurrentIndex() const { return CurrentIndex; }

	// —— 只给 Controller 用 ——
	void SetMembers(TArray<FWuwaTeamMemberUiData> InMembers, int32 InCurrentIndex);
	void SetCurrentIndex(int32 InCurrentIndex);
	/** 返回 false 表示序号无效或数值没变。 */
	bool SetHealth(int32 Index, float Health, float MaxHealth);
	/** 只改等级、元素、能量（共鸣、协奏、特殊）这几项；返回 false 表示序号无效或数值没变。 */
	bool SetAttributes(int32 Index, const FWuwaTeamMemberUiData& Values);
	/** 换掉一个队员的 buff 图标列表；已经在的图标保留原来的先后（SortId），新的排在最后。返回 false 表示序号无效。 */
	bool SetBuffs(int32 Index, TArray<FWuwaBuffUiData> Buffs);

	/** 一个队员当前的 buff 图标；序号无效时为空。 */
	const TArray<FWuwaBuffUiData>* GetBuffs(int32 Index) const { return MemberBuffs.IsValidIndex(Index) ? &MemberBuffs[Index].Buffs : nullptr; }

protected:
	virtual void OnClear() override;
	virtual void OnLeaveLevel() override;

private:
	TArray<FWuwaTeamMemberUiData> Members;
	UPROPERTY()
	TArray<FWuwaMemberBuffList> MemberBuffs;
	int64 NextBuffSortId = 1;
	int32 CurrentIndex = INDEX_NONE;
};
