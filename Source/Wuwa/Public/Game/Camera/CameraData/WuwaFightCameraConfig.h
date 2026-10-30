#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "WuwaFightCameraConfig.generated.h"

class UDataTable;

/*
 * 战斗镜头配置：默认配置 + 配置列表。
 * 战斗镜头和每个子控制器各有一张"编号 → 数值"表，编号就是下面这些枚举的值；控制器启动时登记"几号写进自己的哪个字段"。
 * 这里只列已经用到的编号，别的功能做了再加。
 */

/** "基础"表的编号：战斗镜头自己的配置。 */
UENUM(BlueprintType)
enum class EWuwaFightCameraDefault : uint8
{
	None = 0 UMETA(Hidden),
	ArmLength = 1 UMETA(DisplayName = "基础臂长"),
	MinArmLength = 2 UMETA(DisplayName = "最小臂长"),
	MaxArmLength = 3 UMETA(DisplayName = "最大臂长"),
	Fov = 5 UMETA(DisplayName = "Fov"),
	CameraOffsetX = 6 UMETA(DisplayName = "相机臂偏移X"),
	CameraOffsetY = 7 UMETA(DisplayName = "相机臂偏移Y"),
	CameraOffsetZ = 8 UMETA(DisplayName = "相机臂偏移Z"),
	CollisionProbeSize = 11 UMETA(DisplayName = "障碍碰撞检测胶囊体半径"),
	PitchLimitMin = 45 UMETA(DisplayName = "Pitch限制Min"),
	PitchLimitMax = 46 UMETA(DisplayName = "Pitch限制Max"),
	ArmOffsetX = 62 UMETA(DisplayName = "相机臂原点偏移X"),
	ArmOffsetY = 63 UMETA(DisplayName = "相机臂原点偏移Y"),
	ArmOffsetZ = 64 UMETA(DisplayName = "相机臂原点偏移Z"),
};

/** "镜头输入"表的编号：输入控制器的配置。 */
UENUM(BlueprintType)
enum class EWuwaFightCameraInput : uint8
{
	None = 0 UMETA(Hidden),
	ZoomSpeed = 1 UMETA(DisplayName = "滚轮轴影响臂长系数"),
	SmoothFactorMin = 2 UMETA(DisplayName = "镜头输入缓冲系数最小值"),
	SmoothFactorMax = 3 UMETA(DisplayName = "镜头输入缓冲系数最大值"),
	SmoothFactorRange = 4 UMETA(DisplayName = "镜头输入缓冲系数输入极值"),
	InputSpeedMin = 5 UMETA(DisplayName = "镜头输入速率Min"),
	InputSpeedMax = 6 UMETA(DisplayName = "镜头输入速率Max"),
	SensitivityYawMin = 7 UMETA(DisplayName = "镜头Yaw灵敏度系数最小值"),
	SensitivityYawMax = 8 UMETA(DisplayName = "镜头Yaw灵敏度系数最大值"),
	SensitivityYawRange = 9 UMETA(DisplayName = "镜头Yaw灵敏度系数输入极值"),
	SensitivityPitchMin = 10 UMETA(DisplayName = "镜头Pitch灵敏度系数最小值"),
	SensitivityPitchMax = 11 UMETA(DisplayName = "镜头Pitch灵敏度系数最大值"),
	SensitivityPitchRange = 12 UMETA(DisplayName = "镜头Pitch灵敏度系数输入极值"),
};

/** "Modify镜头"表的编号：镜头修改结束后，各项按速度回去时的速度。 */
UENUM(BlueprintType)
enum class EWuwaFightCameraModify : uint8
{
	None = 0 UMETA(Hidden),
	ArmLengthLagSpeed = 1 UMETA(DisplayName = "臂长还原过渡速度"),
	CameraOffsetLagSpeed = 2 UMETA(DisplayName = "臂偏移还原过渡速度"),
	ArmRotationLagSpeed = 3 UMETA(DisplayName = "旋转还原过渡速度"),
	FovLagSpeed = 4 UMETA(DisplayName = "Fov还原过渡速度"),
};

