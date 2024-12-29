// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "DaWidgetController.h"
#include "GameplayTagContainer.h"

#include "DaOverlayWidgetController.generated.h"

class UDaWidgetMessageData;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMessageWidgetDataSignature, const FDaUIWidgetMessageData&, Data);

/**
 * 
 */
UCLASS(BlueprintType, Blueprintable)
class GAMEPLAYFRAMEWORK_API UDaOverlayWidgetController : public UDaWidgetController
{
	GENERATED_BODY()

public:
	
	UPROPERTY(BlueprintAssignable, Category="UI Messages")
	FMessageWidgetDataSignature MessageWidgetDataDelegate;
	
	UDaOverlayWidgetController();
	
	virtual void BindCallbacksToDependencies() override;

protected:

	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category="UI Data")
	FGameplayTag MessageParentTag;

	// Messages will be broadcast from data asset if they match the message parent tag
	UPROPERTY(EditDefaultsOnly, Category="UI Data")
	TObjectPtr<UDaWidgetMessageData> MessageDataAsset;
	
};