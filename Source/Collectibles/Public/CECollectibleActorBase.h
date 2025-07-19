// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CECollectibleItemInterface.h"
#include "DaInspectableInterface.h"
#include "DaInteractableInterface.h"
#include "GameFramework/Actor.h"
#include "Inventory/DaInventoryItemInterface.h"
#include "CECollectibleActorBase.generated.h"

class UCommonActivatableWidget;
class UDaInspectableComponent;
class UDaActivatableWidget;

UCLASS()
class COLLECTIBLES_API ACECollectibleActorBase : public AActor, public ICECollectibleItemInterface, public IDaInventoryItemInterface, public IDaInspectableInterface, public IDaInteractableInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ACECollectibleActorBase();
	
	UPROPERTY(blueprintReadOnly, Category = "Collectible")
	TObjectPtr<UCECollectibleData> CollectibleData = nullptr;
	
	UPROPERTY(BlueprintReadWrite, Category = "Collectible")
	bool bIsMoving = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Collectible")
	TObjectPtr<UMaterialInterface> RenderTargetMaterial;

	UFUNCTION(BlueprintImplementableEvent, Category = "Collectible")
	UStaticMeshComponent* GetPhysicsMesh();
	
protected:

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Components")
	TObjectPtr<USceneComponent> RootSceneComp;
	
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> PreviewMeshComp;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> DetailMeshComp;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Components")
	TObjectPtr<UDaInspectableComponent> InspectableComp;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Collectible")
	TSubclassOf<UCommonActivatableWidget> AppraisalWidgetClass;

	UPROPERTY(BlueprintReadOnly, Category = "Collectible")
	TObjectPtr<UCommonActivatableWidget> AppraisalWidget = nullptr;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Collectible")
	int32 DepthStencilValue_NotAppraised = 250;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Collectible")
	int32 DepthStencilValue_NotAppraisedHover = 251;

	UPROPERTY(BlueprintReadWrite, EditDefaultsOnly, Category = "Collectible")
	int32 DepthStencilValue_AppraisedHover = 252;
	
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// ICECollectibleItemInterface Begin
	virtual int32 GetItemCoreTags_Implementation(FGameplayTagContainer& OutItemTags) const override;
	virtual USlateBrushAsset* GetItemBrush_Implementation() const override;
	virtual UTexture2D* GetItemIcon_Implementation() const override;
	virtual FCECollectibleDataDef GetDataRef_Implementation() const override;
	virtual void SetCollectibleData_Implementation(UCECollectibleData* Data) override;
	virtual FString GetCollectibleDisplayNames_Implementation(bool bPlayerDesignated) override;
	virtual void SaveCollectible_Implementation(bool bSaveToDisk) override;
	// ICECollectibleItemInterface End

	// IDaInventoryItemInterface Begin
	virtual FName GetItemName_Implementation() const override;
	virtual FName GetItemDescription_Implementation() const override;
	virtual int32 GetItemTags_Implementation(FGameplayTagContainer& OutItemTags) const override;
	virtual UTexture2D* GetItemThumbnail_Implementation() const override;
	virtual UMaterialInterface* GetRenderTargetMaterial_Implementation() const override;
	virtual UStaticMeshComponent* GetMeshComponent_Implementation() const override;
	virtual UDaAbilitySet* GetAbilitySet_Implementation() const override;
	virtual void AddToInventory_Implementation(APawn* InstigatorPawn, bool bDestroyActor) override;
	// IDaInventoryItemInterface End

	// IDaInspectableInterface Begin
	virtual void OnInspectionStarted_Implementation(APawn* InstigatorPawn) override;
	virtual void OnInspectionEnded_Implementation(APawn* InstigatorPawn) override;
	virtual UStaticMeshComponent* GetPreviewMeshComponent_Implementation() const override;
	virtual UStaticMeshComponent* GetDetailedMeshComponent_Implementation() const override;
	// IDaInspectableInterface End

	// IDaInteractableInterface Begin
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	virtual void SecondaryInteract_Implementation(APawn* InstigatorPawn) override;
	virtual FText GetInteractText_Implementation(APawn* InstigatorPawn) override;
	virtual void HighlightActor_Implementation() override;
	virtual void UnHighlightActor_Implementation() override;
	// IDaInteractableInterface End
	
	
};
