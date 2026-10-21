#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "WuwaDamageTextInfo.generated.h"

/**
 * 伤害飘字配置的一行（≈ 原作 DamageText，ConfigDB db_damage_text.db），行名是配置 Id。
 * 选哪一行见 UWuwaDamageNumberSubsystem::SelectTextId（原作 DamageUiManager.Q2t）。
 * 偏移是原作设计分辨率（2560×1440）的像素，显示时按画布比例换算。
 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaDamageTextInfo : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DamageText") FColor TextColor = FColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DamageText") FColor StrokeColor = FColor::Black;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DamageText") FColor CritTextColor = FColor::White;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DamageText") FColor CritStrokeColor = FColor::Black;

	/** 随机偏移范围（原作 Min/MaxDeviationX/Y）。注意原作 DamageViewData 把 Y 的上限取成了 MaxDeviationX，项目照做。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DamageText") float MinDeviationX = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DamageText") float MaxDeviationX = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DamageText") float MinDeviationY = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DamageText") float MaxDeviationY = 0.f;

	/** 动画名（伤害数字预制体上的动画节点）：自己受伤 / 自己暴击 / 打怪 / 打怪暴击 / 文字型。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DamageText") FName OwnDamageSequence;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DamageText") FName OwnCriticalDamageSequence;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DamageText") FName MonsterDamageSequence;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DamageText") FName MonsterCriticalDamageSequence;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DamageText") FName DamageTextSequence;
};
