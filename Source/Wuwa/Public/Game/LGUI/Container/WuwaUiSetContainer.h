#pragma once

#include "CoreMinimal.h"
#include "Game/LGUI/Container/WuwaUiContainer.h"
#include "WuwaUiSetContainer.generated.h"

/** 按名字存，同名只能有一个（≈ 鸣潮 UiViewSetContainer）：HUD / Loading / Debug。 */
UCLASS()
class WUWA_API UWuwaUiSetContainer : public UWuwaUiContainer
{
	GENERATED_BODY()
public:
	virtual void OpenView(UWuwaUiView* View, FWuwaUiDone OnDone) override;
	virtual void CloseView(UWuwaUiView* View, FWuwaUiDone OnDone) override;
	virtual void ClearContainer() override;
	virtual void GetViews(TArray<UWuwaUiView*>& OutViews) const override;

private:
	void RemoveIfSame(UWuwaUiView* View);

	UPROPERTY(Transient) TMap<FName, TObjectPtr<UWuwaUiView>> Views;
};
