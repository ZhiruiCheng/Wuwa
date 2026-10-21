#include "Editor/Commandlets/WuwaCharacterToonSetupCommandlet.h"
#include "Editor/Commandlets/WuwaCharacterRimSetupCommandlet.h"

#if WITH_EDITOR
#include "Editor/Effects/WuwaReferenceAssetImport.h"
#endif

#if WITH_EDITOR
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "HAL/FileManager.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCameraVectorWS.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionEyeAdaptationInverse.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTextureObjectParameter.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionTwoSidedSign.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialParameterCollection.h"
#include "Misc/DateTime.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Rendering/SkeletalMeshModel.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWuwaCharacterToon, Log, All);

UWuwaCharacterToonSetupCommandlet::UWuwaCharacterToonSetupCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	HelpUsage = TEXT("-run=WuwaCharacterToonSetup -Recipe=<toon.json> -Mesh=/Game/... [-Apply]");
}

#if WITH_EDITOR
namespace WuwaCharacterToon
{
	const TCHAR* OutlineMasterPath = TEXT("/Game/Characters/Common/Toon/M_Toon_Outline");

	/**
	 * 描边顶点偏移，按原作 M_CharacterOutline 编译后的顶点着色器（MI_R2T1CalbrenaMd10011Cloth_OL 的 shader map）：
	 * 方向是 UV1 里八面体编码的切线空间平滑法线；在裁剪空间里按宽度偏移，宽高比按 16:9（0.5625）、FOV 按 75°（0.767327）补偿；
	 * 不足一个像素时按 OutlineHoldDistance 平滑抬到约 1 像素；顶点色 B 控制往远离相机方向推（内描边）。
	 * MeshScale / PixelScale 是两处原作数值无法从数据确定的因子，见配方 reconstructionChoices。
	 */
	const TCHAR* OutlineOffsetCode = TEXT(
		"float2 o=UV1;\n"
		"float z=1.0-abs(o.x)-abs(o.y);\n"
		"float t=saturate(-z);\n"
		"float2 xy=o+t*(1.0-2.0*step(0.0,o));\n"
		"float3x3 tbn=(float3x3)Parameters.TangentToWorld;\n"
		"float3 dirWS=normalize(xy.x*tbn[0]+xy.y*tbn[1]+z*tbn[2]);\n"
		"float width=(VertexGMinWidthFactor+(1.0-VertexGMinWidthFactor)*VC.g)*OutlineClipWidth;\n"
		"float3 scale=float3(length(TransformLocalVectorToWorld(Parameters,float3(1,0,0))),length(TransformLocalVectorToWorld(Parameters,float3(0,1,0))),length(TransformLocalVectorToWorld(Parameters,float3(0,0,1))))*MeshScale;\n"
		"float3 twp=GetTranslatedWorldPosition(Parameters);\n"
		"float4x4 toClip=ResolvedView.TranslatedWorldToClip;\n"
		"float w=mul(float4(twp,1.0),toClip).w;\n"
		"float3 off=normalize(mul(dirWS,(float3x3)toClip))*(width*scale)*0.1*(0.767327/ResolvedView.ClipToView[1][1]);\n"
		"off.xy*=ResolvedView.ClipToView[1][1]/(ResolvedView.ClipToView[0][0]*0.5625);\n"
		"float2 pix=w*2.0*ResolvedView.ViewSizeAndInvSize.zw*PixelScale;\n"
		"float2 sgn=float2(off.x>0.0?1.0:-1.0,off.y>0.0?1.0:-1.0);\n"
		"float2 a=abs(off.xy);\n"
		"float2 m=min(max(pix,a),pix*999.0);\n"
		"float2 s=saturate(a/(pix*(OutlineHoldDistance*0.1)));\n"
		"s=s*s*(3.0-2.0*s);\n"
		"off.xy=(a+(m-a)*s)*sgn;\n"
		"float3 offWS=mul(off,(float3x3)ResolvedView.ClipToTranslatedWorld);\n"
		"float3 toCam=normalize(ResolvedView.TranslatedWorldCameraOrigin-twp);\n"
		"offWS+=toCam*(VC.b*InnerOutlineMulti*InnerOutlineOffset*-0.01);\n"
		"return offWS;\n");

	/** 描边颜色：UseMainTex 时漫反射 × OutlineColorTint，否则 OutlineColor；再加材质控制器的描边自发光。 */
	const TCHAR* OutlineColorCode = TEXT(
		"float3 col=saturate(lerp(OutlineColor.rgb,Tex.rgb*MainTexScale*OutlineColorTint.rgb,UseMainTex));\n"
		"return col+E_Outline_EmissionColor.rgb*E_Outline_EmissionIntensity;\n");

	template <typename T>
	T* Add(UMaterial* M, int32 X, int32 Y)
	{
		T* E = NewObject<T>(M, NAME_None, RF_Transactional);
		E->Material = M;
		E->MaterialExpressionEditorX = X;
		E->MaterialExpressionEditorY = Y;
		M->GetExpressionCollection().AddExpression(E);
		return E;
	}

	void Input(UMaterialExpressionCustom* Code, const TCHAR* Name, UMaterialExpression* Source, int32 Output = 0)
	{
		FCustomInput& In = Code->Inputs.AddDefaulted_GetRef();
		In.InputName = Name;
		In.Input.Connect(Output, Source);
	}

