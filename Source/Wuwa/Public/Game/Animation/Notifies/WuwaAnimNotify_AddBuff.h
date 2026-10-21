#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "WuwaAnimNotify_AddBuff.generated.h"

/**
 * 加 buff（≈ 原作 TsAnimNotifyAddBuff）：动画走到这一帧时给自己加一个 buff（原作 buff ID，DT_Buff 的行名）。
 * 只在有权威的一端加（原作 HasBuffAuthority），GE 复制给客户端。例：长离普攻第 4 段 0.38 秒加 1205000001 心眼。
 */
UCLASS(meta = (DisplayName = "Add Buff"))
class WUWA_API UWuwaAnimNotify_AddBuff : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Buff")
	int64 BuffId = 0;

	virtual FString GetNotifyName_Implementation() const override;
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
