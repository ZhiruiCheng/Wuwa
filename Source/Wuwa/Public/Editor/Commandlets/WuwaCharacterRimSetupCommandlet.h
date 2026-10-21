#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaCharacterRimSetupCommandlet.generated.h"

/**
 * 给角色材质加原作的「材质控制器边缘光」：参数名与原作角色母材质一致（E_Rim_UseRim / UseTex / Channel / RimRange /
 * RimColor / Intensity、E_Tex_Color），公式按原作角色母材质编译后的像素着色器：
 *   E_Rim_UseRim > 0.1 时，自发光 += pow(max(1-|N·V|, 0.001), RimRange) × RimColor.rgb × lerp(1, 通道(E_Tex_Color), UseTex) × Intensity。
 * 参数默认值取原作着色器 preshader 参数表（UseRim 0，即平时不显示）。角色材质控制器（UWuwaEffectMaterialControllerSpec）播放时改这些参数。
 *   -run=WuwaCharacterRimSetup -Mesh=/Game/Characters/Role/jiabei/Mesh/jiabei2_0 [-Apply]
 * 不带 -Apply 只列出会改的母材质。-Apply 前备份到 Saved/Backups/CharacterRimSetup；重复运行会替换上次加的节点。
 * 卡通身体母材质 M_Toon_Body 由 WuwaCharacterToonSetup 重建时会直接调 AddRimBlock，不用再单独跑本命令行。
 */
UCLASS()
class WUWA_API UWuwaCharacterRimSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaCharacterRimSetupCommandlet();
	virtual int32 Main(const FString& Params) override;

#if WITH_EDITOR
	/** 把边缘光块加到母材质的自发光上（先移除上次加的）；调用方负责 PreEditChange / PostEditChange 和保存。 */
	static void AddRimBlock(class UMaterial* Material);
#endif
};