	template <typename T>
	T* Package(const FString& Path)
	{
		const FString Name = FPackageName::GetLongPackageAssetName(Path);
		if (T* Existing = LoadObject<T>(nullptr, *(Path + TEXT(".") + Name))) return Existing;
		UPackage* Pkg = CreatePackage(*Path);
		T* Created = NewObject<T>(Pkg, *Name, RF_Public | RF_Standalone | RF_Transactional);
		FAssetRegistryModule::AssetCreated(Created);
		return Created;
	}

	UMaterial* BuildOutlineMaster()
	{
		UMaterial* M = Package<UMaterial>(OutlineMasterPath);
		M->PreEditChange(nullptr);
		M->GetExpressionCollection().Empty();
		// 原作 _OL 材质：只画背面（bIsBackFace）、Masked 裁剪 0.3333、用于骨骼网格。这里用双面 + 正面遮罩实现只画背面。
		M->MaterialDomain = MD_Surface;
		M->BlendMode = BLEND_Masked;
		M->SetShadingModel(MSM_Unlit);
		M->TwoSided = true;
		M->OpacityMaskClipValue = 0.3333f;
		M->bUsedWithSkeletalMesh = true;
		M->bUsedWithMorphTargets = true;

		auto Scalar = [M](const TCHAR* Name, float Default, int32 Y)
		{
			auto* P = Add<UMaterialExpressionScalarParameter>(M, -1100, Y);
			P->ParameterName = Name;
			P->DefaultValue = Default;
			P->Group = TEXT("Outline");
			return P;
		};
		auto Vector = [M](const TCHAR* Name, const FLinearColor& Default, int32 Y)
		{
			auto* P = Add<UMaterialExpressionVectorParameter>(M, -1100, Y);
			P->ParameterName = Name;
			P->DefaultValue = Default;
			P->Group = TEXT("Outline");
			return P;
		};

		// 顶点偏移。默认值取原作母材质 M_CharacterOutline 的默认值。
		auto* Offset = Add<UMaterialExpressionCustom>(M, -500, 400);
		Offset->Description = TEXT("Outline offset (original M_CharacterOutline vertex shader)");
		Offset->OutputType = CMOT_Float3;
		Offset->Inputs.Reset();
		auto* UV1 = Add<UMaterialExpressionTextureCoordinate>(M, -1100, 300);
		UV1->CoordinateIndex = 1;
		Input(Offset, TEXT("UV1"), UV1);
		// 顶点色 RGB（只用 .g 宽度、.b 内描边）。
		Input(Offset, TEXT("VC"), Add<UMaterialExpressionVertexColor>(M, -1100, 360));
		Input(Offset, TEXT("OutlineClipWidth"), Scalar(TEXT("OutlineClipWidth"), 0.7f, 420));
		Input(Offset, TEXT("VertexGMinWidthFactor"), Scalar(TEXT("VertexGMinWidthFactor"), 0.f, 480));
		Input(Offset, TEXT("OutlineHoldDistance"), Scalar(TEXT("OutlineHoldDistance"), 0.5f, 540));
		Input(Offset, TEXT("InnerOutlineMulti"), Scalar(TEXT("InnerOutlineMulti"), 500.f, 600));
		Input(Offset, TEXT("InnerOutlineOffset"), Scalar(TEXT("InnerOutlineOffset"), 0.5f, 660));
		Input(Offset, TEXT("MeshScale"), Scalar(TEXT("MeshScale"), 1.f, 720));
		Input(Offset, TEXT("PixelScale"), Scalar(TEXT("PixelScale"), 1.f, 780));
		Offset->Code = OutlineOffsetCode;
		M->GetEditorOnlyData()->WorldPositionOffset.Connect(0, Offset);

		// 颜色
		auto* Color = Add<UMaterialExpressionCustom>(M, -500, 0);
		Color->Description = TEXT("Outline colour (original M_CharacterOutline pixel shader)");
		Color->OutputType = CMOT_Float3;
		Color->Inputs.Reset();
		auto* Tex = Add<UMaterialExpressionTextureSampleParameter2D>(M, -1100, -200);
		Tex->ParameterName = TEXT("MainTex");
		Tex->Group = TEXT("Outline");
		Input(Color, TEXT("Tex"), Tex);
		Input(Color, TEXT("UseMainTex"), Scalar(TEXT("UseMainTex"), 1.f, -60));
		// 原作描边贴图 _LD 缺像素时用 _D 代替，再按这个系数压暗（见 prepare_toon.py 的 mainTexSubstitute）。
		Input(Color, TEXT("MainTexScale"), Scalar(TEXT("MainTexScale"), 1.f, 240));
		Input(Color, TEXT("OutlineColor"), Vector(TEXT("OutlineColor"), FLinearColor(0, 0, 0, 1), 0), 5);
		Input(Color, TEXT("OutlineColorTint"), Vector(TEXT("OutlineColorTint"), FLinearColor(0.5f, 0.5f, 0.5f, 0), 60), 5);
		Input(Color, TEXT("E_Outline_EmissionColor"), Vector(TEXT("E_Outline_EmissionColor"), FLinearColor(0, 0, 0, 0), 120), 5);
		Input(Color, TEXT("E_Outline_EmissionIntensity"), Scalar(TEXT("E_Outline_EmissionIntensity"), 1.f, 180));
		Color->Code = OutlineColorCode;
		// 和身体一样去掉曝光（项目选择，见 M_Toon_Body 的 ExposureIndependence）。
		auto* Inverse = Add<UMaterialExpressionEyeAdaptationInverse>(M, -200, 0);
		Inverse->LightValueInput.Connect(0, Color);
		Inverse->AlphaInput.Connect(0, Scalar(TEXT("ExposureIndependence"), 1.f, -120));
		M->GetEditorOnlyData()->EmissiveColor.Connect(0, Inverse);

		// 只留背面：TwoSidedSign 背面为 -1。
		auto* Back = Add<UMaterialExpressionCustom>(M, -500, 250);
		Back->OutputType = CMOT_Float1;
		Back->Inputs.Reset();
		Input(Back, TEXT("Side"), Add<UMaterialExpressionTwoSidedSign>(M, -1100, 250));
		Back->Code = TEXT("return saturate(-Side);");
		M->GetEditorOnlyData()->OpacityMask.Connect(0, Back);

		M->PostEditChange();
		return M;
	}

