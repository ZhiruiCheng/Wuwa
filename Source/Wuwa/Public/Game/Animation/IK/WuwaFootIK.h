#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Units/RigUnit.h"
#include "WuwaFootIK.generated.h"

/** Game-thread snapshot. No movement/ability component is read during rig evaluation. */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaFootIKFrame
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Foot IK") float Alpha = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Foot IK") float GroundZ = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Foot IK") bool bGrounded = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Foot IK") bool bSprint = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Foot IK") bool bRun = false;
};

USTRUCT(BlueprintType)
struct WUWA_API FWuwaFootIKSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Foot IK") bool bEnabled = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Foot IK", meta=(ClampMin="0",ClampMax="50")) float MaxStepHeight = 30.f;
	/** Original export's maximum foot detection depth; centimeters in WORLD space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Foot IK", meta=(ClampMin="0",ClampMax="60")) float MaxStepDepth = 35.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Foot IK", meta=(ClampMin="0",ClampMax="30")) float MaxPelvisDrop = 20.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Foot IK", meta=(ClampMin="0",ClampMax="45")) float MaxFootAngle = 35.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Foot IK", meta=(ClampMin="1")) float InterpSpeed = 18.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Foot IK") TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;
};

USTRUCT()
struct WUWA_API FWuwaFootIKWork
{
	GENERATED_BODY()
	UPROPERTY() FVector Offsets = FVector::ZeroVector; // left Z, right Z, pelvis Z (world cm)
	UPROPERTY() FVector LeftNormal = FVector::UpVector;
	UPROPERTY() FVector RightNormal = FVector::UpVector;
	UPROPERTY() FVector LastOrigin = FVector::ZeroVector;
	UPROPERTY() bool bInitialized = false;
};

USTRUCT(BlueprintType)
struct WUWA_API FWuwaFootIKResult
{
	GENERATED_BODY()
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Foot IK") bool bLeftHit = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Foot IK") bool bRightHit = false;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Foot IK") float LeftWeight = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Foot IK") float RightWeight = 0.f;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Foot IK") FVector Offsets = FVector::ZeroVector;
};

namespace WuwaFootIK
{
	/** Same implementation used by the live rig and collision/pose regression tests. */
	WUWA_API FWuwaFootIKResult Solve(const FControlRigExecuteContext& Context, const FWuwaFootIKFrame& Frame,
		const FWuwaFootIKSettings& Settings, FWuwaFootIKWork& Work);
}

/** Terrain-relative correction of the incoming animated pose, including lifted feet. */
USTRUCT(meta=(DisplayName="Wuwa Terrain Foot IK", Category="Wuwa|Animation", Keywords="Feet,Ground,Slope"))
struct WUWA_API FRigUnit_WuwaFootIK : public FRigUnitMutable
{
	GENERATED_BODY()
	RIGVM_METHOD()
	virtual void Execute() override;
	/** Current-frame diagnostic outputs; view these pins in the role's Control Rig debugger. */
	UPROPERTY(meta=(Output)) FWuwaFootIKResult Result;
	UPROPERTY() FWuwaFootIKWork Work;
};
