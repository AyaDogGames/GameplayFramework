// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "DaUserWidgetBase.h"
#include "DaPanelWidget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnPanelClosed);

/**
 * 
 */
UCLASS()
class GAMEPLAYFRAMEWORK_API UDaPanelWidget : public UDaUserWidgetBase
{
	GENERATED_BODY()

public:

	UPROPERTY(BlueprintAssignable);
	FOnPanelClosed OnPanelClosed;

protected:

	virtual void NativeDestruct() override;
};
