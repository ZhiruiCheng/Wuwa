#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "WuwaModelBase.generated.h"

class UWuwaEventSubsystem;

/**
 * Model 基类（≈ 鸣潮 ModelBase）：只存数据和读数据的方法，不处理业务、不碰界面。
 * 是 LocalPlayerSubsystem：UE 自动为每个非抽象子类创建一个实例（≈ ModelManager），切地图也活着，
 * 所以需要跨地图保留的界面状态放在这里。用 LocalPlayer->GetSubsystem<T>() 或界面上的 GetModel<T>() 取。
 */
UCLASS(Abstract)
class WUWA_API UWuwaModelBase : public ULocalPlayerSubsystem
{
	GENERATED_BODY()
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

protected:
	virtual void OnInit() {}
	virtual void OnClear() {}
	/** 这个玩家所在的世界被清理（切地图、结束 PIE）时调用；数据默认保留。 */
	virtual void OnLeaveLevel() {}

private:
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	FDelegateHandle WorldCleanupHandle;
};
