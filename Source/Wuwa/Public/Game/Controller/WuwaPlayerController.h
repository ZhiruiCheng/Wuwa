// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/Tag/WuwaGameTags.h"
#include "Game/Input/WuwaInputTypes.h"
#include "GameFramework/PlayerController.h"
#include "Game/Input/DataAsset/WuwaInputDataAsset.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAbilitySystemComponent.h"
#include "WuwaPlayerController.generated.h"

class UWuwaUIInputHandler;
class UInputMappingContext;
class UWuwaMoveInputHandler;
struct FInputActionInstance;
class UWuwaAbilityInputHandlerComponent;
class UWuwaFormationComponent;
class UWuwaFormationInputHandler;
class UWuwaInputRouterComponent;
/**
 * 
 */
UCLASS()
class WUWA_API AWuwaPlayerController : public APlayerController
{
	GENERATED_BODY()
	
public:
	
#pragma region Lifecycle
	//类声明周期事件
	AWuwaPlayerController();
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	
	//输入上下文失效
	/*Possess(新角色) 这个函数通常用来清理之前角色遗留下来的状态
     └ OnPossess
		├ 如果已经控制着一个角色 → 先 UnPossess() → SetPawn(nullptr)   ← 第 1 次调用
		├ 新角色->PossessedBy(this)
		└ SetPawn(新角色)                                              ← 第 2 次调用*/
	virtual void SetPawn(APawn* InPawn) override;
	//游戏窗口失焦后会导致操作一直按住没有松开
	virtual void FlushPressedKeys() override;
	
#pragma endregion
	//InputMap
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	UWuwaInputDataAsset* InputTagMap;
	
	//ASC
	UPROPERTY()
	TObjectPtr<UWuwaAbilitySystemComponent> AscComponent;
	UWuwaAbilitySystemComponent* GetASC();
	
#pragma  region IMC注册输入
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;
	
	void RegisterMappingContext() const;

#pragma  endregion
	
#pragma region InputRouter and Handlers
	//InputRouter 中转组件引用
	UFUNCTION(BlueprintPure, Category="Wuwa|Input")
	UWuwaInputRouterComponent* GetInputRouter() const { return InputRouter; }
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Wuwa|Input")
	TObjectPtr<UWuwaInputRouterComponent> InputRouter;
	
	//InputHandler 注册所有模块的输入中转类
	void  RegisterInputRouteHandlers();

	/** 先把语义输入记录到当前控制 Pawn 的输入意图，再交给 InputRouter 分发；返回是否有系统处理了它。 */
	bool RouteInputEvent(const FWuwaInputEvent& InputEvent);

	/**
	 * 每个真正要分发的输入（没被屏蔽的）在交给 Handler 之前广播一次。只给界面做按键反馈用（≈ 鸣潮 InputDistributeController.BindActions：
	 * 技能按钮自己监听按键动作，按下时播点击特效），不参与处理、不影响分发结果。
	 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FWuwaOnInputRouted, const FWuwaInputEvent&);
	FWuwaOnInputRouted OnInputRouted;
	UPROPERTY(VisibleAnywhere, Category = "Wuwa|Input")
	TObjectPtr<UWuwaAbilityInputHandlerComponent>AbilityInputHandler;
	UPROPERTY(VisibleAnywhere, Category = "Wuwa|Input")
	TObjectPtr<UWuwaMoveInputHandler> MoveInputHandler;
	UPROPERTY(VisibleAnywhere, Category = "Wuwa|UI")
	TObjectPtr<UWuwaUIInputHandler> UIInputHandler;

#pragma endregion

#pragma region 输入屏蔽
	/**
	 * BlockOwner（通常是某个 Handler，传 this）加一条屏蔽：BlockedRoutes 里这些路由的按下/持续输入不再分发
	 * （松开/取消照常放行），并且给这些路由上正按着的输入补发一次取消。
	 * 同一个 BlockOwner 再加会覆盖它原来那条；只有它自己能移除；它被销毁后这条自动失效。
	 * 多个 BlockOwner 同时屏蔽时取并集。
	 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Input")
	void AddInputBlock(UObject* BlockOwner, const FGameplayTagContainer& BlockedRoutes);
	
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Input")
	void RemoveInputBlock(UObject* BlockOwner);

	/** 这条路由现在有没有被任何 Owner 屏蔽（按路由 Tag 精确匹配）。 */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Input")
	bool IsRouteBlocked(const FGameplayTag& RouteTag) const;

#pragma endregion
	
#pragma region 切人组件
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wuwa|Formation")
	TObjectPtr<UWuwaFormationComponent> FormationComponent;
	/** 按键 1/2/3 → 要切到第几个队员，交给 FormationComponent。 */
	UPROPERTY(VisibleAnywhere, Category = "Wuwa|Formation")
	TObjectPtr<UWuwaFormationInputHandler> FormationInputHandler;
#pragma  endregion
	
protected:
	//注册所有的Action事件,按下对应按键后会将
	virtual void SetupInputComponent() override;
	
private:
	void HandleRoutedInput(const FInputActionInstance& Instance,FGameplayTag InputTag,FGameplayTag RouteTag,EWuwaInputPhase Phase);

	/** 按键 IA 本身或它在 DefaultMappingContext 里的映射配了 Hold 触发器。 */
	bool HasHoldTrigger(const UInputAction* Action) const;

	// Hold 触发器到点后每帧都会 Triggered；记录本次按下已经发过 Held 的动作，按下/松开时清除。
	TSet<TWeakObjectPtr<const UInputAction>> HeldDispatchedActions;
	/** 长按连续触发的按键（输入映射表 bContinuousHold）：Held 每帧都发，不去重。 */
	TSet<TWeakObjectPtr<const UInputAction>> ContinuousHoldActions;

	/** Owner → 它屏蔽的路由。弱引用：Owner 没了这条就不算数。 */
	TMap<TWeakObjectPtr<UObject>, FGameplayTagContainer> InputBlocks;

	/** 已经分发出去、还没松开/取消的输入（按 Action 记最后一次事件）：开始屏蔽某条路由时给它们补发取消。 */
	TMap<TWeakObjectPtr<const UInputAction>, FWuwaInputEvent> ActiveInputs;
	
};
