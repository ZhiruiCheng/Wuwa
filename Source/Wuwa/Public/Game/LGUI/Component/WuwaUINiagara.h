#pragma once

#include "CoreMinimal.h"
#include "Core/Actor/UIBaseActor.h"
#include "Core/ActorComponent/UIDirectMeshRenderable.h"
#include "Core/ActorComponent/UIItem.h"
#include "WuwaUINiagara.generated.h"

class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraRendererProperties;
class UNiagaraSystem;
class UStaticMesh;
class UWuwaUINiagaraRenderer;

/**
 * 在 LGUI 界面里播放 Niagara（≈ 鸣潮自研 LGUI 的 UINiagara，开源 LGUI3 没有）。做法（方案 C）：
 * - 自己持有一个 UNiagaraComponent 跑模拟，但关掉它自己的渲染；
 * - 系统里每个精灵 / 网格渲染器对应一个子组件 UWuwaUINiagaraRenderer（UUIDirectMeshRenderable，一个 drawcall、用渲染器的材质），
 *   每帧把那个发射器的 CPU 粒子数据（位置、颜色、尺寸、旋转、缩放、朝向、翻页帧）转成 LGUI 顶点画进画布，跟着 UI 的层级、透明度、显隐走。
 * 坐标：粒子局部空间的 XY 平面 = 界面平面（原作旧 LGUI 是 X 右、Y 上，网格特效都躺在 XY 平面），
 * 换到 LGUI3 的界面平面（Y 右、Z 上）；NiagaraScale 是原作 UINiagara 的同名缩放（再乘画布换算）。
 * 只支持 CPU 模拟（原作 HUD 用到的 33 个 UI 特效都是）。Niagara 在不能渲染的进程（-NullRHI）里不运行，那时 Play 只会一直等。
 */
UCLASS(ClassGroup = (LGUI), meta = (BlueprintSpawnableComponent))
class WUWA_API UWuwaUINiagara : public UUIItem
{
	GENERATED_BODY()

public:
	UWuwaUINiagara(const FObjectInitializer& ObjectInitializer);

	/** ≈ 原作 SetNiagaraSystem。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|UINiagara")
	void SetNiagaraSystem(UNiagaraSystem* InSystem);
	UNiagaraSystem* GetNiagaraSystem() const { return NiagaraSystem; }

	/** 从头播放（≈ 原作 BattleUiNiagaraItem.Play / SetUIActive(true)）。系统还在编译时等编译完再播。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|UINiagara")
	void Play();
	/** 立刻停掉并清空粒子。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|UINiagara")
	void Stop();
	bool IsPlaying() const;

	/** ≈ 原作 SetNiagaraVarFloat / SetNiagaraVarLinearColor（用户参数，比如 "Dissolve"、"Color"）。 */
	UFUNCTION(BlueprintCallable, Category = "Wuwa|UINiagara")
	void SetFloatParameter(FName Name, float Value);
	UFUNCTION(BlueprintCallable, Category = "Wuwa|UINiagara")
	void SetColorParameter(FName Name, FLinearColor Value);

