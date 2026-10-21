#include "Editor/Commandlets/WuwaChangliAttackEffectSetupCommandlet.h"
#include "Editor/WuwaAnimNotifyEditorUtils.h"

#if WITH_EDITOR
#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "AssetCompilingManager.h"
#include "Editor/Effects/WuwaReferenceAssetImport.h"
#include "Editor/Effects/WuwaReferenceNiagaraBuilder.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "MaterialShared.h"
#include "RHI.h"
#include "ShaderCompiler.h"
#include "Game/Animation/Notifies/WuwaAnimNotify_PlayEffect.h"
#include "Game/Animation/Notifies/WuwaAnimNotifyState_Ghost.h"
#include "Game/EffectModel/Models/WuwaEffectModelDecal.h"
#include "Game/EffectModel/Models/WuwaCharacterMaterialControllerData.h"
#include "Game/EffectModel/Models/WuwaEffectModelGhost.h"
#include "Game/EffectModel/Models/WuwaEffectModelMaterialController.h"
#include "Game/EffectModel/Models/WuwaEffectModelGroup.h"
#include "Game/EffectModel/Models/WuwaEffectModelLight.h"
#include "Game/EffectModel/Models/WuwaEffectModelPostProcess.h"
#include "Game/EffectModel/Models/WuwaEffectModelNiagara.h"
#include "Game/EffectModel/Models/WuwaEffectModelSkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Engine/SkeletalMesh.h"
#include "HAL/FileManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "NiagaraSystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaAttackEffectSetup, Log, All);

UWuwaChangliAttackEffectSetupCommandlet::UWuwaChangliAttackEffectSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpDescription = TEXT("Audits a Changli attack's effects; -Apply builds EffectModel assets from AttackNN.json and installs Play Effect notifies on AM_AttackNN.");
	HelpUsage = TEXT("-run=WuwaChangliAttackEffectSetup [-Character=changli|jiabei|changli_legacy] [-Attack=Attack01] [-Apply]");
}

#if WITH_EDITOR
namespace WuwaAttackEffectSetup
{
	constexpr const TCHAR* SystemPrefix = TEXT("NS_Reference_");
	/** 丝带 SpawnPerUnit → 每秒出生数时假定的武器速度（厘米 / 秒，刀尖挥动的量级）；重建选择，原作按实际移动距离出生。 */
	constexpr float RibbonReferenceSpeed = 1500.f;
	// 旧刀光工具生成的通知前缀；它们的通知类已删除，留在蒙太奇里只是空事件。
	constexpr const TCHAR* LegacyNotifyPrefix = TEXT("WuwaSlashFx.Auto.");
	constexpr const TCHAR* TrackName = TEXT("Wuwa Effects");
	// 径向模糊后处理材质：本项目自己的重建（不含原作数据），放在公共目录里可以提交。
	constexpr const TCHAR* RadialBlurMaterialPath = TEXT("/Game/Effects/Common/PostProcess/M_PP_RadialBlur");
	// 原作各组的 StartTime；起始段结束后停止发射，已有粒子自然播完。
	constexpr float StartTime = 2.f;

	/**
	 * 一段攻击的路径约定；-Character=changli|jiabei|changli_legacy（默认 changli），-Attack=段名（默认 Attack01）。
	 * 配方都由 ArtSource/JiabeiAttack 的管线生成（characters.py），蒙太奇取配方的 montage 字段：
	 *  嘉贝莉娜：配方 ArtSource/JiabeiAttack/<段名>.json，资产在 /Game/Effects/Jiabei/Reference；
	 *  长离：配方 ArtSource/JiabeiAttack/Changli/<段名>.json，资产在 /Game/Effects/Changli/Reference。
	 * changli_legacy：长离早期管线 ArtSource/ChangliSlash，资产在 /Game/Effects/ChangliSlash/Reference，蒙太奇 AM_<段名>。
	 */
	struct FAttack
	{
		FString Character = TEXT("changli");
		FString Name;
		/** 配方里的 montage（长包名，不含 .对象名）；为空时用长离的命名规则。 */
		FString RecipeMontage;

		bool IsJiabei() const { return Character == TEXT("jiabei"); }
		bool IsLegacy() const { return Character == TEXT("changli_legacy"); }
		bool IsKnown() const { return IsJiabei() || IsLegacy() || Character == TEXT("changli"); }
		FString Title() const { return IsJiabei() ? TEXT("Jiabei") : TEXT("Changli"); }
		FString ReferenceRoot() const
		{
			return IsLegacy() ? TEXT("/Game/Effects/ChangliSlash/Reference") : TEXT("/Game/Effects/") + Title() + TEXT("/Reference");
		}
		FString NiagaraFolder() const { return ReferenceRoot() / TEXT("Niagara"); }
		FString MaterialFolder() const { return ReferenceRoot() / TEXT("Materials"); }
		FString TextureFolder() const { return ReferenceRoot() / TEXT("Textures"); }
		FString MeshFolder() const { return ReferenceRoot() / TEXT("Mesh"); }
		FString MontagePath() const
		{
			if (!RecipeMontage.IsEmpty()) return RecipeMontage + TEXT(".") + FPackageName::GetLongPackageAssetName(RecipeMontage);
			return FString::Printf(TEXT("/Game/Characters/Role/changli/AnimMontage/AM_%s.AM_%s"), *Name, *Name);
		}
		FString MontageLabel() const { return FPackageName::GetLongPackageAssetName(FPackageName::ObjectPathToPackageName(MontagePath())); }
		FString ModelFolder() const { return TEXT("/Game/Effects/Models/") + Title() / Name; }
		FString NotifyPrefix() const
		{
			return IsLegacy() ? FString::Printf(TEXT("WuwaFx.Auto.%s."), *Name) : FString::Printf(TEXT("WuwaFx.Auto.%s.%s."), *Title(), *Name);
		}
		/** 同一个蒙太奇上另一条长离管线留下的通知前缀（新旧管线互相替换）；嘉贝莉娜没有。 */
		FString OtherPipelinePrefix() const
		{
			if (IsJiabei()) return FString();
			return IsLegacy() ? FString::Printf(TEXT("WuwaFx.Auto.Changli.%s."), *Name) : FString::Printf(TEXT("WuwaFx.Auto.%s."), *Name);
		}
		/** 配方里 file / Geometry 路径的根目录（贴图、几何两个角色共用）。 */
		FString SourceRoot() const { return FPaths::ProjectDir() / (IsLegacy() ? TEXT("ArtSource/ChangliSlash") : TEXT("ArtSource/JiabeiAttack")); }
		FString RecipeFolder() const { return Character == TEXT("changli") ? SourceRoot() / TEXT("Changli") : SourceRoot(); }
		FString RecipeFile() const { return RecipeFolder() / (Name + TEXT(".json")); }
		FString TuningFile() const { return RecipeFolder() / (Name + TEXT("Tuning.json")); }
	};

	struct FYawKey
	{
		float Time = 0.f;
		float Value = 0.f;
		float ArriveTangent = 0.f;
		float LeaveTangent = 0.f;
	};

	struct FPlacement
	{
		FString System;
		/** 非 Niagara 子项的放置（配方 decals / postProcesses / lights 里的名字）；为空时是 Niagara 系统放置。 */
		FString Child;
		/** 同一个系统 / 子项共用一个 Model 和 Group。 */
		const FString& Key() const { return Child.IsEmpty() ? System : Child; }
		FVector Location = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;
		FVector Scale = FVector::OneVector;
		float Delay = 0.f;
		/** 是否挂在插槽上跟随角色。原作 AnimNotifyEffect 的 Attached 默认为 true（第三段才显式写 false）。 */
		bool bAttached = true;
		/** 原作通知的 SocketName；没写时用 Root。 */
		FName Socket = TEXT("Root");
		TArray<FYawKey> YawKeys;
	};

	TSharedPtr<FJsonObject> LoadJson(const FString& File)
	{
		FString Text;
		TSharedPtr<FJsonObject> Root;
		if (!FFileHelper::LoadFileToString(Text, *File)
			|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root)
		{
			UE_LOG(LogWuwaAttackEffectSetup, Error, TEXT("Cannot read %s"), *File);
			return nullptr;
		}
		return Root;
	}

