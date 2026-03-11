// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CECollectibleData.h"
#include "CECollectibleItemInterface.h"
#include "DaInspectableItem.h"
#include "CECollectibleProxy.generated.h"

struct FCECollectibleDataDef;

// Lightweight version of a collectible Object meant for UI items
UCLASS(BlueprintType)
class COLLECTIBLES_API UCECollectibleProxy : public UObject, public IDaInventoryItemInterface, public ICECollectibleItemInterface
{
	GENERATED_BODY()

public:
	static UCECollectibleProxy* CreateCollectibleProxyFromDataRef(const FCECollectibleDataDef* DataRef, const TSubclassOf<AActor>& ClassToSpawn);

	// IDaInventoryItemInterface
	virtual FPrimaryAssetId GetItemDefinitionID_Implementation() const override { return ItemDefinitionID; }
	virtual int32 GetStackCount_Implementation() const override { return 1; }
	virtual void AddToInventory_Implementation(APawn* InstigatorPawn, bool bDestroyActor = true) override;

	// ICECollectibleItemInterface
	virtual FCECollectibleDataDef GetDataRef_Implementation() const override { return CollectibleDataRef; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory")
	FPrimaryAssetId ItemDefinitionID;

	UPROPERTY()
	FCECollectibleDataDef CollectibleDataRef;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collectible")
	UTexture2D* Thumbnail = nullptr;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collectible")
	TSubclassOf<AActor> CollectibleActorClass = nullptr;
};