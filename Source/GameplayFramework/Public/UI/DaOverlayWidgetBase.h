// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "DaOverlayWidgetController.h"
#include "DaUserWidgetBase.h"
#include "DaOverlayWidgetBase.generated.h"

struct FDaUIWidgetMessageData;
class UDaOverlayWidgetController;
/**
 * 
 */
UCLASS()
class GAMEPLAYFRAMEWORK_API UDaOverlayWidgetBase : public UDaUserWidgetBase
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "OverlayWidget")
	UDaOverlayWidgetController* GetOverlayWidgetController() { return Cast<UDaOverlayWidgetController>(WidgetController); }
	
protected:

	virtual void NativeWidgetControllerSet() override;

	UFUNCTION()
	void OnMessageDataReceived(const FDaUIWidgetMessageData& Data);
	
};
