// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "DaInteractableInterface.h"
#include "GameFramework/Actor.h"
#include "Inventory/DaInventoryItemInterface.h"
#include "DaInspectableItem.generated.h"

class USphereComponent;
struct FStreamableHandle;

UENUM(BlueprintType)
enum class EInspectAlignment : uint8
{
	Center UMETA(DisplayName = "Center"),
	Left UMETA(DisplayName = "Left"),
	Right UMETA(DisplayName = "Right")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnInspectStateChanged,  ADaInspectableItem*, InspectedItem, AActor*, InspectingActor, bool, IsInspecting);

UCLASS()
class GAMEPLAYFRAMEWORK_API ADaInspectableItem : public AActor , public IDaInteractableInterface, public IDaInventoryItemInterface
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ADaInspectableItem();

	// Interactable interface
	virtual void Interact_Implementation(APawn* InstigatorPawn) override;
	virtual void SecondaryInteract_Implementation(APawn* InstigatorPawn) override;
	virtual FText GetInteractText_Implementation(APawn* InstigatorPawn) override;

	// IDaInventoryItemInterface
	virtual FName GetItemName_Implementation() const override { return Name; }
	virtual FName GetItemDescription_Implementation() const override { return Description; }
	virtual int32 GetItemTags_Implementation(FGameplayTagContainer& OutItemTags) const override;
	virtual UMaterialInterface* GetRenderTargetMaterial_Implementation() const override { return RenderTargetMaterial; }
	virtual UDaAbilitySet* GetAbilitySet_Implementation() const override { return nullptr; }
	virtual UStaticMeshComponent* GetMeshComponent_Implementation() const override { return PreviewMeshComponent; }
	virtual void AddToInventory(APawn* InstigatorPawn, bool bDestroyActor = true) override;	
	
	// Inspectable
	UFUNCTION(BlueprintCallable, Category = "Inspect")
	void Inspect(APawn* InstigatorPawn, float ViewportPercentage = 0.5f, EInspectAlignment Alignment = EInspectAlignment::Center);

	UFUNCTION(BlueprintCallable, Category = "Inspect")
	void HideDetailedMesh();

	UFUNCTION(BlueprintCallable, Category = "Inspect")
	void RotateDetailedMesh(float DeltaPitch, float DeltaYaw);

	UFUNCTION(BlueprintCallable, Category = "Inspect")
	void ZoomDetailedMesh(float DeltaZoom);

	UPROPERTY(BlueprintAssignable, Category="PickupItem")
	FOnInspectStateChanged OnInspectStateChanged;
	
protected:
	
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Components")
	TObjectPtr<USphereComponent> SphereComp;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> PreviewMeshComponent;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, Category="Components")
	TObjectPtr<UStaticMeshComponent> InspectMeshComponent;

	UPROPERTY(EditAnywhere, Category = "Inspect")
	TSoftObjectPtr<UStaticMesh> DetailedMeshPtr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Inspect")
	int CustomDepthStencilValue = 250;
	
	// Viewport configuration
	UPROPERTY(EditAnywhere, Category = "Inspect")
	float ViewportPercentage = 0.6f;

	UPROPERTY(EditAnywhere, Category = "Inspect")
	float CameraDistanceMultiplier = 1.2f;

	// Initial Forward Offset from Viewport
	UPROPERTY(EditAnywhere, Category = "Inspect")
	float InitialCameraDistanceOffset;
	
	UPROPERTY(EditAnywhere, Category = "Inspect")
	float MinCameraDistance;

	UPROPERTY(EditAnywhere, Category = "Inspect")
	float MaxCameraDistance;

	// exponential smoothing, Lower = smoother
	UPROPERTY(EditAnywhere, Category = "Inspect")
	float ZoomSmoothingFactor;

	// Smooth rotation transition, Higher = faster
	UPROPERTY(EditAnywhere, Category = "Inspect")
	float RotationSmoothingSpeed;
	
	UPROPERTY(EditAnywhere, Category = "Inspect")
	EInspectAlignment InspectAlignment = EInspectAlignment::Center;

	UPROPERTY(EditAnywhere, Category = "Inspect")
	float AlignmentShiftMultiplier = 0.2f;
	
	UPROPERTY(EditAnywhere, Category = "Inspect")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InventoryItems")
	FName Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InventoryItems")
	FGameplayTagContainer TypeTags;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "InventoryItems|Icon")
	TObjectPtr<UMaterialInterface> RenderTargetMaterial;
	
	UFUNCTION(BlueprintNativeEvent, Category="Inspect")
	void PlaceDetailMeshInView();

	virtual void Tick(float DeltaSeconds) override;

private:

	UPROPERTY()
	UStaticMesh* LoadedDetailedMesh;
	
	void LoadDetailMesh();
	void OnDetailedMeshLoaded();
	
	// Asset loading handles
	TSharedPtr<FStreamableHandle> DetailedMeshHandle;

	bool bIsInspecting = false;

	UPROPERTY()
	APawn* InspectingPawn;

	float CurrentViewportPercentage;
	EInspectAlignment CurrentAlignment;
	
	// Mesh properties
	float CameraDistance;
	FRotator CurrentRotation;
	FVector CurrentLocation;

	// Rotation deltas set via RotateDetailedMesh function
	float InputDeltaPitch;
	float InputDeltaYaw;

	// Zoom delta set via ZoomDetailedMesh function
	float InputDeltaZoom;

	// Bounds and scaling
	float ScaleFactor;
	FVector CenteringOffset;

	// Utility functions
	void UpdateMeshTransform(float DeltaTime);
	
};
