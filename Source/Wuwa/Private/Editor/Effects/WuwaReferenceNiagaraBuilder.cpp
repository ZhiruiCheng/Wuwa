#include "Editor/Effects/WuwaReferenceNiagaraBuilder.h"

#if WITH_EDITOR
#include "EdGraphSchema_Niagara.h"
#include "Materials/MaterialInterface.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraGraph.h"
#include "NiagaraMeshRendererProperties.h"
#include "NiagaraNodeAssignment.h"
#include "NiagaraNodeCustomHlsl.h"
#include "NiagaraNodeFunctionCall.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraRibbonRendererProperties.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemFactoryNew.h"
#include "UObject/UnrealType.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaReferenceNiagara, Log, All);

namespace WuwaReferenceNiagaraBuilder
{
#if WITH_EDITOR
	namespace
	{
		template<class T>
		bool SetRapid(FVersionedNiagaraEmitterData& Data, const TCHAR* Suffix, const T& Value)
		{
			TArray<UNiagaraScript*> Scripts;
			Data.GetScripts(Scripts, false);
			bool bFound = false;
			for (UNiagaraScript* Script : Scripts)
			{
				if (!Script) continue;
				TArray<FNiagaraVariable> Variables;
				Script->RapidIterationParameters.GetParameters(Variables);
				for (const FNiagaraVariable& Variable : Variables)
				{
					if (!Variable.GetName().ToString().EndsWith(Suffix)) continue;
					if (Variable.GetSizeInBytes() != sizeof(T))
					{
						UE_LOG(LogWuwaReferenceNiagara, Error, TEXT("Template parameter has an unexpected type: %s"), Suffix);
						return false;
					}
					bFound |= Script->RapidIterationParameters.SetParameterValue(Value, Variable, false);
				}
			}
			if (!bFound) UE_LOG(LogWuwaReferenceNiagara, Error, TEXT("Required template parameter missing: %s"), Suffix);
			return bFound;
		}

		/**
		 * 关掉粒子更新脚本里的某个模板模块。OmnidirectionalBurst 自带 ScaleSpriteSize（随寿命缩小）：
		 * Sprite 锚点在尾端，缩小时前端往回缩，看起来像粒子在倒退。
		 */
		bool DisableUpdateModule(FVersionedNiagaraEmitterData& Data, const TCHAR* ModuleName)
		{
			UNiagaraScriptSource* Source = Data.UpdateScriptProps.Script
				? Cast<UNiagaraScriptSource>(Data.UpdateScriptProps.Script->GetLatestSource()) : nullptr;
			if (!Source || !Source->NodeGraph) return false;
			for (UEdGraphNode* Node : Source->NodeGraph->Nodes)
			{
				auto* Function = Cast<UNiagaraNodeFunctionCall>(Node);
				if (!Function || !Function->FunctionScript || Function->FunctionScript->GetName() != ModuleName) continue;
				FNiagaraStackGraphUtilities::SetModuleIsEnabled(*Function, false);
				return true;
			}
			UE_LOG(LogWuwaReferenceNiagara, Error, TEXT("Template update module missing: %s"), ModuleName);
			return false;
		}

		bool SetOneShot(FVersionedNiagaraEmitterData& Data)
		{
			UNiagaraScriptSource* Source = Data.EmitterUpdateScriptProps.Script
				? Cast<UNiagaraScriptSource>(Data.EmitterUpdateScriptProps.Script->GetLatestSource()) : nullptr;
			if (!Source || !Source->NodeGraph) return false;
			const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();
			for (UEdGraphNode* Node : Source->NodeGraph->Nodes)
			{
				auto* Function = Cast<UNiagaraNodeFunctionCall>(Node);
				if (!Function || !Function->FunctionScript || Function->FunctionScript->GetName() != TEXT("EmitterState")) continue;
				const TPair<FName, int32> Settings[] = {
					{ TEXT("Life Cycle Mode"), 1 }, { TEXT("Loop Behavior"), 1 }, { TEXT("Inactive Response"), 0 }
				};
				for (const auto& Setting : Settings)
				{
					UEdGraphPin* Pin = Function->FindPin(Setting.Key);
					if (!Pin || !Pin->LinkedTo.IsEmpty()) return false;
					FNiagaraVariable Variable(Schema->PinToTypeDefinition(Pin), Pin->PinName);
					Variable.SetValue<int32>(Setting.Value);
					if (!Schema->TryGetPinDefaultValueFromNiagaraVariable(Variable, Pin->DefaultValue)) return false;
				}
				Function->MarkNodeRequiresSynchronization(TEXT("Reference effect: self / once / complete"), true);
				return true;
			}
			return false;
		}

