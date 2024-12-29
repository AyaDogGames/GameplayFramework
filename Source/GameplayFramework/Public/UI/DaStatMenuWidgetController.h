// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "DaWidgetController.h"
#include "GameplayTagContainer.h"
#include "DaStatMenuWidgetController.generated.h"

struct FDaAttributeData;
class UDaAttributeInfo;

/**
 * 
 */
UCLASS(BlueprintType, Blueprintable)
class GAMEPLAYFRAMEWORK_API UDaStatMenuWidgetController : public UDaWidgetController
{
	GENERATED_BODY()

public:

	UFUNCTION(BlueprintCallable, Category = "StatMenuController|GameplayTags")
	FGameplayTagContainer GetPrimaryAttributeTags();

	UFUNCTION(BlueprintCallable, Category = "StatMenuController|GameplayTags")
	FGameplayTagContainer GetSecondaryAttributeTags();

private:

	FGameplayTagContainer GetAttributeTags(const FGameplayTag& ParentTag) const;
	
};
