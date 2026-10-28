using UnrealSharp.GameplayTags;

namespace ManagedWuwa.Core.Tag;

public static class WuwaGameTags
{
    public static readonly FGameplayTag Role_Changli =
        new("Role.Changli", "长离");

    public static readonly FGameplayTag Element_Fusion =
        new("Element.Fusion", "热熔");

    public static readonly FGameplayTag Element_Glacio =
        new("Element.Glacio", "冷凝");

    public static readonly FGameplayTag Element_Electro =
        new("Element.Electro", "导电");

    public static readonly FGameplayTag Element_Aero =
        new("Element.Aero", "气动");

    public static readonly FGameplayTag Element_Spectro =
        new("Element.Spectro", "衍射");

    public static readonly FGameplayTag Element_Havoc =
        new("Element.Havoc", "湮灭");
}