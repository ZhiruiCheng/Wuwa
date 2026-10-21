#include "Game/NewWorld/Character/Common/Component/Skill/WuwaSkillBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Combat/WuwaFightStateBridgeComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaUnifiedStateBridgeComponent.h"
#include "Game/NewWorld/Character/Role/Component/WuwaRoleGaitBridgeComponent.h"

UWuwaSkillBridgeComponent::UWuwaSkillBridgeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWuwaSkillBridgeComponent::BindDependencies(UWuwaFightStateBridgeComponent* InFightState,
	UWuwaUnifiedStateBridgeComponent* InUnifiedState, UWuwaRoleGaitBridgeComponent* InRoleGait)
{
	FightState = InFightState;
	UnifiedState = InUnifiedState;
	RoleGait = InRoleGait;
}

bool UWuwaSkillBridgeComponent::CanBeginSkill_Implementation(UWuwaGameplayAbilityBase*) const { return false; }
int32 UWuwaSkillBridgeComponent::TryBeginSkill_Implementation(UWuwaGameplayAbilityBase*) { return 0; }
bool UWuwaSkillBridgeComponent::EndSkill_Implementation(int32) { return false; }
FWuwaSkillData UWuwaSkillBridgeComponent::GetCurrentSkillData_Implementation() const { return {}; }
bool UWuwaSkillBridgeComponent::SetSkillAcceptInput_Implementation(int32, bool) { return false; }
bool UWuwaSkillBridgeComponent::SetMainSkillReadyEnd_Implementation(int32, bool) { return false; }
bool UWuwaSkillBridgeComponent::BeginSkillAcceptInputWindow_Implementation(int32, int32, int32) { return false; }
bool UWuwaSkillBridgeComponent::EndSkillAcceptInputWindow_Implementation(int32, int32) { return false; }
bool UWuwaSkillBridgeComponent::SetSkillInterruptLevel_Implementation(int32, int32) { return false; }
bool UWuwaSkillBridgeComponent::TryInterruptForAction_Implementation(int32) { return false; }
bool UWuwaSkillBridgeComponent::IsSkillAcceptingInput_Implementation() const { return false; }
bool UWuwaSkillBridgeComponent::IsMainSkillReadyEnd_Implementation() const { return true; }
bool UWuwaSkillBridgeComponent::CallAnimBreakPoint_Implementation(int32) { return false; }
bool UWuwaSkillBridgeComponent::RequestInputCacheClear_Implementation(int32, FGameplayTag) { return false; }

void UWuwaSkillBridgeComponent::BroadcastAnimBreakPoint(int32 SkillHandle)
{
	if (SkillHandle > 0 && GetCurrentSkillData().FightStateHandle == SkillHandle)
		OnAnimBreakPoint.Broadcast(this, SkillHandle);
}

void UWuwaSkillBridgeComponent::BroadcastInputCacheClear(int32 SkillHandle, FGameplayTag InputTag)
{
	if (SkillHandle > 0 && GetCurrentSkillData().FightStateHandle == SkillHandle)
		OnInputCacheClearRequested.Broadcast(this, SkillHandle, InputTag);
}

void UWuwaSkillBridgeComponent::SetSkillTarget(AActor* Target, FName Socket)
{
	SkillTarget = Target;
	SkillTargetSocket = Target ? Socket : NAME_None;
}
