#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WuwaRoleRigEditorLibrary.generated.h"

class UAnimBlueprint;

/** Explicit, editor-only graph migration; never runs during gameplay or asset loading. */
UCLASS()
class WUWA_API UWuwaRoleRigEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Disconnect the obsolete Idle-only trace call and legacy offset inputs; keep pose wiring and role rig selection. */
	UFUNCTION(BlueprintCallable, Category="Wuwa|Editor|Animation")
	static FString ConfigureTerrainFootIK(UAnimBlueprint* Blueprint);
	/** Add FootIKRigClass and bind the existing Control Rig node's On Update callback.
	 * Keeps the default rig and all animation pose links. Returns ERROR on failure.
	 * bSave=false leaves a reviewable in-memory change for the caller to validate and save.
	 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Animation")
	static FString ConfigureRoleFootIK(UAnimBlueprint* Blueprint, bool bSave = false);

	/** Set an inherited FootIKRigClass default on a child animation Blueprint. */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Animation")
	static FString SetRoleFootIKClass(UAnimBlueprint* Blueprint, UClass* RigClass, bool bSave = false);
};
