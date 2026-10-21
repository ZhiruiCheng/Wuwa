#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"
#include "Game/NewWorld/Character/Role/DataAsset/WuwaRoleDataAsset.h"
#include "WuwaHudEvents.generated.h"

/** 战斗 HUD 的事件（≈ 鸣潮 BattleUi 里技能按钮相关的几项）。 */
namespace WuwaHudEvents
{
	/** 某个技能按钮对应的按键按下了；参数 FWuwaSkillButtonPressedEvent。由 UWuwaSkillButtonUiController 发出。 */
	WUWA_API UE_DECLARE_GAMEPLAY_TAG_EXTERN(SkillButtonPressed);
}

USTRUCT(BlueprintType)
struct WUWA_API FWuwaSkillButtonPressedEvent
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, Category = "Hud") EWuwaSkillButtonType ButtonType = EWuwaSkillButtonType::None;
};
