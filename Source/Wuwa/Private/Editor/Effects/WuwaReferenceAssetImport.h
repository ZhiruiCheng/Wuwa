#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class UMaterial;
class UStaticMesh;
class UTexture2D;

/**
 * 把 ArtSource 配方里的原件（PNG 贴图、Geometry/*.json 网格）和重建 shader 变成项目资产。
 * 取自旧刀光导入工具 WuwaSlashFxImportCommandlet（f7eb7ad）的导入部分；不保存资产，由调用方统一保存。
 */
namespace WuwaReferenceAssetImport
{
	/** 已存在则直接沿用（不重新导入），否则从配方的 file 导入。 */
	UTexture2D* FindOrImportTexture(const FString& Folder, const FString& SourceRoot, const FJsonObject& Def, bool& bOutCreated);

	/** 已存在则直接沿用，否则从 SourceRoot/Geometry/<Name>.json 生成静态网格。 */
	UStaticMesh* FindOrImportMesh(const FString& Folder, const FString& SourceRoot, const FString& Name, bool& bOutCreated);

	/** 按层定义（shader / wpo / textures / intensity / sprite）新建或整体重建 M_<id>。 */
	UMaterial* BuildMaterial(const FString& Folder, const FJsonObject& Layer, const TMap<FString, UTexture2D*>& Textures);

	/**
	 * 贴花材质（DeferredDecal、半透明、无光照、自发光）：shader 是重建的 HLSL，输入 UV（贴花投影 UV）、DecalPos（贴花空间 0~1，z 为投影方向），
	 * 以及定义里 scalars / vectors 列出的材质参数（参数名即 HLSL 变量名，EffectModelDecal 的曲线按名写入）。
	 */
	UMaterial* BuildDecalMaterial(const FString& Folder, const FJsonObject& Def, const TMap<FString, UTexture2D*>& Textures);

	/**
	 * 径向模糊后处理材质（EffectModelPostProcess 的 BlurIntensity 用）：参数 Intensity / Center（视口 UV）/ Radius / Hardness。
	 * 库洛原生的径向模糊着色器不在导出里，公式是重建选择；材质不含原作数据，可以提交。
	 */
	UMaterial* BuildRadialBlurMaterial(const FString& PackageName);

	/**
	 * 角色残影材质（EffectModelGhost.MaterialRef 的重建）：Surface、Masked、Unlit，用于骨骼网格。
	 * shader 是重建的 HLSL，输入 N（顶点法线）、V（相机方向）、Fade（CustomPrimitiveData[0]，残影 Spec 写入）
	 * 以及定义里 scalars / vectors 列出的材质参数；输出 rgb = 自发光，a = 不透明度遮罩（裁剪值取定义的 clip）。
	 */
	UMaterial* BuildGhostMaterial(const FString& Folder, const FJsonObject& Def);
}
