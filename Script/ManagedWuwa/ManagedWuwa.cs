using System.Runtime.CompilerServices;
using ManagedWuwa.Core.Tag;
using UnrealSharp.Engine.Core.Modules;

namespace ManagedWuwa;

[UnrealSharp.Attributes.UModule]
public class FManagedWuwa : IModuleInterface
{
    public void StartupModule()
    {
        RuntimeHelpers.RunClassConstructor(
            typeof(WuwaGameTags).TypeHandle
        );
    }

    public void ShutdownModule()
    {

    }
}