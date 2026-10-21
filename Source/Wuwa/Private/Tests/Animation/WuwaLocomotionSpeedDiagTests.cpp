#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/BlendSpace.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimMontage.h"
#include "AnimGraphNode_Base.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"

// 只读诊断：两个角色地面移动混合空间里每个采样的动画帧率、时长、播放倍率，以及根骨 / 骨盆在一个循环里的位移，
// 换算成"按动画本身该移动的速度"，和采样点在混合空间轴上的取值对比，找动画速度和移动速度对不上的地方。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaLocomotionSpeedDiagTest, "Diag.Animation.LocomotionSpeed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWuwaLocomotionSpeedDiagTest::RunTest(const FString& Parameters)
{
	const TCHAR* Paths[] = {
		TEXT("/Game/Characters/Role/changli/BS/BS_GroumdMove.BS_GroumdMove"),
		TEXT("/Game/Characters/Role/jiabei/BS/BS_GroundMove.BS_GroundMove"),
	};
	for (const TCHAR* Path : Paths)
	{
		const UBlendSpace* Space = LoadObject<UBlendSpace>(nullptr, Path);
		if (!Space)
		{
			AddError(FString::Printf(TEXT("Cannot load %s"), Path));
			continue;
		}
		AddInfo(FString::Printf(TEXT("%s: axis X %s [%.0f, %.0f], axis Y %s [%.0f, %.0f]"), *Space->GetName(),
			*Space->GetBlendParameter(0).DisplayName, Space->GetBlendParameter(0).Min, Space->GetBlendParameter(0).Max,
			*Space->GetBlendParameter(1).DisplayName, Space->GetBlendParameter(1).Min, Space->GetBlendParameter(1).Max));
		for (const FBlendSample& Sample : Space->GetBlendSamples())
		{
			const UAnimSequence* Anim = Sample.Animation;
			if (!Anim)
			{
				AddInfo(FString::Printf(TEXT("  (%.0f, %.0f): no animation"), Sample.SampleValue.X, Sample.SampleValue.Y));
				continue;
			}
			const IAnimationDataModel* Model = Anim->GetDataModel();
			const FReferenceSkeleton& Ref = Anim->GetSkeleton()->GetReferenceSkeleton();
			const int32 LastFrame = Model->GetNumberOfFrames();
			auto Travel = [&](const FName Bone) -> double
			{
				if (!Model->IsValidBoneTrackName(Bone)) return -1.0;
				const FVector A = Model->EvaluateBoneTrackTransform(Bone, FFrameTime(0), EAnimInterpolationType::Linear).GetLocation();
				const FVector B = Model->EvaluateBoneTrackTransform(Bone, FFrameTime(LastFrame), EAnimInterpolationType::Linear).GetLocation();
				return FVector::Dist2D(A, B);
			};
			const double Length = Anim->GetPlayLength();
			const double RootTravel = Travel(Ref.GetBoneName(0));
			const double PelvisTravel = Travel(Ref.GetNum() > 1 ? Ref.GetBoneName(1) : NAME_None);
			const double Rate = Sample.RateScale * Anim->RateScale;
			AddInfo(FString::Printf(TEXT("  (%.0f, %.0f) %s: %s fps, %d frames, %.3fs, rate %.2f, root motion %s, %s travel %.1f (%.0f/s at rate), %s travel %.1f (%.0f/s at rate)"),
				Sample.SampleValue.X, Sample.SampleValue.Y, *Anim->GetName(), *Model->GetFrameRate().ToPrettyText().ToString(), LastFrame, Length, Rate,
				Anim->bEnableRootMotion ? TEXT("on") : TEXT("off"),
				*Ref.GetBoneName(0).ToString(), RootTravel, Length > 0 ? RootTravel / Length * Rate : 0.0,
				Ref.GetNum() > 1 ? *Ref.GetBoneName(1).ToString() : TEXT("-"), PelvisTravel, Length > 0 ? PelvisTravel / Length * Rate : 0.0));
		}
	}
	return true;
}

