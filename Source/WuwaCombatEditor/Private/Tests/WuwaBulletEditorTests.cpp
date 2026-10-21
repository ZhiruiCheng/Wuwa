#if WITH_DEV_AUTOMATION_TESTS

#include "Animation/AnimMontage.h"
#include "BulletEditor/SWuwaBulletEditor.h"
#include "BulletEditor/SWuwaBulletViewport.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Engine/DataTable.h"
#include "Engine/SkeletalMesh.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Docking/TabManager.h"
#include "Game/NewWorld/Bullet/WuwaBulletTypes.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Tests/AutomationCommon.h"
#include "Widgets/SWindow.h"

namespace WuwaBulletEditorTests
{
	const TCHAR* ChangliTable = TEXT("/Game/Characters/Role/changli/Data/DT_BulletInfo_Changli.DT_BulletInfo_Changli");
	const FName FirstBullet = TEXT("1205001000");

	/** 打开子弹编辑器页签、载入长离的表，检查伴随资产、拖动控件写回 / 撤销，按蒙太奇开头和中段截图。 */
	class FDriveEditor final : public IAutomationLatentCommand
	{
	public:
		FDriveEditor(FAutomationTestBase* InTest, FString InDirectory) : Test(InTest), Directory(MoveTemp(InDirectory)) {}

		virtual bool Update() override
		{
			const bool bDone = Drive();
			if (bDone && Window.IsValid())
			{
				FSlateApplication::Get().RequestDestroyWindow(Window.ToSharedRef());
				Window.Reset();
			}
			return bDone;
		}

	private:
		bool Drive()
		{
			const double Now = FPlatformTime::Seconds();
			if (StartedAt < 0.0) StartedAt = Now;
			if (Now - StartedAt > 60.0) { Test->AddError(TEXT("Bullet editor test timed out.")); return true; }
			switch (Step)
			{
			case 0: return Open();
			case 1: return Now - StepAt > 1.0 && Inspect();
			case 2: return Now - StepAt > 1.0 && Capture(TEXT("BulletEditor_Start.png"));
			case 3: return Now - StepAt > 1.0 && Capture(TEXT("BulletEditor_Mid.png"));
			case 4: return OpenSector();
			case 5: return Now - StepAt > 1.5 && Capture(TEXT("BulletEditor_Sector.png"));
			default: return true;
			}
		}

		void Next() { ++Step; StepAt = FPlatformTime::Seconds(); }

		bool Open()
		{
			// 页签开在浮动窗口里时，编辑器保存布局（开 / 关页签后）会读窗口的还原尺寸，NullRHI 下的通用窗口不支持（致命错误）。
			// 所以只检查页签已注册，编辑器直接放进一个普通窗口（不参与布局保存）
			Test->TestTrue(TEXT("bullet editor tab is registered"), FGlobalTabmanager::Get()->HasTabSpawner(FName(TEXT("WuwaBulletEditor"))));
			TSharedRef<SWuwaBulletEditor> Opened = SNew(SWuwaBulletEditor);
			Editor = Opened;
			Window = SNew(SWindow).Title(FText::FromString(TEXT("Wuwa Bullet Editor (test)"))).ClientSize(FVector2D(1470, 847))[Opened];
			FSlateApplication::Get().AddWindow(Window.ToSharedRef());
			UDataTable* Table = LoadObject<UDataTable>(nullptr, ChangliTable);
			if (!Test->TestNotNull(TEXT("changli bullet table"), Table)) return true;
			Opened->SetTable(Table);
			Opened->SelectRow(FirstBullet);
			Next();
			return false;
		}

