#include "Game/LGUI/Component/WuwaUINiagara.h"

#include "Core/ActorComponent/LGUICanvas.h"
#include "Core/LGUIMesh/LGUIMeshComponent.h"
#include "Core/LGUIMeshVertex.h"
#include "Core/UIDrawcall.h"
#include "Engine/StaticMesh.h"
#include "NiagaraComponent.h"
#include "NiagaraDataSet.h"
#include "NiagaraDataSetAccessor.h"
#include "NiagaraEmitterInstance.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemInstance.h"
#include "NiagaraSystemInstanceController.h"
#include "StaticMeshResources.h"
#include "Materials/MaterialInterface.h"

namespace WuwaUINiagara
{
	/** 粒子局部空间（XY = 界面平面，原作旧 LGUI 的 X 右 Y 上）→ LGUI3 界面局部空间（Y 右、Z 上、X 是深度）。 */
	FVector ToUILocal(const FVector3f& P, float Scale)
	{
		return FVector(-P.Z * Scale, P.X * Scale, P.Y * Scale);
	}

	FNiagaraEmitterInstance* FindEmitter(UNiagaraComponent* Simulation, int32 EmitterIndex)
	{
		if (!Simulation || !Simulation->IsActive()) return nullptr;
		FNiagaraSystemInstanceControllerPtr Controller = Simulation->GetSystemInstanceController();
		FNiagaraSystemInstance* Instance = Controller.IsValid() ? Controller->GetSystemInstance_Unsafe() : nullptr;
		if (!Instance) return nullptr;
		// 保险：并行模拟还没结束时等它做完（读的是这一帧完成后的粒子数据）
		Controller->WaitForConcurrentTickAndFinalize();
		TArrayView<FNiagaraEmitterInstanceRef> Emitters = Instance->GetEmitters();
		return Emitters.IsValidIndex(EmitterIndex) ? &Emitters[EmitterIndex].Get() : nullptr;
	}
}

// —— UWuwaUINiagara ——

UWuwaUINiagara::UWuwaUINiagara(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// 不用组件 Tick（从 LGUI 预制体加载的界面组件不一定开着 Tick）：每帧在世界的所有 Actor Tick 完之后（Niagara 模拟也完了）刷新
	PrimaryComponentTick.bCanEverTick = false;
}

void UWuwaUINiagara::BeginPlay()
{
	Super::BeginPlay();
	PostActorTickHandle = FWorldDelegates::OnWorldPostActorTick.AddUObject(this, &UWuwaUINiagara::HandlePostActorTick);
	if (bPlayOnActive && GetIsUIActiveInHierarchy())
	{
		Play();
	}
}

void UWuwaUINiagara::EndPlay(const EEndPlayReason::Type Reason)
{
	FWorldDelegates::OnWorldPostActorTick.Remove(PostActorTickHandle);
	PostActorTickHandle.Reset();
	DestroyRenderers();
	if (Simulation)
	{
		Simulation->DestroyComponent();
		Simulation = nullptr;
	}
	Super::EndPlay(Reason);
}

void UWuwaUINiagara::SetNiagaraSystem(UNiagaraSystem* InSystem)
{
	if (NiagaraSystem == InSystem) return;
	NiagaraSystem = InSystem;
	DestroyRenderers();
	if (Simulation)
	{
		Simulation->SetAsset(InSystem);
	}
}

void UWuwaUINiagara::EnsureSimulation()
{
	if (Simulation || !NiagaraSystem || !GetWorld()) return;
	Simulation = NewObject<UNiagaraComponent>(GetOwner(), TEXT("UINiagaraSimulation"), RF_Transient);
	Simulation->SetAutoActivate(false);
	// 只要模拟，不要它自己画（由 UWuwaUINiagaraRenderer 画进 LGUI 画布）；也不参与距离 / 可见性剔除。
	// SetRenderingEnabled 只在"按目标时间播放"模式下生效，所以直接把组件设成不可见（不可见的 Niagara 组件照样模拟）
	Simulation->SetRenderingEnabled(false);
	Simulation->SetVisibility(false);
	Simulation->SetAllowScalability(false);
	// 模拟放在最早的 tick 组跑完（Niagara 默认在工作线程上并行模拟），我们在 TG_PostUpdateWork 读粒子时这一帧的数据已经好了
	Simulation->SetTickBehavior(ENiagaraTickBehavior::ForceTickFirst);
	Simulation->SetupAttachment(this);
	Simulation->RegisterComponent();
	Simulation->SetAsset(NiagaraSystem);
}

