#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Game/Formation/WuwaFormationComponent.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"

// 诊断：打印每个编队角色网格组件和 Root 插槽的世界变换（特效通知按 Root 插槽变换放置，缩放会一起带上）。
namespace RootSocketDiag
{
	class FLogRootSockets final : public IAutomationLatentCommand
	{
	public:
		explicit FLogRootSockets(FAutomationTestBase* InTest) : Test(InTest) {}

		virtual bool Update() override
		{
			UWorld* World = nullptr;
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == EWorldType::PIE && IsValid(Context.World())) World = Context.World();
			}
			APlayerController* Player = World ? World->GetFirstPlayerController() : nullptr;
			UWuwaFormationComponent* Formation = Player ? Player->FindComponentByClass<UWuwaFormationComponent>() : nullptr;
			if (!Formation || Formation->GetMembers().IsEmpty() || !Player->GetPawn())
			{
				return FPlatformTime::Seconds() - StartedAt > 40.0 ? (Test->AddError(TEXT("No formation")), true) : false;
			}
			for (AWuwaCharacter* Member : Formation->GetMembers())
			{
				if (!Member) continue;
				USkeletalMeshComponent* Mesh = Member->GetMesh();
				const FTransform Root = Mesh->GetSocketTransform(TEXT("Root"));
				const FTransform Bone0 = Mesh->GetBoneTransform(0);
				Test->AddInfo(FString::Printf(TEXT("%s mesh scale=%s | bone0 %s scale=%s | Root socket loc=%s rot=%s scale=%s | has Root bone=%d"),
					*Member->GetClass()->GetName(), *Mesh->GetComponentScale().ToString(), *Mesh->GetBoneName(0).ToString(), *Bone0.GetScale3D().ToString(),
					*Root.GetLocation().ToString(), *Root.Rotator().ToString(), *Root.GetScale3D().ToString(), Mesh->GetBoneIndex(TEXT("Root")) != INDEX_NONE));
				Test->AddInfo(FString::Printf(TEXT("%s actor loc=%s rot=%s | mesh comp loc=%s rot=%s | bone0 rot=%s | Root relative to actor: loc=%s rot=%s"),
					*Member->GetClass()->GetName(), *Member->GetActorLocation().ToString(), *Member->GetActorRotation().ToString(),
					*Mesh->GetComponentLocation().ToString(), *Mesh->GetComponentRotation().ToString(), *Bone0.Rotator().ToString(),
					*Member->GetActorTransform().InverseTransformPosition(Root.GetLocation()).ToString(),
					*(Member->GetActorQuat().Inverse() * Root.GetRotation()).Rotator().ToString()));
			}
			return true;
		}

	private:
		FAutomationTestBase* Test;
		double StartedAt = FPlatformTime::Seconds();
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRootSocketDiagTest, "Diag.RootSocketTransforms",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRootSocketDiagTest::RunTest(const FString& Parameters)
{
	if (!AutomationOpenMap(TEXT("/Game/Map/Wuwa"))) return false;
	ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.0f));
	ADD_LATENT_AUTOMATION_COMMAND(RootSocketDiag::FLogRootSockets(this));
	ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
	return true;
}

#endif
