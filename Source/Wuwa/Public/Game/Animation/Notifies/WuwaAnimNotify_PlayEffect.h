#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "WuwaAnimNotify_PlayEffect.generated.h"

class UWuwaEffectModelBase;

/** 一次动画通知发起一次播放；实例与句柄由 EffectSystem 管理，不保存在共享通知对象上。 */
UCLASS(meta = (DisplayName = "Play Effect"))
class WUWA_API UWuwaAnimNotify_PlayEffect : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect")
	TObjectPtr<UWuwaEffectModelBase> Model;

	/** 骨骼或插槽；留空使用 Mesh 的位置。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect|Placement")
	FName SocketName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect|Placement")
	FVector LocationOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect|Placement")
	FRotator RotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect|Placement")
	FVector Scale = FVector::OneVector;

	/** 脚步默认留在触发位置；勾选后持续跟随 Mesh 的指定插槽。 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Effect|Placement")
	bool bFollow = false;

	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
