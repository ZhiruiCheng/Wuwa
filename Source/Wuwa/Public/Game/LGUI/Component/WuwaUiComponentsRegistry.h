#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "WuwaUiComponentsRegistry.generated.h"

/**
 * 挂在界面预制体根节点上（≈ 鸣潮 LGUIComponentsRegistry）。
 * 预制体这边按顺序填 Actor，代码那边用下标当编号取组件：GetButton(0)、GetText(1)……
 */
UCLASS(ClassGroup = (WuwaUI), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaUiComponentsRegistry : public UActorComponent
{
	GENERATED_BODY()
public:
	UWuwaUiComponentsRegistry();

	/** 越界返回 nullptr。 */
	AActor* GetActorAt(int32 Index) const;
	int32 Num() const { return Components.Num(); }

#if WITH_EDITOR
	/** 编辑器工具用：生成测试预制体时填写。 */
	void SetComponents(const TArray<AActor*>& InComponents);
#endif

protected:
	/** 下标就是代码里的编号。只能往后追加，不能插入或删除（否则后面的编号全错）。 */
	UPROPERTY(EditAnywhere, Category = "Wuwa UI")
	TArray<TObjectPtr<AActor>> Components;
};
