#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WuwaAnimationMigrationLibrary.generated.h"

class UAnimSequence;
class UBlendSpace;
class USkeleton;
class UBlueprint;

/** Explicit editor asset migration helpers. Never save packages or run during gameplay. */
UCLASS()
class WUWA_API UWuwaAnimationMigrationLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Copies authored settings, sync markers, notifies and float curves, preserving imported bone tracks. */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Animation")
	static bool CopyAuthoredSettings(UAnimSequence* Source, UAnimSequence* Destination);

	/** Copies a sparse helper pose to a matching bone hierarchy with an explicit translation unit conversion. */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Animation")
	static UAnimSequence* CreateConvertedSupport(UAnimSequence* Source, USkeleton* TargetSkeleton,
		const FString& DestinationPackage, double TranslationScale);

	/** Read-only raw world-pose comparison at every key and half-key; returns a JSON diagnostic. */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Animation")
	static FString CompareConvertedBonePoses(UAnimSequence* Source, UAnimSequence* Destination, double TranslationScale);

	/** Bind a BlendSpace after its samples have been replaced with animations for this skeleton. */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Animation")
	static bool RemapBlendSpace(UBlendSpace* BlendSpace, USkeleton* TargetSkeleton,
		const TArray<UAnimSequence*>& Sources, const TArray<UAnimSequence*>& Destinations);

	/** Raw imported bone-track digest, independent of compression, curves and authored settings. */
	UFUNCTION(BlueprintPure, Category = "Wuwa|Editor|Animation")
	static FString BoneTrackDigest(UAnimSequence* Sequence);

	/** Moves motion from the sole child of an imported wrapper root into the real root (the child keeps its reference pose); preserves descendant poses. */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Animation")
	static bool PromoteChildRootMotion(UAnimSequence* Sequence, FName MotionBone);

	UFUNCTION(BlueprintPure, Category = "Wuwa|Editor|Animation")
	static FTransform ReadRootMotionDelta(UAnimSequence* Sequence);

	UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Animation")
	static bool CompileAnimationBlueprint(UBlueprint* Blueprint);

	/** Also migrate animation assets used as function input defaults, not just asset-player nodes. */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Animation")
	static int32 RemapAnimationPinDefaults(UBlueprint* Blueprint,
		const TArray<UAnimSequence*>& Sources, const TArray<UAnimSequence*>& Destinations);
};
