#include "Editor/Commandlets/WuwaCharacterRimSetupCommandlet.h"

#if WITH_EDITOR
#include "Engine/SkeletalMesh.h"
#include "HAL/FileManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionEyeAdaptationInverse.h"
#include "Materials/MaterialExpressionPixelNormalWS.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Misc/DateTime.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaCharacterRim, Log, All);

UWuwaCharacterRimSetupCommandlet::UWuwaCharacterRimSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpUsage = TEXT("-run=WuwaCharacterRimSetup -Mesh=/Game/... [-Apply]");
}

#if WITH_EDITOR
namespace WuwaCharacterRim
{
	// 本命令行加的节点都带这个描述前缀，重复运行时先移除。
	const FString Tag = TEXT("WuwaRim");

	// 原作角色母材质边缘光段（编译后像素着色器的运算顺序）。原作 E_Tex_UseTex 打开时 E_Tex_Color 会乘贴图采样，
	// 那条路径属于材质控制器的贴图采样功能，这里没有接入，只用 E_Tex_Color 本身。
	const TCHAR* Code = TEXT(
		"float4 tex=E_Tex_Color;\n"
		"float3 channel=lerp(tex.rgb,dot(E_Rim_Channel,tex).xxx,step(0.0,E_Rim_Channel.x));\n"
		"float3 t=lerp(1.0,channel,E_Rim_UseTex);\n"
		"float f=max(1.0-abs(dot(N,V)),0.001);\n"
		"float3 rim=pow(f,E_Rim_RimRange)*E_Rim_RimColor.rgb*t*E_Rim_Intensity;\n"
		"return rim*float(E_Rim_UseRim>0.1);\n");

	template <typename T>
	T* Add(UMaterial* M, int32 X, int32 Y, const FString& Description)
	{
		T* E = NewObject<T>(M, NAME_None, RF_Transactional);
		E->Material = M;
		E->MaterialExpressionEditorX = X;
		E->MaterialExpressionEditorY = Y;
		E->Desc = Description;
		M->GetExpressionCollection().AddExpression(E);
		return E;
	}

	bool IsOurs(const UMaterialExpression* E)
	{
		return E && E->Desc.StartsWith(Tag);
	}

	/** 边缘光要叠到哪个输入上：卡通材质的自发光最后一步是去掉曝光（EyeAdaptationInverse），边缘光加在它前面，和明暗一起去曝光。 */
	FExpressionInput& RimTarget(UMaterial* M)
	{
		FExpressionInput& Emissive = M->GetEditorOnlyData()->EmissiveColor;
		if (auto* Inverse = Cast<UMaterialExpressionEyeAdaptationInverse>(Emissive.Expression)) return Inverse->LightValueInput;
		return Emissive;
	}

	/** 移除上次加的节点；自发光若被我们的 Add 包了一层，接回原来的输入。 */
	void RemovePrevious(UMaterial* M)
	{
		FExpressionInput& Emissive = RimTarget(M);
		if (UMaterialExpressionAdd* Wrapped = Cast<UMaterialExpressionAdd>(Emissive.Expression); IsOurs(Wrapped))
		{
			Emissive = Wrapped->A;
		}
		else if (IsOurs(Emissive.Expression))
		{
			Emissive = FExpressionInput();
		}
		TArray<UMaterialExpression*> Old;
		for (UMaterialExpression* E : M->GetExpressions())
		{
			if (IsOurs(E)) Old.Add(E);
		}
		for (UMaterialExpression* E : Old)
		{
			M->GetExpressionCollection().RemoveExpression(E);
		}
	}

	void AddRim(UMaterial* M)
	{
		const int32 X = -900, Y = 900;
		auto* Art = Add<UMaterialExpressionCustom>(M, X, Y, Tag + TEXT(": original character rim (E_Rim_*), see WuwaCharacterRimSetupCommandlet"));
		Art->OutputType = CMOT_Float3;
		Art->Inputs.Reset();
		auto Input = [Art](const TCHAR* Name, UMaterialExpression* Source, int32 Output = 0)
		{
			FCustomInput& In = Art->Inputs.AddDefaulted_GetRef();
			In.InputName = Name;
			In.Input.Connect(Output, Source);
		};
		Input(TEXT("N"), Add<UMaterialExpressionPixelNormalWS>(M, X - 300, Y, Tag));
		Input(TEXT("V"), Add<UMaterialExpressionCameraVectorWS>(M, X - 300, Y + 60, Tag));
		int32 Row = 0;
		// 默认值：原作角色母材质 shader map 的参数表（MI_R2T1CalbrenaMd10011Cloth 的 preshader，见 ArtSource/JiabeiAttack/preshader.py）。
		auto Scalar = [&](const TCHAR* Name, float Default)
		{
			auto* P = Add<UMaterialExpressionScalarParameter>(M, X - 300, Y + 120 + 60 * Row++, Tag);
			P->ParameterName = Name;
			P->DefaultValue = Default;
			P->Group = TEXT("Material Controller");
			Input(Name, P);
		};
		auto Vector = [&](const TCHAR* Name, const FLinearColor& Default)
		{
			auto* P = Add<UMaterialExpressionVectorParameter>(M, X - 300, Y + 120 + 60 * Row++, Tag);
			P->ParameterName = Name;
			P->DefaultValue = Default;
			P->Group = TEXT("Material Controller");
			Input(Name, P, 5);  // RGBA
		};
		Scalar(TEXT("E_Rim_UseRim"), 0.f);
		Scalar(TEXT("E_Rim_UseTex"), 1.f);
		Scalar(TEXT("E_Rim_RimRange"), 0.5f);
		Scalar(TEXT("E_Rim_Intensity"), 1.f);
		Vector(TEXT("E_Rim_Channel"), FLinearColor(1.f, 0.f, 0.f, 0.f));
		Vector(TEXT("E_Rim_RimColor"), FLinearColor(0.f, 0.f, 0.f, 1.f));
		Vector(TEXT("E_Tex_Color"), FLinearColor(1.f, 1.f, 1.f, 1.f));
		Art->Code = Code;

		FExpressionInput& Emissive = RimTarget(M);
		if (Emissive.Expression)
		{
			auto* Sum = Add<UMaterialExpressionAdd>(M, X + 300, Y, Tag);
			Sum->A = Emissive;
			Sum->B.Connect(0, Art);
			Emissive.Connect(0, Sum);
		}
		else
		{
			Emissive.Connect(0, Art);
		}
	}

