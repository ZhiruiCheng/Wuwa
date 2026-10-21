#include "Commandlets/WuwaSequenceCameraSetupCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "CineCameraActor.h"
#include "CineCameraComponent.h"
#include "Dom/JsonObject.h"
#include "FileHelpers.h"
#include "GameFramework/Character.h"
#include "HAL/FileManager.h"
#include "LevelSequence.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "MovieScene.h"
#include "Channels/MovieSceneDoubleChannel.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Sections/MovieScene3DTransformSection.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Sections/MovieSceneFloatSection.h"
#include "Sections/MovieSceneSpawnSection.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Tracks/MovieScene3DAttachTrack.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Tracks/MovieSceneCameraCutTrack.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Tracks/MovieSceneSpawnTrack.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "GameFramework/Actor.h"
#include "MovieSceneTrack.h"
#include "MovieSceneSection.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/PointLight.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionSceneTexture.h"
#include "Materials/MaterialParameterCollection.h"
#include "Sections/MovieSceneBoolSection.h"
#include "Sections/MovieSceneParameterSection.h"
#include "Tracks/MovieSceneBoolTrack.h"
#include "Tracks/MovieSceneMaterialParameterCollectionTrack.h"

DEFINE_LOG_CATEGORY_STATIC(LogWuwaSequenceCameraSetup, Log, All);

namespace WuwaSequenceCameraSetup
{
	ERichCurveInterpMode Interp(const FString& Name)
	{
		if (Name == TEXT("Linear")) return RCIM_Linear;
		if (Name == TEXT("Constant")) return RCIM_Constant;
		if (Name == TEXT("None")) return RCIM_None;
		return RCIM_Cubic;
	}

	ERichCurveExtrapolation Extrap(const FString& Name)
	{
		if (Name == TEXT("RCCE_Linear")) return RCCE_Linear;
		if (Name == TEXT("RCCE_Cycle")) return RCCE_Cycle;
		if (Name == TEXT("RCCE_CycleWithOffset")) return RCCE_CycleWithOffset;
		if (Name == TEXT("RCCE_Oscillate")) return RCCE_Oscillate;
		if (Name == TEXT("RCCE_None")) return RCCE_None;
		return RCCE_Constant;
	}

	/** 配方的通道 → UE 双精度 / 单精度关键帧通道（两者 API 一样，用模板）。 */
	template <typename ChannelType, typename ValueType>
	void FillChannel(ChannelType& Channel, const TSharedPtr<FJsonObject>& Def)
	{
		Channel.Reset();
		if (!Def) return;
		const TArray<TSharedPtr<FJsonValue>>& Times = Def->GetArrayField(TEXT("times"));
		const TArray<TSharedPtr<FJsonValue>>& Keys = Def->GetArrayField(TEXT("keys"));
		bool bAuto = false;
		auto Data = Channel.GetData();
		for (int32 Index = 0; Index < Times.Num() && Index < Keys.Num(); ++Index)
		{
			const TSharedPtr<FJsonObject> Key = Keys[Index]->AsObject();
			ValueType Value;
			Value.Value = Key->GetNumberField(TEXT("value"));
			Value.InterpMode = Interp(Key->GetStringField(TEXT("interp")));
			Value.TangentMode = static_cast<ERichCurveTangentMode>(static_cast<int32>(Key->GetNumberField(TEXT("tangentMode"))));
			Value.Tangent.ArriveTangent = Key->GetNumberField(TEXT("arrive"));
			Value.Tangent.LeaveTangent = Key->GetNumberField(TEXT("leave"));
			Value.Tangent.ArriveTangentWeight = Key->GetNumberField(TEXT("arriveWeight"));
			Value.Tangent.LeaveTangentWeight = Key->GetNumberField(TEXT("leaveWeight"));
			Value.Tangent.TangentWeightMode = static_cast<ERichCurveTangentWeightMode>(static_cast<int32>(Key->GetNumberField(TEXT("weightMode"))));
			bAuto |= Value.TangentMode == RCTM_Auto;
			Data.AddKey(FFrameNumber(static_cast<int32>(Times[Index]->AsNumber())), Value);
		}
		double Default = 0.0;
		if (Def->TryGetNumberField(TEXT("default"), Default)) Channel.SetDefault(Default);
		Channel.PreInfinityExtrap = Extrap(Def->GetStringField(TEXT("pre")));
		Channel.PostInfinityExtrap = Extrap(Def->GetStringField(TEXT("post")));
		if (bAuto) Channel.AutoSetTangents();
	}

	UMovieSceneFloatTrack* AddFloatTrack(UMovieScene* MovieScene, const FGuid& Binding, const FName Name, const FString& Path,
		const TSharedPtr<FJsonObject>& Channel)
	{
		UMovieSceneFloatTrack* Track = MovieScene->AddTrack<UMovieSceneFloatTrack>(Binding);
		Track->SetPropertyNameAndPath(Name, Path);
		UMovieSceneFloatSection* Section = Cast<UMovieSceneFloatSection>(Track->CreateNewSection());
		Section->SetRange(TRange<FFrameNumber>::All());
		FillChannel<FMovieSceneFloatChannel, FMovieSceneFloatValue>(Section->GetChannel(), Channel);
		Track->AddSection(*Section);
		return Track;
	}

	/** 反射写枚举属性（按名字，例如 SectionEndBehavior = Deactivate）。 */
	bool SetEnumByName(UObject* Object, const TCHAR* Property, const FString& ValueName)
	{
		FEnumProperty* Enum = FindFProperty<FEnumProperty>(Object->GetClass(), Property);
		if (!Enum) return false;
		const int64 Value = Enum->GetEnum()->GetValueByNameString(ValueName);
		if (Value == INDEX_NONE) return false;
		Enum->GetUnderlyingProperty()->SetIntPropertyValue(Enum->ContainerPtrToValuePtr<void>(Object), Value);
		return true;
	}

