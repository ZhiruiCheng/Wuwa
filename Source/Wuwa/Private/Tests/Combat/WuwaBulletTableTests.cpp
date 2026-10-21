#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Engine/DataTable.h"
#include "Game/NewWorld/Bullet/WuwaBulletTypes.h"

namespace WuwaBulletTableTests
{
	const TCHAR* ChangliTable = TEXT("/Game/Characters/Role/changli/Data/DT_BulletInfo_Changli.DT_BulletInfo_Changli");
	const TCHAR* JiabeiTable = TEXT("/Game/Characters/Role/jiabei/Data/DT_BulletInfo_Jiabei.DT_BulletInfo_Jiabei");
	const TCHAR* FuludelisiTable = TEXT("/Game/Characters/Monster/Boss/Fuludelisi/Data/DT_BulletInfo_Fuludelisi.DT_BulletInfo_Fuludelisi");

	UDataTable* Load(FAutomationTestBase& Test, const TCHAR* Path)
	{
		UDataTable* Table = LoadObject<UDataTable>(nullptr, Path);
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s loads"), Path), Table)) return nullptr;
		if (!Test.TestTrue(TEXT("Row struct is FWuwaBulletInfo"), Table->GetRowStruct() == FWuwaBulletInfo::StaticStruct())) return nullptr;
		return Table;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaBulletTableIdsTest, "Wuwa.Combat.Bullet.Table.RowsAreBulletIds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaBulletTableIdsTest::RunTest(const FString& Parameters)
{
	using namespace WuwaBulletTableTests;
	TestEqual(TEXT("Numeric row name parses"), WuwaBullet::ParseBulletId(TEXT("1205001000")), int64(1205001000));
	TestEqual(TEXT("Non-numeric row name is invalid"), WuwaBullet::ParseBulletId(TEXT("Attack01")), int64(0));
	TestEqual(TEXT("Negative row name is invalid"), WuwaBullet::ParseBulletId(TEXT("-5")), int64(0));
	for (const TCHAR* Path : {ChangliTable, JiabeiTable, FuludelisiTable})
	{
		UDataTable* Table = Load(*this, Path);
		if (!Table) return false;
		TestTrue(TEXT("Table has rows"), Table->GetRowMap().Num() > 0);
		Table->ForeachRow<FWuwaBulletInfo>(TEXT("BulletTableTest"), [this](const FName& Name, const FWuwaBulletInfo& Row)
		{
			TestTrue(*FString::Printf(TEXT("%s is a bullet ID"), *Name.ToString()), WuwaBullet::ParseBulletId(Name) > 0);
			TestTrue(*FString::Printf(TEXT("%s lives for a positive time"), *Name.ToString()), Row.Duration > 0.f);
			// 0 是合法值：尺寸为 0 的"父子弹"容器不判定、不命中，只负责生成子子弹；-1 = 同一单位不限次数
			TestTrue(*FString::Printf(TEXT("%s has a valid hit count per victim"), *Name.ToString()), Row.HitsPerVictim >= -1);
		});
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaBulletTableValuesTest, "Wuwa.Combat.Bullet.Table.ChangliRowsMatchOriginal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaBulletTableValuesTest::RunTest(const FString& Parameters)
{
	using namespace WuwaBulletTableTests;
	UDataTable* Table = Load(*this, ChangliTable);
	if (!Table) return false;

	// 普攻1-1：近战长方体，从发射者出生，判定 0.2 秒，每个单位一次
	const FWuwaBulletInfo* First = Table->FindRow<FWuwaBulletInfo>(TEXT("1205001000"), TEXT("BulletTableTest"));
	if (!TestNotNull(TEXT("Attack01 first bullet exists"), First)) return false;
	TestEqual(TEXT("Name"), First->BulletName, FString(TEXT("普攻1-1")));
	TestTrue(TEXT("Box"), First->Shape == EWuwaBulletShape::Box);
	TestEqual(TEXT("Box half extents"), First->Size, FVector(200.0, 170.0, 100.0));
	TestEqual(TEXT("Center offset"), First->CenterOffset, FVector(0.0, 0.0, -20.0));
	TestTrue(TEXT("Born from attacker"), First->BornStandard == EWuwaBulletBornStandard::Attacker);
	TestEqual(TEXT("Born offset"), First->BornOffset, FVector(50.0, 0.0, 120.0));
	TestTrue(TEXT("Stays where it was spawned"), First->FollowType == EWuwaBulletFollowType::Fixed);
	TestEqual(TEXT("Born from the Root bone"), First->BoneName, FName(TEXT("Root")));
	TestEqual(TEXT("Duration"), First->Duration, 0.2f);
	TestEqual(TEXT("Collision duration"), First->CollisionDuration, 0.2f);
	TestEqual(TEXT("Unlimited victims"), First->VictimCount, -1);
	TestEqual(TEXT("Once per victim"), First->HitsPerVictim, 1);
	TestEqual(TEXT("Damage ID equals the bullet ID"), First->DamageId, int64(1205001000));

	// 普攻4 的圆柱：外半径 600、实心、半高 130，每个单位 4 段、间隔 0.07 秒
	const FWuwaBulletInfo* MultiHit = Table->FindRow<FWuwaBulletInfo>(TEXT("1205004002"), TEXT("BulletTableTest"));
	if (!TestNotNull(TEXT("Attack04 multi-hit bullet exists"), MultiHit)) return false;
	TestTrue(TEXT("Cylinder"), MultiHit->Shape == EWuwaBulletShape::Cylinder);
	TestEqual(TEXT("Cylinder radius/inner/half height"), MultiHit->Size, FVector(600.0, 0.0, 130.0));
	TestEqual(TEXT("Four hits per victim"), MultiHit->HitsPerVictim, 4);
	TestEqual(TEXT("Hit interval"), MultiHit->HitInterval, 0.07f);

	// 不造成伤害的触发子弹，以及超过 int32 的伤害 ID
	const FWuwaBulletInfo* Trigger = Table->FindRow<FWuwaBulletInfo>(TEXT("1205004003"), TEXT("BulletTableTest"));
	if (TestNotNull(TEXT("Trigger-only bullet exists"), Trigger)) TestEqual(TEXT("Trigger has no damage"), Trigger->DamageId, int64(0));
	const FWuwaBulletInfo* Shared = Table->FindRow<FWuwaBulletInfo>(TEXT("1205201009"), TEXT("BulletTableTest"));
	if (TestNotNull(TEXT("Shared-damage bullet exists"), Shared))
	{
		TestEqual(TEXT("Damage ID above int32 survives import"), Shared->DamageId, int64(4000000006));
		TestTrue(TEXT("Follows a bone"), Shared->FollowType == EWuwaBulletFollowType::FollowBone);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaBulletTableSnapToBoneTest, "Wuwa.Combat.Bullet.Table.FuludelisiSnapToBone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaBulletTableSnapToBoneTest::RunTest(const FString& Parameters)
{
	using namespace WuwaBulletTableTests;
	UDataTable* Table = Load(*this, FuludelisiTable);
	if (!Table) return false;
	// 急涌·剑1：原作跟随类型 3（吸附到骨骼，出生偏移按骨骼空间）
	const FWuwaBulletInfo* Sword = Table->FindRow<FWuwaBulletInfo>(TEXT("8002500314"), TEXT("BulletTableTest"));
	if (!TestNotNull(TEXT("Rush sword bullet exists"), Sword)) return false;
	TestTrue(TEXT("Snaps to a bone"), Sword->FollowType == EWuwaBulletFollowType::SnapToBone);
	TestTrue(TEXT("Has a bone to snap to"), !Sword->BoneName.IsNone());
	// 潮汐第一刀：普通近战扇形
	const FWuwaBulletInfo* Tide = Table->FindRow<FWuwaBulletInfo>(TEXT("8002500102"), TEXT("BulletTableTest"));
	if (TestNotNull(TEXT("Tide first bullet exists"), Tide)) TestTrue(TEXT("Deals damage"), Tide->DamageId > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaBulletBornTransformTest, "Wuwa.Combat.Bullet.BornTransform",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaBulletBornTransformTest::RunTest(const FString& Parameters)
{
	// 发射者在 (1000, 0, 90) 朝 +Y（Yaw 90），胶囊半高 90
	FWuwaBulletBornContext Context;
	Context.Attacker = FTransform(FRotator(0.f, 90.f, 0.f), FVector(1000.f, 0.f, 90.f));
	Context.CapsuleHalfHeight = 90.f;

	FWuwaBulletInfo Info;
	Info.BornOffset = FVector(50.f, 0.f, 120.f);
	Info.CenterOffset = FVector(0.f, 0.f, -20.f);

	// 没配骨骼：Z 从脚底算，偏移跟发射者朝向转
	FTransform Born = WuwaBullet::ComputeBornTransform(Info, Context);
	TestTrue(TEXT("no bone: offset from feet, rotated by attacker"), Born.GetLocation().Equals(FVector(1000.f, 50.f, 120.f), 0.01));
	TestTrue(TEXT("no bone: faces attacker forward"), Born.GetRotation().GetForwardVector().Equals(FVector(0.f, 1.f, 0.f), 0.001));
	TestTrue(TEXT("center offset is in bullet space"), WuwaBullet::ComputeShapeTransform(Info, Born).GetLocation().Equals(FVector(1000.f, 50.f, 100.f), 0.01));

	// 配了骨骼（Fixed）：骨骼位置 + 按发射者朝向转的偏移；骨骼自身朝向不参与
	Info.BoneName = TEXT("Root");
	Context.Bone = FTransform(FRotator(0.f, 0.f, 90.f), FVector(1000.f, 0.f, 0.f));
	Born = WuwaBullet::ComputeBornTransform(Info, Context);
	TestTrue(TEXT("bone (fixed): bone location + attacker-rotated offset"), Born.GetLocation().Equals(FVector(1000.f, 50.f, 120.f), 0.01));

	// SnapToBone：偏移在骨骼空间、朝向跟骨骼
	Info.FollowType = EWuwaBulletFollowType::SnapToBone;
	Born = WuwaBullet::ComputeBornTransform(Info, Context);
	TestTrue(TEXT("snap to bone: offset in bone space"), Born.GetLocation().Equals(Context.Bone->TransformPosition(Info.BornOffset), 0.01));
	TestTrue(TEXT("snap to bone: bone rotation"), Born.GetRotation().Equals(Context.Bone->GetRotation(), 0.001));

	// 骨骼找不到时按没配骨骼算
	Context.Bone.Reset();
	Info.FollowType = EWuwaBulletFollowType::Fixed;
	TestTrue(TEXT("missing bone falls back to feet"), WuwaBullet::ComputeBornTransform(Info, Context).GetLocation().Equals(FVector(1000.f, 50.f, 120.f), 0.01));

	// 初始旋转叠在基准朝向上
	Info.BoneName = NAME_None;
	Info.BornRotation = FRotator(0.f, 90.f, 0.f);
	TestTrue(TEXT("born rotation composes onto attacker yaw"), WuwaBullet::ComputeBornTransform(Info, Context).GetRotation().GetForwardVector().Equals(FVector(-1.f, 0.f, 0.f), 0.001));

	// 世界原点 / 技能目标
	Info.BornStandard = EWuwaBulletBornStandard::World;
	TestTrue(TEXT("world: offset is world position"), WuwaBullet::ComputeBornTransform(Info, Context).GetLocation().Equals(Info.BornOffset, 0.01));
	// 技能目标（原作 BulletActionInitMove.SVo / IVo）：原点在目标位置，偏移按发射者朝向（朝 +Y）转，不从脚底算
	Info.BornStandard = EWuwaBulletBornStandard::SkillTarget;
	Info.BornRotation = FRotator::ZeroRotator;
	Context.SkillTarget = FTransform(FVector(1000.f, 500.f, 90.f));
	FTransform Target = WuwaBullet::ComputeBornTransform(Info, Context);
	TestTrue(TEXT("skill target: offset from target, attacker-rotated"), Target.GetLocation().Equals(FVector(1000.f, 550.f, 210.f), 0.01));
	TestTrue(TEXT("skill target: faces attacker forward"), Target.GetRotation().GetForwardVector().Equals(FVector(0.f, 1.f, 0.f), 0.001));
	// 超过 BornDistLimit.Y 拉回到 Y；近于 X 推到 X
	Info.BornDistLimit = FVector(0.f, 300.f, 0.f);
	TestTrue(TEXT("skill target: clamped to max distance"), WuwaBullet::ComputeBornBase(Info, Context).GetLocation().Equals(FVector(1000.f, 300.f, 90.f), 0.01));
	Info.BornDistLimit = FVector(800.f, 1400.f, 0.f);
	TestTrue(TEXT("skill target: pushed out to min distance"), WuwaBullet::ComputeBornBase(Info, Context).GetLocation().Equals(FVector(1000.f, 800.f, 90.f), 0.01));
	// 没有目标：发射者胶囊中心，BornDistLimit.Z > 0 时往正前方推
	Context.SkillTarget.Reset();
	Info.BornDistLimit = FVector(0.f, 1400.f, 500.f);
	TestTrue(TEXT("no target: Z in front of the attacker"), WuwaBullet::ComputeBornBase(Info, Context).GetLocation().Equals(FVector(1000.f, 500.f, 90.f), 0.01));
	Info.BornDistLimit = FVector::ZeroVector;
	TestTrue(TEXT("no target, no limit: capsule center"), WuwaBullet::ComputeBornBase(Info, Context).GetLocation().Equals(FVector(1000.f, 0.f, 90.f), 0.01));

	// 碰撞开关：用到的分量都 > 0
	FWuwaBulletInfo Shape;
	Shape.Shape = EWuwaBulletShape::Sector;
	Shape.Size = FVector(300.f, 0.f, 50.f);
	TestTrue(TEXT("sector ignores angle for the collision switch"), WuwaBullet::HasCollision(Shape));
	Shape.Shape = EWuwaBulletShape::Box;
	TestFalse(TEXT("box with a zero extent has no collision"), WuwaBullet::HasCollision(Shape));
	TestEqual(TEXT("bullet id -> skill id"), WuwaBullet::GetSkillId(1205001000), 1205001);
	return true;
}

#endif
