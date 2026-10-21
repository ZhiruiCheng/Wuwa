#include "Editor/Effects/WuwaUiNiagaraBuilder.h"

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
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSpriteRendererProperties.h"
#include "NiagaraSystem.h"
#include "NiagaraSystemFactoryNew.h"
#include "UObject/UnrealType.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaUiNiagara, Log, All);

namespace WuwaUiNiagaraBuilder
{
#if WITH_EDITOR
	namespace
	{
		// 下面三个辅助函数和 WuwaReferenceNiagaraBuilder 里的相同（那边在匿名命名空间里，没有导出）。
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
						UE_LOG(LogWuwaUiNiagara, Error, TEXT("Template parameter has an unexpected type: %s"), Suffix);
						return false;
					}
					bFound |= Script->RapidIterationParameters.SetParameterValue(Value, Variable, false);
				}
			}
			if (!bFound) UE_LOG(LogWuwaUiNiagara, Error, TEXT("Required template parameter missing: %s"), Suffix);
			return bFound;
		}

		/** 发射器只播一次：Life Cycle Mode = Self、Loop Behavior = Once、Inactive Response = Complete。 */
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
				Function->MarkNodeRequiresSynchronization(TEXT("UI effect: self / once / complete"), true);
				return true;
			}
			return false;
		}

		/** 给 Set Parameters 模块的某个输入挂一个 Custom HLSL 动态输入（表达式）。 */
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
			FStrProperty* CodeProperty = FindFProperty<FStrProperty>(UNiagaraNodeCustomHlsl::StaticClass(), TEXT("CustomHlsl"));
			if (!CodeProperty) return false;
			FGraphNodeCreator<UNiagaraNodeCustomHlsl> Creator(*OverrideNode->GetGraph());
			UNiagaraNodeCustomHlsl* Custom = Creator.CreateNode();
			Custom->ScriptUsage = ENiagaraScriptUsage::DynamicInput;
			Custom->Signature.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetParameterMapDef(), TEXT("Map")));
			Custom->Signature.Outputs.Add(FNiagaraVariable(Target.GetType(), TEXT("CustomHLSLOutput")));
			CodeProperty->SetPropertyValue_InContainer(Custom, Hlsl);
			Custom->NodeComment = TEXT("Rebuilt from the original UI effect script (ArtSource UI effect recipe)");
			Creator.Finalize();
			UEdGraphPin* Input = Custom->FindPin(TEXT("Map"), EGPD_Input);
			UEdGraphPin* Output = Custom->FindPin(TEXT("CustomHLSLOutput"), EGPD_Output);
			if (!Input || !Output) return false;
			Input->MakeLinkTo(MapInput->LinkedTo[0]);
			Output->MakeLinkTo(&Override);
			Custom->MarkNodeRequiresSynchronization(TEXT("UI effect expression"), true);
			return true;
		}

		bool TypeOf(const FString& Type, FNiagaraTypeDefinition& Out)
		{
			if (Type == TEXT("float")) Out = FNiagaraTypeDefinition::GetFloatDef();
			else if (Type == TEXT("vec2")) Out = FNiagaraTypeDefinition::GetVec2Def();
			else if (Type == TEXT("vec3")) Out = FNiagaraTypeDefinition::GetVec3Def();
			else if (Type == TEXT("vec4")) Out = FNiagaraTypeDefinition::GetVec4Def();
			else if (Type == TEXT("color")) Out = FNiagaraTypeDefinition::GetColorDef();
			else if (Type == TEXT("quat")) Out = FNiagaraTypeDefinition::GetQuatDef();
			else if (Type == TEXT("position")) Out = FNiagaraTypeDefinition::GetPositionDef();
			else return false;
			return true;
		}

		/** 在某个脚本（出生 / 更新）末尾加一个 Set Parameters 模块，每个属性一个表达式。 */
		bool AddExpressions(UNiagaraScript* Script, const TArray<FAttribute>& Attributes, const FName EmitterName)
		{
			if (Attributes.IsEmpty()) return true;
			auto* Source = Script ? Cast<UNiagaraScriptSource>(Script->GetLatestSource()) : nullptr;
			UNiagaraNodeOutput* Output = Source && Source->NodeGraph
				? Source->NodeGraph->FindEquivalentOutputNode(Script->GetUsage(), Script->GetUsageId()) : nullptr;
			if (!Output) return false;
			TArray<FNiagaraVariable> Variables;
			TArray<FString> Defaults;
			for (const FAttribute& Attribute : Attributes)
			{
				FNiagaraTypeDefinition Type;
				if (!TypeOf(Attribute.Type, Type))
				{
					UE_LOG(LogWuwaUiNiagara, Error, TEXT("%s: unknown attribute type %s (%s)"), *EmitterName.ToString(), *Attribute.Type, *Attribute.Name);
					return false;
				}
				FNiagaraVariable Variable(Type, *Attribute.Name);
				Variable.AllocateData();
				FString Default;
				if (!GetDefault<UEdGraphSchema_Niagara>()->TryGetPinDefaultValueFromNiagaraVariable(Variable, Default)) return false;
				Variables.Add(Variable);
				Defaults.Add(Default);
			}
			auto* Assignment = FNiagaraStackGraphUtilities::AddParameterModuleToStack(Variables, *Output, INDEX_NONE, Defaults);
			if (!Assignment) return false;
			Assignment->NodeComment = TEXT("UI effect attributes rebuilt from the original script");
			for (int32 Index = 0; Index < Attributes.Num(); ++Index)
			{
				if (!SetCustomInput(*Assignment, Variables[Index], Attributes[Index].Hlsl))
				{
					UE_LOG(LogWuwaUiNiagara, Error, TEXT("%s: cannot set %s"), *EmitterName.ToString(), *Attributes[Index].Name);
					return false;
				}
			}
			Assignment->MarkNodeRequiresSynchronization(TEXT("UI effect attributes"), true);
			return true;
		}

		bool ConfigureEmitter(FNiagaraEmitterHandle& Handle, const FEmitterSpec& Spec)
		{
			const FVersionedNiagaraEmitter Instance = Handle.GetInstance();
			FVersionedNiagaraEmitterData* Data = Handle.GetEmitterData();
			if (!Data || !Instance.Emitter) return false;
			Data->RemoveParent();
			Data->bLocalSpace = true;   // 粒子坐标就是界面节点的局部坐标
			Data->bDeterminism = true;
			Data->RandomSeed = Spec.RandomSeed;
			Data->SimTarget = ENiagaraSimTarget::CPUSim;   // UWuwaUINiagara 只能读 CPU 粒子
			Data->CalculateBoundsMode = ENiagaraEmitterCalculateBoundMode::Fixed;
			Data->FixedBounds = FBox(FVector(-2000), FVector(2000));
			if (!SetOneShot(*Data)
				|| !SetRapid(*Data, TEXT(".SpawnBurst_Instantaneous.Spawn Count"), Spec.SpawnCount)
				|| !SetRapid(*Data, TEXT(".EmitterState.Loop Duration"), Spec.LoopDuration)) return false;
			if (Spec.SpawnTime > 0.f && !SetRapid(*Data, TEXT(".SpawnBurst_Instantaneous.Spawn Time"), Spec.SpawnTime)) return false;
			if (!AddExpressions(Data->SpawnScriptProps.Script, Spec.Spawn, Spec.Name)
				|| !AddExpressions(Data->UpdateScriptProps.Script, Spec.Update, Spec.Name)) return false;

			const TArray<UNiagaraRendererProperties*> Previous = Data->GetRenderers();
			for (UNiagaraRendererProperties* Renderer : Previous) Instance.Emitter->RemoveRenderer(Renderer, Instance.Version);
			if (Spec.Mesh)
			{
				auto* Renderer = NewObject<UNiagaraMeshRendererProperties>(Instance.Emitter, NAME_None, RF_Transactional);
				Renderer->Meshes.SetNum(1);
				Renderer->Meshes[0].Mesh = Spec.Mesh;
				Renderer->SortOrderHint = Spec.SortOrder;
				Renderer->bOverrideMaterials = true;
				Renderer->OverrideMaterials.SetNum(1);
				Renderer->OverrideMaterials[0].ExplicitMat = Spec.Material;
				Instance.Emitter->AddRenderer(Renderer, Instance.Version);
			}
			else
			{
				auto* Renderer = NewObject<UNiagaraSpriteRendererProperties>(Instance.Emitter, NAME_None, RF_Transactional);
				Renderer->Material = Spec.Material;
				Renderer->SortOrderHint = Spec.SortOrder;
				Renderer->PivotInUVSpace = Spec.Pivot;
				Renderer->SubImageSize = Spec.SubImageSize;
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
			if (Spec.Name.IsNone() || Names.Contains(Spec.Name) || !IsValid(Spec.Material) || Spec.SpawnCount <= 0
				|| !FMath::IsFinite(Spec.LoopDuration) || Spec.LoopDuration <= 0.f)
			{
				UE_LOG(LogWuwaUiNiagara, Error, TEXT("Invalid UI emitter specification: %s"), *Spec.Name.ToString());
				return false;
			}
			Names.Add(Spec.Name);
		}
		// 模板：SimpleSpriteBurst（一次爆发；更新阶段有 ParticleState 算 Age / NormalizedAge，我们的表达式放在最后覆盖其它模块）
		UNiagaraEmitter* Template = LoadObject<UNiagaraEmitter>(nullptr,
			TEXT("/Niagara/DefaultAssets/Templates/Emitters/SimpleSpriteBurst.SimpleSpriteBurst"));
		if (!Template) return false;
		System->Modify();
		TSet<FGuid> Previous;
		for (const FNiagaraEmitterHandle& Handle : System->GetEmitterHandles()) Previous.Add(Handle.GetId());
		System->RemoveEmitterHandlesById(Previous);
		if (!System->GetSystemSpawnScript()->GetLatestSource()) UNiagaraSystemFactoryNew::InitializeSystem(System, true);
		for (const FEmitterSpec& Spec : Specs)
		{
			const FGuid Id = FNiagaraEditorUtilities::AddEmitterToSystem(*System, *Template, Template->GetExposedVersion().VersionGuid);
			FNiagaraEmitterHandle* Handle = System->GetEmitterHandles().FindByPredicate([&](const FNiagaraEmitterHandle& Candidate) { return Candidate.GetId() == Id; });
			if (!Handle) return false;
			Handle->SetName(Spec.Name, *System);
			if (!ConfigureEmitter(*Handle, Spec))
			{
				UE_LOG(LogWuwaUiNiagara, Error, TEXT("Failed to configure %s / %s. System was not saved."), *System->GetName(), *Spec.Name.ToString());
				return false;
			}
		}
		System->RequestCompile(true);
		System->WaitForCompilationComplete(false, false);
		if (!System->IsValid())
		{
			UE_LOG(LogWuwaUiNiagara, Error, TEXT("UI Niagara compile failed: %s"), *System->GetPathName());
			return false;
		}
		System->MarkPackageDirty();
		return true;
#else
		return false;
#endif
	}
}