void UWuwaUINiagara::RebuildRenderers()
{
	DestroyRenderers();
	bRenderersBuilt = true;
	if (!NiagaraSystem) return;
	const TArray<FNiagaraEmitterHandle>& Handles = NiagaraSystem->GetEmitterHandles();
	for (int32 EmitterIndex = 0; EmitterIndex < Handles.Num(); ++EmitterIndex)
	{
		const FVersionedNiagaraEmitterData* Data = Handles[EmitterIndex].GetEmitterData();
		if (!Data || !Handles[EmitterIndex].GetIsEnabled()) continue;
		for (UNiagaraRendererProperties* Properties : Data->GetRenderers())
		{
			if (!Properties || !Properties->GetIsEnabled()) continue;
			if (!Properties->IsA<UNiagaraSpriteRendererProperties>() && !Properties->IsA<UNiagaraMeshRendererProperties>()) continue;
			UWuwaUINiagaraRenderer* Renderer = NewObject<UWuwaUINiagaraRenderer>(GetOwner(), NAME_None, RF_Transient);
			Renderer->Setup(this, EmitterIndex, Properties);
			Renderer->RegisterComponent();
			Renderer->AttachToComponent(this, FAttachmentTransformRules::KeepRelativeTransform);
			// 渲染器的原点 = 这个节点的轴心（粒子坐标原点）；大小为 0，只作为画网格的坐标系
			FUIAnchorData Anchor;
			Anchor.AnchorMin = Anchor.AnchorMax = GetPivot();
			Anchor.Pivot = FVector2D(0.5f, 0.5f);
			Anchor.AnchoredPosition = FVector2D::ZeroVector;
			Anchor.SizeDelta = FVector2D::ZeroVector;
			Renderer->SetAnchorData(Anchor);
			Renderer->SetRaycastTarget(false);
			Renderers.Add(Renderer);
		}
	}
}

void UWuwaUINiagara::DestroyRenderers()
{
	for (UWuwaUINiagaraRenderer* Renderer : Renderers)
	{
		if (IsValid(Renderer)) Renderer->DestroyComponent();
	}
	Renderers.Reset();
	bRenderersBuilt = false;
}

void UWuwaUINiagara::Play()
{
	EnsureSimulation();
	if (!Simulation) return;
	if (!bRenderersBuilt) RebuildRenderers();
	// 编辑器里系统刚加载时脚本还在异步编译（IsReadyToRun 为 false），这时 Activate 会直接失败：记下来，编译完再播
	if (!NiagaraSystem->IsReadyToRun())
	{
		bPendingPlay = true;
		return;
	}
	bPendingPlay = false;
	Simulation->Activate(true);
}

void UWuwaUINiagara::Stop()
{
	bPendingPlay = false;
	if (Simulation) Simulation->DeactivateImmediate();
	for (UWuwaUINiagaraRenderer* Renderer : Renderers)
	{
		if (IsValid(Renderer)) Renderer->RefreshFromSimulation();
	}
}

bool UWuwaUINiagara::IsPlaying() const
{
	return bPendingPlay || (Simulation && Simulation->IsActive() && !Simulation->IsComplete());
}

void UWuwaUINiagara::SetFloatParameter(FName Name, float Value)
{
	EnsureSimulation();
	if (Simulation) Simulation->SetVariableFloat(Name, Value);
}

void UWuwaUINiagara::SetColorParameter(FName Name, FLinearColor Value)
{
	EnsureSimulation();
	if (Simulation) Simulation->SetVariableLinearColor(Name, Value);
}

int32 UWuwaUINiagara::GetRenderedParticleCount() const
{
	int32 Count = 0;
	for (const UWuwaUINiagaraRenderer* Renderer : Renderers)
	{
		if (IsValid(Renderer)) Count += Renderer->GetRenderedParticleCount();
	}
	return Count;
}

