#include "Editor/Commandlets/WuwaDebugPrintAuditCommandlet.h"

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "K2Node_CallFunction.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/Parse.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaDebugPrintAudit, Log, All);

UWuwaDebugPrintAuditCommandlet::UWuwaDebugPrintAuditCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpUsage = TEXT("-run=WuwaDebugPrintAudit [-Path=/Game]");
}

#if WITH_EDITOR
namespace WuwaDebugPrintAudit
{
	bool IsPrint(const UK2Node_CallFunction* Node)
	{
		const UFunction* Function = Node ? Node->GetTargetFunction() : nullptr;
		if (!Function || Function->GetOwnerClass() != UKismetSystemLibrary::StaticClass()) return false;
		const FName Name = Function->GetFName();
		return Name == GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintString) || Name == GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintText)
			|| Name == GET_FUNCTION_NAME_CHECKED(UKismetSystemLibrary, PrintWarning);
	}

	FString DescribeInput(const UEdGraphPin* Pin)
	{
		if (!Pin) return TEXT("?");
		if (Pin->LinkedTo.Num() == 0) return FString::Printf(TEXT("\"%s\""), Pin->DefaultValue.IsEmpty() ? *Pin->DefaultTextValue.ToString() : *Pin->DefaultValue);
		const UEdGraphNode* Source = Pin->LinkedTo[0]->GetOwningNode();
		return FString::Printf(TEXT("<%s>"), *Source->GetNodeTitle(ENodeTitleType::ListView).ToString());
	}

	FString PinDefault(const UK2Node_CallFunction* Node, const TCHAR* Name)
	{
		const UEdGraphPin* Pin = Node->FindPin(FName(Name));
		if (!Pin) return TEXT("-");
		return Pin->LinkedTo.Num() > 0 ? TEXT("linked") : Pin->DefaultValue;
	}

	/** 顺着执行线往前找驱动它的事件 / 函数入口（最多走 200 个节点）。 */
	FString FindDriver(const UEdGraphNode* Start)
	{
		TArray<const UEdGraphNode*> Queue = {Start};
		TSet<const UEdGraphNode*> Seen;
		TSet<FString> Drivers;
		for (int32 Steps = 0; Queue.Num() > 0 && Steps < 200; ++Steps)
		{
			const UEdGraphNode* Node = Queue.Pop();
			if (Seen.Contains(Node)) continue;
			Seen.Add(Node);
			if (Node->IsA<UK2Node_Event>() || Node->IsA<UK2Node_FunctionEntry>())
			{
				Drivers.Add(Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
				continue;
			}
			for (const UEdGraphPin* Pin : Node->Pins)
			{
				if (Pin->Direction != EGPD_Input || Pin->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec) continue;
				for (const UEdGraphPin* Linked : Pin->LinkedTo) Queue.Add(Linked->GetOwningNode());
			}
		}
		return Drivers.Num() ? FString::Join(Drivers.Array(), TEXT(" / ")) : TEXT("(not connected)");
	}
}
#endif

int32 UWuwaDebugPrintAuditCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaDebugPrintAudit;
	FString Path = TEXT("/Game");
	FParse::Value(*Params, TEXT("Path="), Path);
	IAssetRegistry& Registry = FAssetRegistryModule::GetRegistry();
	Registry.SearchAllAssets(true);
	TArray<FAssetData> Assets;
	FARFilter Filter;
	Filter.PackagePaths.Add(*Path);
	Filter.bRecursivePaths = true;
	Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
	Filter.bRecursiveClasses = true;
	Registry.GetAssets(Filter, Assets);
	Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.PackageName.LexicalLess(B.PackageName); });

	int32 Total = 0, OnScreen = 0;
	for (const FAssetData& Asset : Assets)
	{
		// 只看项目自己的蓝图（跳过插件 / 引擎示例里的）
		if (Asset.PackageName.ToString().StartsWith(TEXT("/Game/Developers"))) continue;
		const UBlueprint* Blueprint = Cast<UBlueprint>(Asset.GetAsset());
		if (!Blueprint) continue;
		TArray<UEdGraph*> Graphs;
		Blueprint->GetAllGraphs(Graphs);
		for (const UEdGraph* Graph : Graphs)
		{
			for (const UEdGraphNode* GraphNode : Graph->Nodes)
			{
				const UK2Node_CallFunction* Node = Cast<UK2Node_CallFunction>(GraphNode);
				// "...ForObjects" 射线 / 形状检测的 ObjectTypes 为空：引擎每次调用都报 "Invalid object types" 并直接返回没命中
				const UFunction* Called = Node ? Node->GetTargetFunction() : nullptr;
				const UEdGraphPin* ObjectTypes = Node ? Node->FindPin(TEXT("ObjectTypes")) : nullptr;
				if (Called && Called->GetOwnerClass() == UKismetSystemLibrary::StaticClass() && Called->GetName().Contains(TEXT("ForObjects"))
					&& ObjectTypes && ObjectTypes->LinkedTo.Num() == 0)
				{
					UE_LOG(LogWuwaDebugPrintAudit, Display, TEXT("EMPTYOBJECTTYPES|%s|%s|%s|%s|%s"), *Asset.PackageName.ToString(), *Graph->GetName(),
						*Called->GetName(), *FindDriver(Node), *Node->NodeGuid.ToString());
				}
				if (!IsPrint(Node)) continue;
				const UEdGraphPin* Exec = Node->GetExecPin();
				const bool bConnected = Exec && Exec->LinkedTo.Num() > 0;
				const FString Screen = PinDefault(Node, TEXT("bPrintToScreen"));
				++Total;
				if (bConnected && Screen != TEXT("false")) ++OnScreen;
				UE_LOG(LogWuwaDebugPrintAudit, Display, TEXT("PRINT|%s|%s|%s|%s|screen=%s|log=%s|duration=%s|%s|%s"),
					*Asset.PackageName.ToString(), *Graph->GetName(), *Node->GetTargetFunction()->GetName(),
					*DescribeInput(Node->FindPin(TEXT("InString")) ? Node->FindPin(TEXT("InString")) : Node->FindPin(TEXT("InText"))),
					*Screen, *PinDefault(Node, TEXT("bPrintToLog")), *PinDefault(Node, TEXT("Duration")),
					bConnected ? *FindDriver(Node) : TEXT("(not connected)"), *Node->NodeGuid.ToString());
			}
		}
	}
	UE_LOG(LogWuwaDebugPrintAudit, Display, TEXT("%d print nodes, %d connected and printing to screen, in %d blueprints scanned under %s."), Total, OnScreen, Assets.Num(), *Path);
	return 0;
#else
	return 1;
#endif
}
