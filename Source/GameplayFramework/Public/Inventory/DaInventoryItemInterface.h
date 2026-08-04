// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DaInventoryItemInterface.generated.h"

UINTERFACE(Blueprintable)
class GAMEPLAYFRAMEWORK_API UDaInventoryItemInterface : public UInterface
{
	GENERATED_BODY()
};

class GAMEPLAYFRAMEWORK_API IDaInventoryItemInterface
{
	GENERATED_BODY()

public:

	// Returns the PrimaryAssetId of the UDaItemDefinition for this item
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Inventory")
	FPrimaryAssetId GetItemDefinitionID() const;

	// How many of this item the pickup represents (default 1)
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Inventory")
	int32 GetStackCount() const;

	// Add this item to the instigator's inventory
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Inventory")
	void AddToInventory(APawn* InstigatorPawn, bool bDestroyActor = true);
};
