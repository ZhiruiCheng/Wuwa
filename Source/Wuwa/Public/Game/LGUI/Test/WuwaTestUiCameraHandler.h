#pragma once

#include "CoreMinimal.h"
#include "Game/LGUI/Core/WuwaUiCameraHandler.h"
#include "WuwaTestUiCameraHandler.generated.h"

/** 测试用的 UI 相机处理器：只记录收到的 Push/Pop，真正的实现在相机系统里。 */
UCLASS()
class WUWA_API UWuwaTestUiCameraHandler : public UObject, public IWuwaUiCameraHandler
{
	GENERATED_BODY()
public:
	virtual void PushUiCamera(int32 ViewId, FName ViewName, FName CameraMode) override
	{
		Stack.Add({ViewId, CameraMode});
		Log.Add(FString::Printf(TEXT("Push %s %s"), *ViewName.ToString(), *CameraMode.ToString()));
	}

	virtual void PopUiCamera(int32 ViewId) override
	{
		Stack.RemoveAll([ViewId](const TPair<int32, FName>& Entry) { return Entry.Key == ViewId; });
		Log.Add(FString::Printf(TEXT("Pop %d"), ViewId));
	}

	/** 当前生效的相机（栈顶）；没有返回 None。 */
	FName GetActiveCameraMode() const { return Stack.IsEmpty() ? NAME_None : Stack.Last().Value; }

	TArray<TPair<int32, FName>> Stack;
	TArray<FString> Log;
};
