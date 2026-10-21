#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "WuwaAnimNotify_Footstep.generated.h"

/**
 * 脚步声，对应原作 TsAnimNotifyFootstepAudio：先在角色的 AkComponent 上设置步态开关（footstep_variant，
 * 原作 E_FootstepVariant：land / run / runstop / sprint / sprintstop / walk / walkstop / turnback），再播放角色的脚步事件。
 * 原作的脚步事件来自角色音频配置表（db_audio.db 的 FootstepEvent），这里由安装工具按角色写进通知。
 * 原作还按脚下物理材质设置 footstep_texture；项目地面没有物理材质，Wwise 里已折叠成默认的 DirtSurface。
 */
UCLASS(meta = (DisplayName = "Wwise Footstep"))
class WUWA_API UWuwaAnimNotify_Footstep : public UAnimNotify
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Audio", meta = (AllowedClasses = "/Script/AkAudio.AkAudioEvent"))
	FSoftObjectPath AudioEvent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Audio")
	FName SwitchGroup = TEXT("footstep_variant");

	/** 步态开关值（原作 Variant）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Audio")
	FName Variant = TEXT("walk");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Audio")
	FName SocketName;

	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
	virtual FString GetNotifyName_Implementation() const override;
};
