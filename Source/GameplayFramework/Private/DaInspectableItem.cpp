// Copyright Dream Awake Solutions LLC

#include "DaInspectableItem.h"

#include "DaCharacter.h"
#include "DaPlayerState.h"
#include "GameplayFramework.h"
#include "Components/SphereComponent.h"
#include "Engine/AssetManager.h"
#include "Inventory/DaInventoryComponent.h"

#define LOCTEXT_NAMESPACE "InspectableItems"

static TAutoConsoleVariable<bool> CVarInspectDebug(TEXT("da.InspectDebug"), false, TEXT("Log Inspectable Item Debug Info"), ECVF_Cheat);
static TAutoConsoleVariable<bool> CVarInspectTickDebug(TEXT("da.InspectOnTickDebug"), false, TEXT("Log Inspectable Item on tick Debug Info"), ECVF_Cheat);

ADaInspectableItem::ADaInspectableItem()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	
	SphereComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	RootComponent = SphereComp;
	
	PreviewMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewMesh"));
	PreviewMeshComponent->SetupAttachment(RootComponent);
	PreviewMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	InspectMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("InspectMesh"));
	InspectMeshComponent->SetupAttachment(RootComponent);
	InspectMeshComponent->SetVisibility(false); // Initially hidden
	InspectMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Initialize defaults
	CameraDistance = 1.2f;
	MinCameraDistance = 5.0f;
	MaxCameraDistance = 500.0f;
	InitialCameraDistanceOffset = 30.0f;

	ZoomSmoothingFactor = 0.2f;
	RotationSmoothingSpeed = 30.0f;
	
	InputDeltaPitch = 0.0f;
	InputDeltaYaw = 0.0f;
	InputDeltaZoom = 0.0f;

	CurrentRotation = FRotator::ZeroRotator;
	CurrentLocation = FVector::ZeroVector;

	ScaleFactor = 1.0f;
}

void ADaInspectableItem::AddToInventory(APawn* InstigatorPawn, bool bDestroyActor)
{
	checkf(TypeTags.IsValid(), TEXT("ADaInspectableItem: TypeTags is not valid!"));

	if (APlayerState* PS = InstigatorPawn->GetPlayerState())
	{
		UDaInventoryComponent* InventoryComponent = Cast<ADaPlayerState>(PS)->GetInventoryComponent();
		if (InventoryComponent)
		{
			if (InventoryComponent->AddItem(this) && HasAuthority())
			{
				Execute_ItemAddedToInventory(this);

				if (bDestroyActor)
					Destroy();
			}
		}
	}
}

void ADaInspectableItem::Inspect(APawn* InstigatorPawn, float ViewportPct,
                                 EInspectAlignment Alignment)
{
	// This is just for viewing on clients
	if (!InstigatorPawn->IsLocallyControlled()) return;

	if (bIsInspecting)
	{
		HideDetailedMesh();
		return;
	}
	
	// Hold a reference for tick
	InspectingPawn = InstigatorPawn;
	CurrentViewportPercentage = ViewportPct;
	CurrentAlignment = Alignment;
	InspectMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	
	if (LoadedDetailedMesh == nullptr)
	{
		LoadDetailMesh();
	}
	else
	{
		InspectMeshComponent->SetVisibility(true);
		PreviewMeshComponent->SetVisibility(false);
		
		PlaceDetailMeshInView();
	}
}

int32 ADaInspectableItem::GetItemTags_Implementation(FGameplayTagContainer& OutItemTags) const
{
	OutItemTags = TypeTags;
	return OutItemTags.IsValid() ? OutItemTags.Num() : 0;
}

void ADaInspectableItem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bIsInspecting && LoadedDetailedMesh)
	{
		// TODO: Do this based on player movement events or anything that marks dirty and update needed, rather than tick
		UpdateMeshTransform(DeltaSeconds);
	}
}

