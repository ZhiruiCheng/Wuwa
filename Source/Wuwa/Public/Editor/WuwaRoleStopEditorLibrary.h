#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WuwaRoleStopEditorLibrary.generated.h"

class UAnimBlueprint;
class UAnimSequence;

/** Explicit editor migration. All functions leave assets unsaved for caller validation. */
UCLASS()
class WUWA_API UWuwaRoleStopEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	/** Join marked Walk/Run stops to the foot-contact sync group; preserve unmarked Sprint stops and transitions. Unsaved. */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Animation")
	static FString ConfigureStopLocomotionSync(UAnimBlueprint* Blueprint);

	/** Separate the existing Move additive into StartMove, preserving player GUIDs and child overrides. Unsaved. */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Animation")
	static FString SplitTemplateMoveStart(UAnimBlueprint* Blueprint);

	/** Rebuild SM_Ground.Stop with six independent players and entry callbacks; returns OK or ERROR. */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Animation")
	static FString ConfigureTemplateStops(UAnimBlueprint* Blueprint);

	/** Sequence order: Walk L, Walk R, Run L, Run R, Sprint L, Sprint R. Clears only old stop overrides. */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Animation")
	static FString SetChildStopAnimations(UAnimBlueprint* Blueprint, const TArray<UAnimSequence*>& Sequences);

	/** Each vector is (right foot minus left foot) projected onto actor forward and actor up, in cm. */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Animation")
	static FString SetSprintStopEntryFootRelations(UAnimBlueprint* Blueprint, FVector2D Left, FVector2D Right);
};