	FVector2D ReadRange(const FJsonObject& Object, const TCHAR* Field)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object.TryGetArrayField(Field, Values) || Values->Num() != 2) return FVector2D(NAN, NAN);
		return FVector2D((*Values)[0]->AsNumber(), (*Values)[1]->AsNumber());
	}

	FVector ReadVector(const TSharedPtr<FJsonObject>& Object, const TCHAR* Field, const FVector& Fallback)
	{
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		if (!Object->TryGetArrayField(Field, Values) || Values->Num() != 3) return Fallback;
		return FVector((*Values)[0]->AsNumber(), (*Values)[1]->AsNumber(), (*Values)[2]->AsNumber());
	}

	bool ReadRecipe(const FAttack& Attack, TArray<FPlacement>& OutPlacements, float& OutNotifySeconds)
	{
		const FString File = Attack.RecipeFile();
		const TSharedPtr<FJsonObject> Root = LoadJson(File);
		if (!Root) return false;
		double NotifySeconds = 0;
		const TArray<TSharedPtr<FJsonValue>>* Placements = nullptr;
		// solo 配方（ArtSource/JiabeiAttack/solo.py）：每层一个系统，只建资产，不放通知
		bool bSolo = false;
		Root->TryGetBoolField(TEXT("solo"), bSolo);
		if (!Root->TryGetNumberField(TEXT("notifyAuthoredSeconds"), NotifySeconds)
			|| !Root->TryGetArrayField(TEXT("placements"), Placements) || (Placements->IsEmpty() && !bSolo))
		{
			UE_LOG(LogWuwaAttackEffectSetup, Error, TEXT("%s has no placements or notifyAuthoredSeconds."), *File);
			return false;
		}
		OutNotifySeconds = static_cast<float>(NotifySeconds);
		for (const TSharedPtr<FJsonValue>& Value : *Placements)
		{
			const TSharedPtr<FJsonObject> Object = Value->AsObject();
			if (!Object) return false;
			FPlacement& P = OutPlacements.AddDefaulted_GetRef();
			if (!Object->TryGetStringField(TEXT("decal"), P.Child) && !Object->TryGetStringField(TEXT("model"), P.Child))
				P.System = Object->GetStringField(TEXT("system"));
			P.Location = ReadVector(Object, TEXT("location"), FVector::ZeroVector);
			// 旧配方按 FRotator(Pitch, Yaw, Roll) 记录。
			const FVector Euler = ReadVector(Object, TEXT("rotation"), FVector::ZeroVector);
			P.Rotation = FRotator(Euler.X, Euler.Y, Euler.Z);
			P.Scale = ReadVector(Object, TEXT("scale"), FVector::OneVector);
			P.Delay = static_cast<float>(Object->GetNumberField(TEXT("delay")));
			Object->TryGetBoolField(TEXT("attached"), P.bAttached);
			FString Socket;
			if (Object->TryGetStringField(TEXT("socket"), Socket)) P.Socket = FName(*Socket);
			const TArray<TSharedPtr<FJsonValue>>* Keys = nullptr;
			if (Object->TryGetArrayField(TEXT("localYawKeys"), Keys))
			{
				for (const TSharedPtr<FJsonValue>& KeyValue : *Keys)
				{
					const TSharedPtr<FJsonObject> Key = KeyValue->AsObject();
					P.YawKeys.Add({static_cast<float>(Key->GetNumberField(TEXT("time"))), static_cast<float>(Key->GetNumberField(TEXT("value"))),
						static_cast<float>(Key->GetNumberField(TEXT("arriveTangent"))), static_cast<float>(Key->GetNumberField(TEXT("leaveTangent")))});
				}
			}
		}
		return true;
	}

	/** 人工调整（AttackNNTuning.json）：贴近游戏截图的放大与调色，不是原作数据。文件不存在则不调整。 */
	struct FMaterialTuning
	{
		FLinearColor Tint = FLinearColor::White;
		float Intensity = 1.f;
		/** 贴图 RGBA 对形状遮罩的权重；不设置则保持重建材质原来的常量。 */
		TOptional<FLinearColor> SilhouetteWeights;
	};

	/** Sprite 层的发射参数；区间为 [最小, 最大]。 */
	struct FSpriteTuning
	{
		int32 Count = 1;
		FVector2D Lifetime;
		FVector2D SizeMin;
		FVector2D SizeMax;
		FVector2D Radius;
		FVector2D Speed;
		float Drag = 0.f;
		bool bSizeFollowsScale = false;
		/** 发射弧：起始角与跨度（度，发射器局部 XY 平面）；默认整圈。 */
		FVector2D Arc = FVector2D(0, 360);
		float Height = 8.f;
		bool bRadialVelocity = false;
		/** 以下来自原作出生脚本 / 渲染器（嘉贝莉娜配方的 emission）：速度模式 0 环 1 圆锥 2 球面，圆锥轴与半角，位置偏移，朝向 / 对齐 / 锚点。 */
		int32 VelocityMode = 0;
		FVector ConeDirection = FVector(1, 0, 0);
		float ConeAngle = 0.f;
		FVector PositionOffset = FVector::ZeroVector;
		bool bFaceCamera = false;
		bool bVelocityAligned = true;
		FVector2D Pivot = FVector2D(0.5, 1.0);
		/** 原作出生脚本的自定义对齐 / 朝向向量（零向量 = 没有）与 Sprite 旋转（度）。 */
		FVector CustomAlignment = FVector::ZeroVector;
		FVector SpriteFacing = FVector::ZeroVector;
		float SpriteRotation = 0.f;
		/** 配方 emission.table：逐粒子参数表（原作按发射器时间生成的层），非空时取代上面的随机分布。 */
		TArray<WuwaReferenceNiagaraBuilder::FTableParticle> Table;
	};

	struct FTuning
	{
		TMap<FString, float> ModelScale;
		TMap<FString, FMaterialTuning> Materials;
		/** 系统名 → 层名 → 参数。列出的系统会整个重建，所以必须覆盖它的每一层。 */
		TMap<FString, TMap<FString, FSpriteTuning>> SpriteLayers;
	};

	FSpriteTuning ReadSpriteTuning(const FJsonObject& Object)
	{
		FSpriteTuning Sprite;
		Sprite.Count = static_cast<int32>(Object.GetNumberField(TEXT("count")));
		Sprite.Lifetime = ReadRange(Object, TEXT("lifetime"));
		Sprite.SizeMin = ReadRange(Object, TEXT("sizeMin"));
		Sprite.SizeMax = ReadRange(Object, TEXT("sizeMax"));
		Sprite.Radius = ReadRange(Object, TEXT("radius"));
		Sprite.Speed = ReadRange(Object, TEXT("speed"));
		Sprite.Drag = static_cast<float>(Object.GetNumberField(TEXT("drag")));
		Object.TryGetBoolField(TEXT("sizeFollowsScale"), Sprite.bSizeFollowsScale);
		Object.TryGetBoolField(TEXT("radialVelocity"), Sprite.bRadialVelocity);
		Object.TryGetNumberField(TEXT("height"), Sprite.Height);
		if (Object.HasField(TEXT("arc"))) Sprite.Arc = ReadRange(Object, TEXT("arc"));
		double Number = 0.0;
		if (Object.TryGetNumberField(TEXT("velocityMode"), Number)) Sprite.VelocityMode = static_cast<int32>(Number);
		if (Object.TryGetNumberField(TEXT("coneAngle"), Number)) Sprite.ConeAngle = static_cast<float>(Number);
		const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
		auto Vec3 = [&](const TCHAR* Name, FVector& Out) { if (Object.TryGetArrayField(Name, Values) && Values->Num() == 3) Out = FVector((*Values)[0]->AsNumber(), (*Values)[1]->AsNumber(), (*Values)[2]->AsNumber()); };
		Vec3(TEXT("coneDirection"), Sprite.ConeDirection);
		Vec3(TEXT("positionOffset"), Sprite.PositionOffset);
		Object.TryGetBoolField(TEXT("faceCamera"), Sprite.bFaceCamera);
		Object.TryGetBoolField(TEXT("velocityAligned"), Sprite.bVelocityAligned);
		if (Object.HasField(TEXT("pivot"))) Sprite.Pivot = ReadRange(Object, TEXT("pivot"));
		Vec3(TEXT("customAlignment"), Sprite.CustomAlignment);
		Vec3(TEXT("spriteFacing"), Sprite.SpriteFacing);
		if (Object.TryGetNumberField(TEXT("spriteRotation"), Number)) Sprite.SpriteRotation = static_cast<float>(Number);
		const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
		if (Object.TryGetArrayField(TEXT("table"), Rows))
		{
			auto Numbers = [](const FJsonObject& Row, const TCHAR* Name, int32 Count)
			{
				TArray<double> Out;
				const TArray<TSharedPtr<FJsonValue>>* Array = nullptr;
				if (Row.TryGetArrayField(Name, Array) && Array->Num() == Count)
					for (const TSharedPtr<FJsonValue>& V : *Array) Out.Add(V->AsNumber());
				return Out;
			};
			for (const TSharedPtr<FJsonValue>& Value : *Rows)
			{
				const FJsonObject& Row = *Value->AsObject();
				const TArray<double> Size = Numbers(Row, TEXT("size"), 2), Position = Numbers(Row, TEXT("position"), 3);
				const TArray<double> Velocity = Numbers(Row, TEXT("velocity"), 3), Color = Numbers(Row, TEXT("color"), 4);
				if (Size.Num() != 2 || Position.Num() != 3 || Velocity.Num() != 3 || Color.Num() != 4) continue;
				WuwaReferenceNiagaraBuilder::FTableParticle& P = Sprite.Table.AddDefaulted_GetRef();
				P.Delay = static_cast<float>(Row.GetNumberField(TEXT("delay")));
				P.Lifetime = static_cast<float>(Row.GetNumberField(TEXT("lifetime")));
				P.Size = FVector2D(Size[0], Size[1]);
				P.Position = FVector(Position[0], Position[1], Position[2]);
				P.Velocity = FVector(Velocity[0], Velocity[1], Velocity[2]);
				P.Color = FLinearColor(Color[0], Color[1], Color[2], Color[3]);
			}
		}
		return Sprite;
	}

	bool ReadTuning(const FAttack& Attack, FTuning& Out)
	{
		const FString File = Attack.TuningFile();
		if (!IFileManager::Get().FileExists(*File)) return true;
		const TSharedPtr<FJsonObject> Root = LoadJson(File);
		if (!Root) return false;
		const TSharedPtr<FJsonObject>* Scales = nullptr;
		if (Root->TryGetObjectField(TEXT("modelScale"), Scales))
		{
			for (const auto& Pair : (*Scales)->Values) Out.ModelScale.Add(Pair.Key, static_cast<float>(Pair.Value->AsNumber()));
		}
		const TSharedPtr<FJsonObject>* Materials = nullptr;
		if (Root->TryGetObjectField(TEXT("materials"), Materials))
		{
			for (const auto& Pair : (*Materials)->Values)
			{
				const TSharedPtr<FJsonObject> Object = Pair.Value->AsObject();
				if (!Object) return false;
				FMaterialTuning& Tuning = Out.Materials.Add(Pair.Key);
				const FVector Tint = ReadVector(Object, TEXT("tint"), FVector::OneVector);
				Tuning.Tint = FLinearColor(Tint.X, Tint.Y, Tint.Z);
				Tuning.Intensity = static_cast<float>(Object->GetNumberField(TEXT("intensity")));
				const TArray<TSharedPtr<FJsonValue>>* Weights = nullptr;
				if (Object->TryGetArrayField(TEXT("silhouetteWeights"), Weights))
				{
					if (Weights->Num() != 4) return false;
					Tuning.SilhouetteWeights = FLinearColor((*Weights)[0]->AsNumber(), (*Weights)[1]->AsNumber(), (*Weights)[2]->AsNumber(), (*Weights)[3]->AsNumber());
				}
			}
		}
		const TSharedPtr<FJsonObject>* Systems = nullptr;
		if (Root->TryGetObjectField(TEXT("spriteLayers"), Systems))
		{
			for (const auto& SystemPair : (*Systems)->Values)
			{
				const TSharedPtr<FJsonObject> Layers = SystemPair.Value->AsObject();
				if (!Layers) return false;
				TMap<FString, FSpriteTuning>& OutLayers = Out.SpriteLayers.Add(SystemPair.Key);
				for (const auto& LayerPair : Layers->Values)
				{
					const TSharedPtr<FJsonObject> Object = LayerPair.Value->AsObject();
					if (!Object) return false;
					OutLayers.Add(LayerPair.Key, ReadSpriteTuning(*Object));
				}
			}
		}
		return true;
	}

	/**
	 * 把重建 shader 里 silhouette 的通道权重常量换成 SilhouetteWeights 向量参数（已换过则只改默认值）。
	 * 原作的 Base_AlphaSwitch 只有数值、没有主材质图，dot 权重本身就是重建写法。
	 */
	bool ApplySilhouetteWeights(UMaterial& Material, const FLinearColor& Weights)
	{
		static const FString ParameterName = TEXT("SilhouetteWeights");
		static const FString Prefix = TEXT("float silhouette=saturate(dot(base,");
		UMaterialExpressionVectorParameter* Parameter = nullptr;
		UMaterialExpressionCustom* Code = nullptr;
		for (UMaterialExpression* Expression : Material.GetExpressions())
		{
			if (auto* Vector = Cast<UMaterialExpressionVectorParameter>(Expression); Vector && Vector->ParameterName == *ParameterName) Parameter = Vector;
			if (auto* Custom = Cast<UMaterialExpressionCustom>(Expression); Custom && Custom->Code.Contains(Prefix)) Code = Custom;
		}
		if (!Code) return false;
		if (!Parameter)
		{
			const int32 Start = Code->Code.Find(Prefix) + Prefix.Len();
			const int32 End = Code->Code.Find(TEXT(")"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start);
			if (End == INDEX_NONE || !Code->Code.Mid(Start, End - Start).StartsWith(TEXT("float4("))) return false;
			// End 指向 float4(...) 的右括号。
			Code->Code = Code->Code.Left(Start) + ParameterName + Code->Code.RightChop(End + 1);
			Parameter = NewObject<UMaterialExpressionVectorParameter>(&Material, NAME_None, RF_Transactional);
			Parameter->ParameterName = *ParameterName;
			Parameter->MaterialExpressionEditorX = -750;
			Parameter->MaterialExpressionEditorY = -200;
			Material.GetExpressionCollection().AddExpression(Parameter);
			FCustomInput& Input = Code->Inputs.AddDefaulted_GetRef();
			Input.InputName = *ParameterName;
			// 输出 5 是 RGBA（float4），与原来的 float4 常量同型。
			Input.Input.Connect(5, Parameter);
		}
		Parameter->DefaultValue = Weights;
		return true;
	}

	/** 在重建材质的自发光输出上乘一个 Tint 参数（已存在则只改默认值），并设置 Intensity 默认值。 */
	UMaterial* ApplyMaterialTuning(const FAttack& Attack, const FString& Name, const FMaterialTuning& Tuning)
	{
		UMaterial* Material = LoadObject<UMaterial>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), *Attack.MaterialFolder(), *Name, *Name));
		if (!Material)
		{
			UE_LOG(LogWuwaAttackEffectSetup, Error, TEXT("Missing material %s"), *Name);
			return nullptr;
		}
		Material->PreEditChange(nullptr);
		UMaterialExpressionVectorParameter* Tint = nullptr;
		UMaterialExpressionScalarParameter* Intensity = nullptr;
		for (UMaterialExpression* Expression : Material->GetExpressions())
		{
			if (auto* Vector = Cast<UMaterialExpressionVectorParameter>(Expression); Vector && Vector->ParameterName == TEXT("Tint")) Tint = Vector;
			if (auto* Scalar = Cast<UMaterialExpressionScalarParameter>(Expression); Scalar && Scalar->ParameterName == TEXT("Intensity")) Intensity = Scalar;
		}
		if (!Tint)
		{
			FColorMaterialInput& Emissive = Material->GetEditorOnlyData()->EmissiveColor;
			if (!Emissive.Expression)
			{
				UE_LOG(LogWuwaAttackEffectSetup, Error, TEXT("%s has no emissive input to tint."), *Name);
				return nullptr;
			}
			Tint = NewObject<UMaterialExpressionVectorParameter>(Material, NAME_None, RF_Transactional);
			Tint->ParameterName = TEXT("Tint");
			Tint->MaterialExpressionEditorX = -250;
			Tint->MaterialExpressionEditorY = -200;
			Material->GetExpressionCollection().AddExpression(Tint);
			auto* Multiply = NewObject<UMaterialExpressionMultiply>(Material, NAME_None, RF_Transactional);
			Multiply->MaterialExpressionEditorX = -120;
			Multiply->MaterialExpressionEditorY = -120;
			Material->GetExpressionCollection().AddExpression(Multiply);
			Multiply->A.Connect(Emissive.OutputIndex, Emissive.Expression);
			Multiply->B.Connect(0, Tint);
			Emissive.Connect(0, Multiply);
		}
		Tint->DefaultValue = Tuning.Tint;
		if (Tuning.SilhouetteWeights && !ApplySilhouetteWeights(*Material, *Tuning.SilhouetteWeights))
		{
			UE_LOG(LogWuwaAttackEffectSetup, Error, TEXT("%s: cannot find the silhouette expression in the reconstructed shader."), *Name);
			return nullptr;
		}
		if (Intensity) Intensity->DefaultValue = Tuning.Intensity;
		Material->PostEditChange();
		return Material;
	}

	/** 配方里某个系统的定义；没有则报错。 */
	TSharedPtr<FJsonObject> FindSystemDef(const FAttack& Attack, const TSharedPtr<FJsonObject>& Recipe, const FString& SystemName)
	{
		for (const TSharedPtr<FJsonValue>& Value : Recipe->GetArrayField(TEXT("systems")))
		{
			if (Value->AsObject()->GetStringField(TEXT("name")) == SystemName) return Value->AsObject();
		}
		UE_LOG(LogWuwaAttackEffectSetup, Error, TEXT("%s has no recipe entry for %s"), *Attack.RecipeFile(), *SystemName);
		return nullptr;
	}

	/**
	 * 整个重建一个参考 Niagara 系统（不存在则新建）。
	 * bGenerate（配方 generate=true）：Mesh 层按配方导入网格，带 shader 的层重建材质；否则只允许 Sprite 层并沿用已有材质。
	 * Sprite 层的发射参数都来自 Tuning 的 spriteLayers。
	 */
	UNiagaraSystem* RebuildSystem(const FAttack& Attack, const FJsonObject& SystemDef, const TMap<FString, FSpriteTuning>* SpriteLayers,
		bool bGenerate, const TMap<FString, UTexture2D*>& Textures, TArray<UObject*>& Changed)
	{
		const FString SystemName = SystemDef.GetStringField(TEXT("name"));
		const FString PackageName = Attack.NiagaraFolder() / SystemName;
		UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *(PackageName + TEXT(".") + SystemName), nullptr, LOAD_NoWarn);
		if (!System)
		{
			if (FPackageName::DoesPackageExist(PackageName))
			{
				UE_LOG(LogWuwaAttackEffectSetup, Error, TEXT("%s exists but is not a Niagara system."), *PackageName);
				return nullptr;
			}
			System = NewObject<UNiagaraSystem>(CreatePackage(*PackageName), *SystemName, RF_Public | RF_Standalone | RF_Transactional);
			FAssetRegistryModule::AssetCreated(System);
		}
		TArray<WuwaReferenceNiagaraBuilder::FEmitterSpec> Specs;
		for (const TSharedPtr<FJsonValue>& Value : SystemDef.GetArrayField(TEXT("layers")))
		{
			const TSharedPtr<FJsonObject> Layer = Value->AsObject();
			const FString LayerName = Layer->GetStringField(TEXT("name"));
			const bool bSprite = Layer->GetBoolField(TEXT("sprite"));
			// Tuning 里的人工参数优先，没有则用配方层自带的重建默认值（emission）。
			const FSpriteTuning* Sprite = SpriteLayers ? SpriteLayers->Find(LayerName) : nullptr;
			FSpriteTuning RecipeEmission;
			const TSharedPtr<FJsonObject>* Emission = nullptr;
			if (!Sprite && bSprite && Layer->TryGetObjectField(TEXT("emission"), Emission))
			{
				RecipeEmission = ReadSpriteTuning(**Emission);
				Sprite = &RecipeEmission;
			}
			// 重建会替换系统里全部发射器：Sprite 层必须都给了发射参数；非生成系统不处理 Mesh 层。
			if ((bSprite && !Sprite) || (!bSprite && !bGenerate))
			{
				UE_LOG(LogWuwaAttackEffectSetup, Error, TEXT("%s/%s has no emission parameters to rebuild with; refusing to rebuild the system."), *SystemName, *LayerName);
				return nullptr;
			}
			WuwaReferenceNiagaraBuilder::FEmitterSpec& Spec = Specs.AddDefaulted_GetRef();
			Spec.Name = FName(*LayerName);
			Spec.SortOrder = static_cast<int32>(Layer->GetNumberField(TEXT("sortOrder")));
			Spec.PivotOffset = ReadVector(Layer, TEXT("pivot"), FVector::ZeroVector);
			// 各层用不同种子，避免两层粒子落在同样的角度上。
			Spec.RandomSeed += 101 * Specs.Num();
			// 原作发射器推后爆发（发射器状态循环延迟 / 爆发时刻，prepare_attack 跑系统脚本得出）
			double SpawnTime = 0.0;
			if (Layer->TryGetNumberField(TEXT("spawnTime"), SpawnTime)) Spec.SpawnTime = static_cast<float>(SpawnTime);
			const FString MaterialName = TEXT("M_") + Layer->GetStringField(TEXT("id"));
			if (bGenerate && Layer->HasField(TEXT("shader")))
			{
				UMaterial* Material = WuwaReferenceAssetImport::BuildMaterial(Attack.MaterialFolder(), *Layer, Textures);
				if (!Material) return nullptr;
				Changed.AddUnique(Material);
				Spec.Material = Material;
			}
			else
			{
				Spec.Material = LoadObject<UMaterialInterface>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), *Attack.MaterialFolder(), *MaterialName, *MaterialName));
			}
			if (!Spec.Material)
			{
				UE_LOG(LogWuwaAttackEffectSetup, Error, TEXT("Missing material %s"), *MaterialName);
				return nullptr;
			}
			const TSharedPtr<FJsonObject>* Ribbon = nullptr;
			if (Layer->TryGetObjectField(TEXT("ribbon"), Ribbon))
			{
				// 丝带（武器拖尾）：原作 SpawnPerUnit（每厘米出生数）按典型刀尖速度换成每秒出生数（重建选择，见配方说明）
				Spec.bRibbon = true;
				Spec.Lifetime = Spec.LifetimeMax = static_cast<float>((*Ribbon)->GetNumberField(TEXT("lifetime")));
				for (const TSharedPtr<FJsonValue>& W : (*Ribbon)->GetArrayField(TEXT("widths"))) Spec.RibbonWidths.Add(static_cast<float>(W->AsNumber()));
				double PerUnit = 0.0, Rate = 0.0, Tiling = 1.0;
				if ((*Ribbon)->TryGetNumberField(TEXT("spawnPerUnit"), PerUnit) && PerUnit > 0.0) Spec.RibbonSpawnRate = static_cast<float>(PerUnit * RibbonReferenceSpeed);
				else if ((*Ribbon)->TryGetNumberField(TEXT("spawnRate"), Rate) && Rate > 0.0) Spec.RibbonSpawnRate = static_cast<float>(Rate);
				if ((*Ribbon)->TryGetNumberField(TEXT("uvTilingLength"), Tiling) && Tiling > 0.0) Spec.RibbonUVTilingLength = static_cast<float>(Tiling);
				continue;
			}
			if (!bSprite)
			{
				bool bCreated = false;
				Spec.Mesh = WuwaReferenceAssetImport::FindOrImportMesh(Attack.MeshFolder(), Attack.SourceRoot(), Layer->GetStringField(TEXT("mesh")), bCreated);
				if (!Spec.Mesh) return nullptr;
				if (bCreated) Changed.AddUnique(Spec.Mesh);
				Spec.Lifetime = Spec.LifetimeMax = static_cast<float>(Layer->GetNumberField(TEXT("lifetime")));
				Spec.MeshPosition = ReadVector(Layer, TEXT("particlePosition"), FVector::ZeroVector);
				continue;
			}
			Spec.Count = Sprite->Count;
			Spec.Lifetime = Sprite->Lifetime.X;
			Spec.LifetimeMax = Sprite->Lifetime.Y;
			Spec.SpriteSizeMin = Sprite->SizeMin;
			Spec.SpriteSizeMax = Sprite->SizeMax;
			Spec.SpawnRadiusMin = Sprite->Radius.X;
			Spec.SpawnRadiusMax = Sprite->Radius.Y;
			Spec.SpeedMin = Sprite->Speed.X;
			Spec.SpeedMax = Sprite->Speed.Y;
			Spec.Drag = Sprite->Drag;
			Spec.bSizeFollowsOwnerScale = Sprite->bSizeFollowsScale;
			Spec.bRadialVelocity = Sprite->bRadialVelocity;
			Spec.RingStartAngleDegrees = Sprite->Arc.X;
			Spec.RingArcDegrees = Sprite->Arc.Y;
			Spec.RingHeight = Sprite->Height;
			Spec.VelocityMode = Sprite->VelocityMode;
			Spec.ConeDirection = Sprite->ConeDirection;
			Spec.ConeAngleDegrees = Sprite->ConeAngle;
			Spec.PositionOffset = Sprite->PositionOffset;
			Spec.bFaceCamera = Sprite->bFaceCamera;
			Spec.bVelocityAligned = Sprite->bVelocityAligned;
			Spec.Pivot = Sprite->Pivot;
			Spec.CustomAlignment = Sprite->CustomAlignment;
			Spec.SpriteFacing = Sprite->SpriteFacing;
			Spec.SpriteRotation = Sprite->SpriteRotation;
			if (!Sprite->Table.IsEmpty())
			{
				// 逐粒子表：同一出生时刻的粒子分成一组，每组一个发射器，在该时刻一次爆发；组内按 ExecIndex 查表。
				const WuwaReferenceNiagaraBuilder::FEmitterSpec Base = Spec;
				Specs.Pop();
				TMap<int32, TArray<WuwaReferenceNiagaraBuilder::FTableParticle>> Groups;
				for (const WuwaReferenceNiagaraBuilder::FTableParticle& P : Sprite->Table) Groups.FindOrAdd(FMath::RoundToInt(P.Delay * 1000.f)).Add(P);
				Groups.KeySort(TLess<int32>());
				int32 GroupIndex = 0;
				for (const TPair<int32, TArray<WuwaReferenceNiagaraBuilder::FTableParticle>>& Group : Groups)
				{
					WuwaReferenceNiagaraBuilder::FEmitterSpec& G = Specs.Add_GetRef(Base);
					G.Name = FName(*FString::Printf(TEXT("%s_t%02d"), *LayerName, GroupIndex));
					G.RandomSeed += 7 * ++GroupIndex;
					G.Table = Group.Value;
					G.Count = G.Table.Num();
					G.SpawnTime = G.Table[0].Delay;
					G.Lifetime = G.LifetimeMax = 0.f;
					G.SpriteSizeMin = G.SpriteSizeMax = G.Table[0].Size;
					for (const WuwaReferenceNiagaraBuilder::FTableParticle& P : G.Table) G.LifetimeMax = FMath::Max(G.LifetimeMax, P.Lifetime);
					G.Lifetime = G.LifetimeMax;
				}
			}
		}
		if (!WuwaReferenceNiagaraBuilder::BuildSystem(System, Specs))
		{
			UE_LOG(LogWuwaAttackEffectSetup, Error, TEXT("Building %s failed (%d emitter specs)"), *System->GetName(), Specs.Num());
			return nullptr;
		}
		return System;
	}

	/** NS_Reference_Attack01_N_Dg → N_Dg：去掉前缀和攻击段名（其他段可复用同一个系统）。 */
	FString Suffix(const FString& System)
	{
		FString Name = System.StartsWith(SystemPrefix) ? System.RightChop(FCString::Strlen(SystemPrefix)) : System;
		int32 Underscore = INDEX_NONE;
		if (Name.StartsWith(TEXT("Attack")) && Name.FindChar(TEXT('_'), Underscore)) Name.RightChopInline(Underscore + 1);
		return Name;
	}

	template <typename T>
	T* LoadOrCreate(const FAttack& Attack, const FString& AssetName)
	{
		const FString PackageName = Attack.ModelFolder() / AssetName;
		const FString ObjectPath = PackageName + TEXT(".") + AssetName;
		if (UObject* Existing = LoadObject<UObject>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn))
		{
			if (T* Typed = Cast<T>(Existing)) return Typed;
			UE_LOG(LogWuwaAttackEffectSetup, Error, TEXT("%s exists but is a %s."), *ObjectPath, *Existing->GetClass()->GetName());
			return nullptr;
		}
		return NewObject<T>(CreatePackage(*PackageName), *AssetName, RF_Public | RF_Standalone | RF_Transactional);
	}

	/** 配方里的曲线关键帧（原作 RichCurve 的时间 / 值 / 插值 / 切线）写入 FRichCurve，切线按原值保留。 */
	void ReadCurveKeys(const TArray<TSharedPtr<FJsonValue>>& Keys, FRichCurve& Curve)
	{
		Curve.Reset();
		for (const TSharedPtr<FJsonValue>& Value : Keys)
		{
			const TSharedPtr<FJsonObject> Key = Value->AsObject();
			const FKeyHandle Handle = Curve.AddKey(Key->GetNumberField(TEXT("time")), Key->GetNumberField(TEXT("value")));
			FRichCurveKey& Added = Curve.GetKey(Handle);
			const FString Interp = Key->GetStringField(TEXT("interp"));
			Added.InterpMode = Interp == TEXT("RCIM_Linear") ? RCIM_Linear : Interp == TEXT("RCIM_Constant") ? RCIM_Constant : RCIM_Cubic;
			Added.TangentMode = RCTM_User;
			Added.ArriveTangent = Key->GetNumberField(TEXT("arrive"));
			Added.LeaveTangent = Key->GetNumberField(TEXT("leave"));
		}
	}

	/** 配方里的曲线值：关键帧数组是曲线，数字是常量。 */
	void ReadFloatCurve(const TSharedPtr<FJsonValue>& Value, FWuwaEffectFloatCurve& Curve)
	{
		Curve.bUseCurve = Value->Type == EJson::Array;
		if (Curve.bUseCurve) ReadCurveKeys(Value->AsArray(), Curve.Curve.EditorCurveData);
		else Curve.Constant = static_cast<float>(Value->AsNumber());
	}

	/** 配方里的颜色值：{R,G,B,A} 四条关键帧是曲线，四个数字是常量。 */
	void ReadColorCurve(const TSharedPtr<FJsonValue>& Value, FWuwaEffectColorCurve& Curve)
	{
		Curve.bUseCurve = Value->Type == EJson::Object;
		if (Curve.bUseCurve)
		{
			const TSharedPtr<FJsonObject> Channels = Value->AsObject();
			ReadCurveKeys(Channels->GetArrayField(TEXT("R")), Curve.CurveR.EditorCurveData);
			ReadCurveKeys(Channels->GetArrayField(TEXT("G")), Curve.CurveG.EditorCurveData);
			ReadCurveKeys(Channels->GetArrayField(TEXT("B")), Curve.CurveB.EditorCurveData);
			ReadCurveKeys(Channels->GetArrayField(TEXT("A")), Curve.CurveA.EditorCurveData);
		}
		else
		{
			const TArray<TSharedPtr<FJsonValue>>& V = Value->AsArray();
			Curve.Constant = FLinearColor(V[0]->AsNumber(), V[1]->AsNumber(), V[2]->AsNumber(), V[3]->AsNumber());
		}
	}

	void ReadLifetime(UWuwaEffectModelBase& Model, const TSharedPtr<FJsonObject>& Recipe)
	{
		double Seconds = 0;
		if (Recipe->TryGetNumberField(TEXT("startTime"), Seconds)) Model.StartTime = static_cast<float>(Seconds);
		if (Recipe->TryGetNumberField(TEXT("endTime"), Seconds)) Model.EndTime = static_cast<float>(Seconds);
	}

	/**
	 * 原作 EffectModelPostProcess（DA_Fx_Post_*）的曲线与 Override 照抄到 UWuwaEffectModelPostProcess；
	 * 径向模糊材质是本项目的适配资产（RadialBlurMaterial）。
	 */
	void ConfigurePostProcess(UWuwaEffectModelPostProcess& Post, UMaterialInterface* RadialBlur, const TSharedPtr<FJsonObject>& Model)
	{
		Post.Modify();
		ReadLifetime(Post, Model);
		Post.RadialBlurMaterial = RadialBlur;
		const TSharedPtr<FJsonObject> Curves = Model->GetObjectField(TEXT("floatCurves"));
		const TSharedPtr<FJsonObject> Overrides = Model->GetObjectField(TEXT("overrides"));
		auto Field = [&](const TCHAR* Name, bool* Override, FWuwaEffectFloatCurve& Curve)
		{
			if (const TSharedPtr<FJsonValue> Value = Curves->TryGetField(Name)) ReadFloatCurve(Value, Curve);
			if (Override) *Override = Overrides->HasField(Name) && Overrides->GetBoolField(Name);
		};
		Field(TEXT("BlurIntensity"), &Post.BlurIntensityOverride, Post.BlurIntensity);
		Field(TEXT("SceneFringeIntensity"), &Post.SceneFringeIntensityOverride, Post.SceneFringeIntensity);
		Field(TEXT("VignetteIntensity"), &Post.VignetteIntensityOverride, Post.VignetteIntensity);
		Field(TEXT("RadialBlurHardness"), nullptr, Post.RadialBlurHardness);
		Field(TEXT("RadialBlurRadius"), nullptr, Post.RadialBlurRadius);
	}

	/** 原作 EffectModelLight（DA_Fx_Light_*）的位置、强度、颜色、半径照抄到 UWuwaEffectModelLight；没给半径时保留资产上的值。 */
	void ConfigureLight(UWuwaEffectModelLight& Light, const TSharedPtr<FJsonObject>& Model)
	{
		Light.Modify();
		ReadLifetime(Light, Model);
		Light.Location = FWuwaEffectVectorCurve(ReadVector(Model, TEXT("location"), FVector::ZeroVector));
		ReadFloatCurve(Model->TryGetField(TEXT("intensity")), Light.Intensity);
		ReadColorCurve(Model->TryGetField(TEXT("color")), Light.LightColor);
		if (const TSharedPtr<FJsonValue> Radius = Model->TryGetField(TEXT("radius"))) ReadFloatCurve(Radius, Light.AttenuationRadius);
	}

	/** 原作 EffectModelSkeletalMesh（DA_Fx_Mesh_*）：网格 / 动画引用项目里导入好的资产，时间与变换照抄。 */
	bool ConfigureSkeletalMesh(UWuwaEffectModelSkeletalMesh& Model, const FJsonObject& Def)
	{
		USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *Def.GetStringField(TEXT("mesh")));
		UAnimSequence* Animation = LoadObject<UAnimSequence>(nullptr, *Def.GetStringField(TEXT("animation")));
		if (!Mesh || !Animation)
		{
			UE_LOG(LogWuwaAttackEffectSetup, Error, TEXT("%s: import %s first (Tools/Blender/uemodel_to_fbx.py, then UE ImportAssets into %s); missing %s"),
				*Def.GetStringField(TEXT("name")), *Def.GetStringField(TEXT("fbx")), *Def.GetStringField(TEXT("importTo")),
				!Mesh ? *Def.GetStringField(TEXT("mesh")) : *Def.GetStringField(TEXT("animation")));
			return false;
		}
		const TSharedPtr<FJsonObject> Recipe = Def.GetObjectField(TEXT("model"));
		Model.Modify();
		Model.SkeletalMeshRef = Mesh;
		Model.AnimationRef = Animation;
		ReadLifetime(Model, Recipe);
		Model.LoopTime = static_cast<float>(Recipe->GetNumberField(TEXT("loopTime")));
		Model.Looping = Recipe->GetBoolField(TEXT("looping"));
		Model.HideFrames = static_cast<int32>(Recipe->GetNumberField(TEXT("hideFrames")));
		Model.CastShadow = Recipe->GetBoolField(TEXT("castShadow"));
		Model.Location = FWuwaEffectVectorCurve(ReadVector(Recipe, TEXT("location"), FVector::ZeroVector));
		const FVector Rotation = ReadVector(Recipe, TEXT("rotation"), FVector::ZeroVector);
		Model.Rotation = FWuwaEffectVectorCurve(Rotation);
		Model.Scale = FWuwaEffectVectorCurve(ReadVector(Recipe, TEXT("scale"), FVector::OneVector));
		return true;
	}

	/** 原作角色材质控制器（PD_CharacterControllerData 的 DataCache）的时间轴与边缘光参数组照抄到控制器数据。 */
	void ConfigureMaterialController(UWuwaCharacterMaterialControllerData& Data, const TSharedPtr<FJsonObject>& Model)
	{
		Data.Modify();
		Data.StartTime = static_cast<float>(Model->GetNumberField(TEXT("startTime")));
		Data.LoopTime = static_cast<float>(Model->GetNumberField(TEXT("loopTime")));
		Data.EndTime = static_cast<float>(Model->GetNumberField(TEXT("endTime")));
		Data.bIgnoreTimeDilation = Model->GetBoolField(TEXT("ignoreTimeDilation"));
		Data.bUseRim = Model->GetBoolField(TEXT("useRim"));
		Data.bRimUseTex = Model->GetBoolField(TEXT("rimUseTex"));
		const TArray<TSharedPtr<FJsonValue>>& Channel = Model->GetArrayField(TEXT("rimChannel"));
		Data.RimChannel = FLinearColor(Channel[0]->AsNumber(), Channel[1]->AsNumber(), Channel[2]->AsNumber(), Channel[3]->AsNumber());
		auto Floats = [&Model](const TCHAR* Field, FWuwaMaterialControlFloatGroup& Group)
		{
			const TSharedPtr<FJsonObject> Phases = Model->GetObjectField(Field);
			ReadFloatCurve(Phases->TryGetField(TEXT("Start")), Group.Start);
			ReadFloatCurve(Phases->TryGetField(TEXT("Loop")), Group.Loop);
			ReadFloatCurve(Phases->TryGetField(TEXT("End")), Group.End);
		};
		auto Colors = [&Model](const TCHAR* Field, FWuwaMaterialControlColorGroup& Group)
		{
			const TSharedPtr<FJsonObject> Phases = Model->GetObjectField(Field);
			ReadColorCurve(Phases->TryGetField(TEXT("Start")), Group.Start);
			ReadColorCurve(Phases->TryGetField(TEXT("Loop")), Group.Loop);
			ReadColorCurve(Phases->TryGetField(TEXT("End")), Group.End);
		};
		Floats(TEXT("rimRange"), Data.RimRange);
		Colors(TEXT("rimColor"), Data.RimColor);
		Floats(TEXT("rimIntensity"), Data.RimIntensity);
	}

	/** 原作 EffectModelDecal（DA_Fx_Decal_*）的变换、Z 向淡出与材质参数曲线照抄到 UWuwaEffectModelDecal。 */
	void ConfigureDecal(UWuwaEffectModelDecal& Decal, UMaterialInterface* Material, const TSharedPtr<FJsonObject>& Model)
	{
		Decal.Modify();
		Decal.DecalMaterialRef = Material;
		Decal.Location = FWuwaEffectVectorCurve(ReadVector(Model, TEXT("location"), FVector::ZeroVector));
		Decal.Rotation = FWuwaEffectVectorCurve(ReadVector(Model, TEXT("rotation"), FVector::ZeroVector));
		Decal.Scale = FWuwaEffectVectorCurve(ReadVector(Model, TEXT("scale"), FVector::OneVector));
		ReadLifetime(Decal, Model);
		Decal.MaterialFloatParameters.Reset();
		for (const auto& Pair : Model->GetObjectField(TEXT("floatCurves"))->Values)
		{
			ReadFloatCurve(Pair.Value, Decal.MaterialFloatParameters.Add(FName(*Pair.Key)));
		}
		Decal.MaterialColorParameters.Reset();
		for (const auto& Pair : Model->GetObjectField(TEXT("colorCurves"))->Values)
		{
			ReadColorCurve(Pair.Value, Decal.MaterialColorParameters.Add(FName(*Pair.Key)));
		}
	}

	bool Save(UObject* Asset)
	{
		UPackage* Package = Asset->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		Args.Error = GWarn;
		Asset->MarkPackageDirty();
		if (!UPackage::SavePackage(Package, Asset, *Filename, Args))
		{
			UE_LOG(LogWuwaAttackEffectSetup, Error, TEXT("Could not save %s"), *Filename);
			return false;
		}
		UE_LOG(LogWuwaAttackEffectSetup, Display, TEXT("Saved %s"), *Package->GetName());
		return true;
	}

	bool IsOurs(const FAttack& Attack, const FAnimNotifyEvent& Event)
	{
		const FString Name = Event.NotifyName.ToString();
		const FString Other = Attack.OtherPipelinePrefix();
		const bool bPrefix = Name.StartsWith(Attack.NotifyPrefix()) || (!Other.IsEmpty() && Name.StartsWith(Other));
		return (bPrefix && (!Event.Notify || Event.Notify->IsA<UWuwaAnimNotify_PlayEffect>())
				&& (!Event.NotifyStateClass || Event.NotifyStateClass->IsA<UWuwaAnimNotifyState_Ghost>()))
			|| (Name.StartsWith(LegacyNotifyPrefix) && !Event.Notify);
	}
}
#endif

int32 UWuwaChangliAttackEffectSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaAttackEffectSetup;
	const bool bApply = FParse::Param(*Params, TEXT("Apply"));
	FAttack Attack;
	Attack.Name = TEXT("Attack01");
	FParse::Value(*Params, TEXT("Character="), Attack.Character);
	Attack.Character.ToLowerInline();
	FParse::Value(*Params, TEXT("Attack="), Attack.Name);
	if (!Attack.IsKnown())
	{
		UE_LOG(LogWuwaAttackEffectSetup, Error, TEXT("-Character must be changli, jiabei or changli_legacy, got %s"), *Attack.Character);
		return 1;
	}
	// 长离沿用 AttackNN；嘉贝莉娜的段名就是配方文件名（Attack01..04、Charge）
	// 长离（新管线）的 Solo_AttackNN 是 solo.py 生成的单层排查配方
	const FString StageName = !Attack.IsLegacy() ? Attack.Name.Replace(TEXT("Solo_"), TEXT("")) : Attack.Name;
	// 旧管线只有 AttackNN；新管线（和嘉贝莉娜一样）段名就是配方文件名，空中段 AirAttack01 / AirAttack_Loop 等也行（蒙太奇 AM_<段名>）
	const bool bAttackNumber = StageName.StartsWith(TEXT("Attack")) && StageName.Len() == 8 && StageName.RightChop(6).IsNumeric();
	if (Attack.IsLegacy() ? !bAttackNumber : (!Attack.IsJiabei() && !bAttackNumber && !FPaths::FileExists(Attack.RecipeFile())))
	{
		UE_LOG(LogWuwaAttackEffectSetup, Error, TEXT("-Attack must look like Attack02, got %s"), *Attack.Name);
		return 1;
	}

	TArray<FPlacement> Placements;
	float NotifySeconds = 0.f;
	if (!ReadRecipe(Attack, Placements, NotifySeconds)) return 1;
	if (const TSharedPtr<FJsonObject> RecipeHeader = LoadJson(Attack.RecipeFile()))
	{
		RecipeHeader->TryGetStringField(TEXT("montage"), Attack.RecipeMontage);
	}
	FTuning Tuning;
	if (!ReadTuning(Attack, Tuning)) return 1;
	for (const auto& Pair : Tuning.ModelScale)
		UE_LOG(LogWuwaAttackEffectSetup, Display, TEXT("Tuning: model scale %s = %.2f"), *Pair.Key, Pair.Value);
	for (const auto& Pair : Tuning.Materials)
		UE_LOG(LogWuwaAttackEffectSetup, Display, TEXT("Tuning: %s tint=%s intensity=%.2f"), *Pair.Key, *Pair.Value.Tint.ToString(), Pair.Value.Intensity);
	for (const auto& Pair : Tuning.SpriteLayers)
		UE_LOG(LogWuwaAttackEffectSetup, Display, TEXT("Tuning: rebuild sprite system %s (%d layers)"), *Pair.Key, Pair.Value.Num());
	UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, *Attack.MontagePath());
	if (!Montage)
	{
		UE_LOG(LogWuwaAttackEffectSetup, Error, TEXT("Missing %s"), *Attack.MontagePath());
		return 1;
	}

	for (const FAnimNotifyEvent& Event : Montage->Notifies)
	{
		UE_LOG(LogWuwaAttackEffectSetup, Display, TEXT("%s notify %.3fs %s (%s)%s"), *Attack.MontageLabel(), Event.GetTime(), *Event.NotifyName.ToString(),
			Event.Notify ? *Event.Notify->GetClass()->GetName() : Event.NotifyStateClass ? *Event.NotifyStateClass->GetClass()->GetName() : TEXT("empty"),
			IsOurs(Attack, Event) ? TEXT(" [will be replaced]") : TEXT(""));
	}
	for (const FPlacement& P : Placements)
	{
		UE_LOG(LogWuwaAttackEffectSetup, Display, TEXT("Placement %s at %.4fs loc=%s rot=%s scale=%s yawKeys=%d attached=%d"), *P.Key(),
			NotifySeconds + P.Delay, *P.Location.ToString(), *P.Rotation.ToString(), *P.Scale.ToString(), P.YawKeys.Num(), P.bAttached);
	}
	if (!bApply)
	{
		UE_LOG(LogWuwaAttackEffectSetup, Display, TEXT("Audit only. Run with -Apply to build the models and notifies."));
		return 0;
	}

	// 要重建的系统：配方 generate=true 的（导入原件、重建材质），以及 Tuning 里给了 Sprite 参数的。
	TArray<UObject*> Changed;
	const TSharedPtr<FJsonObject> Recipe = LoadJson(Attack.RecipeFile());
	if (!Recipe) return 1;
	TArray<TSharedPtr<FJsonObject>> Rebuilds;
	bool bAnyGenerate = false;
	for (const TSharedPtr<FJsonValue>& Value : Recipe->GetArrayField(TEXT("systems")))
	{
		const TSharedPtr<FJsonObject> Def = Value->AsObject();
		// 没有可建层的系统（只有拖尾 / 扭曲之类）：配方把它记在 skippedChildren，放置里也不会引用，这里不建。
		const TArray<TSharedPtr<FJsonValue>>* Layers = nullptr;
		if (!Def->TryGetArrayField(TEXT("layers"), Layers) || Layers->IsEmpty())
		{
			UE_LOG(LogWuwaAttackEffectSetup, Display, TEXT("Skipping %s: no buildable layer"), *Def->GetStringField(TEXT("name")));
			continue;
		}
		bool bGenerate = false;
		Def->TryGetBoolField(TEXT("generate"), bGenerate);
		bAnyGenerate |= bGenerate;
		if (bGenerate || Tuning.SpriteLayers.Contains(Def->GetStringField(TEXT("name")))) Rebuilds.Add(Def);
	}
	for (const auto& Pair : Tuning.SpriteLayers)
	{
		if (!FindSystemDef(Attack, Recipe, Pair.Key)) return 1;
	}
	TMap<FString, UTexture2D*> Textures;
	if (bAnyGenerate)
	{
		const TArray<TSharedPtr<FJsonValue>>* TextureDefs = nullptr;
		if (Recipe->TryGetArrayField(TEXT("textures"), TextureDefs))
		{
			for (const TSharedPtr<FJsonValue>& Value : *TextureDefs)
			{
				bool bCreated = false;
				UTexture2D* Texture = WuwaReferenceAssetImport::FindOrImportTexture(Attack.TextureFolder(), Attack.SourceRoot(), *Value->AsObject(), bCreated);
				if (!Texture) return 1;
				if (bCreated) Changed.Add(Texture);
				Textures.Add(Value->AsObject()->GetStringField(TEXT("name")), Texture);
			}
		}
	}
	for (const TSharedPtr<FJsonObject>& Def : Rebuilds)
	{
		bool bGenerate = false;
		Def->TryGetBoolField(TEXT("generate"), bGenerate);
		UNiagaraSystem* System = RebuildSystem(Attack, *Def, Tuning.SpriteLayers.Find(Def->GetStringField(TEXT("name"))), bGenerate, Textures, Changed);
		if (!System) return 1;
		Changed.Add(System);
	}

	// 贴花（原作 EffectModelDecal）：重建贴花材质；Model 的变换、Z 向淡出、材质参数曲线照抄原作 DA。
	TMap<FString, UWuwaEffectModelGroup*> Groups;
	const TArray<TSharedPtr<FJsonValue>>* DecalDefs = nullptr;
	if (Recipe->TryGetArrayField(TEXT("decals"), DecalDefs))
	{
		for (const TSharedPtr<FJsonValue>& Value : *DecalDefs)
		{
			const TSharedPtr<FJsonObject> Def = Value->AsObject();
			const FString Name = Def->GetStringField(TEXT("name"));
			UMaterial* Material = WuwaReferenceAssetImport::BuildDecalMaterial(Attack.MaterialFolder(), *Def->GetObjectField(TEXT("material")), Textures);
			UWuwaEffectModelDecal* Decal = LoadOrCreate<UWuwaEffectModelDecal>(Attack, FString::Printf(TEXT("DA_Fx_%s_%s_%s"), *Attack.Title(), *Attack.Name, *Name));
			UWuwaEffectModelGroup* Group = LoadOrCreate<UWuwaEffectModelGroup>(Attack, FString::Printf(TEXT("DA_Fx_%s_%s_Group_%s"), *Attack.Title(), *Attack.Name, *Name));
			if (!Material || !Decal || !Group) return 1;
			ConfigureDecal(*Decal, Material, Def->GetObjectField(TEXT("model")));
			Group->Modify();
			Group->StartTime = Decal->StartTime;
			Group->EffectData.Reset();
			Group->EffectData.Add(Decal, 0.f);
			Changed.Add(Material);
			Changed.Add(Decal);
			Changed.Add(Group);
			Groups.Add(Name, Group);
		}
	}
	// 后处理、灯光（原作 EffectModelPostProcess / EffectModelLight）：每个 DA 一个 Model 和一个 Group。
	auto AddChildGroup = [&](const FString& Name, UWuwaEffectModelBase* Model) -> bool
	{
		UWuwaEffectModelGroup* Group = LoadOrCreate<UWuwaEffectModelGroup>(Attack, FString::Printf(TEXT("DA_Fx_%s_%s_Group_%s"), *Attack.Title(), *Attack.Name, *Name));
		if (!Group) return false;
		Group->Modify();
		Group->StartTime = Model->StartTime;
		Group->EffectData.Reset();
		Group->EffectData.Add(Model, 0.f);
		Changed.Add(Model);
		Changed.Add(Group);
		Groups.Add(Name, Group);
		return true;
	};
	const TArray<TSharedPtr<FJsonValue>>* PostDefs = nullptr;
	if (Recipe->TryGetArrayField(TEXT("postProcesses"), PostDefs) && !PostDefs->IsEmpty())
	{
		UMaterial* RadialBlur = WuwaReferenceAssetImport::BuildRadialBlurMaterial(RadialBlurMaterialPath);
		if (!RadialBlur) return 1;
		Changed.Add(RadialBlur);
		for (const TSharedPtr<FJsonValue>& Value : *PostDefs)
		{
			const TSharedPtr<FJsonObject> Def = Value->AsObject();
			const FString Name = Def->GetStringField(TEXT("name"));
			UWuwaEffectModelPostProcess* Post = LoadOrCreate<UWuwaEffectModelPostProcess>(Attack, FString::Printf(TEXT("DA_Fx_%s_%s_%s"), *Attack.Title(), *Attack.Name, *Name));
			if (!Post) return 1;
			ConfigurePostProcess(*Post, RadialBlur, Def->GetObjectField(TEXT("model")));
			if (!AddChildGroup(Name, Post)) return 1;
		}
	}
	// 角色残影（原作 EffectModelGhost + AnimNotifyStateGhost）：重建残影材质，Model 只放材质与透明度曲线。
	TMap<FString, UWuwaEffectModelGhost*> Ghosts;
	const TArray<TSharedPtr<FJsonValue>>* GhostDefs = nullptr;
	if (Recipe->TryGetArrayField(TEXT("ghosts"), GhostDefs))
	{
		for (const TSharedPtr<FJsonValue>& Value : *GhostDefs)
		{
			const TSharedPtr<FJsonObject> Def = Value->AsObject();
			const FString Name = Def->GetStringField(TEXT("name"));
			UMaterial* Material = WuwaReferenceAssetImport::BuildGhostMaterial(Attack.MaterialFolder(), *Def->GetObjectField(TEXT("material")));
			UWuwaEffectModelGhost* Ghost = LoadOrCreate<UWuwaEffectModelGhost>(Attack, FString::Printf(TEXT("DA_Fx_%s_%s_%s"), *Attack.Title(), *Attack.Name, *Name));
			if (!Material || !Ghost) return 1;
			Ghost->Modify();
			Ghost->MaterialRef = Material;
			// 原作 DA 的 AlphaCurve 没有关键帧也没有常量时保留资产上的值（本项目默认常量 1）。
			const TSharedPtr<FJsonValue> Alpha = Def->TryGetField(TEXT("alphaCurve"));
			if (Alpha && Alpha->Type != EJson::Null) ReadFloatCurve(Alpha, Ghost->AlphaCurve);
			Changed.Add(Material);
			Changed.Add(Ghost);
			Ghosts.Add(Name, Ghost);
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* GhostWindows = nullptr;
	Recipe->TryGetArrayField(TEXT("ghostWindows"), GhostWindows);
	// 角色材质控制器：控制器数据 + 播放它的 Model（Model 的起始段 = 控制器总时长）。
	const TArray<TSharedPtr<FJsonValue>>* ControllerDefs = nullptr;
	if (Recipe->TryGetArrayField(TEXT("materialControllers"), ControllerDefs))
	{
		for (const TSharedPtr<FJsonValue>& Value : *ControllerDefs)
		{
			const TSharedPtr<FJsonObject> Def = Value->AsObject();
			const FString Name = Def->GetStringField(TEXT("name"));
			UWuwaCharacterMaterialControllerData* Data = LoadOrCreate<UWuwaCharacterMaterialControllerData>(Attack, FString::Printf(TEXT("PD_%s_%s_%s"), *Attack.Title(), *Attack.Name, *Name));
			UWuwaEffectModelMaterialController* Controller = LoadOrCreate<UWuwaEffectModelMaterialController>(Attack, FString::Printf(TEXT("DA_Fx_%s_%s_%s"), *Attack.Title(), *Attack.Name, *Name));
			if (!Data || !Controller) return 1;
			ConfigureMaterialController(*Data, Def->GetObjectField(TEXT("model")));
			Controller->Modify();
			Controller->ControllerData = Data;
			Controller->MaterialControllerData = FSoftObjectPath(Def->GetStringField(TEXT("source")).Replace(TEXT("Client/Content/"), TEXT("/Game/")));
			Controller->StartTime = Data->WholeTime();
			Changed.Add(Data);
			if (!AddChildGroup(Name, Controller)) return 1;
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* LightDefs = nullptr;
	if (Recipe->TryGetArrayField(TEXT("lights"), LightDefs))
	{
		for (const TSharedPtr<FJsonValue>& Value : *LightDefs)
		{
			const TSharedPtr<FJsonObject> Def = Value->AsObject();
			const FString Name = Def->GetStringField(TEXT("name"));
			UWuwaEffectModelLight* Light = LoadOrCreate<UWuwaEffectModelLight>(Attack, FString::Printf(TEXT("DA_Fx_%s_%s_%s"), *Attack.Title(), *Attack.Name, *Name));
			if (!Light) return 1;
			ConfigureLight(*Light, Def->GetObjectField(TEXT("model")));
			if (!AddChildGroup(Name, Light)) return 1;
		}
	}
	const TArray<TSharedPtr<FJsonValue>>* SkeletalDefs = nullptr;
	if (Recipe->TryGetArrayField(TEXT("skeletalMeshes"), SkeletalDefs))
	{
		for (const TSharedPtr<FJsonValue>& Value : *SkeletalDefs)
		{
			const TSharedPtr<FJsonObject> Def = Value->AsObject();
			const FString Name = Def->GetStringField(TEXT("name"));
			UWuwaEffectModelSkeletalMesh* Model = LoadOrCreate<UWuwaEffectModelSkeletalMesh>(Attack, FString::Printf(TEXT("DA_Fx_%s_%s_%s"), *Attack.Title(), *Attack.Name, *Name));
			if (!Model || !ConfigureSkeletalMesh(*Model, *Def)) return 1;
			if (!AddChildGroup(Name, Model)) return 1;
		}
	}
	for (const FPlacement& P : Placements)
	{
		if (!P.Child.IsEmpty() && !Groups.Contains(P.Child))
		{
			UE_LOG(LogWuwaAttackEffectSetup, Error, TEXT("Placement references unknown child model %s"), *P.Child);
			return 1;
		}
	}

	// 每个 Niagara 系统一个 Model 和一个 Group，对应原作 DA_Fx_R1aNN_* 与 DA_Fx_Group_R1aNN_*。
	for (const FPlacement& P : Placements)
	{
		if (Groups.Contains(P.Key())) continue;
		UNiagaraSystem* System = LoadObject<UNiagaraSystem>(nullptr, *FString::Printf(TEXT("%s/%s.%s"), *Attack.NiagaraFolder(), *P.System, *P.System));
		if (!System)
		{
			UE_LOG(LogWuwaAttackEffectSetup, Error, TEXT("Missing Niagara system %s"), *P.System);
			return 1;
		}
		const FString Name = Suffix(P.System);
		UWuwaEffectModelNiagara* Niagara = LoadOrCreate<UWuwaEffectModelNiagara>(Attack, FString::Printf(TEXT("DA_Fx_%s_%s_%s"), *Attack.Title(), *Attack.Name, *Name));
		UWuwaEffectModelGroup* Group = LoadOrCreate<UWuwaEffectModelGroup>(Attack, FString::Printf(TEXT("DA_Fx_%s_%s_Group_%s"), *Attack.Title(), *Attack.Name, *Name));
		if (!Niagara || !Group) return 1;

		Niagara->Modify();
		Niagara->NiagaraRef = System;
		Niagara->StartTime = StartTime;
		// 人工放大：重建的 Niagara 缺少原作粒子随寿命放大的部分，整体比游戏里小。
		const float* ModelScale = Tuning.ModelScale.Find(Name);
		Niagara->Scale = FWuwaEffectVectorCurve(FVector(ModelScale ? *ModelScale : 1.f));
		Niagara->Rotation = FWuwaEffectVectorCurve();
		// 原作子 DA 的 Location 常量（例如地裂在前方 150~350）；复用别段的系统不在本段配方里，没有位移。
		Niagara->Location = FWuwaEffectVectorCurve();
		for (const TSharedPtr<FJsonValue>& Value : Recipe->GetArrayField(TEXT("systems")))
		{
			if (Value->AsObject()->GetStringField(TEXT("name")) == P.System)
				Niagara->Location = FWuwaEffectVectorCurve(ReadVector(Value->AsObject(), TEXT("modelLocation"), FVector::ZeroVector));
		}
		if (!P.YawKeys.IsEmpty())
		{
			// 原作子 DA 的 Rotation.Curve[2]：播放过程中绕局部 Z 轴转动。
			Niagara->Rotation.bUseCurve = true;
			FRichCurve& Curve = Niagara->Rotation.CurveZ.EditorCurveData;
			Curve.Reset();
			for (const FYawKey& Key : P.YawKeys)
			{
				const FKeyHandle Handle = Curve.AddKey(Key.Time, Key.Value);
				FRichCurveKey& Added = Curve.GetKey(Handle);
				Added.InterpMode = RCIM_Cubic;
				Added.TangentMode = RCTM_User;
				Added.ArriveTangent = Key.ArriveTangent;
				Added.LeaveTangent = Key.LeaveTangent;
			}
		}
		Group->Modify();
		Group->StartTime = StartTime;
		Group->EffectData.Reset();
		Group->EffectData.Add(Niagara, 0.f);
		Changed.Add(Niagara);
		Changed.Add(Group);
		Groups.Add(P.System, Group);
	}

	bool bSolo = false;
	Recipe->TryGetBoolField(TEXT("solo"), bSolo);
	int32 Removed = 0;
	if (!bSolo)
	{
	Montage->Modify();
	Removed = Montage->Notifies.RemoveAll([&Attack](const FAnimNotifyEvent& Event) { return IsOurs(Attack, Event); });
	int32 Track = Montage->AnimNotifyTracks.IndexOfByPredicate([](const FAnimNotifyTrack& T) { return T.TrackName == TrackName; });
	if (Track == INDEX_NONE)
	{
		Track = Montage->AnimNotifyTracks.AddDefaulted();
		Montage->AnimNotifyTracks[Track].TrackName = TrackName;
		Montage->AnimNotifyTracks[Track].TrackColor = FLinearColor(1.f, .25f, .035f);
	}
	for (int32 Index = 0; Index < Placements.Num(); ++Index)
	{
		const FPlacement& P = Placements[Index];
		auto* Notify = NewObject<UWuwaAnimNotify_PlayEffect>(Montage, NAME_None, RF_Transactional);
		Notify->Model = Groups[P.Key()];
		Notify->SocketName = P.Socket;
		Notify->LocationOffset = P.Location;
		Notify->RotationOffset = P.Rotation;
		Notify->Scale = P.Scale;
		Notify->bFollow = P.bAttached;
		FAnimNotifyEvent& Event = Montage->Notifies.AddDefaulted_GetRef();
		Event.Notify = Notify;
		Event.NotifyName = FName(*FString::Printf(TEXT("%s%02d.%s"), *Attack.NotifyPrefix(), Index + 1, *Suffix(P.Key())));
		Event.TrackIndex = Track;
		Event.NotifyTriggerChance = 1.f;
		Event.TriggerTimeOffset = 0.f;
		Event.Link(Montage, NotifySeconds + P.Delay);
		Event.Guid = FGuid::NewGuid();
	}
	for (int32 Index = 0; GhostWindows && Index < GhostWindows->Num(); ++Index)
	{
		const TSharedPtr<FJsonObject> Window = (*GhostWindows)[Index]->AsObject();
		UWuwaEffectModelGhost** Ghost = Ghosts.Find(Window->GetStringField(TEXT("ghost")));
		if (!Ghost)
		{
			UE_LOG(LogWuwaAttackEffectSetup, Error, TEXT("Ghost window references unknown ghost %s"), *Window->GetStringField(TEXT("ghost")));
			return 1;
		}
		auto* State = NewObject<UWuwaAnimNotifyState_Ghost>(Montage, NAME_None, RF_Transactional);
		State->Model = *Ghost;
		State->bUseSpawnRate = Window->GetBoolField(TEXT("useSpawnRate"));
		State->SpawnRate = static_cast<float>(Window->GetNumberField(TEXT("spawnRate")));
		State->SpawnInterval = static_cast<float>(Window->GetNumberField(TEXT("spawnInterval")));
		State->GhostLifeTime = static_cast<float>(Window->GetNumberField(TEXT("ghostLifeTime")));
		const float Begin = NotifySeconds + static_cast<float>(Window->GetNumberField(TEXT("delay")));
		const float Duration = static_cast<float>(Window->GetNumberField(TEXT("duration")));
		FAnimNotifyEvent& Event = Montage->Notifies.AddDefaulted_GetRef();
		Event.NotifyStateClass = State;
		Event.NotifyName = FName(*FString::Printf(TEXT("%sGhost%02d"), *Attack.NotifyPrefix(), Index + 1));
		Event.TrackIndex = Track;
		Event.NotifyTriggerChance = 1.f;
		Event.TriggerTimeOffset = 0.f;
		Event.Link(Montage, Begin);
		Event.SetDuration(Duration);
		Event.EndLink.Link(Montage, Begin + Duration);
		Event.Guid = FGuid::NewGuid();
	}
	Montage->SortNotifies();
	WuwaAnimNotifyEditor::RefreshTriggerOffsets(Montage);
	Montage->RefreshCacheData();
	Changed.Add(Montage);
	}

	for (const auto& Pair : Tuning.Materials)
	{
		UMaterial* Material = ApplyMaterialTuning(Attack, Pair.Key, Pair.Value);
		if (!Material) return 1;
		Changed.Add(Material);
	}

	// 新建 / 重建的材质先编译完并确认没有 shader 错误再保存。
	FAssetCompilingManager::Get().FinishAllCompilation();
	if (GShaderCompilingManager) GShaderCompilingManager->FinishAllCompilation();
	for (UObject* Asset : Changed)
	{
		const UMaterial* Material = Cast<UMaterial>(Asset);
		const FMaterialResource* Resource = Material ? Material->GetMaterialResource(GMaxRHIShaderPlatform) : nullptr;
		if (!Resource || Resource->GetCompileErrors().IsEmpty()) continue;
		for (const FString& Error : Resource->GetCompileErrors()) UE_LOG(LogWuwaAttackEffectSetup, Error, TEXT("%s: %s"), *Material->GetName(), *Error);
		return 1;
	}
	for (UObject* Asset : Changed)
	{
		if (!Save(Asset)) return 1;
	}
	UE_LOG(LogWuwaAttackEffectSetup, Display, TEXT("Rebuilt %d systems, built %d models/groups, tuned %d materials, saved %d assets, replaced %d old notifies, installed %d Play Effect notifies on %s."),
		Rebuilds.Num(), Groups.Num() * 2, Tuning.Materials.Num(), Changed.Num(), Removed, Placements.Num(), *Attack.MontageLabel());
	return 0;
#else
	UE_LOG(LogWuwaAttackEffectSetup, Error, TEXT("WuwaChangliAttackEffectSetup requires an editor build."));
	return 1;
#endif
}

#if WITH_EDITOR
#include "HAL/IConsoleManager.h"

/**
 * 编辑器开着时也能重建特效：在编辑器进程里跑同一个 commandlet（参数和 -run= 时一样）。
 * 例：Wuwa.Fx.Setup -Character=changli -Attack=AirAttack03 -Apply
 * 独立的 commandlet 进程在编辑器开着时存不了盘、也用不到 Live Coding 改过的代码，所以从编辑器 / MCP 里调这个。
 */
static FAutoConsoleCommand GWuwaFxSetupCommand(
	TEXT("Wuwa.Fx.Setup"),
	TEXT("Runs WuwaChangliAttackEffectSetup inside the editor, e.g. Wuwa.Fx.Setup -Character=changli -Attack=AirAttack03 -Apply"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		UWuwaChangliAttackEffectSetupCommandlet* Commandlet = NewObject<UWuwaChangliAttackEffectSetupCommandlet>();
		const int32 Result = Commandlet->Main(FString::Join(Args, TEXT(" ")));
		UE_LOG(LogWuwaAttackEffectSetup, Display, TEXT("Wuwa.Fx.Setup %s finished with exit code %d"), *FString::Join(Args, TEXT(" ")), Result);
	}));
#endif