void ADaInspectableItem::UpdateMeshTransform(float DeltaTime)
{
	if (!InspectMeshComponent)
		return;
	
	// Update rotation
	FRotator NewRotation = CurrentRotation;
	NewRotation.Pitch += InputDeltaPitch;
	NewRotation.Yaw += InputDeltaYaw;

	// Update camera distance
	CameraDistance = FMath::Clamp(CameraDistance - InputDeltaZoom * 10.0f, MinCameraDistance, MaxCameraDistance);
	
	// Reset input deltas (processed for this tick)
	InputDeltaPitch = 0.0f;
	InputDeltaYaw = 0.0f;
	InputDeltaZoom = 0.0f;
	
	APlayerController* PlayerController = InspectingPawn->GetLocalViewingPlayerController();
	if (PlayerController && PlayerController->PlayerCameraManager)
	{
		FVector CameraLocation;
		FRotator CameraRotation;
		PlayerController->PlayerCameraManager->GetCameraViewPoint(CameraLocation, CameraRotation);
		FVector ForwardVector = CameraRotation.Vector();
		
		// Calculate new mesh location
		// Note: need to add an slight offset to push mesh away from camera since the CameraDistance was previously used to calculate the scale factor for the mesh
		FVector ViewportPosition = CameraLocation + ForwardVector * (CameraDistance+InitialCameraDistanceOffset);
		FVector NewLocation = ViewportPosition - CenteringOffset;

		// Smooth the transition using exponential smoothing
		CurrentLocation = FMath::VInterpTo(CurrentLocation, NewLocation, DeltaTime, ZoomSmoothingFactor / DeltaTime);

		// Smooth rotation transition, Higher = faster
		CurrentRotation = FMath::RInterpTo(CurrentRotation, NewRotation, DeltaTime, RotationSmoothingSpeed);
		
		// Apply updated transform
		InspectMeshComponent->SetWorldLocation(CurrentLocation);
		InspectMeshComponent->SetWorldRotation(CurrentRotation);

		if (CVarInspectTickDebug.GetValueOnGameThread())
		{
			LOG("CameraDistance: %f, CameraLocation: %s, CameraRotationForwardVector: %s", CameraDistance, *CameraLocation.ToString(), *ForwardVector.ToString());
			LOG("ViewportPosition: %s, NewLocation: %s, CurrentRotation: %s", *ViewportPosition.ToString(), *NewLocation.ToString(), *CurrentRotation.ToString());
			DrawDebugSphere(GetWorld(), ViewportPosition, 10.0f, 12, FColor::Red, false, 1.0f);
			DrawDebugSphere(GetWorld(), NewLocation, 10.0f, 12, FColor::Green, false, 1.0f);
		}
	}
	
}


void ADaInspectableItem::LoadDetailMesh()
{
	// Start async loading of the detailed mesh
	DetailedMeshHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		DetailedMeshPtr.ToSoftObjectPath(),
		FStreamableDelegate::CreateUObject(this, &ADaInspectableItem::OnDetailedMeshLoaded)
	);
}

void ADaInspectableItem::OnDetailedMeshLoaded()
{
	if (DetailedMeshPtr.IsValid())
	{
		LoadedDetailedMesh = Cast<UStaticMesh>(DetailedMeshPtr.Get());
		if (LoadedDetailedMesh)
		{
			InspectMeshComponent->SetStaticMesh(LoadedDetailedMesh);
			
			InspectMeshComponent->SetVisibility(true);
			PreviewMeshComponent->SetVisibility(false);
			
			PlaceDetailMeshInView();
		}
	}
	else
	{
		LOG_ERROR("Failed to load detailed mesh.");
		HideDetailedMesh();
	}
}

