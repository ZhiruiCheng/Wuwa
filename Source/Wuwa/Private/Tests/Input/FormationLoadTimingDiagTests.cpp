#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR

#include "Misc/AutomationTest.h"
#include "HAL/PlatformTime.h"
#include "Game/NewWorld/Character/Role/DataAsset/WuwaRoleDataAsset.h"
#include "Game/NewWorld/Character/Role/WuwaCharacter.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"

// 只读诊断：在一个刚启动的编辑器进程里，量每个角色类 LoadSynchronous（连带网格、动画、蒙太奇等依赖）要多久。
// 要在没打开过地图的新进程里单独跑这一个测试，否则资产已经在内存里，量出来接近 0。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFormationLoadTimingDiagTest, "Diag.Formation.RoleLoadTiming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFormationLoadTimingDiagTest::RunTest(const FString& Parameters)
{
	const TCHAR* Paths[] = {
		TEXT("/Game/Characters/Role/Common/Data/DA_Role_Jiabeilina.DA_Role_Jiabeilina"),
		TEXT("/Game/Characters/Role/Common/Data/DA_Role_Changli.DA_Role_Changli"),
	};
	for (const TCHAR* Path : Paths)
	{
		double Start = FPlatformTime::Seconds();
		UWuwaRoleDataAsset* RoleData = LoadObject<UWuwaRoleDataAsset>(nullptr, Path);
		const double DataSeconds = FPlatformTime::Seconds() - Start;
		if (!RoleData)
		{
			AddError(FString::Printf(TEXT("Cannot load %s"), Path));
			continue;
		}
		Start = FPlatformTime::Seconds();
		UClass* Class = RoleData->CharacterClass.LoadSynchronous();
		const double ClassSeconds = FPlatformTime::Seconds() - Start;
		AddInfo(FString::Printf(TEXT("%s: data asset %.3fs, character class %s %.3fs"), *RoleData->GetName(), DataSeconds,
			*GetNameSafe(Class), ClassSeconds));
	}
	return true;
}

// 只读诊断：角色蓝图硬引用（加载时会被一起同步加载）的所有包，按磁盘大小排序，看是谁拖慢了加载。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFormationRoleDependencyDiagTest, "Diag.Formation.RoleDependencies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFormationRoleDependencyDiagTest::RunTest(const FString& Parameters)
{
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	Registry.SearchAllAssets(true);
	const TCHAR* Roots[] = {
		TEXT("/Game/Characters/Role/changli/BP_Role_Changli"),
		TEXT("/Game/Characters/Role/jiabei/BP_Role_jiabei"),
	};
	for (const TCHAR* RootPackage : Roots)
	{
		TSet<FName> Visited;
		TArray<FName> Queue{FName(RootPackage)};
		while (!Queue.IsEmpty())
		{
			const FName Package = Queue.Pop();
			if (Visited.Contains(Package)) continue;
			Visited.Add(Package);
			TArray<FName> Deps;
			Registry.GetDependencies(Package, Deps, UE::AssetRegistry::EDependencyCategory::Package, UE::AssetRegistry::EDependencyQuery::Hard);
			for (const FName& Dep : Deps)
			{
				if (Dep.ToString().StartsWith(TEXT("/Game/"))) Queue.Add(Dep);
			}
		}
		TArray<TPair<int64, FString>> Sizes;
		int64 Total = 0;
		TMap<FString, int64> ByFolder;
		for (const FName& Package : Visited)
		{
			FString File;
			if (!FPackageName::DoesPackageExist(Package.ToString(), &File)) continue;
			const int64 Size = IFileManager::Get().FileSize(*File);
			const FString Uexp = FPaths::ChangeExtension(File, TEXT("uexp"));
			const int64 Total1 = Size + FMath::Max<int64>(0, IFileManager::Get().FileSize(*Uexp));
			Total += Total1;
			Sizes.Emplace(Total1, Package.ToString());
			ByFolder.FindOrAdd(FPaths::GetPath(Package.ToString())) += Total1;
		}
		Sizes.Sort([](const TPair<int64, FString>& A, const TPair<int64, FString>& B) { return A.Key > B.Key; });
		AddInfo(FString::Printf(TEXT("%s: %d packages, %.1f MB on disk"), RootPackage, Visited.Num(), Total / 1048576.0));
		ByFolder.ValueSort([](int64 A, int64 B) { return A > B; });
		int32 Shown = 0;
		for (const TPair<FString, int64>& Folder : ByFolder)
		{
			if (Shown++ >= 10) break;
			AddInfo(FString::Printf(TEXT("  folder %-70s %7.1f MB"), *Folder.Key, Folder.Value / 1048576.0));
		}
		for (int32 i = 0; i < FMath::Min(12, Sizes.Num()); ++i)
		{
			AddInfo(FString::Printf(TEXT("  %7.1f MB  %s"), Sizes[i].Key / 1048576.0, *Sizes[i].Value));
		}
	}
	return true;
}

#endif
