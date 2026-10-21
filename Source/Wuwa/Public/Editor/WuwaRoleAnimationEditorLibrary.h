#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WuwaRoleAnimationEditorLibrary.generated.h"

class UAnimBlueprint;
class UAnimationAsset;
class USkeleton;

/** Explicit, unsaved editor migrations for skeleton-bound children of an animation template. */
UCLASS()
class WUWA_API UWuwaRoleAnimationEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Converts the existing asset in place, retaining its package and generated-class path.
	 * Removes its own graphs and duplicate variables, then inherits the parent's graph.
	 * Preserves persistent, editable same-name/same-type defaults from the previous CDO.
	 * Never saves. On failure the caller must discard the unsaved package.
	 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Role Animation")
	static bool ConvertToTemplateChild(UAnimBlueprint* Child, UAnimBlueprint* Parent, USkeleton* Target);

	/**
	 * Replaces this child's complete asset-override set using compiled parent node GUIDs.
	 * Supports sequence players/evaluators and BlendSpace players/evaluators, including
	 * parent nodes whose asset is empty. Compiles and verifies the resulting CDO. Never saves.
	 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Role Animation")
	static bool SetParentAssetOverrides(UAnimBlueprint* Child,
		const TArray<FGuid>& ParentNodeGuids, const TArray<UAnimationAsset*>& Assets);

	/** Returns authored sequence/BlendSpace player and evaluator paths mapped to stable parent-node GUIDs.
	 * Includes empty template nodes and does not compile, change, or save the blueprint. */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Role Animation")
	static TMap<FString, FGuid> GetAssetPlayerNodeGuids(UAnimBlueprint* Blueprint);

	/** Replace only matching K2 function-input animation literals with inherited object-variable getters.
	 * Adds variables with empty defaults. Returns the replaced pin count, or -1 on failure. Never saves. */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Role Animation")
	static int32 ParameterizeAnimationLiterals(UAnimBlueprint* Blueprint,
		const TArray<UAnimationAsset*>& Sources, const TArray<FName>& VariableNames);

	/** Replace reads of an uninitialized character cache with TryGetPawnOwner -> pure Cast To Character.
	 * Retains the unused variable with the generic Character type. Returns replaced getters, or -1. */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Role Animation")
	static int32 ReplaceCharacterReferenceWithOwner(UAnimBlueprint* Blueprint, FName VariableName);
};
