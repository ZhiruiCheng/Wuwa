#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Tickable.h"
#include "WuwaSequenceCameraSubsystem.generated.h"

class ACharacter;
class ALevelSequenceActor;
class ULevelSequence;

/** 原作 特写镜头配置（TsAnimNotifySwitchSequenceCamera.特写镜头配置）里项目用到的部分。 */
USTRUCT(BlueprintType)
struct WUWA_API FWuwaSequenceCameraConfig
{
	GENERATED_BODY()

	/** 原作 CameraSequence（WuwaSequenceCameraSetup 按原作重建的 LevelSequence）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera")
	TSoftObjectPtr<ULevelSequence> CameraSequence;

	/** 原作 BlendInTime：切进镜头的混合时间。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera", meta = (Units = "s"))
	float BlendInTime = 0.f;

	/** 原作 BlendOutTime：结束时混回战斗镜头的时间。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera", meta = (Units = "s"))
	float BlendOutTime = 0.f;

	/** 原作 IsHideHud / HideHudTime：隐藏战斗 UI 多久（秒）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera")
	bool bHideHud = false;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera", meta = (Units = "s"))
	float HideHudTime = 0.f;

	/** 原作 SpecificSequenceTime（> 0 时）：最多播这么久（序列更长也在这里结束）。 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wuwa|Camera", meta = (Units = "s"))
	float SpecificSequenceTime = 0.f;
};

/**
 * 技能特写镜头的播放（≈ 原作 SequenceCameraPlayerComponent.PlayCameraSequence）：
 * - 生成 LevelSequenceActor 播整段序列：Tag Role 的绑定换成放技能的角色；原点是挂在角色 Mesh 的 AttachSocket 上的 Actor；
 * - 序列里 Tag SequenceCamera 的相机生成后，玩家视角混过去（BlendInTime）；序列自己的镜头切换轨道关掉；
 * - 播到 min(序列结束, SpecificSequenceTime) 时停在最后一帧，视角按 BlendOutTime 混回角色，混完再停序列、销毁；
 * - bHideHud：隐藏主 HUD HideHudTime 秒（原作隐藏战斗界面和 Float / Pop / Guide 层，项目先只隐藏主 HUD）。
 * 同一时间只播一段；新的会先结束旧的。
 */
UCLASS()
class WUWA_API UWuwaSequenceCameraSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UWuwaSequenceCameraSubsystem* Get(const UObject* WorldContext);

	bool PlayCameraSequence(const FWuwaSequenceCameraConfig& Config, ACharacter* Character, FName AttachSocket);
	void StopSequence(bool bImmediate = false);
	bool IsPlaying() const { return Sequence.IsValid(); }
	/** 正在用的镜头 Actor（测试 / 调试用）。 */
	AActor* GetCameraActor() const { return CameraActor.Get(); }

	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
	virtual void Deinitialize() override;

private:
	void SetHudHidden(bool bHidden);
	void Cleanup();

	TWeakObjectPtr<ALevelSequenceActor> Sequence;
	TWeakObjectPtr<AActor> Origin;
	TWeakObjectPtr<AActor> CameraActor;
	TWeakObjectPtr<ACharacter> Character;
	FWuwaSequenceCameraConfig Current;
	double PlayTime = 0.0;
	double EndTime = 0.0;
	double BlendOutUntil = -1.0;
	double HudHiddenUntil = -1.0;
	bool bHudHidden = false;
};
