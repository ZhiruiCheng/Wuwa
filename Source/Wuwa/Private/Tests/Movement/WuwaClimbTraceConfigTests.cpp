#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Game/NewWorld/Character/Common/Component/Move/WuwaMovementComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaClimbTraceConfigTest, "Wuwa.Movement.ClimbTraceHasObjectTypes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaClimbTraceConfigTest::RunTest(const FString& Parameters)
{
	// 攀爬检测的 ObjectTypes 为空时引擎每帧报 "Invalid object types" 并返回没命中；真实角色蓝图上必须有值
	for (const TCHAR* Path : {TEXT("/Game/Characters/Role/changli/BP_Role_Changli.BP_Role_Changli_C"),
		TEXT("/Game/Characters/Role/jiabei/BP_Role_jiabei.BP_Role_jiabei_C")})
	{
		const UClass* RoleClass = LoadClass<AWuwaCharacter>(nullptr, Path);
		if (!TestNotNull(*FString::Printf(TEXT("%s loads"), Path), RoleClass)) continue;
		const UWuwaMovementComponent* Movement = Cast<UWuwaMovementComponent>(RoleClass->GetDefaultObject<AWuwaCharacter>()->GetCharacterMovement());
		if (!TestNotNull(TEXT("role uses the Wuwa movement component"), Movement)) continue;
		AddInfo(FString::Printf(TEXT("%s ObjectTypes: %s"), *RoleClass->GetName(), *FString::JoinBy(Movement->ObjectTypes, TEXT(", "),
			[](const TEnumAsByte<EObjectTypeQuery>& Type) { return StaticEnum<ECollisionChannel>()->GetNameStringByValue(UEngineTypes::ConvertToCollisionChannel(Type.GetValue())); })));
		TestTrue(*FString::Printf(TEXT("%s climb trace has object types"), *RoleClass->GetName()), Movement->ObjectTypes.Num() > 0);
		TestTrue(TEXT("climb trace hits world static geometry"),
			Movement->ObjectTypes.Contains(UEngineTypes::ConvertToObjectType(ECC_WorldStatic)));
	}
	return true;
}

#endif
