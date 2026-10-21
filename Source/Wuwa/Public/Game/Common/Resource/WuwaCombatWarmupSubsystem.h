#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WuwaCombatWarmupSubsystem.generated.h"

struct FStreamableHandle;

/**
 * 战斗预热（加载界面的 Warmup 阶段调用）：把"第一次放技能 / 第一次命中"时才会同步加载、现编译的东西提前做掉。
 *
 * 角色、Boss 的蒙太奇和特效是硬引用，进场时已经加载了；但它们身上还有很多软引用要等用到时才同步加载（卡一下），比如
 * 动画通知 / 特效模型里的 Wwise 事件（第一次加载还会初始化 Wwise 模块）、战斗配置表（阵营、锁定、伤害）、子弹表里的顿帧曲线、受击蒙太奇。
 * Begin 时用反射扫一遍"已加载的动画通知、特效模型、战斗 / 战斗 UI 配置、场景里每个战斗单位（含它的子弹表、受击效果表）"的
 * 全部软引用（TSoftObjectPtr / FSoftObjectPath，跳过 /Script/ 下的类），一次异步加载，句柄存到世界结束，保证不被 GC 回收。
 * 编辑器里还会把编译结果过期的 Niagara 系统请求编译（正常情况下用 WuwaNiagaraCompileSetup 存好就不会有）。
 */
UCLASS()
class WUWA_API UWuwaCombatWarmupSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	static UWuwaCombatWarmupSubsystem* Get(const UObject* WorldContext);

	/** 开始预热（重复调用无副作用）。 */
	void Begin();
	bool HasBegun() const { return bBegun; }
	/** 0~1。 */
	float GetProgress() const;
	bool IsDone() const;
	int32 GetWarmedAssetCount() const { return AssetCount; }

	virtual void Deinitialize() override;

	/** 反射收集一个对象 / 结构里的全部软引用（递归结构、数组、Map、Set）。测试也用。 */
	static void CollectSoftReferences(const UStruct* Struct, const void* Data, TSet<FSoftObjectPath>& Out);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	TSharedPtr<FStreamableHandle> Handle;
	bool bBegun = false;
	int32 AssetCount = 0;
	/** 请求了编译、还没编完的 Niagara 系统。 */
	TArray<TWeakObjectPtr<UObject>> CompilingSystems;
	int32 CompileTotal = 0;
};
