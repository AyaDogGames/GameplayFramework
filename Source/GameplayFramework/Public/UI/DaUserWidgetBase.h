// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DaUserWidgetBase.generated.h"

/**
 * 
 */
UCLASS()
class GAMEPLAYFRAMEWORK_API UDaUserWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable)
	void SetWidgetController(UObject* InWidgetController);
	
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UObject> WidgetController;

protected:

	// Called when controller data is ready to be used in blueprint
	UFUNCTION(BlueprintImplementableEvent)
	void WidgetControllerSet();

	// subclasses should call super at end of overrided method. Base Impl Will call WidgetControllerSet Blueprint event.
	virtual void NativeWidgetControllerSet();
	
};