	const TCHAR* BodyMasterPath = TEXT("/Game/Characters/Common/Toon/M_Toon_Body");
	const TCHAR* CollectionPath = TEXT("/Game/Characters/Common/Toon/MPC_WuwaToon");

	/**
	 * 身体卡通材质。按原作的部分：
	 *  - 基础色 = 漫反射 × BaseColorTint（原作基础 Pass；区域 ID 的皮肤染色在原作默认参数下不生效）。
	 *  - 法线：法线贴图 xy×2-1 再乘 NormalStrength，z=1（原作基础 Pass）。
	 *  - 区域 ID 的 R：≥0.9 皮肤，0.5~0.9 丝袜（原作基础 Pass 的阈值）。
	 *  - 丝袜：按 N·V 在 StockingEdgeColor 与 StockingColor 之间过渡（StockingRange），叠 StockingLightColor 高光（StockingLightRange），
	 *    按 StockingIntensity 混合（原作基础 Pass 的运算顺序；正对相机那一端的颜色参数是按语义对应的 StockingColor）。
	 *  - 脸：SDF 阴影按原作脸部基础 Pass —— 光方向投影到头部平面，光在另一侧时 U 镜像，取 SDF 贴图 B 通道，
	 *    阈值 1-B^1.5，与 (1-前方·光)/2 比较，分界在 0.5（SDFOffset 0.5 来自原作 preshader）。
	 *  - 头发 HM 贴图（原作头发基础 Pass）：R 是高光笔触，基础色 += HMHighlight × max(R-(1-N·V),0)，只在朝向相机处出现；
	 *    G 低于 HMShadowThreshold 的部分把半兰伯特压到不超过 HMShadowMax（常暗的 AO 区），按 min(G,阈值)/阈值 过渡。
	 *    0.1 / 0.2 / 0.4 是原作头发、刘海 shader map 里这几个槽位的值（烘焙数据里参数名被剥掉了，名字是项目起的）。
	 * 重建的部分（原作在库洛延迟光照里，未解码）：主光半兰伯特 + smoothstep(ShadowProcess ∓ ShadowWidth/2) 的明暗、
	 * 暗部 = 基础色 × 暗部色（皮肤用原作 SkinShadowColor1，其余用项目默认 ShadowTint）再按 SolidShadowSaturation 调饱和度、乘主光颜色。
	 */
	const TCHAR* BodyCode = TEXT(
		"float3 base=Base.rgb*BaseColorTint.rgb;\n"
		"float3x3 tbn=(float3x3)Parameters.TangentToWorld;\n"
		"float2 nxy=(Texture2DSample(NormalTex,NormalTexSampler,UV).xy*2.0-1.0)*NormalStrength;\n"
		"float3 n=normalize(lerp(tbn[2],nxy.x*tbn[0]+nxy.y*tbn[1]+tbn[2],UseNormalMap));\n"
		"float idR=Texture2DSample(IDTex,IDTexSampler,UV).r*UseIDMap;\n"
		"float skin=step(0.9,idR);\n"
		"float stocking=step(0.5,idR)*(1.0-skin);\n"
		"float ndv=dot(n,normalize(V));\n"
		"float3 st=lerp(base,base*lerp(StockingEdgeColor.rgb,StockingColor.rgb,smoothstep(StockingRangeMin,StockingRangeMax,ndv)),StockingIntensity);\n"
		"st+=smoothstep(StockingLightRangeMin,StockingLightRangeMax,saturate(ndv))*StockingLightColor.rgb;\n"
		"base=lerp(base,st,stocking*UseStocking);\n"
		"float4 hm=Texture2DSample(HMTex,HMTexSampler,UV);\n"
		"base+=UseHM*HMHighlight*max(hm.r-(1.0-saturate(ndv)),0.0);\n"
		"float3 L=normalize(MainLightDirection.xyz);\n"
		"float hl=dot(n,L)*0.5+0.5;\n"
		"float ao=saturate(min(hm.g,HMShadowThreshold)/max(HMShadowThreshold,0.01));\n"
		"hl=lerp(hl,lerp(clamp(hl,0.0,HMShadowMax),hl,ao),UseHM);\n"
		"float lit=smoothstep(ShadowProcess-ShadowWidth*0.5,ShadowProcess+ShadowWidth*0.5,hl);\n"
		"float2 l2=normalize(float2(dot(L,normalize(HeadForward.xyz)),dot(L,normalize(HeadRight.xyz)))+float2(1e-5,0.0));\n"
		"float2 suv=float2(lerp(UV.x,1.0-UV.x,step(l2.y,0.0)),UV.y);\n"
		"float B=Texture2DSample(SDFTex,SDFTexSampler,suv).b;\n"
		"float thr=1.0-pow(max(B,1e-6),1.5)*step(1e-6,B);\n"
		"float u=saturate(1.5-0.5*(thr-(1.0-l2.x)*0.5+1.0)-SDFOffset);\n"
		"float w=max(max(SDFSmoothness,ShadowWidth)*0.5,1e-4);\n"
		"lit=lerp(lit,1.0-smoothstep(0.5-w,0.5+w,u),UseSDF);\n"
		"lit=lerp(lit,1.0,ForceLit);\n"
		"float3 shadow=base*lerp(ShadowTint.rgb,SkinShadowColor.rgb,skin);\n"
		"float lum=dot(shadow,float3(0.3,0.59,0.11));\n"
		"shadow=max(lerp(lum.xxx,shadow,SolidShadowSaturation),0.0);\n"
		"return lerp(lerp(shadow,base,lit)*MainLightColor.rgb*CharacterLightTint.rgb,float3(u,B,lit),DebugSDF*UseSDF);\n");

