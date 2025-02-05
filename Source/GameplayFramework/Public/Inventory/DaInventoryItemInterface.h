// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "DaInventoryItemInterface.generated.h"

class UDaAbilitySet;
struct FDaInventoryItemData;

// This class does not need to be modified.
UINTERFACE()
class GAMEPLAYFRAMEWORK_API UDaInventoryItemInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class GAMEPLAYFRAMEWORK_API IDaInventoryItemInterface
{
	GENERATED_BODY()

public:
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "InventoryItems")
	FName GetItemName() const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "InventoryItems")
	FName GetItemDescription() const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "InventoryItems")
	int32 GetItemTags(FGameplayTagContainer& OutItemTags) const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "InventoryItems")
	UTexture2D* GetItemThumbnail() const;
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "InventoryItems")
	UMaterialInterface* GetRenderTargetMaterial() const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "InventoryItems")
	UStaticMeshComponent* GetMeshComponent() const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "InventoryItems")
	UDaAbilitySet* GetAbilitySet() const;

	UFUNCTION(BlueprintImplementableEvent)
	void ItemAddedToInventory();
	
	virtual void AddToInventory(APawn* InstigatorPawn, bool bDestroyActor = true) = 0;
};
