#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimationAsset.h"
#include "AnimGraphNode_AssetPlayerBase.h"
#include "AnimGraphNode_Base.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"

// 只读诊断：打印 ABP（默认 ABP_RoleBase）的模板/骨骼/父类、每个播放节点挂的资源、在内存里编译一次的结果，以及有哪些子 ABP。不保存任何东西。
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FAbpRoleBaseDiagTest, "Diag.Anim.AbpDump",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
void FAbpRoleBaseDiagTest::GetTests(TArray<FString>& OutBeautifiedNames, TArray<FString>& OutTestCommands) const
{
	OutBeautifiedNames.Add(TEXT("RoleBase"));
	OutTestCommands.Add(TEXT("/Game/Characters/Role/Common/ABP_RoleBase.ABP_RoleBase"));
	OutBeautifiedNames.Add(TEXT("Changli"));
	OutTestCommands.Add(TEXT("/Game/Characters/Role/changli/ABP_Changli.ABP_Changli"));
	OutBeautifiedNames.Add(TEXT("Jiabei"));
	OutTestCommands.Add(TEXT("/Game/Characters/Role/Common/ABP_Role_Jiabei.ABP_Role_Jiabei"));
}
bool FAbpRoleBaseDiagTest::RunTest(const FString& Parameters)
{
	const FString Path = Parameters.IsEmpty() ? TEXT("/Game/Characters/Role/Common/ABP_RoleBase.ABP_RoleBase") : Parameters;
	UAnimBlueprint* AnimBP = LoadObject<UAnimBlueprint>(nullptr, *Path);
	if (!AnimBP)
	{
		AddError(FString::Printf(TEXT("Cannot load %s"), *Path));
		return false;
	}

	AddInfo(FString::Printf(TEXT("bIsTemplate=%d TargetSkeleton=%s ParentClass=%s"), AnimBP->bIsTemplate,
		*GetPathNameSafe(AnimBP->TargetSkeleton), *GetPathNameSafe(AnimBP->ParentClass)));

	for (const FAnimParentNodeAssetOverride& Override : AnimBP->ParentAssetOverrides)
	{
		AddInfo(FString::Printf(TEXT("  override %s -> %s"), *Override.ParentNodeGuid.ToString(), *GetPathNameSafe(Override.NewAsset)));
	}

	TArray<UAnimGraphNode_Base*> Nodes;
	FBlueprintEditorUtils::GetAllNodesOfClass(AnimBP, Nodes);
	TMap<FString, int32> ClassCounts;
	for (UAnimGraphNode_Base* Node : Nodes)
	{
		++ClassCounts.FindOrAdd(Node->GetClass()->GetName());
		if (UAnimGraphNode_AssetPlayerBase* Player = Cast<UAnimGraphNode_AssetPlayerBase>(Node))
		{
			AddInfo(FString::Printf(TEXT("  player [%s] %s -> %s"), *GetNameSafe(Node->GetGraph()),
				*Node->GetNodeTitle(ENodeTitleType::ListView).ToString(), *GetPathNameSafe(Player->GetAnimationAsset())));
		}
	}
	for (const TPair<FString, int32>& Pair : ClassCounts)
	{
		AddInfo(FString::Printf(TEXT("  node %-40s x%d"), *Pair.Key, Pair.Value));
	}
	AddInfo(FString::Printf(TEXT("  variables=%d functionGraphs=%d"), AnimBP->NewVariables.Num(), AnimBP->FunctionGraphs.Num()));

	FCompilerResultsLog Results;
	Results.bSilentMode = true;
	FKismetEditorUtilities::CompileBlueprint(AnimBP, EBlueprintCompileOptions::SkipSave, &Results);
	AddInfo(FString::Printf(TEXT("compile: status=%d errors=%d warnings=%d"), static_cast<int32>(AnimBP->Status), Results.NumErrors, Results.NumWarnings));
	for (const TSharedRef<FTokenizedMessage>& Message : Results.Messages)
	{
		if (Message->GetSeverity() <= EMessageSeverity::Warning)
		{
			AddInfo(FString::Printf(TEXT("  [%d] %s"), static_cast<int32>(Message->GetSeverity()), *Message->ToText().ToString()));
		}
	}

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	TArray<FAssetData> AnimBPs;
	Registry.GetAssetsByClass(UAnimBlueprint::StaticClass()->GetClassPathName(), AnimBPs);
	for (const FAssetData& Data : AnimBPs)
	{
		const FString Parent = Data.GetTagValueRef<FString>(FBlueprintTags::ParentClassPath);
		if (Parent.Contains(TEXT("ABP_RoleBase")))
		{
			AddInfo(FString::Printf(TEXT("  child ABP: %s"), *Data.GetObjectPathString()));
		}
	}
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"

// 只读诊断：比较两套角色网格的参考骨骼（骨骼名、数量、关键骨骼的参考姿势、网格包围盒）。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRoleSkeletonCompareDiagTest, "Diag.Anim.SkeletonCompare",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRoleSkeletonCompareDiagTest::RunTest(const FString& Parameters)
{
	USkeletalMesh* A = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Role/changli/NewModel/changli2_0.changli2_0"));
	USkeletalMesh* B = LoadObject<USkeletalMesh>(nullptr, TEXT("/Game/Characters/Role/jiabei/Mesh/jiabei2_0.jiabei2_0"));
	if (!A || !B)
	{
		AddError(TEXT("Cannot load meshes."));
		return false;
	}
	const FReferenceSkeleton& RefA = A->GetRefSkeleton();
	const FReferenceSkeleton& RefB = B->GetRefSkeleton();
	AddInfo(FString::Printf(TEXT("Changli: bones=%d skeleton=%s bounds=%s postProcess=%s"), RefA.GetNum(), *GetPathNameSafe(A->GetSkeleton()),
		*A->GetBounds().BoxExtent.ToString(), *GetPathNameSafe(A->GetPostProcessAnimBlueprint())));
	AddInfo(FString::Printf(TEXT("Jiabei : bones=%d skeleton=%s bounds=%s postProcess=%s"), RefB.GetNum(), *GetPathNameSafe(B->GetSkeleton()),
		*B->GetBounds().BoxExtent.ToString(), *GetPathNameSafe(B->GetPostProcessAnimBlueprint())));
	if (USkeleton* SkB = B->GetSkeleton())
	{
		TArray<FString> Compatible;
		for (const TSoftObjectPtr<USkeleton>& S : SkB->GetCompatibleSkeletons()) Compatible.Add(S.ToString());
		AddInfo(FString::Printf(TEXT("Jiabei compatible skeletons: [%s]"), *FString::Join(Compatible, TEXT(", "))));
	}

	TArray<FString> OnlyA, OnlyB;
	for (int32 i = 0; i < RefA.GetNum(); ++i) if (RefB.FindBoneIndex(RefA.GetBoneName(i)) == INDEX_NONE) OnlyA.Add(RefA.GetBoneName(i).ToString());
	for (int32 i = 0; i < RefB.GetNum(); ++i) if (RefA.FindBoneIndex(RefB.GetBoneName(i)) == INDEX_NONE) OnlyB.Add(RefB.GetBoneName(i).ToString());
	AddInfo(FString::Printf(TEXT("only in Changli (%d): %s"), OnlyA.Num(), *FString::Join(TArray<FString>(OnlyA.GetData(), FMath::Min(40, OnlyA.Num())), TEXT(" "))));
	AddInfo(FString::Printf(TEXT("only in Jiabei (%d): %s"), OnlyB.Num(), *FString::Join(TArray<FString>(OnlyB.GetData(), FMath::Min(40, OnlyB.Num())), TEXT(" "))));

	for (int32 i = 0; i < FMath::Min(12, RefB.GetNum()); ++i)
	{
		const FName Name = RefB.GetBoneName(i);
		const int32 IA = RefA.FindBoneIndex(Name);
		const FTransform& TB = RefB.GetRefBonePose()[i];
		AddInfo(FString::Printf(TEXT("  %-28s Jiabei T=%s S=%s | Changli T=%s S=%s"), *Name.ToString(), *TB.GetTranslation().ToString(), *TB.GetScale3D().ToString(),
			IA != INDEX_NONE ? *RefA.GetRefBonePose()[IA].GetTranslation().ToString() : TEXT("-"),
			IA != INDEX_NONE ? *RefA.GetRefBonePose()[IA].GetScale3D().ToString() : TEXT("-")));
	}
	return true;
}
#endif
