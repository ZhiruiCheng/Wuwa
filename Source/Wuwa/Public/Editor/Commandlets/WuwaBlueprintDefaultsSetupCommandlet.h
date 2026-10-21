#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "WuwaBlueprintDefaultsSetupCommandlet.generated.h"

/**
 * 通用的蓝图默认值 / 资产属性写入：配方 JSON { source, blueprints: [ { blueprint | asset, properties: { 属性名: 文本值 } } ] }。
 * blueprint = 改蓝图生成类的默认对象；asset = 直接改资产本身（数据资产等，例如输入命令 DA 的 Rules）。
 * 文本值按 UE 的属性文本格式（和复制粘贴属性一样，如对象引用写 "/Game/A/B.B"、整数写 "10"），先全部解析，写错时不动资产。
 * 实例化子对象（例：输入规则 Conditions 里的条件对象）写在 subobjects：[{ name, class, properties }]，建在默认对象 / 资产下（同名同类复用），
 * properties 里按路径 "<资产路径>.<资产名>:<name>" 引用；类可以是 C# 类（/Script/ManagedWuwa.X）。
 * 默认只读，打印差异；-Apply 才写入，改动的蓝图先备份到 Saved/Backups/BlueprintDefaultsSetup/<时间>/。
 *   -run=WuwaBlueprintDefaultsSetup -Recipe=ArtSource/Combat/role_defaults.json [-Apply]
 */
UCLASS()
class WUWA_API UWuwaBlueprintDefaultsSetupCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UWuwaBlueprintDefaultsSetupCommandlet();
	virtual int32 Main(const FString& Params) override;
};