		bool Inspect()
		{
			TSharedPtr<SWuwaBulletEditor> Pinned = Editor.Pin();
			if (!Pinned) return true;
			Test->TestNotNull(TEXT("skill table found next to the bullet table"), Pinned->GetSkillTable());
			Test->TestNotNull(TEXT("character blueprint found in the role folder"), Pinned->GetCharacterClass());
			Test->TestNotNull(TEXT("montage picked from the skill table"), Pinned->GetMontage());
			Test->TestEqual(TEXT("first bullet selected"), Pinned->GetSelectedRowName(), FirstBullet);
			TSharedPtr<FWuwaBulletViewportClient> Client = Pinned->GetViewport() ? Pinned->GetViewport()->GetBulletClient() : nullptr;
			if (!Test->TestTrue(TEXT("viewport client"), Client.IsValid())) return true;
			const USkeletalMeshComponent* Mesh = Client->GetPreviewMesh();
			Test->TestTrue(TEXT("preview mesh uses the character skeletal mesh"), Mesh && Mesh->GetSkeletalMeshAsset());
			Test->AddInfo(FString::Printf(TEXT("Character %s, montage %s, mesh %s"), *GetNameSafe(Pinned->GetCharacterClass()),
				*GetNameSafe(Pinned->GetMontage()), Mesh ? *GetNameSafe(Mesh->GetSkeletalMeshAsset()) : TEXT("-")));

			// 拖动平移控件：沿出生偏移坐标系的 X 拖 10 → BornOffset.X + 10；写回表；撤销后还原
			UDataTable* Table = Pinned->GetTable();
			const FWuwaBulletInfo Before = *Table->FindRow<FWuwaBulletInfo>(FirstBullet, TEXT("Test"));
			Client->SetWidgetMode(UE::Widget::WM_Translate);
			Client->SetWidgetCoordSystemSpace(COORD_Local);
			const FVector AxisX = Client->GetWidgetCoordSystem().GetScaledAxis(EAxis::X);
			FVector Drag = AxisX * 10.0;
			FRotator Rot = FRotator::ZeroRotator;
			FVector Scale = FVector::ZeroVector;
			Test->TestTrue(TEXT("widget drag handled"), Client->InputWidgetDelta(nullptr, EAxisList::X, Drag, Rot, Scale));
			Pinned->CommitEditedRow(FText::FromString(TEXT("Test drag")));
			const FWuwaBulletInfo* After = Table->FindRow<FWuwaBulletInfo>(FirstBullet, TEXT("Test"));
			Test->TestEqual(TEXT("drag along X moves BornOffset.X by 10"), After->BornOffset.X, Before.BornOffset.X + 10.0, 0.01);
			Test->TestEqual(TEXT("drag along X keeps BornOffset.Z"), After->BornOffset.Z, Before.BornOffset.Z, 0.01);
			GEditor->UndoTransaction();
			Test->TestEqual(TEXT("undo restores the table row"), Table->FindRow<FWuwaBulletInfo>(FirstBullet, TEXT("Test"))->BornOffset.X, Before.BornOffset.X, 0.01);
			Test->TestEqual(TEXT("undo reloads the edited copy"), Pinned->GetSelectedBullet()->BornOffset.X, Before.BornOffset.X, 0.01);

			RootAtStart = RootBone(Client.Get());
			Next();
			return false;
		}

		/** 芙露德莉丝的表里第一颗带初始旋转的扇形子弹。 */
		bool OpenSector()
		{
			TSharedPtr<SWuwaBulletEditor> Pinned = Editor.Pin();
			UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/Characters/Monster/Boss/Fuludelisi/Data/DT_BulletInfo_Fuludelisi.DT_BulletInfo_Fuludelisi"));
			if (!Pinned || !Test->TestNotNull(TEXT("fuludelisi bullet table"), Table)) return true;
			Pinned->SetTable(Table);
			FName Sector;
			Table->ForeachRow<FWuwaBulletInfo>(TEXT("Test"), [&Sector](const FName& Name, const FWuwaBulletInfo& Row)
			{
				if (Sector.IsNone() && Row.Shape == EWuwaBulletShape::Sector && !Row.BornRotation.IsNearlyZero()) Sector = Name;
			});
			if (!Test->TestFalse(TEXT("found a rotated sector bullet"), Sector.IsNone())) return true;
			Pinned->SelectRow(Sector);
			Test->AddInfo(FString::Printf(TEXT("Sector bullet %s, character %s, mesh %s, montage %s (skill table montages resolve by ID prefix)"), *Sector.ToString(), *GetNameSafe(Pinned->GetCharacterClass()),
				*GetNameSafe(Pinned->GetPreviewMesh()), *GetNameSafe(Pinned->GetMontage())));
			// 大卡有怪物蓝图（WuwaMonsterSetup 生成的 BP_Monster_Fuludelisi）：预览用蓝图，不再按骨架找网格
			const UClass* BossClass = Pinned->GetCharacterClass();
			Test->TestTrue(TEXT("boss character blueprint found in the role folder"), BossClass && BossClass->GetName().StartsWith(TEXT("BP_Monster_Fuludelisi")));
			Test->TestNull(TEXT("no skeleton-matched preview mesh when the blueprint exists"), Pinned->GetPreviewMesh());
			Test->TestNotNull(TEXT("boss montage resolved from 2-digit bullet suffix"), Pinned->GetMontage());
			Next();
			return false;
		}

