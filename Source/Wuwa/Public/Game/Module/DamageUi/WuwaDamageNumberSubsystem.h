#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Game/Module/DamageUi/WuwaDamageTextInfo.h"
#include "WuwaDamageNumberSubsystem.generated.h"

class AActor;
class ULGUICanvasScaler;
class ULGUIPrefab;
class UUIEffectOutline;
class UUIItem;
class UUIText;
class UWuwaUiTweenGroup;
struct FWuwaDamageEvent;

/** 一个伤害数字要显示的内容（≈ 原作 DamageInfo）。 */
struct FWuwaDamageNumberRequest
{
	float Damage = 0.f;
	int32 Element = 0;
	FVector WorldLocation = FVector::ZeroVector;
	bool bOwnPlayer = false;
	bool bCritical = false;
	bool bCure = false;
	/** 伤害表的 DamageTextType（-1 不显示）。 */
	int32 DamageTextType = 0;
	/** 文字型飘字（免疫等）；空就显示数字。 */
	FString Text;
};

/**
 * 一个伤害数字（≈ 原作 DamageView）：池里的一个预制体实例。世界坐标在出现时定下，之后每帧重新投影到屏幕，加上随机偏移；
 * 动画是预制体动画节点上的补间（UWuwaUiTweenGroup），1.2 秒后回收。
 *
 * 预制体根节点上的 WuwaUiComponentsRegistry：0 数字 Num、1 DamageNum、2 文字 Text2、3 暴击节点、4~10 动画节点
 * （自己受伤 / 自己暴击 / 打怪 / 打怪暴击 / Buff / 特殊 / 特殊暴击）。
 */
UCLASS()
class WUWA_API UWuwaDamageNumber : public UObject
{
	GENERATED_BODY()

public:
	/** 原作 DamageView 的 ANIM_TIME。 */
	static constexpr float Lifetime = 1.2f;

	bool Init(AActor* InActor);
	void Show(const FWuwaDamageNumberRequest& Request, const FWuwaDamageTextInfo& Info, const FVector2D& CanvasOffset);
	void Hide();
	/** 返回 false 表示播完了。 */
	bool Tick(float DeltaSeconds);
	/** 重新投影：画布坐标（左下为原点）加偏移。 */
	void SetCanvasPosition(const FVector2D& CanvasPosition);

	AActor* GetActor() const { return Actor; }
	const FVector& GetWorldLocation() const { return WorldLocation; }
	const FVector2D& GetOffset() const { return Offset; }
	UUIText* GetNumberText() const { return NumberText; }
	FName GetPlayingAnimation() const { return PlayingAnimation; }
	float GetRemaining() const { return Remaining; }

private:
	UPROPERTY(Transient) TObjectPtr<AActor> Actor;
	UPROPERTY(Transient) TObjectPtr<UUIItem> Root;
	UPROPERTY(Transient) TObjectPtr<UUIText> NumberText;
	UPROPERTY(Transient) TObjectPtr<UUIText> WordText;
	UPROPERTY(Transient) TObjectPtr<UUIEffectOutline> NumberOutline;
	UPROPERTY(Transient) TObjectPtr<UUIItem> CriticalItem;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<UWuwaUiTweenGroup>> Animations;
	UPROPERTY(Transient) TObjectPtr<UWuwaUiTweenGroup> Playing;
	FName PlayingAnimation;
	FVector WorldLocation = FVector::ZeroVector;
	FVector2D Offset = FVector2D::ZeroVector;
	float Remaining = 0.f;
};

/**
 * 伤害数字（≈ 原作 DamageUiManager + DamageUiController）：监听伤害结算（UWuwaDamageSubsystem::OnDamage），
 * 每帧最多出一个新数字（原作 MAX_DAMAGE_PER_FRAME = 1，其余排队），从池里取预制体实例挂到 HUD 层最底下的容器里。
 * 配色和动画按伤害飘字配置（FWuwaDamageTextInfo）：元素决定哪一行，受击者是当前操控角色时用"自己"的动画，否则"打怪"的动画，暴击用暴击色和暴击动画。
 * 暴击 UINiagara、按距离缩放（原作 Min/MaxDamageOffsetScale 只在性能版飘字里用）、合并飘字还没做。
 */
UCLASS()
class WUWA_API UWuwaDamageNumberSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	static UWuwaDamageNumberSubsystem* Get(const UObject* WorldContext);

	/** 原作 Q2t：DamageTextType 有效（非 0、-1、2）用它；否则伤害 0 → 9，元素 > 0 → 元素，治疗 → 8，其余 → 10。 */
	static int32 SelectTextId(int32 Element, bool bCure, float Damage, int32 DamageTextType);

	/** 原作 ApplyDamage：排队，下一帧起显示。 */
	void ApplyDamage(const FWuwaDamageNumberRequest& Request);

	const TArray<TObjectPtr<UWuwaDamageNumber>>& GetActiveNumbers() const { return Active; }
	int32 GetQueuedCount() const { return Queue.Num(); }
	/** 显示开关（原作 SetDamageViewVisible）。 */
	void SetVisible(bool bInVisible);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	virtual bool IsTickableInEditor() const override { return false; }

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void HandleDamage(const FWuwaDamageEvent& Event);
	/** HUD 层里的容器、画布缩放；界面还没建好时返回 false。 */
	bool EnsureContainer();
	UWuwaDamageNumber* Acquire();
	void Release(UWuwaDamageNumber* Number);
	void Spawn(const FWuwaDamageNumberRequest& Request);
	/** 世界坐标 → 画布坐标（左下为原点）；在屏幕外 / 投影失败返回 false。 */
	bool Project(const FVector& WorldLocation, FVector2D& OutCanvas) const;

	TArray<FWuwaDamageNumberRequest> Queue;
	UPROPERTY(Transient) TArray<TObjectPtr<UWuwaDamageNumber>> Active;
	UPROPERTY(Transient) TArray<TObjectPtr<UWuwaDamageNumber>> Pool;
	UPROPERTY(Transient) TObjectPtr<UUIItem> Container;
	UPROPERTY(Transient) TObjectPtr<ULGUICanvasScaler> Scaler;
	UPROPERTY(Transient) TObjectPtr<ULGUIPrefab> Prefab;
	UPROPERTY(Transient) TObjectPtr<UDataTable> TextTable;
	FDelegateHandle DamageHandle;
	bool bVisible = true;
};