	bool Backup(const TArray<UMaterial*>& Materials, const FString& Root)
	{
		for (const UMaterial* M : Materials)
		{
			FString Relative = M->GetOutermost()->GetName();
			if (!Relative.RemoveFromStart(TEXT("/Game/"))) return false;
			for (const TCHAR* Extension : { TEXT(".uasset"), TEXT(".uexp") })
			{
				const FString Source = FPackageName::LongPackageNameToFilename(M->GetOutermost()->GetName(), Extension);
				if (!IFileManager::Get().FileExists(*Source)) continue;
				const FString Destination = Root / (Relative + Extension);
				if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Destination), true)
					|| IFileManager::Get().Copy(*Destination, *Source, false, true) != COPY_OK) return false;
			}
		}
		UE_LOG(LogWuwaCharacterRim, Display, TEXT("Backed up to %s"), *Root);
		return true;
	}

	bool Save(UObject* Asset)
	{
		UPackage* Package = Asset->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		Args.Error = GWarn;
		Asset->MarkPackageDirty();
		const bool bSaved = UPackage::SavePackage(Package, Asset, *Filename, Args);
		UE_LOG(LogWuwaCharacterRim, Display, TEXT("%s %s"), bSaved ? TEXT("Saved") : TEXT("FAILED to save"), *Package->GetName());
		return bSaved;
	}
}
#endif

#if WITH_EDITOR
void UWuwaCharacterRimSetupCommandlet::AddRimBlock(UMaterial* Material)
{
	WuwaCharacterRim::RemovePrevious(Material);
	WuwaCharacterRim::AddRim(Material);
}
#endif

int32 UWuwaCharacterRimSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaCharacterRim;
	FString MeshPath;
	if (!FParse::Value(*Params, TEXT("Mesh="), MeshPath))
	{
		UE_LOG(LogWuwaCharacterRim, Error, TEXT("Usage: %s"), *HelpUsage);
		return 1;
	}
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
	if (!Mesh)
	{
		UE_LOG(LogWuwaCharacterRim, Error, TEXT("Missing skeletal mesh %s"), *MeshPath);
		return 1;
	}
	TArray<UMaterial*> Materials;
	for (const FSkeletalMaterial& Slot : Mesh->GetMaterials())
	{
		UMaterial* Base = Slot.MaterialInterface ? Slot.MaterialInterface->GetMaterial() : nullptr;
		// 只改项目里的材质：引擎、插件里的材质（例如 FBX 导入的 /InterchangeAssets 默认材质）不动。
		if (!Base || !Base->GetOutermost()->GetName().StartsWith(TEXT("/Game/")))
		{
			if (Base) UE_LOG(LogWuwaCharacterRim, Display, TEXT("Slot %s -> %s (not a project material, skipped)"), *Slot.MaterialSlotName.ToString(), *Base->GetPathName());
			continue;
		}
		if (Base->bUseMaterialAttributes)
		{
			UE_LOG(LogWuwaCharacterRim, Error, TEXT("%s uses material attributes; not supported."), *Base->GetPathName());
			return 1;
		}
		Materials.AddUnique(Base);
		UE_LOG(LogWuwaCharacterRim, Display, TEXT("Slot %s -> %s"), *Slot.MaterialSlotName.ToString(), *Base->GetPathName());
	}
	if (!FParse::Param(*Params, TEXT("Apply")))
	{
		UE_LOG(LogWuwaCharacterRim, Display, TEXT("Audit only: %d base materials. Run with -Apply to add the rim block."), Materials.Num());
		return 0;
	}
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/CharacterRimSetup")
		/ FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	if (!Backup(Materials, Root)) return 1;
	for (UMaterial* M : Materials)
	{
		M->PreEditChange(nullptr);
		AddRimBlock(M);
		M->PostEditChange();
	}
	for (UMaterial* M : Materials)
	{
		if (!Save(M)) return 1;
	}
	UE_LOG(LogWuwaCharacterRim, Display, TEXT("Added the rim block to %d base materials."), Materials.Num());
	return 0;
#else
	return 1;
#endif
}