		static FVector RootBone(const FWuwaBulletViewportClient* Client)
		{
			const USkeletalMeshComponent* Mesh = Client->GetPreviewMesh();
			return Mesh && Mesh->GetNumBones() > 0 ? Mesh->GetBoneTransform(0, FTransform::Identity).GetLocation() : FVector::ZeroVector;
		}

		bool Capture(const TCHAR* Name)
		{
			TSharedPtr<SWuwaBulletEditor> Pinned = Editor.Pin();
			if (!Pinned) return true;
			TSharedPtr<FWuwaBulletViewportClient> Client = Pinned->GetViewport()->GetBulletClient();
			if (Step == 3)
			{
				// 根骨骼应该锁在原点（根运动交给发射者），发射者跟着根运动走
				Test->AddInfo(FString::Printf(TEXT("root bone (component space) start %s mid %s, attacker %s"), *RootAtStart.ToString(),
					*RootBone(Client.Get()).ToString(), *Client->GetAttacker().GetLocation().ToString()));
			}
			TSharedPtr<SWindow> Shown = FSlateApplication::Get().FindWidgetWindow(Pinned.ToSharedRef());
			TArray<FColor> Pixels;
			FIntVector Size;
			if (Shown && FSlateApplication::Get().TakeScreenshot(Shown.ToSharedRef(), Pixels, Size) && Size.X > 0)
			{
				for (FColor& Pixel : Pixels) Pixel.A = 255;
				const FString Path = Directory / Name;
				Test->TestTrue(TEXT("saved bullet editor screenshot"), FImageUtils::SaveImageByExtension(*Path, FImageView(Pixels.GetData(), Size.X, Size.Y)));
				Test->AddInfo(FString::Printf(TEXT("Screenshot %s (%dx%d)"), *Path, Size.X, Size.Y));
			}
			else
			{
				Test->AddWarning(TEXT("Could not take a screenshot of the bullet editor window."));
			}
			if (Step == 2)
			{
				Pinned->SetPreviewTime(Pinned->GetMontage() ? Pinned->GetMontage()->GetPlayLength() * 0.5f : 0.f);
			}
			Next();
			return Step > 5;   // 最后一张截完才结束
		}

		FAutomationTestBase* Test;
		FString Directory;
		TWeakPtr<SWuwaBulletEditor> Editor;
		TSharedPtr<SWindow> Window;
		int32 Step = 0;
		double StartedAt = -1.0;
		double StepAt = 0.0;
		FVector RootAtStart = FVector::ZeroVector;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaBulletEditorSmokeTest, "Wuwa.Editor.BulletEditor.Smoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FWuwaBulletEditorSmokeTest::RunTest(const FString& Parameters)
{
	const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Diagnostics/BulletEditor"));
	IFileManager::Get().MakeDirectory(*Directory, true);
	ADD_LATENT_AUTOMATION_COMMAND(WuwaBulletEditorTests::FDriveEditor(this, Directory));
	return true;
}

#endif