	/** 生成物共用的轨道：生成（默认值 + 关键帧）、变换（9 条通道）、可选的挂接（相机 / 角色）。 */
	void AddSpawnTransformAttach(UMovieScene& Scene, const FGuid& Guid, const FJsonObject& Def, const FGuid& RoleGuid, const FGuid& CameraGuid)
	{
		UMovieScene* MovieScene = &Scene;
		// 生成：默认值 + 关键帧（原作在激活前一帧生成）
		{
			UMovieSceneSpawnTrack* Spawn = MovieScene->AddTrack<UMovieSceneSpawnTrack>(Guid);
			UMovieSceneSpawnSection* Section = Cast<UMovieSceneSpawnSection>(Spawn->CreateNewSection());
			Section->SetRange(TRange<FFrameNumber>::All());
			const TSharedPtr<FJsonObject>* SpawnDef = nullptr;
			bool bDefault = true;
			if (Def.TryGetObjectField(TEXT("spawn"), SpawnDef))
			{
				(*SpawnDef)->TryGetBoolField(TEXT("default"), bDefault);
				const TArray<TSharedPtr<FJsonValue>>& Times = (*SpawnDef)->GetArrayField(TEXT("times"));
				const TArray<TSharedPtr<FJsonValue>>& Values = (*SpawnDef)->GetArrayField(TEXT("values"));
				auto Data = Section->GetChannel().GetData();
				for (int32 Index = 0; Index < Times.Num() && Index < Values.Num(); ++Index)
					Data.AddKey(FFrameNumber(static_cast<int32>(Times[Index]->AsNumber())), Values[Index]->AsBool());
			}
			Section->GetChannel().SetDefault(bDefault);
			Spawn->AddSection(*Section);
		}
		{
			UMovieScene3DTransformTrack* Track = MovieScene->AddTrack<UMovieScene3DTransformTrack>(Guid);
			UMovieScene3DTransformSection* Section = Cast<UMovieScene3DTransformSection>(Track->CreateNewSection());
			Section->SetRange(TRange<FFrameNumber>::All());
			TArrayView<FMovieSceneDoubleChannel*> Channels = Section->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
			const TArray<TSharedPtr<FJsonValue>>& Defs = Def.GetArrayField(TEXT("transform"));
			for (int32 Index = 0; Index < 9 && Index < Channels.Num() && Index < Defs.Num(); ++Index)
				FillChannel<FMovieSceneDoubleChannel, FMovieSceneDoubleValue>(*Channels[Index], Defs[Index]->AsObject());
			Track->AddSection(*Section);
		}
		const TSharedPtr<FJsonObject>* Attach = nullptr;
		if (Def.TryGetObjectField(TEXT("attach"), Attach))
		{
			const FString Target = (*Attach)->GetStringField(TEXT("target"));
			UMovieScene3DAttachTrack* Track = MovieScene->AddTrack<UMovieScene3DAttachTrack>(Guid);
			const int32 AttachStart = static_cast<int32>((*Attach)->GetNumberField(TEXT("start")));
			const int32 AttachEnd = static_cast<int32>((*Attach)->GetNumberField(TEXT("end")));
			Track->AddConstraint(FFrameNumber(AttachStart), AttachEnd - AttachStart, FName(*(*Attach)->GetStringField(TEXT("socket"))),
				FName(*(*Attach)->GetStringField(TEXT("component"))), UE::MovieScene::FRelativeObjectBindingID(Target == TEXT("camera") ? CameraGuid : RoleGuid));
		}
	}

	/**
	 * 一个 Niagara 生成物：NiagaraActor 模板（组件 Asset = 重建系统，不自动激活），生成轨道、变换轨道、可选的挂接（相机 / 角色），
	 * 组件子绑定上的 Niagara 系统轨道（激活区段 + 结束行为）。原作序列就是这样排的（NiagaraComponent0 子绑定上放系统轨道）。
	 */
	bool AddNiagaraSpawnable(ULevelSequence& Sequence, const FJsonObject& Def, const FGuid& RoleGuid, const FGuid& CameraGuid)
	{
		UMovieScene* MovieScene = Sequence.GetMovieScene();
		const FString Name = Def.GetStringField(TEXT("name"));
		UClass* ActorClass = FindObject<UClass>(nullptr, TEXT("/Script/Niagara.NiagaraActor"));
		UClass* SystemTrackClass = FindObject<UClass>(nullptr, TEXT("/Script/Niagara.MovieSceneNiagaraSystemTrack"));
		UObject* System = LoadObject<UObject>(nullptr, *Def.GetStringField(TEXT("system")));
		if (!ActorClass || !SystemTrackClass || !System)
		{
			UE_LOG(LogWuwaSequenceCameraSetup, Error, TEXT("%s: Niagara classes or system %s missing"), *Name, *Def.GetStringField(TEXT("system")));
			return false;
		}
		const FName TemplateName = MakeUniqueObjectName(MovieScene, ActorClass, FName(*FString::Printf(TEXT("Fx_%s"), *System->GetName())));
		AActor* Template = NewObject<AActor>(MovieScene, ActorClass, TemplateName, RF_Transactional);
		USceneComponent* Component = Template->GetRootComponent();
		FObjectProperty* Asset = Component ? FindFProperty<FObjectProperty>(Component->GetClass(), TEXT("Asset")) : nullptr;
		if (!Asset)
		{
			UE_LOG(LogWuwaSequenceCameraSetup, Error, TEXT("%s: NiagaraComponent.Asset not found"), *Name);
			return false;
		}
		Asset->SetObjectPropertyValue_InContainer(Component, System);
		Component->bAutoActivate = false;
		const FGuid Guid = MovieScene->AddSpawnable(Name, *Template);
		AddSpawnTransformAttach(*MovieScene, Guid, Def, RoleGuid, CameraGuid);
		// 组件子绑定 + Niagara 系统轨道（激活区段）
		const FGuid ComponentGuid = MovieScene->AddPossessable(Component->GetName(), Component->GetClass());
		if (FMovieScenePossessable* Possessable = MovieScene->FindPossessable(ComponentGuid)) Possessable->SetParent(Guid, MovieScene);
		Sequence.BindPossessableObject(ComponentGuid, *Component, Template);
		const TSharedPtr<FJsonObject>* Activation = nullptr;
		if (Def.TryGetObjectField(TEXT("activation"), Activation))
		{
			UMovieSceneTrack* Track = MovieScene->AddTrack(SystemTrackClass, ComponentGuid);
			UMovieSceneSection* Section = Track ? Track->CreateNewSection() : nullptr;
			if (!Section)
			{
				UE_LOG(LogWuwaSequenceCameraSetup, Error, TEXT("%s: cannot create Niagara system section"), *Name);
				return false;
			}
			const int32 ActiveStart = static_cast<int32>((*Activation)->GetNumberField(TEXT("start")));
			const int32 ActiveEnd = static_cast<int32>((*Activation)->GetNumberField(TEXT("end")));
			Section->SetRange(TRange<FFrameNumber>(FFrameNumber(ActiveStart), FFrameNumber(ActiveEnd)));
			FString EndBehavior;
			if ((*Activation)->TryGetStringField(TEXT("endBehavior"), EndBehavior) && !SetEnumByName(Section, TEXT("SectionEndBehavior"), EndBehavior))
				UE_LOG(LogWuwaSequenceCameraSetup, Warning, TEXT("%s: SectionEndBehavior %s not set"), *Name, *EndBehavior);
			Track->AddSection(*Section);
		}
		UE_LOG(LogWuwaSequenceCameraSetup, Display, TEXT("Niagara spawnable %s -> %s"), *Name, *System->GetName());
		return true;
	}

