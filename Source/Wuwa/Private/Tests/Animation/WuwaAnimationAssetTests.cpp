#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Game/NewWorld/Character/Common/Component/Anim/WuwaAnimInstance.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimBlueprint.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaAnimationAssetConnectionTest, "Wuwa.Animation.AssetConnection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaAnimationAssetConnectionTest::RunTest(const FString& Parameters)
{
	UAnimBlueprint* Blueprint = LoadObject<UAnimBlueprint>(nullptr,
		TEXT("/Game/Characters/Role/changli/ABP_Changli.ABP_Changli"));
	if (!TestNotNull(TEXT("Existing Changli AnimBP loads"), Blueprint))
	{
		return false;
	}
	// 长离的 ABP 现在是模板 ABP_RoleBase 的子 ABP，原生数据桥 UWuwaAnimInstance 在继承链更上层。
	TestTrue(TEXT("AnimBP inherits the native movement data bridge"),
		Blueprint->ParentClass && Blueprint->ParentClass->IsChildOf(UWuwaAnimInstance::StaticClass()));
	TestTrue(TEXT("Saved AnimBP has no compile errors"),
		Blueprint->Status == BS_UpToDate || Blueprint->Status == BS_UpToDateWithWarnings);
	if (!TestNotNull(TEXT("AnimBP has a generated class"), Blueprint->GeneratedClass.Get()))
	{
		return false;
	}
	TestTrue(TEXT("Generated animation instances use WuwaAnimInstance"),
		Blueprint->GeneratedClass->IsChildOf(UWuwaAnimInstance::StaticClass()));
	UClass* CharacterClass = LoadClass<AWuwaCharacter>(nullptr,
		TEXT("/Game/Characters/Role/changli/BP_Role_Changli.BP_Role_Changli_C"));
	if (!TestNotNull(TEXT("Player character blueprint loads"), CharacterClass))
	{
		return false;
	}
	const AWuwaCharacter* Character = CharacterClass->GetDefaultObject<AWuwaCharacter>();
	TestEqual(TEXT("Player mesh still uses the existing Changli AnimBP"),
		Character->GetMesh()->GetAnimClass(), Blueprint->GeneratedClass.Get());
	return true;
}

#endif
