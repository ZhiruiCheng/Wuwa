#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WuwaRoleJumpEditorLibrary.generated.h"

class UAnimBlueprint;
class UAnimSequence;
class UBlendSpace;
class UBlendSpace1D;

/**
 * 跳跃 / 落地的动画蓝图迁移（编辑器显式调用，都不保存，由调用方验证后保存）。
 * 结构按原作 ABP_BaseRole 普通运动状态机的 起跳 / 下落 / 落地 三块；状态名见 WuwaJumpLand（WuwaAnimInstance.h），
 * 过渡条件由 UWuwaAnimInstance::UpdateJumpLand 算成布尔，过渡图里只读一个布尔。
 */
UCLASS()
class WUWA_API UWuwaRoleJumpEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/**
	 * 在模板的 Locmotion 状态机里加 JumpStart（内含 SM_Jump：JumpTakeoff → JumpLoop → JumpFail）、Fall、Land 三个状态和它们的过渡。
	 * 播放器的资产绑定到 UWuwaAnimInstance 的属性（JumpLeftBlendSpace 等），由子蓝图默认值提供。已经有 JumpStart 时不改。
	 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Animation")
	static FString AddTemplateJumpLand(UAnimBlueprint* Blueprint);

	/** 子蓝图默认值：左 / 右脚起跳混合空间、空中循环、Fail、下落、轻落地、重落地。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Animation")
	static FString SetChildJumpLandAnimations(UAnimBlueprint* Blueprint, UBlendSpace* JumpLeft, UBlendSpace* JumpRight,
		UAnimSequence* JumpLoop, UAnimSequence* JumpFail, UAnimSequence* Fall, UAnimSequence* LandLight, UAnimSequence* LandHeavy);

	/**
	 * 模板 SM_Ground 加 Idle → Move（规则 bLandIntoMove，优先级排在 Idle 现有出口前面，混合 0.2）：带着速度落地且有输入时直接进移动循环。
	 * 已经有这条过渡时不改。不保存。
	 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Animation")
	static FString AddGroundLandIntoMove(UAnimBlueprint* Blueprint);

	/** 重建混合空间的运行时插值数据（BlendSpaceData）；返回是否有插值段。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Animation")
	static bool ResampleBlendSpace(UBlendSpace* BlendSpace);

	/** 原作 BS_Jump_LF / BS_Jump_RF：一维，轴 Walk/Run 0..1（GridNum 2），Walk 在 0、Run 在 1。资产已存在时返回空。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Animation")
	static UBlendSpace1D* CreateJumpBlendSpace(const FString& AssetPath, UAnimSequence* Walk, UAnimSequence* Run);
};
