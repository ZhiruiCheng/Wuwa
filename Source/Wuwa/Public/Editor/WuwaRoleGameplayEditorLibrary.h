#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "WuwaRoleGameplayEditorLibrary.generated.h"

class UAnimMontage;
class UAnimSequence;
class UBlueprint;

/** Unsaved editor helpers for configuring another role's existing gameplay framework. */
UCLASS()
class WUWA_API UWuwaRoleGameplayEditorLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** Rebinds a one-segment montage to a same-length converted sequence, retaining all events. */
    UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Role Animation")
    static bool RemapMontageSequence(UAnimMontage* Montage, UAnimSequence* Sequence);

    /** Copies skeleton named events (no notify objects), scaling their positions to the new sequence. */
    UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Role Animation")
    static int32 CopyNamedSequenceNotifies(UAnimSequence* Source, UAnimSequence* Destination);

    UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Role Animation")
    static bool ConfigureRoleGameplayMontage(UAnimMontage* TemplateMontage, UAnimMontage* Destination, UAnimSequence* Sequence);

    UFUNCTION(BlueprintCallable, Category = "Wuwa|Editor|Role Animation")
    static bool RemapGameplayAbility(UBlueprint* BP, const TArray<UAnimMontage*>& Sources,
        const TArray<UAnimMontage*>& Destinations, bool bRepairDash);
};
