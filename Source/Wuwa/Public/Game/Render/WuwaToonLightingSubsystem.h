#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WuwaToonLightingSubsystem.generated.h"

class UMaterialInstanceDynamic;
class UMaterialParameterCollection;
class USkeletalMeshComponent;

/** 一个角色网格上用 SDF 阴影的脸部材质，以及头骨的前 / 右轴（头骨局部空间）。 */
USTRUCT()
struct FWuwaToonFaceState
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;

	FName Bone;
	FVector LocalForward = FVector::XAxisVector;
	FVector LocalRight = FVector::YAxisVector;
};

/**
 * 仿鸣潮角色材质的运行时数据：
 * - 每帧找场景里的太阳光（Atmosphere Sun Light 优先，否则强度最大的方向光），把「指向光源的方向」和光的颜色写进材质参数集 MPC_WuwaToon；
 * - 角色网格上 UseSDF=1 的脸部材质，每帧写入头骨的前方 / 右方（HeadForward / HeadRight），供脸部 SDF 阴影使用。
 * 角色卡通材质是 Unlit，在材质里自己算明暗，所以需要这些数据。
 */
UCLASS()
class WUWA_API UWuwaToonLightingSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static const TCHAR* CollectionPath;

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	using FFaceState = FWuwaToonFaceState;
	void UpdateFaces(UWorld* World);

	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollection> Collection;

	// 动态材质实例由网格的材质覆盖持有，这里不需要额外保活。
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, FWuwaToonFaceState> Faces;
};
