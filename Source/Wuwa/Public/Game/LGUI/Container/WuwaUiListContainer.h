#pragma once

#include "CoreMinimal.h"
#include "Game/LGUI/Container/WuwaUiContainer.h"
#include "WuwaUiListContainer.generated.h"

/** 列表，同一个界面可以开多个实例（≈ 鸣潮 UiViewListContainer）：Pop / Guide。 */
UCLASS()
class WUWA_API UWuwaUiListContainer : public UWuwaUiContainer
{
	GENERATED_BODY()
public:
	virtual void OpenView(UWuwaUiView* View, FWuwaUiDone OnDone) override;
	virtual void CloseView(UWuwaUiView* View, FWuwaUiDone OnDone) override;
	virtual void ClearContainer() override;
	virtual void GetViews(TArray<UWuwaUiView*>& OutViews) const override;

private:
	UPROPERTY(Transient) TArray<TObjectPtr<UWuwaUiView>> Views;
};