/** 过渡曲线的类型：把 0~1 的进度换成 0~1 的过渡比例。 */
UENUM(BlueprintType)
enum class EWuwaCameraCurveType : uint8
{
	Linear = 0 UMETA(DisplayName = "线性"),
	SquaredConcave = 1 UMETA(DisplayName = "二阶-凹函数"),
	SquaredConvex = 2 UMETA(DisplayName = "二阶-凸函数"),
	CubicConcave = 3 UMETA(DisplayName = "三阶-凹函数"),
	CubicConvex = 4 UMETA(DisplayName = "三阶-凸函数"),
	CubicS = 5 UMETA(DisplayName = "三阶-S函数"),
	CubicReverseS = 6 UMETA(DisplayName = "三阶-反S函数"),
	PowerConcave = 7 UMETA(DisplayName = "幂函数-凹函数"),
	PowerConvex = 8 UMETA(DisplayName = "幂函数-凸函数"),
};

/** 一条过渡曲线：曲线类型 + 参数 N（各类型怎么用 N 见 C# ZrCameraCurve）。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaCameraCurve
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	EWuwaCameraCurveType CurveType = EWuwaCameraCurveType::Linear;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Camera")
	float N = 0.f;
};

/** 配置行的类型：基础镜头一直生效；子镜头在角色带上它的标签时生效。 */
UENUM(BlueprintType)
enum class EWuwaFightCameraConfigType : uint8
{
	Base = 0 UMETA(DisplayName = "基础镜头"),
	Sub = 2 UMETA(DisplayName = "子镜头"),
};

/**
 * 配置列表的一行：
 * - 基础镜头：一直生效，通常勾 bResetDefaultConfig；
 * - 子镜头：控制的角色身上有 Tag 时生效（比如瞄准），同时生效的行按 Priority 从低到高叠加，高的覆盖低的；
 * 每一行只写要改的编号，没写的沿用下面的行 / 默认配置。生效和失效时镜头按淡入 / 淡出时间过渡。
 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaFightCameraConfigRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	EWuwaFightCameraConfigType Type = EWuwaFightCameraConfigType::Sub;

	/**  Tag：控制的角色带上这个标签时这一行生效（子镜头用）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	FGameplayTag Tag;

	/**  优先级：数大的后叠加。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	int32 Priority = 0;

	/**  淡入时间 / 淡入曲线：这一行开始生效时镜头的过渡。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (Units = "s"))
	float FadeInTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	FWuwaCameraCurve FadeInCurve;

	/**  淡出时间 / 淡出曲线：这一行失效时镜头的过渡。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera", meta = (Units = "s"))
	float FadeOutTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	FWuwaCameraCurve FadeOutCurve;

	/**  启用Modify镜头：关掉时，这一行生效期间技能镜头修改不生效。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	bool bEnableModifyCamera = true;

	/**  是否重置默认配置：叠配置时从这一行开始（比它优先级低的行不再生效）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	bool bResetDefaultConfig = false;

	/**  是否重置镜头锁定：算"哪些控制器被关掉"时从这一行开始。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	bool bResetCameraLock = false;

	/** 是否独立过渡时间：这一行的过渡还没走完又切了配置，新的过渡不用自己的时间，而是走完这次剩下的时间。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	bool bUniqueFade = false;

	/** 基础：只写要改的编号。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	TMap<EWuwaFightCameraDefault, float> Base;

	/** 镜头输入 / 镜头输入曲线：只写要改的编号。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	TMap<EWuwaFightCameraInput, float> Input;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	TMap<EWuwaFightCameraInput, FWuwaCameraCurve> InputCurves;

	/** Modify镜头：只写要改的编号。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Camera")
	TMap<EWuwaFightCameraModify, float> ModifyCamera;
};

/**
 * 战斗镜头配置资产：Base / Input / ModifyCamera 是默认配置（构造函数里填好，只含用到的编号），ConfigList 是配置列表。
 * C# 战斗镜头按固定路径加载这个资产。
 */
UCLASS(BlueprintType)
class WUWA_API UWuwaFightCameraConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UWuwaFightCameraConfig();

	/** 基础：战斗镜头自己的默认配置。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Base")
	TMap<EWuwaFightCameraDefault, float> Base;

	/** 镜头输入 / 镜头输入曲线：输入控制器的默认配置。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TMap<EWuwaFightCameraInput, float> Input;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TMap<EWuwaFightCameraInput, FWuwaCameraCurve> InputCurves;

	/** Modify镜头：镜头修改控制器的默认配置。（不能直接叫 Modify：会遮住 UObject::Modify()） */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Modify")
	TMap<EWuwaFightCameraModify, float> ModifyCamera;

	/** 配置列表：基础镜头 + 按标签生效的子镜头（行结构 FWuwaFightCameraConfigRow）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ConfigList", meta = (RequiredAssetDataTags = "RowStructure=/Script/Wuwa.WuwaFightCameraConfigRow"))
	TObjectPtr<UDataTable> ConfigList;
};