	const TCHAR* WhitePath = TEXT("/Game/Characters/Common/Toon/T_Toon_White");

	/** 线性 1×1 白图：贴图参数的默认值（没有法线 / ID / SDF 时由开关关掉）。 */
	UTexture2D* BuildWhite()
	{
		UTexture2D* T = Package<UTexture2D>(WhitePath);
		T->PreEditChange(nullptr);
		const FColor White = FColor::White;
		T->Source.Init(1, 1, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(&White));
		T->SRGB = false;
		T->CompressionSettings = TC_VectorDisplacementmap;
		T->PostEditChange();
		return T;
	}

	UMaterialParameterCollection* BuildCollection()
	{
		UMaterialParameterCollection* C = Package<UMaterialParameterCollection>(CollectionPath);
		C->PreEditChange(nullptr);
		auto Ensure = [C](const TCHAR* Name, const FLinearColor& Default)
		{
			if (C->GetVectorParameterByName(Name)) return;
			FCollectionVectorParameter& P = C->VectorParameters.AddDefaulted_GetRef();
			P.ParameterName = Name;
			P.DefaultValue = Default;
		};
		// 默认值只在没有方向光时用：斜上方白光。
		Ensure(TEXT("MainLightDirection"), FLinearColor(0.4f, 0.3f, 0.87f, 0.f));
		Ensure(TEXT("MainLightColor"), FLinearColor::White);
		// 电影镜头（原作 Kuro 后处理体积的角色 GI 主光颜色）乘在主光上；只由镜头序列的 MPC 轨道写，平时白色。
		// MainLightColor 归 UWuwaToonLightingSubsystem 每帧按方向光写，序列不能动它。
		Ensure(TEXT("CharacterLightTint"), FLinearColor::White);
		C->PostEditChange();
		return C;
	}

