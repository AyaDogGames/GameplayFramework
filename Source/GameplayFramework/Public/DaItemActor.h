// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "DaInteractableInterface.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "Inventory/DaInventoryItemInterface.h"
#include "DaItemActor.generated.h"

class UDaAbilitySystemComponent;
class UDaAbilitySet;
class USphereComponent;

UCLASS(BlueprintType, Blueprintable)
class GAMEPLAYFRAMEWORK_API ADaItemActor : public AActor, public IDaInteractableInterface, public IAbilitySystemInterface, public IDaInventoryItemInterface
{
	GENERATED_BODY()

public:
	ADaItemActor();

	// IDaInteractableInterface
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	virtual void SecondaryInteract_Implementation(APawn* InstigatorPawn) override;
	virtual FText GetInteractText_Implementation(APawn* InstigatorPawn) override;
	virtual void HighlightActor_Implementation() override;
	virtual void UnHighlightActor_Implementation() override;

	// IAbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;

	// IDaInventoryItemInterface
	virtual FPrimaryAssetId GetItemDefinitionID_Implementation() const override { return ItemDefinitionID; }
	virtual int32 GetStackCount_Implementation() const override { return 1; }
	virtual void AddToInventory_Implementation(APawn* InstigatorPawn, bool bDestroyActor = true) override;

	void GrantSetToActor(UDaAbilitySystemComponent* ReceivingASC);

protected:

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Components")
	TObjectPtr<USphereComponent> SphereComp;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Components")
	TObjectPtr<UDaAbilitySystemComponent> AbilitySystemComponent;

	// The item definition this actor represents in the inventory system
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory")
	FPrimaryAssetId ItemDefinitionID;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "InventoryItems")
	FName Name;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "InventoryItems")
	FName Description;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "InventoryItems")
	FGameplayTagContainer TypeTags;

	UPROPERTY(EditDefaultsOnly, Category="InventoryItems")
	TObjectPtr<UDaAbilitySet> OwnedAbilitySet;

	UPROPERTY(BlueprintReadOnly, Category="InventoryItems")
	bool bHighlighted = false;

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
};
