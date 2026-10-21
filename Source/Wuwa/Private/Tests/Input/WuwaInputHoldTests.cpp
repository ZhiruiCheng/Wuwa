#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputTriggers.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaInputHoldAssetsTest, "Wuwa.Input.Hold.AssetConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaInputHoldAssetsTest::RunTest(const FString& Parameters)
{
	// 只读检查真实 IMC：每个按键（Boolean）动作都配了非一次性的 Hold，控制器才能产生 Held。
	// 时长 0.5 秒；普攻 IA_Attack 按原作 BP_InputBase 长按配置是 0.3 秒（ArtSource/Combat/input_hold_triggers.json）。
	const UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/CoreInput/Contexts/IMC_Character.IMC_Character"));
	if (!TestNotNull(TEXT("Character mapping context exists"), Context))
	{
		return false;
	}

	auto FindHold = [](const TArray<TObjectPtr<UInputTrigger>>& Triggers) -> const UInputTriggerHold*
	{
		for (const UInputTrigger* Trigger : Triggers)
		{
			if (const UInputTriggerHold* Hold = Cast<UInputTriggerHold>(Trigger)) return Hold;
		}
		return nullptr;
	};

	for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
	{
		const UInputAction* Action = Mapping.Action;
		if (!Action || Action->ValueType != EInputActionValueType::Boolean) continue;
		const UInputTriggerHold* Hold = FindHold(Action->Triggers);
		if (!Hold) Hold = FindHold(Mapping.Triggers);
		const FString Name = FString::Printf(TEXT("%s (%s)"), *Action->GetName(), *Mapping.Key.ToString());
		if (!TestNotNull(*FString::Printf(TEXT("%s has a Hold trigger"), *Name), Hold)) continue;
		const float Expected = Action->GetName() == TEXT("IA_Attack") ? 0.3f : 0.5f;
		TestEqual(*FString::Printf(TEXT("%s holds for %.1f seconds"), *Name, Expected), Hold->HoldTimeThreshold, Expected);
		// 一次性 Hold 触发后立刻回到 None，按键还按着就会收到 Completed，被当成松开。
		TestFalse(*FString::Printf(TEXT("%s Hold is not one-shot"), *Name), Hold->bIsOneShot);
	}
	return true;
}

#endif
