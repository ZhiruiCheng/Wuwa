#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "AbilitySystemComponent.h"
#include "Algo/Find.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Game/Controller/WuwaPlayerController.h"
#include "Game/Formation/WuwaFormationComponent.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaAttributeSet.h"
#include "Game/NewWorld/Character/Common/Component/Abilities/WuwaElement.h"
#include "Game/NewWorld/Character/Role/DataAsset/WuwaRoleDataAsset.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"

// 角色生成后 ASC 属性 = 原作基础属性 × 等级成长（UWuwaRoleAttributeInit，数据由 -run=WuwaRoleAttributeSetup 写入）。
// 期望值是按原作数据手算的：数值 = 向下取整(1 级值 × 成长倍率)，同等级取突破后的倍率。
namespace WuwaRoleAttributeTests
{
	struct FExpected
	{
		int32 BasePropertyId;
		int32 Level;
		float MaxHealth, Attack, Defense, MaxEnergy, MaxSpecialEnergy1, MaxSpecialEnergy2;
	};

	// 长离 1205：1 级 HP 831 / ATK 37 / DEF 90，80 级突破 6 倍率 11.6573 / 11.6573 / 11.3982；共鸣能量上限 12500，特殊能量 1 上限 4。
	// 嘉贝莉娜 1208：1 级 HP 824 / ATK 37 / DEF 91，90 级突破 6 倍率 12.5 / 12.5 / 12.2222；特殊能量 2 上限 40；
	// 特殊能量 1（能量条）上限客户端表里是 0，按能量条配置推断 100（prepare_role_attributes SPECIAL_ENERGY_MAX_PLACEHOLDER）。
	const FExpected Expected[] = {
		{1205, 80, 9687.f, 431.f, 1025.f, 12500.f, 4.f, 0.f},
		{1208, 90, 10300.f, 462.f, 1112.f, 12500.f, 100.f, 40.f},
	};

	class FCheckAttributes final : public IAutomationLatentCommand
	{
	public:
		explicit FCheckAttributes(FAutomationTestBase* InTest) : Test(InTest) {}

		virtual bool Update() override
		{
			const double Now = FPlatformTime::Seconds();
			if (StartedAt < 0.0) StartedAt = Now;
			AWuwaPlayerController* PC = nullptr;
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == EWorldType::PIE && IsValid(Context.World())) PC = Cast<AWuwaPlayerController>(Context.World()->GetFirstPlayerController());
			}
			UWuwaFormationComponent* Formation = PC ? PC->FormationComponent.Get() : nullptr;
			if (!Formation || Formation->GetMembers().Num() < 2)
			{
				// 新进程第一次加载队伍（冷缓存、NullRHI）要 30~50 秒，和其他 PIE 测试一样等到 120 秒
				if (Now - StartedAt > 120.0) { Test->AddError(TEXT("The formation did not spawn.")); return true; }
				return false;
			}
			int32 Checked = 0;
			for (const AWuwaCharacter* Role : Formation->GetMembers())
			{
				const UAbilitySystemComponent* ASC = Role ? Role->GetAbilitySystemComponent() : nullptr;
				const FExpected* Want = Role && Role->RoleData
					? Algo::FindBy(Expected, Role->RoleData->BasePropertyId, &FExpected::BasePropertyId) : nullptr;
				if (!ASC || !Want) continue;
				++Checked;
				using S = UWuwaAttributeSet;
				const FString Name = Role->RoleData->GetName();
				auto Check = [&](const TCHAR* What, const FGameplayAttribute& Attribute, float Value)
				{
					Test->TestEqual(*FString::Printf(TEXT("[%s] %s"), *Name, What), ASC->GetNumericAttribute(Attribute), Value, 0.01f);
				};
				Check(TEXT("Level"), S::GetLevelAttribute(), static_cast<float>(Want->Level));
				Check(TEXT("ElementType = Fusion"), S::GetElementTypeAttribute(), static_cast<float>(EWuwaElement::Fusion));
				Check(TEXT("MaxHealth"), S::GetMaxHealthAttribute(), Want->MaxHealth);
				Check(TEXT("Health starts full"), S::GetHealthAttribute(), Want->MaxHealth);
				Check(TEXT("Attack"), S::GetAttackAttribute(), Want->Attack);
				Check(TEXT("Defense"), S::GetDefenseAttribute(), Want->Defense);
				Check(TEXT("CritRate"), S::GetCritRateAttribute(), 0.05f);
				Check(TEXT("CritDamage"), S::GetCritDamageAttribute(), 1.5f);
				Check(TEXT("MaxEnergy"), S::GetMaxEnergyAttribute(), Want->MaxEnergy);
				Check(TEXT("Energy starts empty"), S::GetEnergyAttribute(), 0.f);
				Check(TEXT("MaxSpecialEnergy1"), S::GetMaxSpecialEnergy1Attribute(), Want->MaxSpecialEnergy1);
				Check(TEXT("MaxSpecialEnergy2"), S::GetMaxSpecialEnergy2Attribute(), Want->MaxSpecialEnergy2);
				Test->TestTrue(*FString::Printf(TEXT("[%s] MaxConcertoEnergy > 0"), *Name), ASC->GetNumericAttribute(S::GetMaxConcertoEnergyAttribute()) > 0.f);
			}
			Test->TestEqual(TEXT("Both test roles were checked (DA_Role_* BasePropertyId set?)"), Checked, 2);
			return true;
		}

	private:
		FAutomationTestBase* Test;
		double StartedAt = -1.0;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaRoleAttributeInitTest, "Wuwa.Ability.RoleAttributes.InitFromBaseProperty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWuwaRoleAttributeInitTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/Map/Wuwa"))) return false;
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(WuwaRoleAttributeTests::FCheckAttributes(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
