// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DaAbilitySystemLibrary.generated.h"

class UDaInventoryWidgetController;
class UDaStatMenuWidgetController;
class UDaOverlayWidgetController;
/**
 * 
 */
UCLASS()
class GAMEPLAYFRAMEWORK_API UDaAbilitySystemLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	
	UFUNCTION(BlueprintPure, Category = "DaAbilitySystemLibrary|GameplayTags")
	static int32 GetAttributesWithParentTag(const FGameplayTag& ParentTag, const FGameplayTagContainer& FromContainer, FGameplayTagContainer& OutContainer);

	UFUNCTION(BlueprintPure, Category = "DaAbilitySystemLibrary|GameplayTags")
	static FGameplayTag GetLeafTag(const FGameplayTag& ParentTag, const FGameplayTagContainer& FromContainer);
	
	UFUNCTION(BlueprintPure, Category="DaAbilitySystemLibrary|WidgetController")
	static UDaOverlayWidgetController* GetOverlayWidgetController(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category="DaAbilitySystemLibrary|WidgetController")
	static UDaStatMenuWidgetController* GetStatMenuWidgetController(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category="DaAbilitySystemLibrary|WidgetController")
	static UDaInventoryWidgetController* GetInventoryMenuWidgetController(const UObject* WorldContextObject);
};