// Adjust mesh placement in the viewport
void ADaInspectableItem::PlaceDetailMeshInView_Implementation()
{
	check(InspectingPawn);

	// Apply initial scaling
	InspectMeshComponent->SetWorldScale3D(FVector(1.0f));
	InspectMeshComponent->MarkRenderStateDirty();
	InspectMeshComponent->UpdateBounds();
	
	// Calculate initial mesh fit (center, scale, and position)
	FBoxSphereBounds MeshBounds = InspectMeshComponent->Bounds;
	FVector MeshOrigin = MeshBounds.Origin;
	FVector MeshExtent = MeshBounds.BoxExtent;
	float MeshDiameter = MeshExtent.GetMax() * 2.0f;

	// Calculate required scale to fit some percentage of the viewport
	APlayerController* PlayerController = InspectingPawn->GetLocalViewingPlayerController();
	if (PlayerController && PlayerController->PlayerCameraManager)
	{
		FVector CameraLocation;
		FRotator CameraRotation;
		PlayerController->PlayerCameraManager->GetCameraViewPoint(CameraLocation, CameraRotation);
		
		float FOV = PlayerController->PlayerCameraManager->GetFOVAngle();
		float FOVRads = FMath::DegreesToRadians(FOV * 0.5f);
		float FOVHalfHeight = FMath::Tan(FOVRads);
		
		// Dynamically adjust camera distance to fit the mesh
		CameraDistance = (MeshDiameter / 2.0f) / FOVHalfHeight;
		
		// Add a multiplier to slightly increase the distance for a better fit and clamp to avoid clipping
		CameraDistance = FMath::Clamp(CameraDistance*CameraDistanceMultiplier, MinCameraDistance, MaxCameraDistance);
		
		// Calculate the viewport height in world units using the dynamic CameraDistance
		float ViewportHeightWorldUnits = CameraDistance * FOVHalfHeight * 2.0f;
		
		// Calculate desired size in world units
		float DesiredHeight = CurrentViewportPercentage * ViewportHeightWorldUnits;
		
		// Calculate the scaling factor to make the mesh fit the desired height
		ScaleFactor = DesiredHeight / MeshDiameter;
		
		// Apply Initial Scale
		InspectMeshComponent->SetWorldScale3D(FVector(ScaleFactor));
		
		// Force bounds update
		InspectMeshComponent->MarkRenderStateDirty();
		InspectMeshComponent->UpdateBounds();
		
		// Get the updated mesh component's bounds in local space
		MeshBounds = InspectMeshComponent->GetStaticMesh()->GetBounds();
		CenteringOffset = MeshBounds.Origin * InspectMeshComponent->GetComponentScale();
		
		CurrentLocation = InspectMeshComponent->GetComponentLocation();
		CurrentRotation = InspectMeshComponent->GetComponentRotation();
		
		FVector AlignmentOffset = FVector::ZeroVector;
		if (CurrentAlignment != EInspectAlignment::Center)
		{
			// Calculate viewport width for alignment offset 
			FVector2D ViewportSize;
			GEngine->GameViewport->GetViewportSize(ViewportSize);
			float AspectRatio = ViewportSize.X / ViewportSize.Y;
			float ViewportWidth = ViewportHeightWorldUnits * AspectRatio;
			
			switch (CurrentAlignment)
			{
			case EInspectAlignment::Left:
				AlignmentOffset = CameraRotation.Quaternion().GetRightVector() * (ViewportWidth * AlignmentShiftMultiplier); // Left-shift
				break;
			case EInspectAlignment::Right:
				AlignmentOffset = -CameraRotation.Quaternion().GetRightVector() * (ViewportWidth * AlignmentShiftMultiplier); // Right-shift
				break;
			default:
				break; // Centered
			}
		}
		CenteringOffset+=AlignmentOffset;
		
		if (CVarInspectDebug.GetValueOnGameThread())
		{
			LOG("MeshOrigin: %s, MeshExtent: %s", *MeshOrigin.ToString(), *MeshExtent.ToString());
			LOG("MeshDiameter: %f, ViewportHeightWorldUnits: %f, DesiredHeight: %f, ScaleFactor: %f, FOV: %f",
				   MeshDiameter, ViewportHeightWorldUnits, DesiredHeight, ScaleFactor, FOV);
			LOG("CameraDistance: %f, CenteringOffset: %s", CameraDistance, *CenteringOffset.ToString());
		}

		bIsInspecting = true;
		OnInspectStateChanged.Broadcast(this, InspectingPawn, true);
	}
}

void ADaInspectableItem::HideDetailedMesh()
{
	// Show preview mesh
	PreviewMeshComponent->SetVisibility(true);

	// Hide reset and disable collision on detailed mesh
	InspectMeshComponent->SetVisibility(false);
	InspectMeshComponent->SetRelativeTransform(FTransform());
	InspectMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Disables calling updates on detail mesh in Tick function 
	bIsInspecting = false; 
	OnInspectStateChanged.Broadcast(this, InspectingPawn, false);
}

void ADaInspectableItem::RotateDetailedMesh(float DeltaPitch, float DeltaYaw)
{
	if (bIsInspecting)
	{
		InputDeltaPitch = DeltaPitch;
		InputDeltaYaw = DeltaYaw;
	}
}

void ADaInspectableItem::ZoomDetailedMesh(float DeltaZoom)
{
	if (bIsInspecting)
	{
		InputDeltaZoom = DeltaZoom;
	}
}

void ADaInspectableItem::Interact_Implementation(APawn* InstigatorPawn)
{
	AddToInventory(InstigatorPawn);
}

void ADaInspectableItem::SecondaryInteract_Implementation(APawn* InstigatorPawn)
{
	if (ADaCharacter* PlayerCharacter = Cast<ADaCharacter>(InstigatorPawn))
	{
		PlayerCharacter->SetInspectedItem(this);
	}
	
	Inspect(InstigatorPawn, ViewportPercentage, InspectAlignment);
}

FText ADaInspectableItem::GetInteractText_Implementation(APawn* InstigatorPawn)
{
	return FText::Format(LOCTEXT("Inspectable_HighlightText", "Inspect {0}"), FText::FromName(Name));
}

#undef LOCTEXT_NAMESPACE