int32 UWuwaUINiagara::GetSimulatedParticleCount() const
{
	int32 Count = 0;
	if (!NiagaraSystem) return 0;
	for (int32 Index = 0; Index < NiagaraSystem->GetEmitterHandles().Num(); ++Index)
	{
		const FNiagaraEmitterInstance* Emitter = WuwaUINiagara::FindEmitter(Simulation, Index);
		const FNiagaraDataBuffer* Buffer = Emitter ? Emitter->GetParticleData().GetCurrentData() : nullptr;
		Count += Buffer ? static_cast<int32>(Buffer->GetNumInstances()) : 0;
	}
	return Count;
}

void UWuwaUINiagara::HandlePostActorTick(UWorld* World, ELevelTick TickType, float DeltaSeconds)
{
	if (World != GetWorld()) return;
	if (bPendingPlay && NiagaraSystem && NiagaraSystem->IsReadyToRun())
	{
		Play();
	}
	for (UWuwaUINiagaraRenderer* Renderer : Renderers)
	{
		if (IsValid(Renderer)) Renderer->RefreshFromSimulation();
	}
}

void UWuwaUINiagara::OnUIActiveInHierachy(bool ActiveOrInactive)
{
	Super::OnUIActiveInHierachy(ActiveOrInactive);
	if (!HasBegunPlay()) return;
	if (ActiveOrInactive && bPlayOnActive)
	{
		Play();
	}
	else if (!ActiveOrInactive)
	{
		Stop();
	}
}

// —— UWuwaUINiagaraRenderer ——

UWuwaUINiagaraRenderer::UWuwaUINiagaraRenderer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWuwaUINiagaraRenderer::Setup(UWuwaUINiagara* InOwner, int32 InEmitterIndex, UNiagaraRendererProperties* InProperties)
{
	Owner = InOwner;
	EmitterIndex = InEmitterIndex;
	Properties = InProperties;
	Material = nullptr;
	if (const UNiagaraMeshRendererProperties* MeshProperties = Cast<UNiagaraMeshRendererProperties>(InProperties))
	{
		bIsMesh = true;
		const UStaticMesh* Mesh = MeshProperties->Meshes.Num() > 0 ? MeshProperties->Meshes[0].Mesh.Get() : nullptr;
		if (MeshProperties->bOverrideMaterials && MeshProperties->OverrideMaterials.Num() > 0)
		{
			Material = MeshProperties->OverrideMaterials[0].ExplicitMat;
		}
		if (!Material && Mesh)
		{
			Material = Mesh->GetMaterial(0);
		}
		if (!CacheMesh(Mesh))
		{
			UE_LOG(LogTemp, Warning, TEXT("UINiagara: mesh %s has no CPU-readable LOD0 (enable Allow CPU Access)."), *GetNameSafe(Mesh));
		}
	}
	else if (const UNiagaraSpriteRendererProperties* SpriteProperties = Cast<UNiagaraSpriteRendererProperties>(InProperties))
	{
		bIsMesh = false;
		Material = SpriteProperties->Material;
	}
}

bool UWuwaUINiagaraRenderer::CacheMesh(const UStaticMesh* Mesh)
{
	MeshCache = FMeshCache();
	const FStaticMeshRenderData* RenderData = Mesh ? Mesh->GetRenderData() : nullptr;
	if (!RenderData || RenderData->LODResources.Num() == 0) return false;
	const FStaticMeshLODResources& LOD = RenderData->LODResources[0];
	const FPositionVertexBuffer& Positions = LOD.VertexBuffers.PositionVertexBuffer;
	const FStaticMeshVertexBuffer& Vertices = LOD.VertexBuffers.StaticMeshVertexBuffer;
	const int32 NumVertices = Positions.GetNumVertices();
	if (NumVertices == 0 || !Positions.GetVertexData() || !Vertices.GetTexCoordData()) return false;
	MeshCache.Positions.SetNum(NumVertices);
	MeshCache.UVs.SetNum(NumVertices);
	for (int32 Index = 0; Index < NumVertices; ++Index)
	{
		MeshCache.Positions[Index] = Positions.VertexPosition(Index);
		MeshCache.UVs[Index] = Vertices.GetVertexUV(Index, 0);
	}
	LOD.IndexBuffer.GetCopy(MeshCache.Indices);
	return MeshCache.Indices.Num() > 0;
}

bool UWuwaUINiagaraRenderer::HasDrawcall() const
{
	return drawcall.IsValid() && drawcall->DrawcallRenderSection.IsValid() && drawcall->DrawcallMesh.IsValid();
}

