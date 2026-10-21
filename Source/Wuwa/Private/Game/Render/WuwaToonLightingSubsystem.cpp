#include "Game/Render/WuwaToonLightingSubsystem.h"

#include "Components/DirectionalLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialParameterCollection.h"
#include "UObject/UObjectIterator.h"

const TCHAR* UWuwaToonLightingSubsystem::CollectionPath = TEXT("/Game/Characters/Common/Toon/MPC_WuwaToon.MPC_WuwaToon");

namespace WuwaToonLighting
{
	const FName Direction(TEXT("MainLightDirection"));
	const FName Color(TEXT("MainLightColor"));
	const FName UseSDF(TEXT("UseSDF"));
	const FName HeadForward(TEXT("HeadForward"));
	const FName HeadRight(TEXT("HeadRight"));
	const FName HeadBones[] = { TEXT("Bip001Head"), TEXT("Bip001-Head"), TEXT("Head"), TEXT("head") };

	/** 头骨局部坐标轴里最接近给定世界方向的那根（±X/±Y/±Z）。 */
	FVector BestLocalAxis(const FQuat& Bone, const FVector& World)
	{
		const FVector Axes[] = { FVector::XAxisVector, -FVector::XAxisVector, FVector::YAxisVector, -FVector::YAxisVector, FVector::ZAxisVector, -FVector::ZAxisVector };
		FVector Best = Axes[0];
		double BestDot = -2.0;
		for (const FVector& Axis : Axes)
		{
			const double Dot = Bone.RotateVector(Axis) | World;
			if (Dot > BestDot) { BestDot = Dot; Best = Axis; }
		}
		return Best;
	}
}

void UWuwaToonLightingSubsystem::Tick(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World) return;
	if (!Collection)
	{
		Collection = LoadObject<UMaterialParameterCollection>(nullptr, CollectionPath);
		if (!Collection) return;
	}

	const UDirectionalLightComponent* Sun = nullptr;
	for (TObjectIterator<UDirectionalLightComponent> It; It; ++It)
	{
		const UDirectionalLightComponent* Light = *It;
		if (Light->GetWorld() != World || !Light->IsRegistered() || !Light->IsVisible()) continue;
		const bool bBetter = !Sun
			|| (Light->IsUsedAsAtmosphereSunLight() && !Sun->IsUsedAsAtmosphereSunLight())
			|| (Light->IsUsedAsAtmosphereSunLight() == Sun->IsUsedAsAtmosphereSunLight() && Light->Intensity > Sun->Intensity);
		if (bBetter) Sun = Light;
	}
	if (Sun)
	{
		// 方向光沿组件前方照射，材质要的是从表面指向光源的方向。
		const FVector ToLight = -Sun->GetForwardVector();
		UKismetMaterialLibrary::SetVectorParameterValue(World, Collection, WuwaToonLighting::Direction, FLinearColor(ToLight.X, ToLight.Y, ToLight.Z, 0.f));
		// 只给颜色不给强度：卡通材质的输出已经去掉了曝光（见 M_Toon_Body 的 ExposureIndependence）。
		UKismetMaterialLibrary::SetVectorParameterValue(World, Collection, WuwaToonLighting::Color, Sun->GetLightColor());
	}

	UpdateFaces(World);
}

void UWuwaToonLightingSubsystem::UpdateFaces(UWorld* World)
{
	// 脸的 SDF 阴影要头部的前方 / 右方（原作由库洛的角色常量缓冲提供，这里取头骨朝向）。
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		USkeletalMeshComponent* Mesh = It->GetMesh();
		if (!Mesh || !Mesh->GetSkeletalMeshAsset()) continue;
		FFaceState* State = Faces.Find(Mesh);
		if (!State)
		{
			State = &Faces.Add(Mesh);
			for (const FName Bone : WuwaToonLighting::HeadBones)
			{
				if (Mesh->GetBoneIndex(Bone) != INDEX_NONE) { State->Bone = Bone; break; }
			}
			for (int32 Slot = 0; State->Bone != NAME_None && Slot < Mesh->GetNumMaterials(); ++Slot)
			{
				UMaterialInterface* Material = Mesh->GetMaterial(Slot);
				float UseSDF = 0.f;
				if (!Material || !Material->GetScalarParameterValue(FHashedMaterialParameterInfo(WuwaToonLighting::UseSDF), UseSDF) || UseSDF < 0.5f) continue;
				if (UMaterialInstanceDynamic* Dynamic = Mesh->CreateDynamicMaterialInstance(Slot)) State->Materials.Add(Dynamic);
			}
			if (!State->Materials.IsEmpty())
			{
				// 首次看到时按角色当前朝向认出头骨的前 / 右轴。
				const FQuat Bone = Mesh->GetBoneQuaternion(State->Bone, EBoneSpaces::WorldSpace);
				State->LocalForward = WuwaToonLighting::BestLocalAxis(Bone, It->GetActorForwardVector());
				State->LocalRight = WuwaToonLighting::BestLocalAxis(Bone, It->GetActorRightVector());
				UE_LOG(LogTemp, Display, TEXT("WuwaToon: %s face SDF on %d materials, head bone %s, local forward %s, right %s"),
					*It->GetName(), State->Materials.Num(), *State->Bone.ToString(), *State->LocalForward.ToString(), *State->LocalRight.ToString());
			}
		}
		if (State->Materials.IsEmpty()) continue;
		const FQuat Bone = Mesh->GetBoneQuaternion(State->Bone, EBoneSpaces::WorldSpace);
		const FVector Forward = Bone.RotateVector(State->LocalForward);
		const FVector Right = Bone.RotateVector(State->LocalRight);
		for (UMaterialInstanceDynamic* Material : State->Materials)
		{
			if (!IsValid(Material)) continue;
			Material->SetVectorParameterValue(WuwaToonLighting::HeadForward, FLinearColor(Forward.X, Forward.Y, Forward.Z, 0.f));
			Material->SetVectorParameterValue(WuwaToonLighting::HeadRight, FLinearColor(Right.X, Right.Y, Right.Z, 0.f));
		}
	}
}

TStatId UWuwaToonLightingSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UWuwaToonLightingSubsystem, STATGROUP_Tickables);
}

bool UWuwaToonLightingSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor;
}
