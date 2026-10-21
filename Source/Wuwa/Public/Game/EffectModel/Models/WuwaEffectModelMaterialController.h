#pragma once

#include "Game/EffectModel/Base/WuwaEffectModelBase.h"
#include "UObject/SoftObjectPath.h"
#include "WuwaEffectModelMaterialController.generated.h"

class UWuwaCharacterMaterialControllerData;

/**
 * 对应原作 EffectModelMaterialController 的两种材质控制器资源引用。
 * 原作 Spec 把资源交给目标角色的 CharRenderingComponent，并在停止时移除返回的控制器句柄。
 * 本项目的执行数据是 ControllerData（从原作 PD_CharacterControllerData 转出）；两个原作路径只作记录。
 * 原作动画通知 AnimNotifyAddMaterialControllerData 直接播放控制器数据，本项目用 PlayEffect 通知播放本 Model 代替。
 */
UCLASS(BlueprintType, Blueprintable)
class WUWA_API UWuwaEffectModelMaterialController : public UWuwaEffectModelBase
{
	GENERATED_BODY()

public:
	/** 原作单个角色材质控制器数据；相关通知引用 PD_CharacterControllerData_C。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material Controller")
	FSoftObjectPath MaterialControllerData;

	/** 原作控制器组数据；相关通知引用 PD_CharacterControllerDataGroup_C。允许与单个控制器同时配置。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material Controller")
	FSoftObjectPath MaterialControllerGroupData;

	/** 实际播放的控制器数据：作用于播放请求 AttachTo 所属角色的全部骨骼网格。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material Controller")
	TObjectPtr<UWuwaCharacterMaterialControllerData> ControllerData;
};
