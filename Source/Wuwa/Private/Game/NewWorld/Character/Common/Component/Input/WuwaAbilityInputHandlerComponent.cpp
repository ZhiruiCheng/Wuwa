// Fill out your copyright notice in the Description page of Project Settings.


#include "Game/NewWorld/Character/Common/Component/Input/WuwaAbilityInputHandlerComponent.h"
#include "Game/NewWorld/Character/Common/Component/Input/UWuwaCombatInputRuntimeBridge.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAbilitySystemComponent.h"
#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillBridgeComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"


// 生成负责总体调度的C#组件
UWuwaAbilityInputHandlerComponent::UWuwaAbilityInputHandlerComponent()
{
	RuntimeClass = TSoftClassPtr<UWuwaCombatInputRuntimeBridge>(FSoftObjectPath(TEXT("/Script/UnrealSharp.WuwaCombatInputRuntime_C")));
	PrimaryComponentTick.bCanEverTick = true;
	/*TG_PrePhysics      ← 默认值，大部分 Actor/组件在这里（移动输入、逻辑）
	TG_StartPhysics
	TG_DuringPhysics
	TG_EndPhysics
	TG_PostPhysics     ← 物理结果已出（这里 UWorld 更新所有 PlayerController 的相机）
	TG_PostUpdateWork  ← 你设置的这个
	TG_LastDemotable*/
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}


//将输入时间交给脚本层处理,传入当前配置好的Tag的声明周期时间
bool UWuwaAbilityInputHandlerComponent::HandleWuwaInput_Implementation(const FWuwaInputEvent& InputEvent)
{
	UWuwaAbilitySystemComponent* ASC = GetInputASC();
	if (!ASC || !EnsureRuntime()) return false;
	BindSkillEvents(ASC);

	const float* Override = BufferLifetimeOverrides.Find(InputEvent.InputTag);
	const float Lifetime = Override ? *Override : DefaultBufferLifetimeSeconds;
	Runtime->ProcessInput(ASC, InputEvent, Lifetime);
	// 表示事件已交给战斗输入系统处理，不表示 GA 激活成功。
	return true;
}

//每帧处理Input的Pending List
void UWuwaAbilityInputHandlerComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	if (!IsValid(Runtime)) return;

	UWuwaAbilitySystemComponent* ASC = GetInputASC();
	if (!ASC)
	{
		ResetRuntime();
		return;
	}
	BindSkillEvents(ASC);
	Runtime->ProcessPendingInput(ASC);
}

UWuwaAbilitySystemComponent* UWuwaAbilityInputHandlerComponent::GetInputASC() const
{
	AWuwaPlayerController* PC = Cast<AWuwaPlayerController>(GetOwner());
	if (!IsValid(PC) || !PC->IsLocalController() || !IsValid(PC->GetPawn())) return nullptr;
	UWuwaAbilitySystemComponent* ASC = PC->GetASC();
	return IsValid(ASC) && ASC->GetAvatarActor() == PC->GetPawn() ? ASC : nullptr;
}

//注册断点事件和清空缓存事件
void UWuwaAbilityInputHandlerComponent::BindSkillEvents(UWuwaAbilitySystemComponent* ASC)
{
	const AWuwaCharacter* Character = ASC ? Cast<AWuwaCharacter>(ASC->GetAvatarActor()) : nullptr;
	UWuwaSkillBridgeComponent* Skills = IsValid(Character) ? Character->SkillComponent.Get() : nullptr;
	if (BoundSkills.Get() == Skills) return;
	UnbindSkillEvents();
	if (!IsValid(Skills)) return;
	BoundSkills = Skills;
	Skills->OnAnimBreakPoint.AddDynamic(this, &ThisClass::HandleAnimBreakPoint);
	Skills->OnInputCacheClearRequested.AddDynamic(this, &ThisClass::HandleInputCacheClear);
}

void UWuwaAbilityInputHandlerComponent::UnbindSkillEvents()
{
	if (UWuwaSkillBridgeComponent* Skills = BoundSkills.Get())
	{
		Skills->OnAnimBreakPoint.RemoveDynamic(this, &ThisClass::HandleAnimBreakPoint);
		Skills->OnInputCacheClearRequested.RemoveDynamic(this, &ThisClass::HandleInputCacheClear);
	}
	BoundSkills.Reset();
}

bool UWuwaAbilityInputHandlerComponent::IsCurrentSkillEvent(UWuwaSkillBridgeComponent* Skills, int32 SkillHandle) const
{
	const UWuwaAbilitySystemComponent* ASC = GetInputASC();
	const AWuwaCharacter* Character = ASC ? Cast<AWuwaCharacter>(ASC->GetAvatarActor()) : nullptr;
	return IsValid(Skills) && BoundSkills.Get() == Skills && IsValid(Character)
		&& Character->SkillComponent == Skills && SkillHandle > 0
		&& Skills->GetCurrentSkillData().FightStateHandle == SkillHandle;
}

//当有新的断电就让Runtime处理一次
void UWuwaAbilityInputHandlerComponent::HandleAnimBreakPoint(UWuwaSkillBridgeComponent* Skills, int32 SkillHandle)
{
	if (IsValid(Runtime) && IsCurrentSkillEvent(Skills, SkillHandle))
		Runtime->ProcessPendingInput(GetInputASC());
}

//清空输入缓存
void UWuwaAbilityInputHandlerComponent::HandleInputCacheClear(UWuwaSkillBridgeComponent* Skills,
	int32 SkillHandle, FGameplayTag InputTag)
{
	if (IsCurrentSkillEvent(Skills, SkillHandle)) ClearBufferedInput(InputTag);
}

int32 UWuwaAbilityInputHandlerComponent::ClearBufferedInput(FGameplayTag InputTag)
{
	return IsValid(Runtime) ? Runtime->ClearBufferedInput(InputTag) : 0;
}

//确保这个实例存在
bool UWuwaAbilityInputHandlerComponent::EnsureRuntime()
{
	if (IsValid(Runtime)){return true;}

	//RuntimeClass软引用只存路径,有需要的时候才会加载
	UClass* Class = RuntimeClass.LoadSynchronous();//LoadSynchronous() 的作用是：如果这个类已经在内存里就直接返回；如果还没加载，就当场同步把它从磁盘加载进来，然后返回 UClass*。

	if (!Class || Class->HasAnyClassFlags(CLASS_Abstract))//防止实例化失败或者是抽象函数不让访问
	{
		UE_LOG(LogTemp, Error,TEXT("CombatInputRuntime unavailable: %s"),*RuntimeClass.ToString());
		return false;
	}

	Runtime = NewObject<UWuwaCombatInputRuntimeBridge>(this, Class);

	return IsValid(Runtime);
}

void UWuwaAbilityInputHandlerComponent::ResetRuntime()
{
	UnbindSkillEvents();
	if (IsValid(Runtime)){Runtime->ResetInput();}
}

void UWuwaAbilityInputHandlerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ResetRuntime();
	Runtime = nullptr;
	Super::EndPlay(EndPlayReason);
}
