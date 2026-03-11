// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "DaInspectableInterface.h"
#include "DaInteractableInterface.h"
#include "GameFramework/Actor.h"
#include "Inventory/DaInventoryItemInterface.h"
#include "DaInspectableItem.generated.h"

class UDaInspectableComponent;
class USphereComponent;

UCLASS()
class GAMEPLAYFRAMEWORK_API ADaInspectableItem : public AActor , public IDaInspectableInterface
	, public IDaInteractableInterface, public IDaInventoryItemInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ADaInspectableItem();

	// IDaInspectableInterface
	virtual UStaticMeshComponent* GetPreviewMeshComponent_Implementation() const override;
	virtual UStaticMeshComponent* GetDetailedMeshComponent_Implementation() const override;
	virtual void OnInspectionStarted_Implementation(APawn* InstigatorPawn) override;
	virtual void OnInspectionEnded_Implementation(APawn* InstigatorPawn) override;
	
	// Interactable interface
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	virtual void SecondaryInteract_Implementation(APawn* InstigatorPawn) override;
	virtual FText GetInteractText_Implementation(APawn* InstigatorPawn) override;

	// IDaInventoryItemInterface
	virtual FPrimaryAssetId GetItemDefinitionID_Implementation() const override { return ItemDefinitionID; }
	virtual int32 GetStackCount_Implementation() const override { return 1; }
	virtual void AddToInventory_Implementation(APawn* InstigatorPawn, bool bDestroyActor = true) override;
	
protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<USphereComponent> SphereComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> PreviewMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> DetailedMeshComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UDaInspectableComponent> InspectableComponent;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	int CustomDepthStencilValue = 250;
	
	UPROPERTY(EditAnywhere, Category = "Interaction")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Interaction")
	FText InteractionText;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	FName Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	bool bCanBeAddedToInventory;

	// The item definition this inspectable represents in the inventory system
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory")
	FPrimaryAssetId ItemDefinitionID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inventory")
	FGameplayTagContainer TypeTags;
};