// 只读诊断：动画蓝图里每个播放节点（序列 / 混合空间）引用的资产和 PlayRate，以及每个蒙太奇的倍率和片段时长，
// 找残留的播放速度补偿（例如旧 24fps 导出时期加的 1.25）。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWuwaAnimRateDiagTest, "Diag.Animation.PlayRates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWuwaAnimRateDiagTest::RunTest(const FString& Parameters)
{
	const TCHAR* Blueprints[] = {
		TEXT("/Game/Characters/Role/changli/ABP_Changli.ABP_Changli"),
		TEXT("/Game/Characters/Role/jiabei/ABP_Role_Jiabei.ABP_Role_Jiabei"),
	};
	for (const TCHAR* Path : Blueprints)
	{
		UAnimBlueprint* Blueprint = LoadObject<UAnimBlueprint>(nullptr, Path);
		if (!Blueprint)
		{
			AddError(FString::Printf(TEXT("Cannot load %s"), Path));
			continue;
		}
		// 子动画蓝图没有自己的图表，节点在父蓝图里；子蓝图只记录替换了哪些节点的资产。
		for (const FAnimParentNodeAssetOverride& Override : Blueprint->ParentAssetOverrides)
		{
			AddInfo(FString::Printf(TEXT("%s: asset override -> %s (%.3fs), PlayRate n/a"), *Blueprint->GetName(), *GetNameSafe(Override.NewAsset),
				Override.NewAsset ? Override.NewAsset->GetPlayLength() : 0.f));
		}
		UAnimBlueprint* GraphOwner = Blueprint;
		while (GraphOwner && GraphOwner->FunctionGraphs.IsEmpty() && GraphOwner->UbergraphPages.IsEmpty())
		{
			GraphOwner = UAnimBlueprint::GetParentAnimBlueprint(GraphOwner);
		}
		if (!GraphOwner) continue;
		AddInfo(FString::Printf(TEXT("%s: graphs from %s"), *Blueprint->GetName(), *GraphOwner->GetName()));
		TArray<UEdGraph*> Graphs;
		GraphOwner->GetAllGraphs(Graphs);
		int32 AnimNodes = 0;
		for (const UEdGraph* Graph : Graphs) for (const UEdGraphNode* EdNode : Graph->Nodes) AnimNodes += EdNode && EdNode->IsA<UAnimGraphNode_Base>();
		AddInfo(FString::Printf(TEXT("%s: %d graphs, %d anim nodes, PlayRate listing below"), *Blueprint->GetName(), Graphs.Num(), AnimNodes));
		for (const UEdGraph* Graph : Graphs)
		{
			for (const UEdGraphNode* EdNode : Graph->Nodes)
			{
				const UAnimGraphNode_Base* Node = Cast<UAnimGraphNode_Base>(EdNode);
				const FStructProperty* NodeProperty = Node ? Node->GetFNodeProperty() : nullptr;
				if (!NodeProperty) continue;
				const void* Data = NodeProperty->ContainerPtrToValuePtr<void>(Node);
				FString Asset;
				float PlayRate = 1.f;
				bool bHasRate = false;
				for (TFieldIterator<FProperty> It(NodeProperty->Struct); It; ++It)
				{
					if (const FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(*It))
					{
						if (const UAnimationAsset* AnimAsset = Cast<UAnimationAsset>(Object->GetObjectPropertyValue_InContainer(Data)))
							Asset = FString::Printf(TEXT("%s (%.3fs)"), *AnimAsset->GetName(), AnimAsset->GetPlayLength());
					}
					else if (It->GetFName() == TEXT("PlayRate"))
					{
						if (const FFloatProperty* Float = CastField<FFloatProperty>(*It)) { PlayRate = Float->GetPropertyValue_InContainer(Data); bHasRate = true; }
					}
				}
				if (Asset.IsEmpty() && !bHasRate) continue;
				AddInfo(FString::Printf(TEXT("%s / %s / %s: %s, PlayRate %.2f"), *Blueprint->GetName(), *Graph->GetName(),
					*Node->GetNodeTitle(ENodeTitleType::ListView).ToString(), Asset.IsEmpty() ? TEXT("(pin)") : *Asset, PlayRate));
			}
		}
	}
	const TCHAR* Folders[] = { TEXT("/Game/Characters/Role/changli/AnimMontage"), TEXT("/Game/Characters/Role/jiabei/AnimMontage") };
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	for (const TCHAR* Folder : Folders)
	{
		TArray<FAssetData> Assets;
		Registry.GetAssetsByPath(FName(Folder), Assets, true);
		for (const FAssetData& AssetData : Assets)
		{
			const UAnimMontage* Montage = Cast<UAnimMontage>(AssetData.GetAsset());
			if (!Montage) continue;
			FString Segments;
			for (const FSlotAnimationTrack& Slot : Montage->SlotAnimTracks)
			{
				for (const FAnimSegment& Segment : Slot.AnimTrack.AnimSegments)
				{
					const UAnimSequenceBase* Anim = Segment.GetAnimReference();
					Segments += FString::Printf(TEXT(" [%s %.3fs, %s fps, segment rate %.2f]"), *GetNameSafe(Anim), Anim ? Anim->GetPlayLength() : 0.f,
						Anim ? *Anim->GetSamplingFrameRate().ToPrettyText().ToString() : TEXT("-"), Segment.AnimPlayRate);
				}
			}
			AddInfo(FString::Printf(TEXT("%s: RateScale %.2f, length %.3fs%s"), *Montage->GetName(), Montage->RateScale, Montage->GetPlayLength(), *Segments));
		}
	}
	return true;
}

#endif
