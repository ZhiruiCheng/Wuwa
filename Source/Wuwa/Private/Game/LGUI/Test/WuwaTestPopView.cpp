#include "Game/LGUI/Test/WuwaTestPopView.h"

#include "Core/ActorComponent/UIText.h"
#include "Engine/World.h"
#include "Game/LGUI/Core/WuwaUiLog.h"
#include "Game/LGUI/Core/WuwaUiManager.h"
#include "HAL/IConsoleManager.h"
#include "Interaction/UIButtonComponent.h"

#define WUWA_TEST_VIEW_LOG(Hook) \
	HookLog.Add(TEXT(Hook)); \
	UE_LOG(LogWuwaUI, Display, TEXT("[%s #%u] %s"), *GetViewName().ToString(), GetUniqueID(), TEXT(Hook))

void UWuwaTestPopView::OnCreate()       { WUWA_TEST_VIEW_LOG("OnCreate"); }
void UWuwaTestPopView::OnStart()
{
	WUWA_TEST_VIEW_LOG("OnStart");
	// 进程内累计的打开次数：每次打开都是新的界面对象，所以计数放在静态变量里。
	static int32 OpenCount = 0;
	if (UUIText* CountText = GetText(1))
	{
		CountText->SetText(FText::FromString(FString::Printf(TEXT("Opened %d times"), ++OpenCount)));
	}
}
void UWuwaTestPopView::OnBeforeShow()   { WUWA_TEST_VIEW_LOG("OnBeforeShow"); }
void UWuwaTestPopView::OnAfterShow()    { WUWA_TEST_VIEW_LOG("OnAfterShow"); }
void UWuwaTestPopView::OnBeforeHide()   { WUWA_TEST_VIEW_LOG("OnBeforeHide"); }
void UWuwaTestPopView::OnAfterHide()    { WUWA_TEST_VIEW_LOG("OnAfterHide"); }
void UWuwaTestPopView::OnBeforeDestroy(){ WUWA_TEST_VIEW_LOG("OnBeforeDestroy"); }

#undef WUWA_TEST_VIEW_LOG

void UWuwaTestPopView::OnRegisterComponent()
{
	ComponentRegisterInfos = {
		{0, UUIButtonComponent::StaticClass()},
		{1, UUIText::StaticClass()},
	};
	BtnBindInfo = {
		{0, [this]()
		{
			++CloseClickCount;
			CloseMe();
		}},
	};
}

namespace WuwaUiTestCommands
{
	FName ViewNameFromArgs(const TArray<FString>& Args)
	{
		return Args.Num() > 0 ? FName(*Args[0]) : FName(TEXT("TestPopView"));
	}

	UWuwaUiManager* GetManager(UWorld* World)
	{
		UWuwaUiManager* Manager = UWuwaUiManager::Get(World);
		if (!Manager)
		{
			UE_LOG(LogWuwaUI, Error, TEXT("No UiManager (needs a local player)."));
		}
		return Manager;
	}

	static FAutoConsoleCommandWithWorldAndArgs OpenCommand(
		TEXT("Wuwa.Ui.Open"), TEXT("Wuwa.Ui.Open [ViewName=TestPopView]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UWuwaUiManager* Manager = GetManager(World)) { Manager->OpenView(ViewNameFromArgs(Args)); }
		}));

	static FAutoConsoleCommandWithWorldAndArgs CloseCommand(
		TEXT("Wuwa.Ui.Close"), TEXT("Wuwa.Ui.Close [ViewName=TestPopView]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UWuwaUiManager* Manager = GetManager(World)) { Manager->CloseView(ViewNameFromArgs(Args)); }
		}));

	/** 同一帧 Open → Close → Open：最终应该显示着，旧对象走完 Hide/Destroy。 */
	static FAutoConsoleCommandWithWorldAndArgs StressCommand(
		TEXT("Wuwa.Ui.Stress"), TEXT("Wuwa.Ui.Stress [ViewName=TestPopView]: Open, Close, Open in one frame"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UWuwaUiManager* Manager = GetManager(World))
			{
				const FName ViewName = ViewNameFromArgs(Args);
				Manager->OpenView(ViewName);
				Manager->CloseView(ViewName);
				Manager->OpenView(ViewName);
				Manager->DumpViews();
			}
		}));

	/** 加载中销毁：Open 后立刻 Close（还在 Creating）。 */
	static FAutoConsoleCommandWithWorldAndArgs OpenCloseCommand(
		TEXT("Wuwa.Ui.OpenClose"), TEXT("Wuwa.Ui.OpenClose [ViewName=TestPopView]: close while still loading"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
		{
			if (UWuwaUiManager* Manager = GetManager(World))
			{
				const FName ViewName = ViewNameFromArgs(Args);
				Manager->OpenView(ViewName);
				Manager->CloseView(ViewName);
				Manager->DumpViews();
			}
		}));

	static FAutoConsoleCommandWithWorldAndArgs DumpCommand(
		TEXT("Wuwa.Ui.Dump"), TEXT("Wuwa.Ui.Dump: list views and their states"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>&, UWorld* World)
		{
			if (UWuwaUiManager* Manager = GetManager(World)) { Manager->DumpViews(); }
		}));
}