	/**
	 * 一个静态网格生成物（原作序列里直接摆的 StaticMeshActor，例：嘉贝大招的漩涡云）：模板的网格 / 覆盖材质换成项目资产，
	 * 不投影、不碰撞；生成 / 变换 / 挂接轨道同 Niagara 生成物。
	 */
	bool AddStaticMeshSpawnable(ULevelSequence& Sequence, const FJsonObject& Def, const FGuid& RoleGuid, const FGuid& CameraGuid)
	{
		UMovieScene* MovieScene = Sequence.GetMovieScene();
		const FString Name = Def.GetStringField(TEXT("name"));
		UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Def.GetStringField(TEXT("mesh")));
		if (!Mesh)
		{
			UE_LOG(LogWuwaSequenceCameraSetup, Error, TEXT("%s: static mesh %s missing (run build_sequence_meshes.py)"), *Name, *Def.GetStringField(TEXT("mesh")));
			return false;
		}
		const FName TemplateName = MakeUniqueObjectName(MovieScene, AStaticMeshActor::StaticClass(), FName(*FString::Printf(TEXT("Mesh_%s"), *Mesh->GetName())));
		AStaticMeshActor* Template = NewObject<AStaticMeshActor>(MovieScene, TemplateName, RF_Transactional);
		UStaticMeshComponent* Component = Template->GetStaticMeshComponent();
		Component->SetMobility(EComponentMobility::Movable);
		Component->SetStaticMesh(Mesh);
		Component->SetCastShadow(false);
		Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		const TArray<TSharedPtr<FJsonValue>>& Materials = Def.GetArrayField(TEXT("materials"));
		for (int32 Index = 0; Index < Materials.Num(); ++Index)
		{
			if (Materials[Index]->IsNull()) continue;
			UMaterialInterface* Material = LoadObject<UMaterialInterface>(nullptr, *Materials[Index]->AsString());
			if (!Material)
			{
				UE_LOG(LogWuwaSequenceCameraSetup, Error, TEXT("%s: material %s missing"), *Name, *Materials[Index]->AsString());
				return false;
			}
			Component->SetMaterial(Index, Material);
		}
		const FGuid Guid = MovieScene->AddSpawnable(Name, *Template);
		AddSpawnTransformAttach(*MovieScene, Guid, Def, RoleGuid, CameraGuid);
		UE_LOG(LogWuwaSequenceCameraSetup, Display, TEXT("Static mesh spawnable %s -> %s"), *Name, *Mesh->GetName());
		return true;
	}

	/** 电影后处理（原作 Kuro 后处理体积的项目版）共用资产：MPC 与两个后处理材质都在这个目录，每次按代码重建材质。 */
	const TCHAR* PostProcessFolder = TEXT("/Game/Characters/SequenceCamera/Common");

	/** MPC 的标量参数与默认值（默认值 = 不起作用）。 */
	const TPair<const TCHAR*, float> CinematicParameters[] = {
		{ TEXT("SceneTint"), 1.f }, { TEXT("BurstDissolve"), 0.f },
		{ TEXT("RadialBlurIntensity"), 0.f }, { TEXT("RadialBlurRadius"), 0.08f }, { TEXT("RadialBlurHardness"), 0.5f },
		{ TEXT("RadialBlurCenterX"), 0.5f }, { TEXT("RadialBlurCenterY"), 0.5f }, { TEXT("EffectVignette"), 0.f },
	};

	/** 环境压黑（BeforeDOF：之后才合成的 AfterDOF 半透明特效不受影响）；CustomDepth 在前面的像素 = 角色，保留原色。
	 *  原作 SceneColorTint（白→黑）与 BurstDissolveThreshold（0→1）都把场景压到黑，这里两者相乘（重建选择）。 */
	const TCHAR* EnvironmentCode = TEXT(
		"float Visible = saturate(SceneTint) * (1.0 - saturate(BurstDissolve));\n"
		"float Character = CustomDepth.r <= SceneDepth.r + 2.0 ? 1.0 : 0.0;\n"
		"return lerp(Scene.rgb * Visible, Scene.rgb, Character);\n");

	/** 原作 Kuro 径向模糊（中心 / 半径 / 硬度 / 强度）与特效暗角，色调映射之后整屏处理（含特效）。
	 *  参数语义按名字推断：半径内不模糊，硬度越大过渡越窄，强度 = 朝中心拉丝的长度（重建选择，按视频调：
	 *  每单位强度拉到中心距离的 3%；8% 时 267.4–267.8 的画卷和漩涡被抹成楔形，视频只有轻微拉丝）。 */
	const TCHAR* ScreenCode = TEXT(
		"float2 Vp = GetViewportUV(Parameters);\n"
		"float Aspect = View.ViewSizeAndInvSize.x * View.ViewSizeAndInvSize.w;\n"
		"float2 C = float2(CenterX, CenterY);\n"
		"float2 D = Vp - C;\n"
		"float Dist = length(D * float2(Aspect, 1.0));\n"
		"float Soft = lerp(0.6, 0.02, saturate(Hardness));\n"
		"float Mask = smoothstep(max(Radius, 0.0), max(Radius, 0.0) + Soft, Dist);\n"
		"float Amount = max(Intensity, 0.0) * 0.03 * Mask;\n"
		"float3 Acc = 0;\n"
		"for (int I = 0; I < 16; ++I)\n"
		"{\n"
		"    float T = (float)I / 15.0;\n"
		"    Acc += SceneTextureLookup(ViewportUVToSceneTextureUV(C + D * (1.0 - Amount * T), 14), 14, false).rgb;\n"
		"}\n"
		"float3 Col = Acc / 16.0;\n"
		"float V = saturate(Vignette) * smoothstep(0.35, 1.0, length((Vp - 0.5) * float2(Aspect, 1.0) * 1.1));\n"
		"return Col * (1.0 - V) + Dummy.r * 0.0;\n");

	UMaterialParameterCollection* EnsureCollection()
	{
		const FString Path = FString(PostProcessFolder) / TEXT("MPC_WuwaCinematic");
		UPackage* Package = CreatePackage(*Path);
		Package->FullyLoad();
		UMaterialParameterCollection* Collection = FindObject<UMaterialParameterCollection>(Package, TEXT("MPC_WuwaCinematic"));
		if (!Collection)
		{
			Collection = NewObject<UMaterialParameterCollection>(Package, TEXT("MPC_WuwaCinematic"), RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Collection);
		}
		bool bChanged = false;
		for (const TPair<const TCHAR*, float>& Parameter : CinematicParameters)
		{
			// 已有参数保留（Id 不变，引用它的材质不用重编）
			if (Collection->GetParameterId(Parameter.Key).IsValid()) continue;
			FCollectionScalarParameter Scalar;
			Scalar.ParameterName = Parameter.Key;
			Scalar.DefaultValue = Parameter.Value;
			Collection->ScalarParameters.Add(Scalar);
			bChanged = true;
		}
		if (bChanged)
		{
			Collection->PostEditChange();
			Package->MarkPackageDirty();
		}
		return Collection;
	}

	UMaterialExpressionSceneTexture* AddSceneTexture(UMaterial* Material, ESceneTextureId Id)
	{
		UMaterialExpressionSceneTexture* Node = NewObject<UMaterialExpressionSceneTexture>(Material);
		Node->SceneTextureId = Id;
		Material->GetExpressionCollection().AddExpression(Node);
		return Node;
	}

	UMaterialExpressionCollectionParameter* AddCollectionParameter(UMaterial* Material, UMaterialParameterCollection* Collection, const TCHAR* Name)
	{
		UMaterialExpressionCollectionParameter* Node = NewObject<UMaterialExpressionCollectionParameter>(Material);
		Node->Collection = Collection;
		Node->ParameterName = Name;
		Node->ParameterId = Collection->GetParameterId(Name);
		Material->GetExpressionCollection().AddExpression(Node);
		return Node;
	}

	/** 后处理材质：一个 Custom 节点（HLSL）接自发光；Inputs = {输入名, 节点}。 */
	UMaterial* BuildPostProcessMaterial(const FString& Name, EBlendableLocation Location, const TCHAR* Code,
		const TFunctionRef<TArray<TPair<FName, UMaterialExpression*>>(UMaterial*)>& MakeInputs)
	{
		const FString Path = FString(PostProcessFolder) / Name;
		UPackage* Package = CreatePackage(*Path);
		Package->FullyLoad();
		UMaterial* Material = FindObject<UMaterial>(Package, *Name);
		if (!Material)
		{
			Material = NewObject<UMaterial>(Package, *Name, RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(Material);
		}
		Material->PreEditChange(nullptr);
		Material->GetExpressionCollection().Empty();
		Material->MaterialDomain = MD_PostProcess;
		Material->BlendableLocation = Location;
		UMaterialExpressionCustom* Custom = NewObject<UMaterialExpressionCustom>(Material);
		Custom->OutputType = CMOT_Float3;
		Custom->Code = Code;
		Custom->Inputs.Reset();
		for (const TPair<FName, UMaterialExpression*>& Input : MakeInputs(Material))
		{
			FCustomInput& Slot = Custom->Inputs.AddDefaulted_GetRef();
			Slot.InputName = Input.Key;
			Slot.Input.Expression = Input.Value;
		}
		Material->GetExpressionCollection().AddExpression(Custom);
		Material->GetEditorOnlyData()->EmissiveColor.Expression = Custom;
		Material->PostEditChange();
		Package->MarkPackageDirty();
		return Material;
	}

	/**
	 * 原作 Kuro 后处理体积 → 无界 PostProcessVolume 生成物（混合两个电影后处理材质）+ MPC 轨道（配方 postProcess.parameters）；
	 * 环境压黑要靠 CustomDepth 认出角色：Role 的 CharacterMesh0 子绑定上放 bRenderCustomDepth 布尔轨道（序列结束恢复）。
	 */
	bool AddPostProcess(ULevelSequence& Sequence, const FJsonObject& Def, const FGuid& RoleGuid, TArray<UPackage*>& OutPackages)
	{
		UMovieScene* MovieScene = Sequence.GetMovieScene();
		UMaterialParameterCollection* Collection = EnsureCollection();
		auto Parameter = [Collection](UMaterial* Material, const TCHAR* Name) -> UMaterialExpression* { return AddCollectionParameter(Material, Collection, Name); };
		UMaterial* Environment = BuildPostProcessMaterial(TEXT("M_PP_CinematicEnvironment"), BL_SceneColorBeforeDOF, EnvironmentCode,
			[&](UMaterial* M) -> TArray<TPair<FName, UMaterialExpression*>>
			{
				return { { TEXT("Scene"), AddSceneTexture(M, PPI_PostProcessInput0) }, { TEXT("CustomDepth"), AddSceneTexture(M, PPI_CustomDepth) },
					{ TEXT("SceneDepth"), AddSceneTexture(M, PPI_SceneDepth) }, { TEXT("SceneTint"), Parameter(M, TEXT("SceneTint")) },
					{ TEXT("BurstDissolve"), Parameter(M, TEXT("BurstDissolve")) } };
			});
		UMaterial* Screen = BuildPostProcessMaterial(TEXT("M_PP_CinematicScreen"), BL_SceneColorAfterTonemapping, ScreenCode,
			[&](UMaterial* M) -> TArray<TPair<FName, UMaterialExpression*>>
			{
				return { { TEXT("Dummy"), AddSceneTexture(M, PPI_PostProcessInput0) }, { TEXT("Intensity"), Parameter(M, TEXT("RadialBlurIntensity")) },
					{ TEXT("Radius"), Parameter(M, TEXT("RadialBlurRadius")) }, { TEXT("Hardness"), Parameter(M, TEXT("RadialBlurHardness")) },
					{ TEXT("CenterX"), Parameter(M, TEXT("RadialBlurCenterX")) }, { TEXT("CenterY"), Parameter(M, TEXT("RadialBlurCenterY")) },
					{ TEXT("Vignette"), Parameter(M, TEXT("EffectVignette")) } };
			});
		OutPackages.Append({ Collection->GetOutermost(), Environment->GetOutermost(), Screen->GetOutermost() });

		APostProcessVolume* Template = NewObject<APostProcessVolume>(MovieScene, APostProcessVolume::StaticClass(), TEXT("CinematicPostProcess"), RF_Transactional);
		Template->bUnbound = true;
		Template->Priority = 1000.f;
		Template->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, Environment));
		Template->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.f, Screen));
		const FGuid Guid = MovieScene->AddSpawnable(TEXT("CinematicPostProcess"), *Template);
		{
			UMovieSceneSpawnTrack* Spawn = MovieScene->AddTrack<UMovieSceneSpawnTrack>(Guid);
			UMovieSceneSpawnSection* Section = Cast<UMovieSceneSpawnSection>(Spawn->CreateNewSection());
			Section->SetRange(TRange<FFrameNumber>::All());
			Section->GetChannel().SetDefault(true);
			Spawn->AddSection(*Section);
		}
		{
			UMovieSceneMaterialParameterCollectionTrack* Track = MovieScene->AddTrack<UMovieSceneMaterialParameterCollectionTrack>();
			Track->MPC = Collection;
			UMovieSceneParameterSection* Section = Cast<UMovieSceneParameterSection>(Track->CreateNewSection());
			Section->SetRange(TRange<FFrameNumber>::All());
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Channel : Def.GetObjectField(TEXT("parameters"))->Values)
			{
				Section->AddScalarParameterKey(FName(*Channel.Key), FFrameNumber(0), 0.f);
				for (FScalarParameterNameAndCurve& Curve : Section->GetScalarParameterNamesAndCurves())
				{
					if (Curve.ParameterName == FName(*Channel.Key)) FillChannel<FMovieSceneFloatChannel, FMovieSceneFloatValue>(Curve.ParameterCurve, Channel.Value->AsObject());
				}
			}
			Track->AddSection(*Section);
		}
		bool bCustomDepth = false;
		if (Def.TryGetBoolField(TEXT("roleCustomDepth"), bCustomDepth) && bCustomDepth)
		{
			// Role 的 Mesh 子绑定（按 ACharacter 默认对象的组件名 CharacterMesh0 绑定，运行时随 Role 换成放技能的角色）
			ACharacter* CharacterDefault = ACharacter::StaticClass()->GetDefaultObject<ACharacter>();
			USkeletalMeshComponent* Mesh = CharacterDefault->GetMesh();
			const FGuid MeshGuid = MovieScene->AddPossessable(Mesh->GetName(), Mesh->GetClass());
			if (FMovieScenePossessable* Possessable = MovieScene->FindPossessable(MeshGuid)) Possessable->SetParent(RoleGuid, MovieScene);
			Sequence.BindPossessableObject(MeshGuid, *Mesh, CharacterDefault);
			UMovieSceneBoolTrack* Track = MovieScene->AddTrack<UMovieSceneBoolTrack>(MeshGuid);
			Track->SetPropertyNameAndPath(TEXT("bRenderCustomDepth"), TEXT("bRenderCustomDepth"));
			UMovieSceneBoolSection* Section = Cast<UMovieSceneBoolSection>(Track->CreateNewSection());
			Section->SetRange(TRange<FFrameNumber>::All());
			Section->GetChannel().SetDefault(true);
			Track->AddSection(*Section);
		}
		UE_LOG(LogWuwaSequenceCameraSetup, Display, TEXT("Post process: %d MPC channels, role custom depth %d"), Def.GetObjectField(TEXT("parameters"))->Values.Num(), bCustomDepth);
		return true;
	}

	/** 原作点光源生成物：衰减方式按配方 inverseSquared（原作没改就是平方反比；旧式时用 LightFalloffExponent），强度无单位，变换相对序列原点；强度轨道放在 LightComponent0 子绑定上。 */
	bool AddLightSpawnable(ULevelSequence& Sequence, const FJsonObject& Def)
	{
		UMovieScene* MovieScene = Sequence.GetMovieScene();
		const FString Name = Def.GetStringField(TEXT("name"));
		APointLight* Template = NewObject<APointLight>(MovieScene, APointLight::StaticClass(),
			MakeUniqueObjectName(MovieScene, APointLight::StaticClass(), FName(*FString::Printf(TEXT("Light_%s"), *Name.Replace(TEXT(" "), TEXT("_"))))), RF_Transactional);
		UPointLightComponent* Light = Cast<UPointLightComponent>(Template->GetLightComponent());
		if (!Light) return false;
		Light->SetMobility(EComponentMobility::Movable);
		Light->bUseInverseSquaredFalloff = Def.GetBoolField(TEXT("inverseSquared"));
		Light->IntensityUnits = ELightUnits::Unitless;
		Light->LightFalloffExponent = static_cast<float>(Def.GetNumberField(TEXT("falloffExponent")));
		Light->AttenuationRadius = static_cast<float>(Def.GetNumberField(TEXT("attenuationRadius")));
		const TArray<TSharedPtr<FJsonValue>>& Color = Def.GetArrayField(TEXT("color"));
		Light->LightColor = FLinearColor(static_cast<float>(Color[0]->AsNumber()), static_cast<float>(Color[1]->AsNumber()), static_cast<float>(Color[2]->AsNumber())).ToFColor(true);
		Light->CastShadows = false;
		const TSharedPtr<FJsonObject> Intensity = Def.GetObjectField(TEXT("intensity"));
		Light->Intensity = static_cast<float>(Intensity->GetNumberField(TEXT("default")));
		const FGuid Guid = MovieScene->AddSpawnable(Name, *Template);
		{
			UMovieSceneSpawnTrack* Spawn = MovieScene->AddTrack<UMovieSceneSpawnTrack>(Guid);
			UMovieSceneSpawnSection* Section = Cast<UMovieSceneSpawnSection>(Spawn->CreateNewSection());
			Section->SetRange(TRange<FFrameNumber>::All());
			Section->GetChannel().SetDefault(true);
			Spawn->AddSection(*Section);
		}
		{
			UMovieScene3DTransformTrack* Track = MovieScene->AddTrack<UMovieScene3DTransformTrack>(Guid);
			UMovieScene3DTransformSection* Section = Cast<UMovieScene3DTransformSection>(Track->CreateNewSection());
			Section->SetRange(TRange<FFrameNumber>::All());
			TArrayView<FMovieSceneDoubleChannel*> Channels = Section->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
			const TArray<TSharedPtr<FJsonValue>>& Defs = Def.GetArrayField(TEXT("transform"));
			for (int32 Index = 0; Index < 9 && Index < Channels.Num() && Index < Defs.Num(); ++Index)
				FillChannel<FMovieSceneDoubleChannel, FMovieSceneDoubleValue>(*Channels[Index], Defs[Index]->AsObject());
			Track->AddSection(*Section);
		}
		if (!Intensity->GetArrayField(TEXT("times")).IsEmpty())
		{
			const FGuid ComponentGuid = MovieScene->AddPossessable(Light->GetName(), Light->GetClass());
			if (FMovieScenePossessable* Possessable = MovieScene->FindPossessable(ComponentGuid)) Possessable->SetParent(Guid, MovieScene);
			Sequence.BindPossessableObject(ComponentGuid, *Light, Template);
			AddFloatTrack(MovieScene, ComponentGuid, TEXT("Intensity"), TEXT("Intensity"), Intensity);
		}
		UE_LOG(LogWuwaSequenceCameraSetup, Display, TEXT("Light spawnable %s (radius %.0f, exponent %.1f)"), *Name, Light->AttenuationRadius, Light->LightFalloffExponent);
		return true;
	}

	/** 原作角色 GI 主光颜色 → 项目卡通材质 MPC（MPC_WuwaToon.CharacterLightTint）的颜色轨道（配方 toonLight.vectors：每个参数 RGBA 四条通道）。 */
	bool AddToonLight(UMovieScene& MovieScene, const FJsonObject& Def)
	{
		UMaterialParameterCollection* Collection = LoadObject<UMaterialParameterCollection>(nullptr, *Def.GetStringField(TEXT("collection")));
		if (!Collection)
		{
			UE_LOG(LogWuwaSequenceCameraSetup, Error, TEXT("Toon MPC %s missing"), *Def.GetStringField(TEXT("collection")));
			return false;
		}
		UMovieSceneMaterialParameterCollectionTrack* Track = MovieScene.AddTrack<UMovieSceneMaterialParameterCollectionTrack>();
		Track->MPC = Collection;
		UMovieSceneParameterSection* Section = Cast<UMovieSceneParameterSection>(Track->CreateNewSection());
		Section->SetRange(TRange<FFrameNumber>::All());
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Vector : Def.GetObjectField(TEXT("vectors"))->Values)
		{
			const FName Parameter(*Vector.Key);
			Section->AddColorParameterKey(Parameter, FFrameNumber(0), FLinearColor::White);
			const TArray<TSharedPtr<FJsonValue>>& Channels = Vector.Value->AsArray();
			for (FColorParameterNameAndCurves& Curves : Section->GetColorParameterNamesAndCurves())
			{
				if (Curves.ParameterName != Parameter) continue;
				FMovieSceneFloatChannel* Targets[] = { &Curves.RedCurve, &Curves.GreenCurve, &Curves.BlueCurve, &Curves.AlphaCurve };
				for (int32 Index = 0; Index < 4 && Index < Channels.Num(); ++Index)
					FillChannel<FMovieSceneFloatChannel, FMovieSceneFloatValue>(*Targets[Index], Channels[Index]->AsObject());
			}
		}
		Track->AddSection(*Section);
		UE_LOG(LogWuwaSequenceCameraSetup, Display, TEXT("Toon light: %d MPC_WuwaToon vectors"), Def.GetObjectField(TEXT("vectors"))->Values.Num());
		return true;
	}
}