	void SetNiagaraScale(float InScale) { NiagaraScale = InScale; }
	float GetNiagaraScale() const { return NiagaraScale; }
	UNiagaraComponent* GetSimulation() const { return Simulation; }
	const TArray<TObjectPtr<UWuwaUINiagaraRenderer>>& GetRenderers() const { return Renderers; }
	/** 测试用：上一帧所有渲染器画了多少个粒子。 */
	int32 GetRenderedParticleCount() const;
	/** 测试 / 排查用：模拟里现在有多少个粒子（不管画没画）、是否还在等编译。 */
	int32 GetSimulatedParticleCount() const;
	bool IsWaitingForCompile() const { return bPendingPlay; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void OnUIActiveInHierachy(bool ActiveOrInactive) override;

	UPROPERTY(EditAnywhere, Category = "Wuwa|UINiagara")
	TObjectPtr<UNiagaraSystem> NiagaraSystem;

	/** 粒子单位 → 界面单位的缩放（原作 UINiagara 的 NiagaraScale × 画布换算）。 */
	UPROPERTY(EditAnywhere, Category = "Wuwa|UINiagara")
	float NiagaraScale = 1.f;

	/** 界面变为显示时自动从头播放（原作很多特效是 SetUIActive(true) 触发的）。 */
	UPROPERTY(EditAnywhere, Category = "Wuwa|UINiagara")
	bool bPlayOnActive = false;

private:
	void EnsureSimulation();
	/** 每帧（这个世界所有 Actor Tick 完之后）：编译好了就开始等着的播放，再让各渲染器读粒子画网格。 */
	void HandlePostActorTick(UWorld* World, ELevelTick TickType, float DeltaSeconds);
	void RebuildRenderers();
	void DestroyRenderers();

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> Simulation;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UWuwaUINiagaraRenderer>> Renderers;

	bool bRenderersBuilt = false;
	/** 请求了播放但系统还没编译好（编辑器里刚加载时），编译完在 Tick 里开始。 */
	bool bPendingPlay = false;
	FDelegateHandle PostActorTickHandle;
};

/**
 * UWuwaUINiagara 的一个渲染器：把一个发射器（EmitterIndex）用一个渲染器（RendererProperties，精灵或网格）的方式画成 LGUI 网格。
 * 顶点缓冲按"粒子容量"预分配（LGUI 更新网格时不会扩容），多余的顶点退化成看不见的点；粒子多于容量时才重建。
 */
UCLASS(ClassGroup = (LGUI), NotBlueprintable)
class WUWA_API UWuwaUINiagaraRenderer : public UUIDirectMeshRenderable
{
	GENERATED_BODY()

public:
	UWuwaUINiagaraRenderer(const FObjectInitializer& ObjectInitializer);

	void Setup(UWuwaUINiagara* InOwner, int32 InEmitterIndex, UNiagaraRendererProperties* InProperties);
	/** 从模拟读粒子、写顶点（UWuwaUINiagara 每帧调用）。 */
	void RefreshFromSimulation();
	int32 GetRenderedParticleCount() const { return RenderedParticles; }
	/** 画布有没有给这个渲染器分配 drawcall（没有就画不出来）。 */
	bool HasDrawcall() const;
	/** 排查用：画布网格的顶点数、这一帧画出的顶点范围（画布空间）、第 Index 个顶点和当前材质。 */
	FString DescribeMesh(int32 Index) const;

protected:
	virtual void OnMeshDataReady() override;
	virtual bool HaveValidData() const override;
	virtual UMaterialInterface* GetMaterial() const override;

private:
	struct FMeshCache
	{
		TArray<FVector3f> Positions;
		TArray<FVector2f> UVs;
		TArray<uint32> Indices;
	};

	bool BuildGeometry();
	void ResizeBuffers(int32 Vertices, int32 Indices);
	bool CacheMesh(const UStaticMesh* Mesh);

	TWeakObjectPtr<UWuwaUINiagara> Owner;
	int32 EmitterIndex = INDEX_NONE;

	UPROPERTY(Transient)
	TObjectPtr<UNiagaraRendererProperties> Properties;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> Material;

	FMeshCache MeshCache;
	bool bIsMesh = false;
	int32 VertexCapacity = 0;
	int32 IndexCapacity = 0;
	int32 RenderedParticles = 0;
	bool bWarnedNoCanvas = false;
	int32 ResizeCount = 0;
};

/** 根节点是 UWuwaUINiagara 的界面 Actor（预制体里放 UI 特效用）。 */
UCLASS(ClassGroup = (LGUI))
class WUWA_API AWuwaUINiagaraActor : public AUIBaseActor
{
	GENERATED_BODY()

public:
	AWuwaUINiagaraActor();
	virtual UUIItem* GetUIItem() const override { return UINiagara; }
	UWuwaUINiagara* GetUINiagara() const { return UINiagara; }

private:
	UPROPERTY(Category = "LGUI", VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UWuwaUINiagara> UINiagara;
};