	UMaterial* BuildBodyMaster(UMaterialParameterCollection* Collection, UTexture2D* White)
	{
		UMaterial* M = Package<UMaterial>(BodyMasterPath);
		M->PreEditChange(nullptr);
		M->GetExpressionCollection().Empty();
		M->GetEditorOnlyData()->EmissiveColor = FColorMaterialInput();
		M->MaterialDomain = MD_Surface;
		M->BlendMode = BLEND_Opaque;
		M->SetShadingModel(MSM_Unlit);
		// 原作角色材质实例都覆盖了 TwoSided；薄片（披风、裙摆）需要双面。
		M->TwoSided = true;
		M->bUsedWithSkeletalMesh = true;
		M->bUsedWithMorphTargets = true;

		auto* Art = Add<UMaterialExpressionCustom>(M, -500, 0);
		Art->Description = TEXT("Toon body (light/shadow reconstructed; see WuwaCharacterToonSetupCommandlet)");
		Art->OutputType = CMOT_Float3;
		Art->Inputs.Reset();
		auto* Tex = Add<UMaterialExpressionTextureSampleParameter2D>(M, -1100, -200);
		Tex->ParameterName = TEXT("BaseTex");
		Tex->Group = TEXT("Toon");
		Input(Art, TEXT("Base"), Tex);
		Input(Art, TEXT("UV"), Add<UMaterialExpressionTextureCoordinate>(M, -1100, -260));
		Input(Art, TEXT("V"), Add<UMaterialExpressionCameraVectorWS>(M, -1100, -320));
		int32 Row = 0;
		// 法线 / ID / SDF / HM 是线性数据贴图，在 HLSL 里按需采样（SDF 的 U 要按光的方向镜像）。
		auto Object = [&](const TCHAR* Name)
		{
			auto* T = Add<UMaterialExpressionTextureObjectParameter>(M, -1400, 60 * Row++);
			T->ParameterName = Name;
			T->Texture = White;
			T->SamplerType = SAMPLERTYPE_LinearColor;
			T->Group = TEXT("Toon");
			Input(Art, Name, T);
		};
		Object(TEXT("NormalTex"));
		Object(TEXT("IDTex"));
		Object(TEXT("SDFTex"));
		Object(TEXT("HMTex"));
		auto Scalar = [&](const TCHAR* Name, float Default)
		{
			auto* P = Add<UMaterialExpressionScalarParameter>(M, -1100, 60 * Row++);
			P->ParameterName = Name;
			P->DefaultValue = Default;
			P->Group = TEXT("Toon");
			Input(Art, Name, P);
		};
		auto Vector = [&](const TCHAR* Name, const FLinearColor& Default)
		{
			auto* P = Add<UMaterialExpressionVectorParameter>(M, -1100, 60 * Row++);
			P->ParameterName = Name;
			P->DefaultValue = Default;
			P->Group = TEXT("Toon");
			Input(Art, Name, P, 5);
		};
		auto Light = [&](const TCHAR* Name)
		{
			auto* P = Add<UMaterialExpressionCollectionParameter>(M, -1100, 60 * Row++);
			P->Collection = Collection;
			P->ParameterName = Name;
			P->ParameterId = Collection->GetParameterId(Name);
			Input(Art, Name, P);
		};
		// 默认值：原作母材质 M_Char_Com 没给到的用项目选择（ShadowTint、ForceLit）。
		Scalar(TEXT("ShadowProcess"), 0.5f);
		Scalar(TEXT("ShadowWidth"), 0.1f);
		Scalar(TEXT("SolidShadowSaturation"), 1.f);
		Scalar(TEXT("ForceLit"), 0.f);
		Scalar(TEXT("NormalStrength"), 1.f);
		Scalar(TEXT("UseNormalMap"), 0.f);
		Scalar(TEXT("UseIDMap"), 0.f);
		Scalar(TEXT("UseStocking"), 0.f);
		Scalar(TEXT("StockingIntensity"), 1.f);
		Scalar(TEXT("StockingRangeMin"), 0.4f);
		Scalar(TEXT("StockingRangeMax"), 1.f);
		Scalar(TEXT("StockingLightRangeMin"), 0.4f);
		Scalar(TEXT("StockingLightRangeMax"), 1.f);
		Scalar(TEXT("UseSDF"), 0.f);
		Scalar(TEXT("SDFSmoothness"), 0.02f);
		Scalar(TEXT("SDFOffset"), 0.5f);
		Scalar(TEXT("DebugSDF"), 0.f);
		Scalar(TEXT("UseHM"), 0.f);
		Scalar(TEXT("HMHighlight"), 0.1f);
		Scalar(TEXT("HMShadowThreshold"), 0.2f);
		Scalar(TEXT("HMShadowMax"), 0.4f);
		Vector(TEXT("BaseColorTint"), FLinearColor::White);
		Vector(TEXT("ShadowTint"), FLinearColor(0.72f, 0.7f, 0.78f, 1.f));
		Vector(TEXT("SkinShadowColor"), FLinearColor(0.569f, 0.549f, 0.47f, 1.f));
		Vector(TEXT("StockingColor"), FLinearColor(0.03f, 0.03f, 0.03f, 0.f));
		Vector(TEXT("StockingEdgeColor"), FLinearColor(0.014f, 0.014f, 0.014f, 0.f));
		Vector(TEXT("StockingLightColor"), FLinearColor(0.014f, 0.014f, 0.014f, 0.f));
		// 由 UWuwaToonLightingSubsystem 每帧写入（头骨朝向）。
		Vector(TEXT("HeadForward"), FLinearColor(1, 0, 0, 0));
		Vector(TEXT("HeadRight"), FLinearColor(0, 1, 0, 0));
		Light(TEXT("MainLightDirection"));
		Light(TEXT("MainLightColor"));
		Light(TEXT("CharacterLightTint"));
		Art->Code = BodyCode;
		// 项目选择：Unlit 输出除以当前曝光，卡通明暗不随场景自动曝光变化（原作的色调映射不同，未还原）。
		// ExposureIndependence=1 完全不受曝光影响，0 按场景曝光走。
		auto* Inverse = Add<UMaterialExpressionEyeAdaptationInverse>(M, -200, 0);
		Inverse->LightValueInput.Connect(0, Art);
		auto* Independence = Add<UMaterialExpressionScalarParameter>(M, -500, -120);
		Independence->ParameterName = TEXT("ExposureIndependence");
		Independence->DefaultValue = 1.f;
		Independence->Group = TEXT("Toon");
		Inverse->AlphaInput.Connect(0, Independence);
		M->GetEditorOnlyData()->EmissiveColor.Connect(0, Inverse);
		// 材质控制器的边缘光（E_Rim_*）：重建母材质会把它冲掉，这里直接加回去
		UWuwaCharacterRimSetupCommandlet::AddRimBlock(M);
		M->PostEditChange();
		return M;
	}

	UTexture2D* FindTexture(const FString& Root, const FString& Name)
	{
		IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
		Registry.ScanPathsSynchronous({ Root }, true);
		TArray<FAssetData> Assets;
		Registry.GetAssetsByPath(FName(*Root), Assets, true);
		for (const FAssetData& Asset : Assets)
		{
			if (Asset.AssetName.ToString() == Name && Asset.AssetClassPath == UTexture2D::StaticClass()->GetClassPathName())
				return Cast<UTexture2D>(Asset.GetAsset());
		}
		return nullptr;
	}

	bool Save(UObject* Asset)
	{
		UPackage* Pkg = Asset->GetOutermost();
		const FString Filename = FPackageName::LongPackageNameToFilename(Pkg->GetName(), FPackageName::GetAssetPackageExtension());
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
		FSavePackageArgs Args;
		Args.TopLevelFlags = RF_Public | RF_Standalone;
		Args.SaveFlags = SAVE_NoError;
		Args.Error = GWarn;
		Asset->MarkPackageDirty();
		const bool bSaved = UPackage::SavePackage(Pkg, Asset, *Filename, Args);
		UE_LOG(LogWuwaCharacterToon, Display, TEXT("%s %s"), bSaved ? TEXT("Saved") : TEXT("FAILED to save"), *Pkg->GetName());
		return bSaved;
	}

