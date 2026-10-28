#include "Game/Common/Debug/WuwaDebug.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Core/Tag/WuwaGameTags.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Formation/WuwaFormationComponent.h"
#include "Game/Input/WuwaInputRouterComponent.h"
#include "Game/Input/WuwaInputTypes.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAttributeSet.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/KismetSystemLibrary.h"

namespace WuwaDebug
{
	static int32 MoveState = 0;
	static int32 AnimParams = 0;
	static int32 CombatInput = 0;
	static int32 Climb = 0;
	static int32 LockOn = 0;
	static int32 SkillMotion = 0;

	static FAutoConsoleVariableRef CVarMoveState(TEXT("wuwa.Debug.MoveState"), MoveState,
		TEXT("1 = 屏幕上显示角色运动状态（Position / Move / Direction / Gait / Action Override / Revision，BP_RoleBase）。"));
	static FAutoConsoleVariableRef CVarAnimParams(TEXT("wuwa.Debug.AnimParams"), AnimParams,
		TEXT("1 = 屏幕上显示动画参数（左右脚进度、播放速率、步态，ABP_RoleBase）。"));
	static FAutoConsoleVariableRef CVarCombatInput(TEXT("wuwa.Debug.CombatInput"), CombatInput,
		TEXT("1 = 屏幕上显示当前主技能状态和预输入缓存（C# CombatInputDebug）。"));
	static FAutoConsoleVariableRef CVarClimb(TEXT("wuwa.Debug.Climb"), Climb,
		TEXT("1 = 画攀爬检测的射线，进入攀爬时提示（UWuwaMovementComponent）。"));
	static FAutoConsoleVariableRef CVarLockOn(TEXT("wuwa.Debug.LockOn"), LockOn,
		TEXT("1 = 画锁定：挑目标时的探测距离和候选（黄色，2 秒），当前目标红线（UWuwaLockOnComponent）。"));
	static FAutoConsoleVariableRef CVarSkillMotion(TEXT("wuwa.Debug.SkillMotion"), SkillMotion,
		TEXT("1 = 画位移吸附：到目标的线和停止距离圈（绿 = 往目标推，黄 = 抵消根运动，UWuwaMovementComponent）。"));

