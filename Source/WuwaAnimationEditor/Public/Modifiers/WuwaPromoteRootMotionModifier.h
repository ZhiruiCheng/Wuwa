#pragma once

#include "CoreMinimal.h"
#include "AnimationModifier.h"
#include "WuwaPromoteRootMotionModifier.generated.h"

/** Move the imported child Root's motion into the FBX wrapper without changing the visible pose. */
UCLASS(DisplayName = "Wuwa Promote Imported Root Motion")
class WUWAANIMATIONEDITOR_API UWuwaPromoteRootMotionModifier : public UAnimationModifier
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category = "Root Motion")
    FName MotionBone = TEXT("Root");

    virtual void OnApply_Implementation(UAnimSequence* AnimationSequence) override;
    virtual void OnRevert_Implementation(UAnimSequence* AnimationSequence) override;

    /** Adds (or reuses) this modifier on the sequence and applies it; never saves the asset. */
    UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Animation")
    static bool ApplyToSequence(UAnimSequence* Sequence);

private:
    UPROPERTY() FName WrapperBone;
    UPROPERTY() TArray<FTransform> OriginalWrapper;
    UPROPERTY() TArray<FTransform> OriginalChild;
    UPROPERTY() FString AppliedTrackDigest;
    UPROPERTY() bool bOriginalNormalizedRootMotionScale = false;
};