	bool Backup(UObject* Asset, const FString& Root)
	{
		FString Relative = Asset->GetOutermost()->GetName();
		if (!Relative.RemoveFromStart(TEXT("/Game/"))) return false;
		for (const TCHAR* Extension : { TEXT(".uasset"), TEXT(".uexp") })
		{
			const FString Source = FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(), Extension);
			if (!IFileManager::Get().FileExists(*Source)) continue;
			const FString Destination = Root / (Relative + Extension);
			if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Destination), true)
				|| IFileManager::Get().Copy(*Destination, *Source, false, true) != COPY_OK) return false;
		}
		UE_LOG(LogWuwaCharacterToon, Display, TEXT("Backed up %s to %s"), *Asset->GetName(), *Root);
		return true;
	}
}
#endif

int32 UWuwaCharacterToonSetupCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	using namespace WuwaCharacterToon;
	FString RecipePath, MeshPath;
	if (!FParse::Value(*Params, TEXT("Recipe="), RecipePath, false) || !FParse::Value(*Params, TEXT("Mesh="), MeshPath))
	{
		UE_LOG(LogWuwaCharacterToon, Error, TEXT("Usage: %s"), *HelpUsage);
		return 1;
	}
	if (FPaths::IsRelative(RecipePath)) RecipePath = FPaths::ProjectDir() / RecipePath;
	FString Text;
	TSharedPtr<FJsonObject> Recipe;
	if (!FFileHelper::LoadFileToString(Text, *RecipePath) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Recipe) || !Recipe)
	{
		UE_LOG(LogWuwaCharacterToon, Error, TEXT("Cannot read recipe %s"), *RecipePath);
		return 1;
	}
	USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, *MeshPath);
	if (!Mesh)
	{
		UE_LOG(LogWuwaCharacterToon, Error, TEXT("Missing skeletal mesh %s"), *MeshPath);
		return 1;
	}

	// 审计：原作描边需要 UV1（平滑法线）和顶点色（宽度 G / 内描边 B）。
	const FSkeletalMeshModel* Model = Mesh->GetImportedModel();
	const int32 NumTexCoords = Model && Model->LODModels.Num() ? Model->LODModels[0].NumTexCoords : 0;
	UE_LOG(LogWuwaCharacterToon, Display, TEXT("Mesh %s: LOD0 UV sets %d, vertex colours %s"), *Mesh->GetName(), NumTexCoords,
		Mesh->GetHasVertexColors() ? TEXT("yes") : TEXT("no"));
	// 原作网格本来就没有顶点色时（配方声明 meshHasNoVertexColors），着色器读到的白色和原作运行时一样，放行。
	bool bNoVertexColorsInSource = false;
	Recipe->TryGetBoolField(TEXT("meshHasNoVertexColors"), bNoVertexColorsInSource);
	if (NumTexCoords < 2 || (!Mesh->GetHasVertexColors() && !bNoVertexColorsInSource))
	{
		UE_LOG(LogWuwaCharacterToon, Error, TEXT("The outline needs UV1 and vertex colours; re-import the mesh with them."));
		return 1;
	}
	const FString MeshFolder = FPackageName::GetLongPackagePath(Mesh->GetOutermost()->GetName());
	const FString CharacterRoot = FPackageName::GetLongPackagePath(MeshFolder);

	struct FSlotPlan { int32 Index; FString Name; TSharedPtr<FJsonObject> Outline; UTexture2D* Texture = nullptr; TSharedPtr<FJsonObject> TextureToImport; };
	TArray<FSlotPlan> Plans;
	const TSharedPtr<FJsonObject> Slots = Recipe->GetObjectField(TEXT("slots"));
	TArray<FSkeletalMaterial>& Materials = Mesh->GetMaterials();
	for (int32 Index = 0; Index < Materials.Num(); ++Index)
	{
		const FString SlotName = Materials[Index].MaterialSlotName.ToString();
		const TSharedPtr<FJsonObject>* Slot = nullptr;
		if (!Slots->TryGetObjectField(SlotName, Slot)) continue;
		const TSharedPtr<FJsonObject>* Outline = nullptr;
		if (!(*Slot)->TryGetObjectField(TEXT("outline"), Outline))
		{
			UE_LOG(LogWuwaCharacterToon, Display, TEXT("Slot %s: no original outline"), *SlotName);
			continue;
		}
		FSlotPlan& Plan = Plans.AddDefaulted_GetRef();
		Plan.Index = Index;
		Plan.Name = SlotName;
		Plan.Outline = *Outline;
		FString Diffuse;
		if ((*Outline)->TryGetStringField(TEXT("diffuse"), Diffuse))
		{
			Plan.Texture = FindTexture(CharacterRoot, Diffuse);
			// 项目里还没有这张图（例如新导入的芙露德莉丝）：配方带了原图时，-Apply 时导入到 Toon/Textures
			const TSharedPtr<FJsonObject>* ImportDef = nullptr;
			if (!Plan.Texture && (*Outline)->TryGetObjectField(TEXT("diffuseTexture"), ImportDef))
			{
				Plan.TextureToImport = *ImportDef;
			}
			else if (!Plan.Texture)
			{
				UE_LOG(LogWuwaCharacterToon, Error, TEXT("Slot %s: texture %s not found under %s"), *SlotName, *Diffuse, *CharacterRoot);
				return 1;
			}
		}
		UE_LOG(LogWuwaCharacterToon, Display, TEXT("Slot %s: outline from %s, texture %s"), *SlotName,
			*(*Outline)->GetStringField(TEXT("source")), Plan.Texture ? *Plan.Texture->GetPathName() : Plan.TextureToImport ? TEXT("(imported from the recipe on -Apply)") : TEXT("(OutlineColor)"));
	}
	if (!FParse::Param(*Params, TEXT("Apply")))
	{
		UE_LOG(LogWuwaCharacterToon, Display, TEXT("Audit only. Run with -Apply to build the outline."));
		return 0;
	}

	const FString BackupRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Backups/CharacterToonSetup")
		/ FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")));
	if (!Backup(Mesh, BackupRoot)) return 1;

	UMaterial* Master = BuildOutlineMaster();
	TArray<UObject*> Changed = { Master };
	for (FSlotPlan& Plan : Plans)
	{
		if (Plan.TextureToImport)
		{
			bool bCreated = false;
			Plan.Texture = WuwaReferenceAssetImport::FindOrImportTexture(MeshFolder / TEXT("Toon/Textures"), TEXT("C:/GamePakExtractor"), *Plan.TextureToImport, bCreated);
			if (!Plan.Texture)
			{
				UE_LOG(LogWuwaCharacterToon, Error, TEXT("Slot %s: cannot import the outline texture"), *Plan.Name);
				return 1;
			}
			if (bCreated) Changed.Add(Plan.Texture);
		}
		const FString ShortName = Plan.Name.Replace(TEXT("MI_"), TEXT(""));
		UMaterialInstanceConstant* Instance = Package<UMaterialInstanceConstant>(MeshFolder / TEXT("Toon") / (TEXT("MI_") + ShortName + TEXT("_OL")));
		Instance->PreEditChange(nullptr);
		Instance->SetParentEditorOnly(Master);
		Instance->ClearParameterValuesEditorOnly();
		for (const auto& Pair : Plan.Outline->GetObjectField(TEXT("scalars"))->Values)
		{
			Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(*Pair.Key), static_cast<float>(Pair.Value->AsNumber()));
		}
		for (const auto& Pair : Plan.Outline->GetObjectField(TEXT("vectors"))->Values)
		{
			const TArray<TSharedPtr<FJsonValue>>& V = Pair.Value->AsArray();
			Instance->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(*Pair.Key),
				FLinearColor(V[0]->AsNumber(), V[1]->AsNumber(), V[2]->AsNumber(), V[3]->AsNumber()));
		}
		Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("UseMainTex")), Plan.Outline->GetBoolField(TEXT("useMainTex")) ? 1.f : 0.f);
		if (Plan.Texture) Instance->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(TEXT("MainTex")), Plan.Texture);
		Instance->PostEditChange();
		Changed.Add(Instance);
		Materials[Plan.Index].OverlayMaterialInterface = Instance;
	}
	// 身体：每个有原作主材质参数的槽位一个卡通材质实例，替换槽位材质。
	UMaterialParameterCollection* Collection = BuildCollection();
	UTexture2D* White = BuildWhite();
	UMaterial* Body = BuildBodyMaster(Collection, White);
	Changed.Add(White);
	Changed.Add(Collection);
	Changed.Add(Body);
	int32 BodySlots = 0;
	for (int32 Index = 0; Index < Materials.Num(); ++Index)
	{
		const FString SlotName = Materials[Index].MaterialSlotName.ToString();
		const TSharedPtr<FJsonObject>* Slot = nullptr;
		const TSharedPtr<FJsonObject>* Params = nullptr;
		if (!Slots->TryGetObjectField(SlotName, Slot) || !(*Slot)->TryGetObjectField(TEXT("body"), Params)) continue;
		const bool bEye = SlotName.EndsWith(TEXT("Eye"));
		const FString Part = SlotName.Replace(TEXT("MI_"), TEXT(""));
		// 道具（特效里的伞）的漫反射不在项目里，配方给出原图时导入到 Toon/Textures；角色的用项目里已有的 T_<部位>_D。
		UTexture2D* Texture = nullptr;
		const TSharedPtr<FJsonObject>* TextureList = nullptr;
		const TSharedPtr<FJsonObject>* DiffuseDef = nullptr;
		if ((*Params)->TryGetObjectField(TEXT("textures"), TextureList) && (*TextureList)->TryGetObjectField(TEXT("diffuse"), DiffuseDef))
		{
			bool bCreated = false;
			Texture = WuwaReferenceAssetImport::FindOrImportTexture(MeshFolder / TEXT("Toon/Textures"), TEXT("C:/GamePakExtractor"), **DiffuseDef, bCreated);
			if (Texture && bCreated) Changed.Add(Texture);
		}
		else
		{
			Texture = FindTexture(CharacterRoot, TEXT("T_") + Part + TEXT("_D"));
		}
		if (!Texture)
		{
			UE_LOG(LogWuwaCharacterToon, Error, TEXT("Slot %s: diffuse T_%s_D not found"), *SlotName, *Part);
			return 1;
		}
		UMaterialInstanceConstant* Instance = Package<UMaterialInstanceConstant>(MeshFolder / TEXT("Toon") / (TEXT("MI_") + Part + TEXT("_Toon")));
		Instance->PreEditChange(nullptr);
		Instance->SetParentEditorOnly(Body);
		Instance->ClearParameterValuesEditorOnly();
		for (const auto& Pair : (*Params)->GetObjectField(TEXT("scalars"))->Values)
		{
			Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(*Pair.Key), static_cast<float>(Pair.Value->AsNumber()));
		}
		// 项目选择：原作眼睛是另一个母材质 M_Char_Eye（眼睛 SDF、高光形状、半透明），还没解码；
		// 先用卡通材质常亮显示贴图本色。原来的 Lit 材质会接收刘海投影、跟着场景曝光，眼睛发黑。
		if (bEye) Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("ForceLit")), 1.f);
		const TSharedPtr<FJsonObject> Vectors = (*Params)->GetObjectField(TEXT("vectors"));
		if (const TSharedPtr<FJsonValue> Tint = Vectors->TryGetField(TEXT("BaseColorTint")))
		{
			const TArray<TSharedPtr<FJsonValue>>& V = Tint->AsArray();
			Instance->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(TEXT("BaseColorTint")), FLinearColor(V[0]->AsNumber(), V[1]->AsNumber(), V[2]->AsNumber(), V[3]->AsNumber()));
		}
		Instance->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(TEXT("BaseTex")), Texture);
		// 原作贴图（法线 / 区域 ID / 脸部 SDF / 头发 HM）导入到 Toon/Textures，并打开对应开关。
		const TSharedPtr<FJsonObject>* TextureDefs = nullptr;
		if ((*Params)->TryGetObjectField(TEXT("textures"), TextureDefs))
		{
			auto Use = [&](const TCHAR* Key, const TCHAR* Param, const TCHAR* Switch)
			{
				const TSharedPtr<FJsonObject>* Def = nullptr;
				if (!(*TextureDefs)->TryGetObjectField(Key, Def)) return true;
				bool bCreated = false;
				UTexture2D* Imported = WuwaReferenceAssetImport::FindOrImportTexture(MeshFolder / TEXT("Toon/Textures"), TEXT("C:/GamePakExtractor"), **Def, bCreated);
				if (!Imported) return false;
				if (bCreated) Changed.Add(Imported);
				Instance->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(Param), Imported);
				Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(Switch), 1.f);
				return true;
			};
			if (!Use(TEXT("normal"), TEXT("NormalTex"), TEXT("UseNormalMap")) || !Use(TEXT("id"), TEXT("IDTex"), TEXT("UseIDMap"))
				|| !Use(TEXT("sdf"), TEXT("SDFTex"), TEXT("UseSDF")) || !Use(TEXT("highlight"), TEXT("HMTex"), TEXT("UseHM"))) return 1;
		}
		const TSharedPtr<FJsonObject> Switches = (*Params)->GetObjectField(TEXT("switches"));
		bool bStocking = false;
		if (Switches->TryGetBoolField(TEXT("UseStocking"), bStocking) && bStocking)
			Instance->SetScalarParameterValueEditorOnly(FMaterialParameterInfo(TEXT("UseStocking")), 1.f);
		for (const TCHAR* Name : { TEXT("StockingColor"), TEXT("StockingEdgeColor"), TEXT("StockingLightColor") })
		{
			if (const TSharedPtr<FJsonValue> C = Vectors->TryGetField(Name))
			{
				const TArray<TSharedPtr<FJsonValue>>& V = C->AsArray();
				Instance->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(Name), FLinearColor(V[0]->AsNumber(), V[1]->AsNumber(), V[2]->AsNumber(), V[3]->AsNumber()));
			}
		}
		if (const TSharedPtr<FJsonValue> Skin = Vectors->TryGetField(TEXT("SkinShadowColor1")))
		{
			const TArray<TSharedPtr<FJsonValue>>& V = Skin->AsArray();
			Instance->SetVectorParameterValueEditorOnly(FMaterialParameterInfo(TEXT("SkinShadowColor")), FLinearColor(V[0]->AsNumber(), V[1]->AsNumber(), V[2]->AsNumber(), 1.f));
		}
		Instance->PostEditChange();
		Changed.Add(Instance);
		Materials[Index].MaterialInterface = Instance;
		++BodySlots;
		UE_LOG(LogWuwaCharacterToon, Display, TEXT("Slot %s: toon body from %s"), *SlotName, *(*Params)->GetStringField(TEXT("source")));
	}
	Mesh->PostEditChange();
	Changed.Add(Mesh);
	for (UObject* Asset : Changed)
	{
		if (!Save(Asset)) return 1;
	}
	UE_LOG(LogWuwaCharacterToon, Display, TEXT("Built the outline for %d slots and the toon body for %d slots."), Plans.Num(), BodySlots);
	return 0;
#else
	return 1;
#endif
}

#if WITH_EDITOR
#include "HAL/IConsoleManager.h"

/** 编辑器开着时跑卡通材质配置：Wuwa.Toon.Setup -Recipe=ArtSource/CharacterToon/changli.toon.json -Mesh=/Game/... -Apply */
static FAutoConsoleCommand GWuwaToonSetupCommand(
	TEXT("Wuwa.Toon.Setup"),
	TEXT("Runs WuwaCharacterToonSetup inside the editor, e.g. Wuwa.Toon.Setup -Recipe=ArtSource/CharacterToon/changli.toon.json -Mesh=/Game/Characters/Role/changli/NewModel/changli2_0 -Apply"),
	FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
	{
		UWuwaCharacterToonSetupCommandlet* Commandlet = NewObject<UWuwaCharacterToonSetupCommandlet>();
		const int32 Result = Commandlet->Main(FString::Join(Args, TEXT(" ")));
		UE_LOG(LogWuwaCharacterToon, Display, TEXT("Wuwa.Toon.Setup finished with exit code %d"), Result);
	}));
#endif