		bool SetCustomInput(UNiagaraNodeAssignment& Assignment, const FNiagaraVariable& Target, const FString& Hlsl)
		{
			const FName ModuleInput(*FString::Printf(TEXT("Module.%s"), *Target.GetName().ToString()));
			const FNiagaraParameterHandle Handle = FNiagaraParameterHandle::CreateAliasedModuleParameterHandle(
				FNiagaraParameterHandle(ModuleInput), &Assignment);
			UEdGraphPin& Override = FNiagaraStackGraphUtilities::GetOrCreateStackFunctionInputOverridePin(
				Assignment, Handle, Target.GetType(), FGuid(), FGuid());
			if (!Override.LinkedTo.IsEmpty()) return false;
			UEdGraphNode* OverrideNode = Override.GetOwningNode();
			UEdGraphPin* MapInput = nullptr;
			for (UEdGraphPin* Pin : OverrideNode->Pins)
				if (Pin->Direction == EGPD_Input
					&& UEdGraphSchema_Niagara::PinToTypeDefinition(Pin) == FNiagaraTypeDefinition::GetParameterMapDef()) MapInput = Pin;
			if (!MapInput || MapInput->LinkedTo.Num() != 1) return false;

			// UE's SetCustomExpressionForFunctionInput and CustomHlsl setters are not DLL-exported.
			// Build the same serialized node using its public signature and reflected HLSL property;
			// the result is a normal editable Custom HLSL dynamic input in the Niagara stack.
			FStrProperty* CodeProperty = FindFProperty<FStrProperty>(UNiagaraNodeCustomHlsl::StaticClass(), TEXT("CustomHlsl"));
			if (!CodeProperty) return false;
			FGraphNodeCreator<UNiagaraNodeCustomHlsl> Creator(*OverrideNode->GetGraph());
			UNiagaraNodeCustomHlsl* Custom = Creator.CreateNode();
			Custom->ScriptUsage = ENiagaraScriptUsage::DynamicInput;
			Custom->Signature.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetParameterMapDef(), TEXT("Map")));
			Custom->Signature.Outputs.Add(FNiagaraVariable(Target.GetType(), TEXT("CustomHLSLOutput")));
			CodeProperty->SetPropertyValue_InContainer(Custom, Hlsl);
			Custom->NodeComment = TEXT("Deterministic local XY ring, tangent velocity and owner-scaled sprite size");
			Creator.Finalize();
			UEdGraphPin* Input = Custom->FindPin(TEXT("Map"), EGPD_Input);
			UEdGraphPin* Output = Custom->FindPin(TEXT("CustomHLSLOutput"), EGPD_Output);
			if (!Input || !Output) return false;
			Input->MakeLinkTo(MapInput->LinkedTo[0]);
			Output->MakeLinkTo(&Override);
			Custom->MarkNodeRequiresSynchronization(TEXT("Reference feather ring expression"), true);
			return true;
		}

		bool SetSpriteRing(FVersionedNiagaraEmitterData& Data, const FEmitterSpec& Spec)
		{
			UNiagaraScript* Spawn = Data.SpawnScriptProps.Script;
			auto* Source = Spawn ? Cast<UNiagaraScriptSource>(Spawn->GetLatestSource()) : nullptr;
			UNiagaraNodeOutput* Output = Source && Source->NodeGraph
				? Source->NodeGraph->FindEquivalentOutputNode(Spawn->GetUsage(), Spawn->GetUsageId()) : nullptr;
			if (!Output) return false;
			// The final spawn module replaces the template's sphere position and radial velocity.
			// Every output remains editable in Niagara and is shared by the renderer's usual bindings.
			FNiagaraVariable Size(FNiagaraTypeDefinition::GetVec2Def(), TEXT("Particles.SpriteSize"));
			Size.SetValue(FVector2f(Spec.SpriteSizeMin));
			FNiagaraVariable Facing(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Particles.SpriteFacing"));
			Facing.SetValue(FVector3f(0, 0, 1));
			FNiagaraVariable Position(FNiagaraTypeDefinition::GetPositionDef(), TEXT("Particles.Position"));
			Position.SetValue(FVector3f::ZeroVector);
			FNiagaraVariable Velocity(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Particles.Velocity"));
			Velocity.SetValue(FVector3f::ZeroVector);
			FNiagaraVariable Color(FNiagaraTypeDefinition::GetColorDef(), TEXT("Particles.Color"));
			Color.SetValue(FLinearColor::White);
			FNiagaraVariable Lifetime(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Particles.Lifetime"));
			Lifetime.SetValue(Spec.Lifetime);
			const bool bTable = !Spec.Table.IsEmpty();
			TArray<FNiagaraVariable> Variables = { Size, Facing, Position, Velocity };
			if (bTable)
			{
				Variables.Add(Color);
				Variables.Add(Lifetime);
			}
			TArray<FString> Defaults;
			for (const FNiagaraVariable& Variable : Variables)
			{
				FString Default;
				if (!GetDefault<UEdGraphSchema_Niagara>()->TryGetPinDefaultValueFromNiagaraVariable(Variable, Default)) return false;
				Defaults.Add(Default);
			}
			auto* Assignment = FNiagaraStackGraphUtilities::AddParameterModuleToStack(Variables, *Output, INDEX_NONE, Defaults);
			if (!Assignment) return false;
			Assignment->NodeComment = TEXT("Feather ring: local XY arc position, tangent or radial velocity, plane-facing normal and width / length");
			if (bTable)
			{
				// 逐粒子表：按 ExecIndex 选行（嵌套三目，组内粒子数很少）。数值来自原作出生脚本在各出生时刻的求值。
				const int32 N = Spec.Table.Num();
				auto Chain = [N](TFunctionRef<FString(const FTableParticle&)> Value, const TArray<FTableParticle>& Rows)
				{
					FString Expression = Value(Rows[N - 1]);
					for (int32 Index = N - 2; Index >= 0; --Index)
					{
						Expression = FString::Printf(TEXT("(Engine.ExecIndex == %d ? %s : %s)"), Index, *Value(Rows[Index]), *Expression);
					}
					return Expression;
				};
				const FString TablePosition = Chain([](const FTableParticle& R) { return FString::Printf(TEXT("float3(%.6f, %.6f, %.6f)"), R.Position.X, R.Position.Y, R.Position.Z); }, Spec.Table);
				const FString TableVelocity = Chain([](const FTableParticle& R) { return FString::Printf(TEXT("float3(%.6f, %.6f, %.6f)"), R.Velocity.X, R.Velocity.Y, R.Velocity.Z); }, Spec.Table);
				const FString TableSize = FString::Printf(TEXT("(%s%s)"),
					*Chain([](const FTableParticle& R) { return FString::Printf(TEXT("float2(%.6f, %.6f)"), R.Size.X, R.Size.Y); }, Spec.Table),
					Spec.bSizeFollowsOwnerScale ? TEXT(" * Engine.Owner.Scale.x") : TEXT(""));
				const FString TableColor = Chain([](const FTableParticle& R) { return FString::Printf(TEXT("float4(%.6f, %.6f, %.6f, %.6f)"), R.Color.R, R.Color.G, R.Color.B, R.Color.A); }, Spec.Table);
				const FString TableLifetime = Chain([](const FTableParticle& R) { return FString::Printf(TEXT("%.6f"), R.Lifetime); }, Spec.Table);
				if (!SetCustomInput(*Assignment, Position, TablePosition) || !SetCustomInput(*Assignment, Velocity, TableVelocity)
					|| !SetCustomInput(*Assignment, Size, TableSize) || !SetCustomInput(*Assignment, Color, TableColor)
					|| !SetCustomInput(*Assignment, Lifetime, TableLifetime)) return false;
				Assignment->NodeComment = TEXT("Per-particle table from the original spawn script evaluated at each birth time (WuwaChangliAttackEffectSetup)");
				Assignment->MarkNodeRequiresSynchronization(TEXT("Reference table spawn"), true);
				return true;
			}
			// Stratify around the authored arc, with deterministic per-particle jitter. Both dynamic
			// inputs use the same angle formula so velocity is orthogonal to the spawn radius.
			// DynamicInput wraps this text as one expression, not a function body.
			const FString Angle = FString::Printf(TEXT(
				"(%.9f + (float(Engine.ExecIndex) + 0.2 + "
				"frac(sin((float(Engine.ExecIndex) + %.9f) * 12.9898) * 43758.5453) * 0.6) / %.9f * %.9f)"),
				FMath::DegreesToRadians(double(Spec.RingStartAngleDegrees)), double(Spec.RandomSeed) + 1.0,
				double(Spec.Count), FMath::DegreesToRadians(double(Spec.RingArcDegrees)));
			const FString Radius = FString::Printf(TEXT(
				"lerp(%.9f, %.9f, frac(sin((float(Engine.ExecIndex) + %.9f) * 63.7264) * 10449.8937))"),
				double(Spec.SpawnRadiusMin), double(Spec.SpawnRadiusMax), double(Spec.RandomSeed) + 43.0);
			const FString PositionHlsl = FString::Printf(TEXT(
				"(float3(cos(%s) * %s, sin(%s) * %s, "
				"(frac(sin((float(Engine.ExecIndex) + %.9f) * 78.233) * 24634.6345) - 0.5) * %.9f) + float3(%.9f, %.9f, %.9f))"),
				*Angle, *Radius, *Angle, *Radius,
				double(Spec.RandomSeed) + 7.0, double(Spec.RingHeight),
				Spec.PositionOffset.X, Spec.PositionOffset.Y, Spec.PositionOffset.Z);
			// Sprite 尺寸不受组件缩放影响；需要时乘 Owner 缩放，让 EffectModel 的 Scale 对 Sprite 同样生效。
			const FString SizeHlsl = FString::Printf(TEXT(
				"(lerp(float2(%.9f, %.9f), float2(%.9f, %.9f), "
				"frac(sin((float(Engine.ExecIndex) + %.9f) * 91.3458) * 47453.5453))%s)"),
				double(Spec.SpriteSizeMin.X), double(Spec.SpriteSizeMin.Y), double(Spec.SpriteSizeMax.X), double(Spec.SpriteSizeMax.Y),
				double(Spec.RandomSeed) + 31.0, Spec.bSizeFollowsOwnerScale ? TEXT(" * Engine.Owner.Scale.x") : TEXT(""));
			// 圆锥 / 球面：两个确定性随机数 u、w（按 ExecIndex 哈希）取方向；圆锥在轴向周围 cos θ ∈ [cos 半角, 1] 内均匀
			const FString U = FString::Printf(TEXT("frac(sin((float(Engine.ExecIndex) + %.9f) * 17.137) * 31718.927)"), double(Spec.RandomSeed) + 3.0);
			const FString W = FString::Printf(TEXT("frac(sin((float(Engine.ExecIndex) + %.9f) * 53.871) * 21417.311)"), double(Spec.RandomSeed) + 5.0);
			const FVector Axis = Spec.ConeDirection.GetSafeNormal(UE_SMALL_NUMBER, FVector(1, 0, 0));
			const FVector Side = FMath::Abs(Axis.Z) < 0.99 ? FVector::CrossProduct(Axis, FVector(0, 0, 1)).GetSafeNormal() : FVector(1, 0, 0);
			const FVector Up = FVector::CrossProduct(Side, Axis);
			const double CosMin = Spec.VelocityMode == 2 ? -1.0 : FMath::Cos(FMath::DegreesToRadians(double(Spec.ConeAngleDegrees)));
			const FString CosT = FString::Printf(TEXT("lerp(%.9f, 1.0, %s)"), CosMin, *U);
			const FString Spread = FString::Printf(TEXT("(sqrt(max(1.0 - %s * %s, 0.0)) * (cos(%s * 6.2831853) * float3(%.9f, %.9f, %.9f) + sin(%s * 6.2831853) * float3(%.9f, %.9f, %.9f)) + %s * float3(%.9f, %.9f, %.9f))"),
				*CosT, *CosT, *W, Side.X, Side.Y, Side.Z, *W, Up.X, Up.Y, Up.Z, *CosT, Axis.X, Axis.Y, Axis.Z);
			const FString Direction = Spec.VelocityMode != 0 ? Spread
				: Spec.bRadialVelocity
				? FString::Printf(TEXT("float3(cos(%s), sin(%s), 0.0)"), *Angle, *Angle)
				: FString::Printf(TEXT("float3(-sin(%s), cos(%s), 0.0) * %.9f"), *Angle, *Angle, Spec.bClockwise ? -1.0 : 1.0);
			const FString VelocityHlsl = FString::Printf(TEXT(
				"(%s * lerp(%.9f, %.9f, frac(sin((float(Engine.ExecIndex) + %.9f) * 39.3467) * 47453.5453)))"),
				*Direction, double(Spec.SpeedMin), double(Spec.SpeedMax), double(Spec.RandomSeed) + 19.0);
			if (!SetCustomInput(*Assignment, Position, PositionHlsl) || !SetCustomInput(*Assignment, Velocity, VelocityHlsl)
				|| !SetCustomInput(*Assignment, Size, SizeHlsl)) return false;
			Assignment->MarkNodeRequiresSynchronization(TEXT("Reference feather ring spawn"), true);
			return true;
		}

		/** 模板参数找不到时把模板脚本里的全部快速迭代参数名打出来，便于对上名字。 */
		void LogRapidParameters(FVersionedNiagaraEmitterData& Data)
		{
			TArray<UNiagaraScript*> Scripts;
			Data.GetScripts(Scripts, false);
			for (UNiagaraScript* Script : Scripts)
			{
				if (!Script) continue;
				TArray<FNiagaraVariable> Variables;
				Script->RapidIterationParameters.GetParameters(Variables);
				for (const FNiagaraVariable& Variable : Variables)
					UE_LOG(LogWuwaReferenceNiagara, Display, TEXT("  template parameter [%s] %s (%d bytes)"), *Script->GetName(), *Variable.GetName().ToString(), Variable.GetSizeInBytes());
			}
		}

		/** 在某个脚本（出生 / 更新）的末尾追加一个参数赋值模块；HlslByVariable 里给了的变量用自定义 HLSL 表达式。 */
		bool AppendAssignment(UNiagaraScript* Script, const TArray<FNiagaraVariable>& Variables, const TMap<FName, FString>& HlslByVariable, const TCHAR* Comment)
		{
			auto* Source = Script ? Cast<UNiagaraScriptSource>(Script->GetLatestSource()) : nullptr;
			UNiagaraNodeOutput* Output = Source && Source->NodeGraph
				? Source->NodeGraph->FindEquivalentOutputNode(Script->GetUsage(), Script->GetUsageId()) : nullptr;
			if (!Output) return false;
			TArray<FString> Defaults;
			for (const FNiagaraVariable& Variable : Variables)
			{
				FString Default;
				if (!GetDefault<UEdGraphSchema_Niagara>()->TryGetPinDefaultValueFromNiagaraVariable(Variable, Default)) return false;
				Defaults.Add(Default);
			}
			auto* Assignment = FNiagaraStackGraphUtilities::AddParameterModuleToStack(Variables, *Output, INDEX_NONE, Defaults);
			if (!Assignment) return false;
			Assignment->NodeComment = Comment;
			for (const FNiagaraVariable& Variable : Variables)
			{
				const FString* Hlsl = HlslByVariable.Find(Variable.GetName());
				if (Hlsl && !SetCustomInput(*Assignment, Variable, *Hlsl)) return false;
			}
			Assignment->MarkNodeRequiresSynchronization(Comment, true);
			return true;
		}

		/** 宽度曲线（等间隔采样）→ 按归一化年龄分段线性插值的表达式。 */
		FString PiecewiseLinear(const TArray<float>& Samples, const TCHAR* X)
		{
			// 采样点太多时抽稀到最多 17 个（16 段），保留首尾
			TArray<float> S;
			const int32 Count = FMath::Min(Samples.Num(), 17);
			for (int32 I = 0; I < Count; ++I)
			{
				const float T = Count == 1 ? 0.f : float(I) / float(Count - 1) * float(Samples.Num() - 1);
				const int32 Lo = FMath::FloorToInt(T);
				const int32 Hi = FMath::Min(Lo + 1, Samples.Num() - 1);
				S.Add(FMath::Lerp(Samples[Lo], Samples[Hi], T - Lo));
			}
			if (S.Num() == 1) return FString::Printf(TEXT("%.6f"), S[0]);
			const float Step = 1.f / float(S.Num() - 1);
			FString Expression = FString::Printf(TEXT("%.6f"), S.Last());
			for (int32 I = S.Num() - 2; I >= 0; --I)
			{
				Expression = FString::Printf(TEXT("(%s < %.6f ? lerp(%.6f, %.6f, saturate((%s - %.6f) / %.6f)) : %s)"),
					X, Step * (I + 1), S[I], S[I + 1], X, Step * I, Step, *Expression);
			}
			return Expression;
		}

		bool ConfigureRibbon(FNiagaraEmitterHandle& Handle, const FEmitterSpec& Spec)
		{
			const FVersionedNiagaraEmitter Instance = Handle.GetInstance();
			FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData();
			if (!Data || !Instance.Emitter) return false;
			Data->RemoveParent();
			// 拖尾留在世界里：世界空间模拟，跟着组件（武器骨骼）位置持续出生
			Data->bLocalSpace = false;
			Data->bDeterminism = true;
			Data->RandomSeed = Spec.RandomSeed;
			Data->SimTarget = ENiagaraSimTarget::CPUSim;
			Data->CalculateBoundsMode = ENiagaraEmitterCalculateBoundMode::Dynamic;
			if (!SetRapid(*Data, TEXT(".SpawnRate.SpawnRate"), Spec.RibbonSpawnRate))
			{
				LogRapidParameters(*Data);
				return false;
			}
			// Fountain 模板自带的锥形初速度 / 重力：拖尾的点留在出生处（原作只有很弱的矢量场漂移，没有重建）
			TArray<UNiagaraScript*> Scripts;
			Data->GetScripts(Scripts, false);
			for (UNiagaraScript* Script : Scripts)
			{
				if (!Script) continue;
				TArray<FNiagaraVariable> Variables;
				Script->RapidIterationParameters.GetParameters(Variables);
				for (const FNiagaraVariable& Variable : Variables)
					if (Variable.GetName().ToString().EndsWith(TEXT(".GravityForce.Gravity")) && Variable.GetSizeInBytes() == sizeof(FVector3f))
						Script->RapidIterationParameters.SetParameterValue(FVector3f::ZeroVector, Variable, false);
			}
			const FNiagaraVariable Velocity(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Particles.Velocity"));
			FNiagaraVariable Still = Velocity;
			Still.SetValue(FVector3f::ZeroVector);
			const FNiagaraVariable Lifetime(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Particles.Lifetime"));
			const FNiagaraVariable Color(FNiagaraTypeDefinition::GetColorDef(), TEXT("Particles.Color"));
			const FNiagaraVariable Width(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Particles.RibbonWidth"));
			FNiagaraVariable LifetimeValue = Lifetime;
			LifetimeValue.SetValue(Spec.Lifetime);
			FNiagaraVariable White = Color;
			White.SetValue(FLinearColor::White);
			FNiagaraVariable FirstWidth = Width;
			FirstWidth.SetValue(Spec.RibbonWidths.IsEmpty() ? 10.f : Spec.RibbonWidths[0]);
			// 颜色 / 透明度曲线在材质里按归一化年龄查（和其他层一样），粒子颜色保持白色；宽度曲线在更新脚本里按归一化年龄查
			const FString WidthHlsl = PiecewiseLinear(Spec.RibbonWidths.IsEmpty() ? TArray<float>{10.f} : Spec.RibbonWidths, TEXT("Map.Particles.NormalizedAge"));
			if (!AppendAssignment(Data->SpawnScriptProps.Script, { LifetimeValue, White, FirstWidth, Still }, {},
					TEXT("Reference ribbon spawn: original lifetime, white particle colour (curves live in the material), first width sample"))
				|| !AppendAssignment(Data->UpdateScriptProps.Script, { White, FirstWidth, Still }, { { Width.GetName(), WidthHlsl } },
					TEXT("Reference ribbon update: width = original Ribbon Width Scale curve x Initial.RibbonWidth on normalized age")))
			{
				UE_LOG(LogWuwaReferenceNiagara, Error, TEXT("Failed to append ribbon assignment modules"));
				return false;
			}
			const TArray<UNiagaraRendererProperties*> Previous = Data->GetRenderers();
			for (UNiagaraRendererProperties* Renderer : Previous) Instance.Emitter->RemoveRenderer(Renderer, Instance.Version);
			auto* Renderer = NewObject<UNiagaraRibbonRendererProperties>(Instance.Emitter, NAME_None, RF_Transactional);
			Renderer->Material = Spec.Material;
			Renderer->SortOrderHint = Spec.SortOrder;
			Renderer->UV0Settings.DistributionMode = ENiagaraRibbonUVDistributionMode::ScaledUniformly;
			Renderer->UV0Settings.TilingLength = Spec.RibbonUVTilingLength;
			Instance.Emitter->AddRenderer(Renderer, Instance.Version);
			return true;
		}

		bool ConfigureEmitter(FNiagaraEmitterHandle& Handle, const FEmitterSpec& Spec)
		{
			if (Spec.bRibbon) return ConfigureRibbon(Handle, Spec);
			const FVersionedNiagaraEmitter Instance = Handle.GetInstance();
			FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData();
			if (!Data || !Instance.Emitter) return false;
			Data->RemoveParent();
			Data->bLocalSpace = Spec.bLocalSpace;
			Data->bDeterminism = true;
			Data->RandomSeed = Spec.RandomSeed;
			Data->SimTarget = ENiagaraSimTarget::CPUSim;
			Data->CalculateBoundsMode = ENiagaraEmitterCalculateBoundMode::Fixed;
			Data->FixedBounds = FBox(FVector(-600), FVector(600));
			const bool bSprite = Spec.Mesh == nullptr;
			if (!SetOneShot(*Data)
				|| !SetRapid(*Data, TEXT(".SpawnBurst_Instantaneous.Spawn Count"), bSprite ? Spec.Count : 1)
				|| !SetRapid(*Data, TEXT(".EmitterState.Loop Duration"), Spec.SpawnTime + FMath::Max(Spec.Lifetime, Spec.LifetimeMax) + .025f)) return false;
			// SpawnTime 才爆发：逐粒子表的一组同时出生的粒子，或原作推后爆发的层（配方 spawnTime）
			if (Spec.SpawnTime > 0.f && !SetRapid(*Data, TEXT(".SpawnBurst_Instantaneous.Spawn Time"), Spec.SpawnTime)) return false;
			if (bSprite)
			{
				if (!SetRapid(*Data, TEXT(".InitializeParticle.Lifetime Min"), Spec.Lifetime)
					|| !SetRapid(*Data, TEXT(".InitializeParticle.Lifetime Max"), Spec.LifetimeMax)
					|| !SetRapid(*Data, TEXT(".RandomRangeFloat.Minimum"), 0.f)
					|| !SetRapid(*Data, TEXT(".RandomRangeFloat.Maximum"), 0.f)
					|| !SetRapid(*Data, TEXT(".AddVelocity.Velocity Speed Scale"), 0.f)
					|| !SetRapid(*Data, TEXT(".ShapeLocation.Sphere Radius"), 0.f)
					|| !SetRapid(*Data, TEXT(".Drag.Drag"), Spec.Drag)
					|| !SetRapid(*Data, TEXT(".GravityForce.Gravity"), FVector3f(Spec.Gravity))
					|| !SetSpriteRing(*Data, Spec)
					|| !DisableUpdateModule(*Data, TEXT("ScaleSpriteSize"))) return false;
				// 原作出生脚本的对齐 / 朝向 / 旋转：放在出生栈最后，覆盖模板与环形赋值里的值
				TArray<FNiagaraVariable> Orient;
				TMap<FName, FString> OrientHlsl;
				auto Add = [&](const FNiagaraTypeDefinition& Type, const TCHAR* Name, const FString& Hlsl)
				{
					const FNiagaraVariable Variable(Type, Name);
					Orient.Add(Variable);
					OrientHlsl.Add(Variable.GetName(), Hlsl);
				};
				if (!Spec.CustomAlignment.IsNearlyZero())
					Add(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Particles.SpriteAlignment"), FString::Printf(TEXT("float3(%.6f, %.6f, %.6f)"), Spec.CustomAlignment.X, Spec.CustomAlignment.Y, Spec.CustomAlignment.Z));
				if (!Spec.SpriteFacing.IsNearlyZero())
					Add(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Particles.SpriteFacing"), FString::Printf(TEXT("float3(%.6f, %.6f, %.6f)"), Spec.SpriteFacing.X, Spec.SpriteFacing.Y, Spec.SpriteFacing.Z));
				if (!FMath::IsNearlyZero(Spec.SpriteRotation))
					Add(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Particles.SpriteRotation"), FString::Printf(TEXT("%.6f"), Spec.SpriteRotation));
				if (!Orient.IsEmpty() && !AppendAssignment(Data->SpawnScriptProps.Script, Orient, OrientHlsl,
					TEXT("Sprite alignment / facing / rotation from the original spawn script (WuwaReferenceNiagaraBuilder)"))) return false;
			}
			else
			{
				if (!SetRapid(*Data, TEXT(".InitializeParticle.Lifetime"), Spec.Lifetime)) return false;
				if (!Spec.MeshPosition.IsNearlyZero())
				{
					const FNiagaraVariable Position(FNiagaraTypeDefinition::GetPositionDef(), TEXT("Particles.Position"));
					const FString Hlsl = FString::Printf(TEXT("Particles.Position + float3(%.6f, %.6f, %.6f)"), Spec.MeshPosition.X, Spec.MeshPosition.Y, Spec.MeshPosition.Z);
					if (!AppendAssignment(Data->SpawnScriptProps.Script, { Position }, { { Position.GetName(), Hlsl } },
						TEXT("Mesh particle spawn position from the original spawn script (WuwaReferenceNiagaraBuilder)"))) return false;
				}
			}

			const TArray<UNiagaraRendererProperties*> Previous = Data->GetRenderers();
			for (UNiagaraRendererProperties* Renderer : Previous) Instance.Emitter->RemoveRenderer(Renderer, Instance.Version);
			if (bSprite)
			{
				auto* Renderer = NewObject<UNiagaraSpriteRendererProperties>(Instance.Emitter, NAME_None, RF_Transactional);
				Renderer->Material = Spec.Material;
				Renderer->SortOrderHint = Spec.SortOrder;
				// 默认值是 Changli 羽毛层（锚在贴图末端、+Z 朝向、按速度对齐）；嘉贝莉娜的层用原作渲染器的设置
				Renderer->PivotInUVSpace = Spec.Pivot;
				Renderer->FacingMode = Spec.bFaceCamera ? ENiagaraSpriteFacingMode::FaceCamera : ENiagaraSpriteFacingMode::CustomFacingVector;
				Renderer->Alignment = Spec.bVelocityAligned ? ENiagaraSpriteAlignment::VelocityAligned
					: !Spec.CustomAlignment.IsNearlyZero() ? ENiagaraSpriteAlignment::CustomAlignment : ENiagaraSpriteAlignment::Unaligned;
				Instance.Emitter->AddRenderer(Renderer, Instance.Version);
			}
			else
			{
				auto* Renderer = NewObject<UNiagaraMeshRendererProperties>(Instance.Emitter, NAME_None, RF_Transactional);
				Renderer->Meshes.SetNum(1);
				Renderer->Meshes[0].Mesh = Spec.Mesh;
				Renderer->Meshes[0].PivotOffset = Spec.PivotOffset;
				Renderer->Meshes[0].PivotOffsetSpace = ENiagaraMeshPivotOffsetSpace::Mesh;
				Renderer->SortOrderHint = Spec.SortOrder;
				Renderer->bOverrideMaterials = true;
				Renderer->OverrideMaterials.SetNum(1);
				Renderer->OverrideMaterials[0].ExplicitMat = Spec.Material;
				Instance.Emitter->AddRenderer(Renderer, Instance.Version);
			}
			return true;
		}
	}
#endif

	bool BuildSystem(UNiagaraSystem* System, const TArray<FEmitterSpec>& Specs)
	{
#if WITH_EDITOR
		if (!IsValid(System) || Specs.IsEmpty()) return false;
		TSet<FName> Names;
		for (const FEmitterSpec& Spec : Specs)
		{
			if (Spec.Name.IsNone() || Names.Contains(Spec.Name) || !IsValid(Spec.Material)
				|| !FMath::IsFinite(Spec.Lifetime) || Spec.Lifetime <= 0 || Spec.Count <= 0
				|| Spec.PivotOffset.ContainsNaN() || Spec.Gravity.ContainsNaN()
				|| !FMath::IsFinite(Spec.LifetimeMax) || Spec.LifetimeMax < Spec.Lifetime
				|| !FMath::IsFinite(Spec.SpawnRadiusMin) || Spec.SpawnRadiusMin < 0 || !FMath::IsFinite(Spec.SpawnRadiusMax) || Spec.SpawnRadiusMax < Spec.SpawnRadiusMin
				|| !FMath::IsFinite(Spec.RingHeight) || Spec.RingHeight < 0
				|| !FMath::IsFinite(Spec.RingArcDegrees) || Spec.RingArcDegrees <= 0 || Spec.RingArcDegrees > 360
				|| !FMath::IsFinite(Spec.RingStartAngleDegrees)
				|| (!Spec.Mesh && !Spec.bLocalSpace && !Spec.bRibbon)
				|| (Spec.bRibbon && (!FMath::IsFinite(Spec.RibbonSpawnRate) || Spec.RibbonSpawnRate <= 0))
				|| !FMath::IsFinite(Spec.SpeedMin) || !FMath::IsFinite(Spec.SpeedMax)
				|| Spec.SpeedMin < 0 || Spec.SpeedMax < Spec.SpeedMin
				|| !FMath::IsFinite(Spec.Drag) || Spec.Drag < 0
				// Negative dimensions intentionally mirror the exported feather UV orientation.
				|| Spec.SpriteSizeMin.ContainsNaN() || FMath::IsNearlyZero(Spec.SpriteSizeMin.X) || FMath::IsNearlyZero(Spec.SpriteSizeMin.Y)
				|| Spec.SpriteSizeMax.ContainsNaN() || FMath::IsNearlyZero(Spec.SpriteSizeMax.X) || FMath::IsNearlyZero(Spec.SpriteSizeMax.Y))
			{
				UE_LOG(LogWuwaReferenceNiagara, Error, TEXT("Invalid reference emitter specification: %s"), *Spec.Name.ToString());
				return false;
			}
			Names.Add(Spec.Name);
		}
		UNiagaraEmitter* MeshTemplate = LoadObject<UNiagaraEmitter>(nullptr,
			TEXT("/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst.SimpleSpriteBurst"));
		UNiagaraEmitter* SpriteTemplate = LoadObject<UNiagaraEmitter>(nullptr,
			TEXT("/Niagara/DefaultAssets/Templates/Emitters/OmnidirectionalBurst.OmnidirectionalBurst"));
		UNiagaraEmitter* RibbonTemplate = LoadObject<UNiagaraEmitter>(nullptr,
			TEXT("/Niagara/DefaultAssets/Templates/Emitters/Fountain.Fountain"));
		if (!MeshTemplate || !SpriteTemplate || !RibbonTemplate) return false;
		System->Modify();
		TSet<FGuid> Previous;
		for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles()) Previous.Add(Handle.GetId());
		System->RemoveEmitterHandlesById(Previous);
		if (!System->GetSystemSpawnScript()->GetLatestSource()) UNiagaraSystemFactoryNew::InitializeSystem(System, true);
		for (const FEmitterSpec& Spec : Specs)
		{
			UNiagaraEmitter* Template = Spec.bRibbon ? RibbonTemplate : Spec.Mesh ? MeshTemplate : SpriteTemplate;
			const FGuid Id = FNiagaraEditorUtilities::AddEmitterToSystem(*System, *Template, Template->GetExposedVersion().VersionGuid);
			FNiagaraEmitterHandle* Handle = System->GetEmitterHandles().FindByPredicate([&](const FNiagaraEmitterHandle& Candidate) { return Candidate.GetId() == Id; });
			if (!Handle) return false;
			Handle->SetName(Spec.Name, *System);
			if (!ConfigureEmitter(*Handle, Spec))
			{
				UE_LOG(LogWuwaReferenceNiagara, Error, TEXT("Failed to configure %s / %s. System was not saved."), *System->GetName(), *Spec.Name.ToString());
				return false;
			}
		}
		System->RequestCompile(true);
		System->WaitForCompilationComplete(false, false);
		if (!System->IsValid())
		{
			UE_LOG(LogWuwaReferenceNiagara, Error, TEXT("Reference Niagara compile failed: %s"), *System->GetPathName());
			return false;
		}
		System->MarkPackageDirty();
		return true;
#else
		return false;
#endif
	}
}
