#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaElement.h"
#include "Game/NewWorld/Buff/WuwaBuffTypes.h"
#include "WuwaFormationEvents.generated.h"

class UWuwaRoleDataAsset;

/** 队伍 UI 的事件（≈ 鸣潮 EEventName 里编队、切人、队员血量相关的几项）。由 UWuwaFormationUiController 发出。 */
namespace WuwaFormationEvents
{
	/** 队员列表整体变了（开局生成、客户端收到复制）；参数 FWuwaFormationMembersEvent。 */
	WUWA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Members);
	/** 切人了；参数 FWuwaFormationSwitchedEvent。 */
	WUWA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(Switched);
	/** 某个队员的血量或血量上限变了（包括不在场的）；参数 FWuwaFormationMemberHealthEvent。 */
	WUWA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(MemberHealth);
	/** 某个队员血量以外的显示属性（等级、元素、共鸣能量、协奏能量、特殊能量）变了；参数 FWuwaFormationMemberAttributesEvent。 */
	WUWA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(MemberAttributes);
	/** 某个队员身上带图标的 buff 变了（加上、移除）；参数 FWuwaFormationMemberBuffsEvent。≈ 原作 CharOnBuffAddUITexture。 */
	WUWA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(MemberBuffs);
}

/** UI 用的队员快照：只放界面要显示的东西，界面不直接碰角色和 ASC。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaTeamMemberUiData
{
	GENERATED_BODY()

	/** 在队伍里的序号，0 起；对应按键 1/2/3。 */
	UPROPERTY(BlueprintReadOnly, Category = "Formation") int32 Index = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "Formation") TObjectPtr<UWuwaRoleDataAsset> RoleData = nullptr;
	UPROPERTY(BlueprintReadOnly, Category = "Formation") float Health = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Formation") float MaxHealth = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Formation") bool bOnField = false;
	// 下面都来自队员 ASC 的属性（UWuwaAttributeSet）
	UPROPERTY(BlueprintReadOnly, Category = "Formation") int32 Level = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Formation") EWuwaElement Element = EWuwaElement::None;
	UPROPERTY(BlueprintReadOnly, Category = "Formation") float Energy = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Formation") float MaxEnergy = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Formation") float ConcertoEnergy = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Formation") float MaxConcertoEnergy = 0.f;
	/** 特殊能量 1~5（下标 0~4），给角色的特殊能量条用。 */
	UPROPERTY(BlueprintReadOnly, Category = "Formation") TArray<float> SpecialEnergy;
	UPROPERTY(BlueprintReadOnly, Category = "Formation") TArray<float> MaxSpecialEnergy;

	float GetHealthPercent() const { return MaxHealth > 0.f ? FMath::Clamp(Health / MaxHealth, 0.f, 1.f) : 0.f; }
	float GetConcertoPercent() const { return MaxConcertoEnergy > 0.f ? FMath::Clamp(ConcertoEnergy / MaxConcertoEnergy, 0.f, 1.f) : 0.f; }
	float GetEnergyPercent() const { return MaxEnergy > 0.f ? FMath::Clamp(Energy / MaxEnergy, 0.f, 1.f) : 0.f; }
	/** 特殊能量 N（1~5）的比例；没有这项或上限为 0 时是 0。 */
	float GetSpecialEnergyPercent(int32 N) const
	{
		const int32 I = N - 1;
		const float Max = MaxSpecialEnergy.IsValidIndex(I) ? MaxSpecialEnergy[I] : 0.f;
		return Max > 0.f && SpecialEnergy.IsValidIndex(I) ? FMath::Clamp(SpecialEnergy[I] / Max, 0.f, 1.f) : 0.f;
	}
};

USTRUCT(BlueprintType)
struct WUWA_API FWuwaFormationMembersEvent
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Formation") TArray<FWuwaTeamMemberUiData> Members;
	UPROPERTY(BlueprintReadOnly, Category = "Formation") int32 CurrentIndex = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct WUWA_API FWuwaFormationSwitchedEvent
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Formation") int32 OldIndex = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "Formation") int32 NewIndex = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct WUWA_API FWuwaFormationMemberHealthEvent
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Formation") int32 Index = INDEX_NONE;
	UPROPERTY(BlueprintReadOnly, Category = "Formation") float Health = 0.f;
	UPROPERTY(BlueprintReadOnly, Category = "Formation") float MaxHealth = 0.f;
};

/** 一个队员当前全部带图标的 buff（Model 里按队员存一份）。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaMemberBuffList
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Formation") TArray<FWuwaBuffUiData> Buffs;
};

USTRUCT(BlueprintType)
struct WUWA_API FWuwaFormationMemberBuffsEvent
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Formation") int32 Index = INDEX_NONE;
	/** 更新后的全部 buff 图标。 */
	UPROPERTY(BlueprintReadOnly, Category = "Formation") TArray<FWuwaBuffUiData> Buffs;
};

USTRUCT(BlueprintType)
struct WUWA_API FWuwaFormationMemberAttributesEvent
{
	GENERATED_BODY()
	/** 更新后的整份快照（Health 等也是最新的）。 */
	UPROPERTY(BlueprintReadOnly, Category = "Formation") FWuwaTeamMemberUiData Member;
};