UWuwaSequenceCameraSetupCommandlet::UWuwaSequenceCameraSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpUsage = TEXT("-run=WuwaSequenceCameraSetup -Recipe=ArtSource/Sequence/<name>.sequence.json [-Apply]");
}

int32 UWuwaSequenceCameraSetupCommandlet::Main(const FString& Params)
{
	using namespace WuwaSequenceCameraSetup;
	FString RecipeArg;
	if (!FParse::Value(*Params, TEXT("Recipe="), RecipeArg, false))
	{
		UE_LOG(LogWuwaSequenceCameraSetup, Error, TEXT("Usage: %s"), *HelpUsage);
		return 1;
	}
	const FString RecipePath = FPaths::IsRelative(RecipeArg) ? FPaths::ProjectDir() / RecipeArg : RecipeArg;
	FString Text;
	TSharedPtr<FJsonObject> Recipe;
	if (!FFileHelper::LoadFileToString(Text, *RecipePath) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Recipe) || !Recipe)
	{
		UE_LOG(LogWuwaSequenceCameraSetup, Error, TEXT("Cannot read %s"), *RecipePath);
		return 1;
	}
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	const FString AssetPath = Recipe->GetStringField(TEXT("asset"));
	const FString AssetName = FPackageName::GetShortName(AssetPath);
	const TSharedPtr<FJsonObject> CameraDef = Recipe->GetObjectField(TEXT("camera"));
	const int32 Start = static_cast<int32>(Recipe->GetNumberField(TEXT("playbackStart")));
	const int32 End = static_cast<int32>(Recipe->GetNumberField(TEXT("playbackEnd")));
	const int32 Rate = static_cast<int32>(Recipe->GetNumberField(TEXT("displayRate")));
	UE_LOG(LogWuwaSequenceCameraSetup, Display, TEXT("%s: %d..%d @ %d fps, camera keys %d, %d notes (not rebuilt)"), *AssetPath, Start, End, Rate,
		CameraDef->GetArrayField(TEXT("transform"))[0]->AsObject()->GetArrayField(TEXT("times")).Num(), Recipe->GetArrayField(TEXT("notes")).Num());
	if (!bApply)
	{
		UE_LOG(LogWuwaSequenceCameraSetup, Display, TEXT("Read-only; pass -Apply to write."));
		return 0;
	}

	// 已有资产先备份，再整份重建
	const FString Filename = FPackageName::LongPackageNameToFilename(AssetPath, FPackageName::GetAssetPackageExtension());
	if (IFileManager::Get().FileExists(*Filename))
	{
		const FString Backup = FPaths::ProjectSavedDir() / TEXT("Backups/SequenceCameraSetup") / FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")) / FPaths::GetCleanFilename(Filename);
		IFileManager::Get().Copy(*Backup, *Filename);
	}
	UPackage* Package = CreatePackage(*AssetPath);
	Package->FullyLoad();
	ULevelSequence* Sequence = nullptr;
	if (UObject* Existing = StaticFindObjectFast(nullptr, Package, FName(*AssetName)))
	{
		// 编辑器里跑（Wuwa.Seq.Setup）时蒙太奇的镜头通知还指着这个对象：原地换一个新的 MovieScene，不改对象身份
		Sequence = Cast<ULevelSequence>(Existing);
		if (Sequence)
		{
			Sequence->Modify();
			if (UMovieScene* Old = Sequence->GetMovieScene())
				for (int32 Index = 0; Index < Old->GetPossessableCount(); ++Index) Sequence->UnbindPossessableObjects(Old->GetPossessable(Index).GetGuid());
		}
		else
		{
			Existing->Rename(nullptr, GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional);
		}
	}
	if (!Sequence) Sequence = NewObject<ULevelSequence>(Package, FName(*AssetName), RF_Public | RF_Standalone | RF_Transactional);
	Sequence->Initialize();
	UMovieScene* MovieScene = Sequence->GetMovieScene();
	MovieScene->SetTickResolutionDirectly(FFrameRate(static_cast<int32>(Recipe->GetNumberField(TEXT("tickResolution"))), 1));
	MovieScene->SetDisplayRate(FFrameRate(Rate, 1));
	MovieScene->SetPlaybackRange(FFrameNumber(Start), End - Start);

	// Role：运行时换成放技能的角色
	const FGuid RoleGuid = MovieScene->AddPossessable(TEXT("Role"), ACharacter::StaticClass());
	MovieScene->TagBinding(TEXT("Role"), UE::MovieScene::FFixedObjectBindingID(RoleGuid, MovieSceneSequenceID::Root));

	// 相机生成物
	ACineCameraActor* Template = NewObject<ACineCameraActor>(MovieScene, ACineCameraActor::StaticClass(), TEXT("SequenceCamera"), RF_Transactional);
	UCineCameraComponent* CameraComponent = Template->GetCineCameraComponent();
	CameraComponent->Filmback.SensorWidth = static_cast<float>(Recipe->GetNumberField(TEXT("sensorWidth")));
	CameraComponent->FocusSettings.FocusMethod = ECameraFocusMethod::Manual;
	const FGuid CameraGuid = MovieScene->AddSpawnable(TEXT("SequenceCamera"), *Template);
	MovieScene->TagBinding(TEXT("SequenceCamera"), UE::MovieScene::FFixedObjectBindingID(CameraGuid, MovieSceneSequenceID::Root));
	{
		UMovieSceneSpawnTrack* Spawn = MovieScene->AddTrack<UMovieSceneSpawnTrack>(CameraGuid);
		UMovieSceneSpawnSection* Section = Cast<UMovieSceneSpawnSection>(Spawn->CreateNewSection());
		Section->SetRange(TRange<FFrameNumber>::All());
		Section->GetChannel().SetDefault(true);
		Spawn->AddSection(*Section);
	}
	{
		UMovieScene3DTransformTrack* Track = MovieScene->AddTrack<UMovieScene3DTransformTrack>(CameraGuid);
		UMovieScene3DTransformSection* Section = Cast<UMovieScene3DTransformSection>(Track->CreateNewSection());
		Section->SetRange(TRange<FFrameNumber>::All());
		TArrayView<FMovieSceneDoubleChannel*> Channels = Section->GetChannelProxy().GetChannels<FMovieSceneDoubleChannel>();
		const TArray<TSharedPtr<FJsonValue>>& Defs = CameraDef->GetArrayField(TEXT("transform"));
		for (int32 Index = 0; Index < 9 && Index < Channels.Num() && Index < Defs.Num(); ++Index)
		{
			FillChannel<FMovieSceneDoubleChannel, FMovieSceneDoubleValue>(*Channels[Index], Defs[Index]->AsObject());
		}
		Track->AddSection(*Section);
	}
	const TSharedPtr<FJsonObject>* Attach = nullptr;
	if (CameraDef->TryGetObjectField(TEXT("attach"), Attach))
	{
		UMovieScene3DAttachTrack* Track = MovieScene->AddTrack<UMovieScene3DAttachTrack>(CameraGuid);
		const int32 AttachStart = static_cast<int32>((*Attach)->GetNumberField(TEXT("start")));
		const int32 AttachEnd = static_cast<int32>((*Attach)->GetNumberField(TEXT("end")));
		Track->AddConstraint(FFrameNumber(AttachStart), AttachEnd - AttachStart, FName(*(*Attach)->GetStringField(TEXT("socket"))),
			FName(*(*Attach)->GetStringField(TEXT("component"))), UE::MovieScene::FRelativeObjectBindingID(RoleGuid));
	}
	// 相机组件：生成物下的可占有子绑定（同 Sequencer 给生成物加组件轨道的做法）
	const FGuid ComponentGuid = MovieScene->AddPossessable(CameraComponent->GetName(), CameraComponent->GetClass());
	if (FMovieScenePossessable* Possessable = MovieScene->FindPossessable(ComponentGuid)) Possessable->SetParent(CameraGuid, MovieScene);
	Sequence->BindPossessableObject(ComponentGuid, *CameraComponent, Template);
	auto Optional = [&CameraDef](const TCHAR* Field) -> TSharedPtr<FJsonObject>
	{
		const TSharedPtr<FJsonObject>* Value = nullptr;
		return CameraDef->TryGetObjectField(Field, Value) ? *Value : nullptr;
	};
	if (TSharedPtr<FJsonObject> Focal = Optional(TEXT("focalLength")))
		AddFloatTrack(MovieScene, ComponentGuid, GET_MEMBER_NAME_CHECKED(UCineCameraComponent, CurrentFocalLength), TEXT("CurrentFocalLength"), Focal);
	if (TSharedPtr<FJsonObject> Aperture = Optional(TEXT("aperture")))
		AddFloatTrack(MovieScene, ComponentGuid, GET_MEMBER_NAME_CHECKED(UCineCameraComponent, CurrentAperture), TEXT("CurrentAperture"), Aperture);
	if (TSharedPtr<FJsonObject> Focus = Optional(TEXT("focusDistance")))
		AddFloatTrack(MovieScene, ComponentGuid, TEXT("ManualFocusDistance"), TEXT("FocusSettings.ManualFocusDistance"), Focus);

	// 序列里直接放的 Niagara（特效管线 Burst01_Sequence 阶段建的重建系统）。Niagara 的 Sequencer 类按反射用，免得本模块依赖 Niagara。
	const TArray<TSharedPtr<FJsonValue>>* NiagaraDefs = nullptr;
	if (Recipe->TryGetArrayField(TEXT("niagara"), NiagaraDefs))
	{
		for (const TSharedPtr<FJsonValue>& Value : *NiagaraDefs)
		{
			if (!AddNiagaraSpawnable(*Sequence, *Value->AsObject(), RoleGuid, CameraGuid)) return 1;
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* MeshDefs = nullptr;
	if (Recipe->TryGetArrayField(TEXT("staticMeshes"), MeshDefs))
	{
		for (const TSharedPtr<FJsonValue>& Value : *MeshDefs)
		{
			if (!AddStaticMeshSpawnable(*Sequence, *Value->AsObject(), RoleGuid, CameraGuid)) return 1;
		}
	}

	// 原作 Kuro 后处理体积（环境压黑、径向模糊、特效暗角）→ 电影后处理生成物 + MPC 轨道
	TArray<UPackage*> ExtraPackages;
	const TSharedPtr<FJsonObject>* PostDef = nullptr;
	if (Recipe->TryGetObjectField(TEXT("postProcess"), PostDef) && !AddPostProcess(*Sequence, **PostDef, RoleGuid, ExtraPackages)) return 1;

	// 原作点光源（地面橙光等）与角色 GI 主光颜色（→ MPC_WuwaToon）
	const TArray<TSharedPtr<FJsonValue>>* LightDefs = nullptr;
	if (Recipe->TryGetArrayField(TEXT("lights"), LightDefs))
	{
		for (const TSharedPtr<FJsonValue>& Value : *LightDefs)
		{
			if (!AddLightSpawnable(*Sequence, *Value->AsObject())) return 1;
		}
	}
	const TSharedPtr<FJsonObject>* ToonDef = nullptr;
	if (Recipe->TryGetObjectField(TEXT("toonLight"), ToonDef) && !AddToonLight(*MovieScene, **ToonDef)) return 1;

	// 镜头切换轨道：只为编辑器里预览（运行时 bDisableCameraCuts，视角由 UWuwaSequenceCameraSubsystem 切）
	if (UMovieSceneCameraCutTrack* Cuts = Cast<UMovieSceneCameraCutTrack>(MovieScene->AddCameraCutTrack(UMovieSceneCameraCutTrack::StaticClass())))
	{
		Cuts->AddNewCameraCut(UE::MovieScene::FRelativeObjectBindingID(CameraGuid), FFrameNumber(Start));
	}

	FAssetRegistryModule::AssetCreated(Sequence);
	Package->MarkPackageDirty();
	ExtraPackages.Add(Package);
	if (!UEditorLoadingAndSavingUtils::SavePackages(ExtraPackages, false))
	{
		UE_LOG(LogWuwaSequenceCameraSetup, Error, TEXT("Saving %s failed (is the editor open?)"), *AssetPath);
		return 1;
	}
	UE_LOG(LogWuwaSequenceCameraSetup, Display, TEXT("Saved %s"), *AssetPath);
	for (const TSharedPtr<FJsonValue>& Note : Recipe->GetArrayField(TEXT("notes"))) UE_LOG(LogWuwaSequenceCameraSetup, Display, TEXT("  not rebuilt: %s"), *Note->AsString());
	return 0;
}

#include "HAL/IConsoleManager.h"

/** 编辑器开着时重建镜头序列：Wuwa.Seq.Setup -Recipe=ArtSource/Sequence/SC_Changli_Burst01.sequence.json -Apply */
static FAutoConsoleCommand GWuwaSeqSetupCommand(
	TEXT("Wuwa.Seq.Setup"),
	TEXT("Runs WuwaSequenceCameraSetup inside the editor, e.g. Wuwa.Seq.Setup -Recipe=ArtSource/Sequence/SC_Changli_Burst01.sequence.json -Apply"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		UWuwaSequenceCameraSetupCommandlet* Commandlet = NewObject<UWuwaSequenceCameraSetupCommandlet>();
		const int32 Result = Commandlet->Main(FString::Join(Args, TEXT(" ")));
		UE_LOG(LogWuwaSequenceCameraSetup, Display, TEXT("Wuwa.Seq.Setup finished with exit code %d"), Result);
	}));
