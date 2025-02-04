// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "DaInteractableInterface.h"
#include "GameplayTagContainer.h"
#include "GameFramework/Actor.h"
#include "Inventory/DaInventoryItemInterface.h"
#include "DaItemActor.generated.h"

struct FDaInventoryItemData;
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
	virtual FName GetItemName_Implementation() const override { return Name; }
	virtual FName GetItemDescription_Implementation() const override { return Description; }
	virtual int32 GetItemTags_Implementation(FGameplayTagContainer& OutItemTags) const override;
	virtual UMaterialInterface* GetRenderTargetMaterial_Implementation() const override { return RenderTargetMaterial; }
	virtual UDaAbilitySet* GetAbilitySet_Implementation() const override { return OwnedAbilitySet; }
	virtual UStaticMeshComponent* GetMeshComponent_Implementation() const override { return MeshComp; }
	virtual void AddToInventory(APawn* InstigatorPawn, bool bDestroyActor = true) override;
	
	static ADaItemActor* CreateFromInventoryItem(const FDaInventoryItemData& InventoryData);
	void GrantSetToActor(UDaAbilitySystemComponent* ReceivingASC);

protected:

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Components")
	TObjectPtr<USphereComponent> SphereComp;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> MeshComp;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Components")
	TObjectPtr<UDaAbilitySystemComponent> AbilitySystemComponent;

	// Begin IDaInventoryItemInterface Support
	
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "InventoryItems")
	FName Name;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "InventoryItems")
	FName Description;

	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category = "InventoryItems")
	FGameplayTagContainer TypeTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InventoryItems|Icon")
	TObjectPtr<UMaterialInterface> RenderTargetMaterial;

	UPROPERTY(EditDefaultsOnly, Category="InventoryItems")
	TObjectPtr<UDaAbilitySet> OwnedAbilitySet;

	// END IDaInventoryItemInterface Support
	
	UPROPERTY(BlueprintReadOnly, Category="InventoryItems")
	bool bHighlighted = false;

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const override;
};