	/** 正在玩的世界（PIE / 游戏）里的本地玩家控制器；编辑器里从控制台或 MCP 发命令时用。 */
	static AWuwaPlayerController* FindPlayController()
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			if ((Context.WorldType == EWorldType::PIE || Context.WorldType == EWorldType::Game) && Context.World())
			{
				if (AWuwaPlayerController* PC = Cast<AWuwaPlayerController>(Context.World()->GetFirstPlayerController())) return PC;
			}
		}
		return nullptr;
	}

	/** wuwa.Debug.Input <输入 Tag> [Pressed|Held|Released]：当作按键发给输入路由（自动化排查 / MCP 驱动技能用）。 */
	static FAutoConsoleCommandWithArgsAndOutputDevice CmdInput(TEXT("wuwa.Debug.Input"),
		TEXT("wuwa.Debug.Input <InputTag> [Pressed|Held|Released]：模拟一次输入（例 wuwa.Debug.Input Input.Combat.Skill）。"),
		FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, FOutputDevice& Out)
		{
			AWuwaPlayerController* PC = FindPlayController();
			const FGameplayTag Tag = Args.Num() > 0 ? FGameplayTag::RequestGameplayTag(FName(*Args[0]), false) : FGameplayTag();
			if (!PC || !PC->GetInputRouter() || !Tag.IsValid()) { Out.Log(TEXT("wuwa.Debug.Input: no play world or unknown tag")); return; }
			FWuwaInputEvent Event;
			Event.InputTag = Tag;
			Event.RouteTag = FWuwaGameTags::Get().Input_Route_Ability;
			const FString Phase = Args.Num() > 1 ? Args[1] : TEXT("Pressed");
			Event.Phase = Phase == TEXT("Held") ? EWuwaInputPhase::Held : Phase == TEXT("Released") ? EWuwaInputPhase::Released : EWuwaInputPhase::Pressed;
			Event.Timestamp = PC->GetWorld()->GetTimeSeconds();
			Out.Logf(TEXT("wuwa.Debug.Input %s %s -> %s"), *Tag.ToString(), *Phase, PC->GetInputRouter()->DispatchInput(Event) ? TEXT("handled") : TEXT("not handled"));
		}));

	/** wuwa.Debug.SetAttribute <属性名> <数值|max>：改当前角色 UWuwaAttributeSet 的属性（max = 对应的 Max<属性名>）。 */
	static FAutoConsoleCommandWithArgsAndOutputDevice CmdSetAttribute(TEXT("wuwa.Debug.SetAttribute"),
		TEXT("wuwa.Debug.SetAttribute <Attribute> <Value|max>：设置当前角色的属性（例 wuwa.Debug.SetAttribute SpecialEnergy1 max）。"),
		FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, FOutputDevice& Out)
		{
			AWuwaPlayerController* PC = FindPlayController();
			UAbilitySystemComponent* ASC = PC ? UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(PC->GetPawn()) : nullptr;
			auto Find = [](const FString& Name)
			{
				FProperty* Property = FindFProperty<FProperty>(UWuwaAttributeSet::StaticClass(), *Name);
				return Property ? FGameplayAttribute(Property) : FGameplayAttribute();
			};
			const FGameplayAttribute Attribute = Args.Num() > 1 ? Find(Args[0]) : FGameplayAttribute();
			if (!ASC || !Attribute.IsValid()) { Out.Log(TEXT("wuwa.Debug.SetAttribute: no pawn ASC or unknown attribute")); return; }
			float Value = FCString::Atof(*Args[1]);
			if (Args[1] == TEXT("max"))
			{
				bool bFound = false;
				Value = ASC->GetGameplayAttributeValue(Find(TEXT("Max") + Args[0]), bFound);
			}
			ASC->SetNumericAttributeBase(Attribute, Value);
			Out.Logf(TEXT("wuwa.Debug.SetAttribute %s = %.2f"), *Args[0], Value);
		}));

	/** wuwa.Debug.SwitchRole <队伍序号>：切人。 */
	static FAutoConsoleCommandWithArgsAndOutputDevice CmdSwitchRole(TEXT("wuwa.Debug.SwitchRole"),
		TEXT("wuwa.Debug.SwitchRole <Index>：切到队伍里第几个角色（从 0 起）。"),
		FConsoleCommandWithArgsAndOutputDeviceDelegate::CreateLambda([](const TArray<FString>& Args, FOutputDevice& Out)
		{
			AWuwaPlayerController* PC = FindPlayController();
			if (!PC || !PC->FormationComponent || Args.Num() < 1) { Out.Log(TEXT("wuwa.Debug.SwitchRole: no play world")); return; }
			PC->FormationComponent->RequestSwitch(FCString::Atoi(*Args[0]));
		}));

	/** 列出所有 wuwa.* 调试开关：名字、当前值、说明。 */
	static FAutoConsoleCommandWithOutputDevice CmdList(TEXT("wuwa.Debug.List"), TEXT("列出所有 wuwa 调试开关和当前值。"),
		FConsoleCommandWithOutputDeviceDelegate::CreateLambda([](FOutputDevice& Out)
		{
			TArray<TPair<FString, IConsoleObject*>> Objects;
			IConsoleManager::Get().ForEachConsoleObjectThatStartsWith(FConsoleObjectVisitor::CreateLambda(
				[&Objects](const TCHAR* Name, IConsoleObject* Object) { Objects.Emplace(Name, Object); }), TEXT("wuwa."));
			Objects.Sort([](const TPair<FString, IConsoleObject*>& A, const TPair<FString, IConsoleObject*>& B) { return A.Key < B.Key; });
			for (const TPair<FString, IConsoleObject*>& Pair : Objects)
			{
				const IConsoleVariable* Variable = Pair.Value->AsVariable();
				Out.Logf(TEXT("%-28s %-4s %s"), *Pair.Key, Variable ? *Variable->GetString() : TEXT("cmd"), Pair.Value->GetHelp());
			}
		}));
}

bool UWuwaDebugLibrary::IsDebugChannelEnabled(EWuwaDebugChannel Channel)
{
	switch (Channel)
	{
	case EWuwaDebugChannel::MoveState: return WuwaDebug::MoveState > 0;
	case EWuwaDebugChannel::AnimParams: return WuwaDebug::AnimParams > 0;
	case EWuwaDebugChannel::CombatInput: return WuwaDebug::CombatInput > 0;
	case EWuwaDebugChannel::Climb: return WuwaDebug::Climb > 0;
	case EWuwaDebugChannel::LockOn: return WuwaDebug::LockOn > 0;
	case EWuwaDebugChannel::SkillMotion: return WuwaDebug::SkillMotion > 0;
	default: return false;
	}
}

void UWuwaDebugLibrary::DebugPrint(const UObject* WorldContextObject, EWuwaDebugChannel Channel, const FString& InString,
	bool bPrintToScreen, bool bPrintToLog, FLinearColor TextColor, float Duration, const FName Key)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!IsDebugChannelEnabled(Channel)) return;
	UKismetSystemLibrary::PrintString(WorldContextObject, InString, bPrintToScreen, bPrintToLog, TextColor, Duration, Key);
#endif
}