FString UWuwaUINiagaraRenderer::DescribeMesh(int32 Index) const
{
	if (!HasDrawcall()) return TEXT("no drawcall");
	const FLGUIMeshSection* Section = static_cast<const FLGUIMeshSection*>(drawcall->DrawcallRenderSection.Pin().Get());
	const FString Vertex = Section->vertices.IsValidIndex(Index)
		? FString::Printf(TEXT("v%d pos %s color %s"), Index, *Section->vertices[Index].Position.ToString(), *Section->vertices[Index].Color.ToString())
		: TEXT("no vertex");
	FBox3f Extent(ForceInit);
	const int32 Used = FMath::Min(Section->vertices.Num(), RenderedParticles * (bIsMesh ? MeshCache.Positions.Num() : 4));
	for (int32 I = 0; I < Used; ++I) Extent += Section->vertices[I].Position;
	return FString::Printf(TEXT("verts %d tris %d, drawn extent %s, %s, material %s"), Section->vertices.Num(), Section->triangles.Num() / 3,
		*Extent.ToString(), *Vertex, *GetNameSafe(drawcall->RenderMaterial.Get()));
}

bool UWuwaUINiagaraRenderer::HaveValidData() const
{
	// 粒子数会变：始终保留一块（没有粒子时是退化成点的）网格，drawcall 不会被画布拿掉
	return Properties != nullptr;
}

UMaterialInterface* UWuwaUINiagaraRenderer::GetMaterial() const
{
	return Material;
}

void UWuwaUINiagaraRenderer::OnMeshDataReady()
{
	Super::OnMeshDataReady();
	if (drawcall.IsValid() && drawcall->DrawcallRenderSection.IsValid())
	{
		VertexCapacity = IndexCapacity = 0;
		ResizeBuffers(4, 6);
		MarkCanvasUpdate(true, false, false);
	}
}

void UWuwaUINiagaraRenderer::ResizeBuffers(int32 Vertices, int32 Indices)
{
	TSharedPtr<FLGUIRenderSection> Section = drawcall.IsValid() ? drawcall->DrawcallRenderSection.Pin() : nullptr;
	if (!Section || !drawcall->DrawcallMesh.IsValid()) return;
	FLGUIMeshSection* MeshSection = static_cast<FLGUIMeshSection*>(Section.Get());
	VertexCapacity = Vertices;
	IndexCapacity = Indices;
	MeshSection->vertices.SetNumZeroed(VertexCapacity);
	MeshSection->triangles.SetNumZeroed(IndexCapacity);
	drawcall->DrawcallMesh->CreateRenderSectionRenderData(Section);
	drawcall->bMaterialNeedToReassign = true;
	drawcall->bMaterialChanged = true;
}

void UWuwaUINiagaraRenderer::RefreshFromSimulation()
{
	RenderedParticles = 0;
	if (!drawcall.IsValid() || !drawcall->DrawcallRenderSection.IsValid() || !drawcall->DrawcallMesh.IsValid()) return;
	BuildGeometry();
}

