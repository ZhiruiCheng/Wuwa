#pragma once
#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Engine/DeveloperSettings.h"
#include "GameplayTagContainer.h"
#include "Game/LGUI/Layer/WuwaUiLayer.h"
#include "Game/NewWorld/Character/Role/DataAsset/WuwaRoleDataAsset.h"
#include "WuwaUISettings.generated.h"

class ULGUIPrefab;
class USoundBase;
class UWuwaUiView;

/**
 * 界面配置表的一行（≈ 鸣潮 UiViewInfo），行名就是界面名。
 * 输入相关（快捷键、ESC 关闭、鼠标）由项目自己的输入系统处理，不在这里配。
 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaUiViewConfig : public FTableRowBase
{
	GENERATED_BODY()
	/** 界面逻辑类；不填就用 UWuwaUiView 本身（没有逻辑的纯展示界面）。 */
	UPROPERTY(EditAnywhere, Category = "Wuwa UI") TSubclassOf<UWuwaUiView> ViewClass;
	UPROPERTY(EditAnywhere, Category = "Wuwa UI") TSoftObjectPtr<ULGUIPrefab> Prefab;
	UPROPERTY(EditAnywhere, Category = "Wuwa UI") EWuwaUiLayer Layer = EWuwaUiLayer::Normal;
	/** 允许同时打开多个实例（≈ 鸣潮 IsMultipleView）；Set 容器的层（HUD/Loading/Debug）不支持。 */
	UPROPERTY(EditAnywhere, Category = "Wuwa UI") bool bAllowMultiple = false;
	/** Float 层排队用：同一时刻只显示一个，数值大的先显示。 */
	UPROPERTY(EditAnywhere, Category = "Wuwa UI") int32 Priority = 0;
	/** 常驻（≈ IsPermanent）：切地图时关掉的界面里，它会在新地图自动重新打开。 */
	UPROPERTY(EditAnywhere, Category = "Wuwa UI") bool bIsPermanent = false;
	/** 这些界面里任何一个正在显示，就不能打开我（≈ BeObstructView）。 */
	UPROPERTY(EditAnywhere, Category = "Wuwa UI") TArray<FName> BeObstructViews;

	// —— 打开时的效果（打开/关闭动画在预制体根节点的 WuwaUiViewAnimation 组件上配）——

	/** 第一次显示时播放（≈ OpenAudioEvent）。 */
	UPROPERTY(EditAnywhere, Category = "Wuwa UI|Effects") TSoftObjectPtr<USoundBase> OpenSound;
	/** 关闭时播放（≈ CloseAudioEvent）；只是被上层盖住不播。 */
	UPROPERTY(EditAnywhere, Category = "Wuwa UI|Effects") TSoftObjectPtr<USoundBase> CloseSound;
	/** 打开期间的游戏时间缩放（≈ TimeDilation）：1 不变，0 暂停，0.2 放慢；多个界面同时要求时取最慢的，全部关掉后恢复。 */
	UPROPERTY(EditAnywhere, Category = "Wuwa UI|Effects", meta = (ClampMin = "0.0", ClampMax = "1.0")) float TimeDilation = 1.f;
	/** 显示期间在界面后面铺一层背景模糊（游戏画面和下面的界面都会被模糊）。 */
	UPROPERTY(EditAnywhere, Category = "Wuwa UI|Effects") bool bBlurBackground = false;
	UPROPERTY(EditAnywhere, Category = "Wuwa UI|Effects", meta = (EditCondition = "bBlurBackground", ClampMin = "0.0", ClampMax = "100.0")) float BlurStrength = 10.f;
	/** 打开期间要切到的 UI 相机，交给 IWuwaUiCameraHandler（相机系统）处理；None 表示不切。 */
	UPROPERTY(EditAnywhere, Category = "Wuwa UI|Effects") FName UiCameraMode;
	/** 全屏界面（≈ IsFullScreen）：完全显示后把它下面的各层隐藏，减少绘制；开始关闭时恢复。 */
	UPROPERTY(EditAnywhere, Category = "Wuwa UI|Effects") bool bIsFullScreen = false;
};

UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Wuwa UI"))
class WUWA_API UWuwaUISettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere) TSoftObjectPtr<ULGUIPrefab> RootPrefab;
	/** 世界里没有 LGUI EventSystem 时由 UiManager 生成（≈ 鸣潮 LguiEventSystemManager）；默认用 LGUI 自带的预设，鼠标输入它已经处理好。 */
	UPROPERTY(Config, EditAnywhere) TSoftClassPtr<AActor> EventSystemClass;
	/** 界面配置表，行结构是 FWuwaUiViewConfig。 */
	UPROPERTY(Config, EditAnywhere, meta = (RequiredAssetDataTags = "RowStructure=/Script/Wuwa.WuwaUiViewConfig"))
	TSoftObjectPtr<UDataTable> ViewConfigTable;
	/** 加载界面的展示内容（背景图、标题、描述），行结构是 FWuwaLoadingShowRow；每次加载按权重随机选一行（≈ 鸣潮 LoadingShowData）。 */
	UPROPERTY(Config, EditAnywhere, meta = (RequiredAssetDataTags = "RowStructure=/Script/Wuwa.WuwaLoadingShowRow"))
	TSoftObjectPtr<UDataTable> LoadingShowTable;
	/** 元素的界面配置（图标、颜色），行名是元素标签（Element.Fusion……），行结构 FWuwaElementUiInfo（≈ 鸣潮 ElementInfo）。 */
	UPROPERTY(Config, EditAnywhere, meta = (RequiredAssetDataTags = "RowStructure=/Script/Wuwa.WuwaElementUiInfo"))
	TSoftObjectPtr<UDataTable> ElementTable;
	/** 角色的特殊能量条，行名是能量条 Id（UWuwaRoleDataAsset::SpecialEnergyBarId），行结构 FWuwaSpecialEnergyBarInfo（≈ 鸣潮 DT_SpecialEnergyBarInfo）。 */
	UPROPERTY(Config, EditAnywhere, meta = (RequiredAssetDataTags = "RowStructure=/Script/Wuwa.WuwaSpecialEnergyBarInfo"))
	TSoftObjectPtr<UDataTable> SpecialEnergyBarTable;
	/**
	 * 哪个输入对应 HUD 上哪个技能按钮（≈ 原作技能按钮的 ActionName）：按下时那个按钮播点击圈。
	 * 键是输入的 InputTag（DA_IARouterMap 里配的），值是按钮类型（UWuwaHudSkillBarPanel 按类型找按钮）。
	 */
	UPROPERTY(Config, EditAnywhere)
	TMap<FGameplayTag, EWuwaSkillButtonType> SkillButtonInputs;
	/** R 键（共鸣解放）按钮外圈的共鸣能量环（≈ 原作 UiItem_BattleSkillUltraItem = FightSkillUlt），由 UWuwaHudSkillBarPanel 挂到 R 按钮下。 */
	UPROPERTY(Config, EditAnywhere)
	TSoftObjectPtr<ULGUIPrefab> SkillUltPrefab;
};