bool UWuwaUINiagaraRenderer::BuildGeometry()
{
	UWuwaUINiagara* OwnerItem = Owner.Get();
	ULGUICanvas* Canvas = GetRenderCanvas();
	if (!OwnerItem || !Canvas || !Canvas->GetUIItem())
	{
		UE_CLOG(!bWarnedNoCanvas, LogTemp, Warning, TEXT("UINiagara renderer %s: no render canvas, nothing drawn."), *GetPathName());
		bWarnedNoCanvas = true;
		return false;
	}

	FNiagaraEmitterInstance* Emitter = WuwaUINiagara::FindEmitter(OwnerItem->GetSimulation(), EmitterIndex);
	const FNiagaraDataSet* DataSet = Emitter && !Emitter->IsComplete() ? &Emitter->GetParticleData() : nullptr;
	const FNiagaraDataBuffer* Buffer = DataSet ? DataSet->GetCurrentData() : nullptr;
	const int32 NumParticles = Buffer ? static_cast<int32>(Buffer->GetNumInstances()) : 0;

	const int32 VerticesPerParticle = bIsMesh ? MeshCache.Positions.Num() : 4;
	const int32 IndicesPerParticle = bIsMesh ? MeshCache.Indices.Num() : 6;
	const int32 Drawn = VerticesPerParticle > 0 ? NumParticles : 0;
	const int32 NeedVertices = FMath::Max(4, Drawn * VerticesPerParticle);
	const int32 NeedIndices = FMath::Max(6, Drawn * IndicesPerParticle);
	{
		// 画布更新时 LGUI 可能重建这块网格的数据（数组长度变了），那就按当前需要重新分配
		const FLGUIMeshSection* Current = static_cast<const FLGUIMeshSection*>(drawcall->DrawcallRenderSection.Pin().Get());
		if (!Current || Current->vertices.Num() != VertexCapacity || Current->triangles.Num() != IndexCapacity)
		{
			VertexCapacity = IndexCapacity = 0;
		}
	}
	if (NeedVertices > VertexCapacity || NeedIndices > IndexCapacity)
	{
		// 按 1.5 倍扩容，避免粒子数小幅增长时反复重建。重建网格数据后材质要由画布重新指定（LGUI 只在画布更新时给 drawcall 设材质）
		ResizeBuffers(NeedVertices * 3 / 2, NeedIndices * 3 / 2);
		MarkCanvasUpdate(true, false, false);
		++ResizeCount;
		UE_CLOG(ResizeCount == 20, LogTemp, Warning, TEXT("UINiagara renderer %s keeps reallocating its mesh (LGUI resets the section)."), *GetPathName());
	}
	TSharedPtr<FLGUIRenderSection> Section = drawcall->DrawcallRenderSection.Pin();
	if (!Section) return false;
	FLGUIMeshSection* MeshSection = static_cast<FLGUIMeshSection*>(Section.Get());
	TArray<FLGUIMeshVertex>& Vertices = MeshSection->vertices;
	TArray<FLGUIMeshIndexBufferType>& Triangles = MeshSection->triangles;
	if (Vertices.Num() != VertexCapacity || Triangles.Num() != IndexCapacity) return false;

	// 界面节点 → 画布空间（和 UIStaticMesh 一样，LGUI 画布网格的顶点在画布空间）
	const FTransform CanvasInverse = Canvas->GetUIItem()->GetComponentTransform().Inverse();
	FTransform ItemToCanvas;
	FTransform::Multiply(&ItemToCanvas, &GetComponentTransform(), &CanvasInverse);
	const float Scale = OwnerItem->GetNiagaraScale();
	const float UIAlpha = GetFinalAlpha01();

	int32 V = 0;
	int32 I = 0;
	if (Drawn > 0)
	{
		const auto Position = FNiagaraDataSetAccessor<FNiagaraPosition>(*DataSet, TEXT("Position")).GetReader(*DataSet);
		const auto ColorReader = FNiagaraDataSetAccessor<FLinearColor>(*DataSet, TEXT("Color")).GetReader(*DataSet);
		auto WriteVertex = [&](const FVector3f& Local, const FVector2f& UV, const FLinearColor& Tint)
		{
			FLGUIMeshVertex& Vertex = Vertices[V++];
			Vertex.Position = FVector3f(ItemToCanvas.TransformPosition(WuwaUINiagara::ToUILocal(Local, Scale)));
			// 粒子颜色是线性的，原样量化写进顶点色（材质按线性读）；乘上 UI 的透明度（CanvasGroup 淡入淡出）
			FLinearColor Final = Tint;
			Final.A *= UIAlpha;
			Vertex.Color = Final.ToFColor(false);
			Vertex.TextureCoordinate[0] = UV;
			Vertex.TangentZ = FVector3f(1, 0, 0);
			Vertex.TangentZ.Vector.W = 127;
			Vertex.TangentX = FVector3f(0, 1, 0);
		};

		if (bIsMesh)
		{
			const auto ScaleReader = FNiagaraDataSetAccessor<FVector3f>(*DataSet, TEXT("Scale")).GetReader(*DataSet);
			const auto Orientation = FNiagaraDataSetAccessor<FQuat4f>(*DataSet, TEXT("MeshOrientation")).GetReader(*DataSet);
			for (int32 Particle = 0; Particle < Drawn; ++Particle)
			{
				const FVector3f Center = Position.GetSafe(Particle, FNiagaraPosition(ForceInitToZero));
				const FVector3f ParticleScale = ScaleReader.GetSafe(Particle, FVector3f::OneVector);
				const FQuat4f Rotation = Orientation.GetSafe(Particle, FQuat4f::Identity);
				const FLinearColor Tint = ColorReader.GetSafe(Particle, FLinearColor::White);
				const int32 Base = V;
				for (int32 Index = 0; Index < MeshCache.Positions.Num(); ++Index)
				{
					WriteVertex(Center + Rotation.RotateVector(MeshCache.Positions[Index] * ParticleScale), MeshCache.UVs[Index], Tint);
				}
				for (const uint32 Index : MeshCache.Indices)
				{
					Triangles[I++] = static_cast<FLGUIMeshIndexBufferType>(Base + Index);
				}
			}
		}
		else
		{
			const UNiagaraSpriteRendererProperties* Sprite = CastChecked<UNiagaraSpriteRendererProperties>(Properties);
			const auto Size = FNiagaraDataSetAccessor<FVector2f>(*DataSet, TEXT("SpriteSize")).GetReader(*DataSet);
			const auto Rotation = FNiagaraDataSetAccessor<float>(*DataSet, TEXT("SpriteRotation")).GetReader(*DataSet);
			const auto SubImage = FNiagaraDataSetAccessor<float>(*DataSet, TEXT("SubImageIndex")).GetReader(*DataSet);
			const FVector2f Pivot(Sprite->PivotInUVSpace);
			const int32 Columns = FMath::Max(1, FMath::RoundToInt(Sprite->SubImageSize.X));
			const int32 Rows = FMath::Max(1, FMath::RoundToInt(Sprite->SubImageSize.Y));
			// 四个角的 UV（u 向右、v 向下）
			const FVector2f Corners[4] = {FVector2f(0, 0), FVector2f(1, 0), FVector2f(1, 1), FVector2f(0, 1)};
			for (int32 Particle = 0; Particle < Drawn; ++Particle)
			{
				const FVector3f Center = Position.GetSafe(Particle, FNiagaraPosition(ForceInitToZero));
				const FVector2f ParticleSize = Size.GetSafe(Particle, FVector2f(10.f, 10.f));
				const float Radians = FMath::DegreesToRadians(Rotation.GetSafe(Particle, 0.f));
				const float Cos = FMath::Cos(Radians), Sin = FMath::Sin(Radians);
				const FLinearColor Tint = ColorReader.GetSafe(Particle, FLinearColor::White);
				const int32 Frame = FMath::Max(0, FMath::FloorToInt(SubImage.GetSafe(Particle, 0.f))) % (Columns * Rows);
				const FVector2f FrameOrigin(float(Frame % Columns) / Columns, float(Frame / Columns) / Rows);
				const int32 Base = V;
				for (const FVector2f& Corner : Corners)
				{
					// 界面平面里的偏移：x 向右、y 向上（v 向下所以取反），绕深度轴转 SpriteRotation
					const float X = (Corner.X - Pivot.X) * ParticleSize.X;
					const float Y = (Pivot.Y - Corner.Y) * ParticleSize.Y;
					const FVector3f Offset(X * Cos - Y * Sin, X * Sin + Y * Cos, 0.f);
					const FVector2f UV(FrameOrigin.X + Corner.X / Columns, FrameOrigin.Y + Corner.Y / Rows);
					WriteVertex(Center + Offset, UV, Tint);
				}
				const int32 Quad[6] = {0, 1, 2, 0, 2, 3};
				for (const int32 Index : Quad)
				{
					Triangles[I++] = static_cast<FLGUIMeshIndexBufferType>(Base + Index);
				}
			}
		}
	}
	RenderedParticles = Drawn;
	// 剩下的顶点退化成看不见的点
	for (; V < VertexCapacity; ++V)
	{
		Vertices[V].Position = FVector3f::ZeroVector;
		Vertices[V].Color = FColor(0, 0, 0, 0);
	}
	for (; I < IndexCapacity; ++I)
	{
		Triangles[I] = 0;
	}
	drawcall->DrawcallMesh->UpdateMeshSectionRenderData(Section, true, Canvas->GetActualAdditionalShaderChannelFlags());
	return true;
}

// —— AWuwaUINiagaraActor ——

AWuwaUINiagaraActor::AWuwaUINiagaraActor()
{
	PrimaryActorTick.bCanEverTick = false;
	UINiagara = CreateDefaultSubobject<UWuwaUINiagara>(TEXT("UINiagara"));
	RootComponent = UINiagara;
}
