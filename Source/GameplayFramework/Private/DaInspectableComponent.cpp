// Fill out your copyright notice in the Description page of Project Settings.


#include "DaInspectableComponent.h"

#include "DaInspectableInterface.h"
#include "GameplayFramework.h"

static TAutoConsoleVariable<bool> CVarInspectDebug(TEXT("da.InspectDebug"), false, TEXT("Log Inspectable Component Debug Info"), ECVF_Cheat);
static TAutoConsoleVariable<bool> CVarInspectTickDebug(TEXT("da.InspectOnTickDebug"), false, TEXT("Log Inspectable Component on tick Debug Info"), ECVF_Cheat);

UDaInspectableComponent::UDaInspectableComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	bIsInspecting = false;
    
	// Initialize inspection properties
	CameraDistance = 1.2f;
	InputDeltaPitch = 0.0f;
	InputDeltaYaw = 0.0f;
	InputDeltaZoom = 0.0f;
	CurrentViewportPercentage = 0.0f;
	CenteringOffset = FVector::ZeroVector;
	CurrentRotation = FRotator::ZeroRotator;
	CurrentLocation = FVector::ZeroVector;
	BaseDetailMeshTransform = FTransform::Identity;
	CurrentAlignment = EInspectAlignment::Center;
	ScaleFactor = 1.0f;
}

void UDaInspectableComponent::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (Owner && Owner->GetClass()->ImplementsInterface(UDaInspectableInterface::StaticClass()))
	{
		PreviewMeshComponent = IDaInspectableInterface::Execute_GetPreviewMeshComponent(Owner);
		DetailedMeshComponent = IDaInspectableInterface::Execute_GetDetailedMeshComponent(Owner);

		if (DetailedMeshComponent)
		{
			if (!bShowDetailAsPreview)
				DetailedMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
			
			DetailedMeshComponent->SetVisibility(bShowDetailAsPreview, true);

			if (PreviewMeshComponent)
			{
				PreviewMeshComponent->SetVisibility(!bShowDetailAsPreview, true);
			}
		}
	}
	else
	{
		LOG_WARNING("Owner of DaInspectableComponent must implement IDaInspectableInterface");
	}
}

void UDaInspectableComponent::Inspect(APawn* InstigatorPawn, float ViewportPct, EInspectAlignment Alignment)
{
	// This is just for viewing on clients
	if (!InstigatorPawn->IsLocallyControlled()) 
		return;

	if (bIsInspecting)
	{
		return;
	}

	// Cache inspection parameters (used during tick)
	InspectingPawn = InstigatorPawn;
	CurrentViewportPercentage = ViewportPct;
	CurrentAlignment = Alignment;

	// Get mesh components through interface
	AActor* Owner = GetOwner();
	if (Owner && Owner->GetClass()->ImplementsInterface(UDaInspectableInterface::StaticClass()))
	{
		PreviewMeshComponent = IDaInspectableInterface::Execute_GetPreviewMeshComponent(Owner);
		DetailedMeshComponent = IDaInspectableInterface::Execute_GetDetailedMeshComponent(Owner);
		
		if (DetailedMeshComponent)
		{
			// store so we can reset
			BaseDetailMeshTransform = DetailedMeshComponent->GetComponentTransform();
			
			DetailedMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
			DetailedMeshComponent->SetVisibility(true, true);
            
			if (PreviewMeshComponent)
			{
				PreviewMeshComponent->SetVisibility(false, true);
			}

			PlaceDetailMeshInView();
            
			// Notify owner inspection started
			IDaInspectableInterface::Execute_OnInspectionStarted(Owner, InstigatorPawn);
		}
	}
}

void UDaInspectableComponent::HideDetailedMesh()
{
	if (!bIsInspecting) 
		return;

	if (PreviewMeshComponent)
	{
		PreviewMeshComponent->SetVisibility(!bShowDetailAsPreview, true);
	}

	if (DetailedMeshComponent)
	{
		DetailedMeshComponent->SetVisibility(bShowDetailAsPreview, true);
		DetailedMeshComponent->SetWorldTransform(BaseDetailMeshTransform);

		if (!bShowDetailAsPreview)
			DetailedMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Notify owner inspection ended
	AActor* Owner = GetOwner();
	if (Owner && Owner->GetClass()->ImplementsInterface(UDaInspectableInterface::StaticClass()))
	{
		IDaInspectableInterface::Execute_OnInspectionEnded(GetOwner(), InspectingPawn);
	}

	bIsInspecting = false;
	OnInspectStateChanged.Broadcast(GetOwner(), InspectingPawn, false);
}

void UDaInspectableComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                            FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (bIsInspecting)
	{
		UpdateMeshTransform(DeltaTime);
	}
}

void UDaInspectableComponent::UpdateMeshTransform(float DeltaTime)
{
	if (!DetailedMeshComponent || !InspectingPawn)
		return;

	// Update rotation
	FRotator NewRotation = CurrentRotation;
	NewRotation.Pitch += InputDeltaPitch;
	NewRotation.Yaw += InputDeltaYaw;

	// Update camera distance
	CameraDistance = FMath::Clamp(CameraDistance - InputDeltaZoom * 10.0f, MinCameraDistance, MaxCameraDistance);

	// Reset input deltas
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

		// Calculate new mesh location with offset
		FVector ViewportPosition = CameraLocation + ForwardVector * (CameraDistance + InitialCameraDistanceOffset);
		FVector NewLocation = ViewportPosition - CenteringOffset;

		// Smooth transition using exponential smoothing
		CurrentLocation = FMath::VInterpTo(CurrentLocation, NewLocation, DeltaTime, ZoomSmoothingFactor / DeltaTime);

		// Smooth rotation transition, RotationSmoothingSpeed: Higher = faster
		CurrentRotation = FMath::RInterpConstantTo(CurrentRotation, NewRotation, DeltaTime, RotationSmoothingSpeed);

		// Apply transform
		DetailedMeshComponent->SetWorldLocation(CurrentLocation);
		DetailedMeshComponent->SetWorldRotation(CurrentRotation);

		if (CVarInspectTickDebug.GetValueOnGameThread())
		{
			LOG("CameraDistance: %f, Location: %s, Rotation: %s", 
				CameraDistance, *CurrentLocation.ToString(), *CurrentRotation.ToString());
		}
	}
}

void UDaInspectableComponent::PlaceDetailMeshInView()
{
    if (!InspectingPawn || !DetailedMeshComponent)
        return;

    // Update bounds
    DetailedMeshComponent->MarkRenderStateDirty();
    DetailedMeshComponent->UpdateBounds();

	// Get bounds of all static mesh components in world space
    FBoxSphereBounds MeshBounds = GetHierarchyBounds(DetailedMeshComponent, false);
    float MeshDiameter = MeshBounds.BoxExtent.GetMax() * 2.0f;

    // Calculate viewport fit
    APlayerController* PlayerController = InspectingPawn->GetLocalViewingPlayerController();
    if (PlayerController && PlayerController->PlayerCameraManager)
    {
        FVector CameraLocation;
        FRotator CameraRotation;
        PlayerController->PlayerCameraManager->GetCameraViewPoint(CameraLocation, CameraRotation);

        float FOV = PlayerController->PlayerCameraManager->GetFOVAngle();
        float FOVRads = FMath::DegreesToRadians(FOV * 0.5f);
        float FOVHalfHeight = FMath::Tan(FOVRads);

        // Calculate and clamp camera distance
        CameraDistance = (MeshDiameter / 2.0f) / FOVHalfHeight;
        CameraDistance = FMath::Clamp(CameraDistance * CameraDistanceMultiplier, MinCameraDistance, MaxCameraDistance);

        float ViewportHeightWorldUnits = CameraDistance * FOVHalfHeight * 2.0f;
        float DesiredHeight = CurrentViewportPercentage * ViewportHeightWorldUnits;
        ScaleFactor = DesiredHeight / MeshDiameter;

        // Apply scale and update bounds
        FVector CurrentScale = DetailedMeshComponent->GetRelativeScale3D();
        DetailedMeshComponent->SetWorldScale3D(CurrentScale * ScaleFactor);
        DetailedMeshComponent->MarkRenderStateDirty();
        DetailedMeshComponent->UpdateBounds();

    	// Get bounds of just the mesh local space now to caclulate centering offset
    	MeshBounds = GetHierarchyBounds(DetailedMeshComponent, true);
    	CenteringOffset = MeshBounds.Origin * DetailedMeshComponent->GetComponentScale();
    	
        // Handle alignment
        if (CurrentAlignment != EInspectAlignment::Center)
        {
            FVector2D ViewportSize;
            if (GEngine && GEngine->GameViewport)
            {
                GEngine->GameViewport->GetViewportSize(ViewportSize);
                float AspectRatio = ViewportSize.X / ViewportSize.Y;
                float ViewportWidth = ViewportHeightWorldUnits * AspectRatio;

                FVector AlignmentOffset = FVector::ZeroVector;
                switch (CurrentAlignment)
                {
                case EInspectAlignment::Left:
                    AlignmentOffset = CameraRotation.Quaternion().GetRightVector() * (ViewportWidth * AlignmentShiftMultiplier);
                    break;
                case EInspectAlignment::Right:
                    AlignmentOffset = -CameraRotation.Quaternion().GetRightVector() * (ViewportWidth * AlignmentShiftMultiplier);
                    break;
                }
                CenteringOffset += AlignmentOffset;
            }
        }

        if (CVarInspectDebug.GetValueOnGameThread())
        {
            UE_LOG(LogTemp, Log, TEXT("Initial placement - Scale: %f, Distance: %f"), 
                ScaleFactor, CameraDistance);
        }

        CurrentLocation = DetailedMeshComponent->GetComponentLocation();
        CurrentRotation = DetailedMeshComponent->GetComponentRotation();
        bIsInspecting = true;
        
        OnInspectStateChanged.Broadcast(GetOwner(), InspectingPawn, true);
    }
}

void UDaInspectableComponent::RotateDetailedMesh(float DeltaPitch, float DeltaYaw)
{
	if (bIsInspecting)
	{
		InputDeltaPitch = DeltaPitch;
		InputDeltaYaw = DeltaYaw;
	}
}

void UDaInspectableComponent::ZoomDetailedMesh(float DeltaZoom)
{
	if (bIsInspecting)
	{
		InputDeltaZoom = DeltaZoom;
	}
}

FBoxSphereBounds UDaInspectableComponent::GetHierarchyBounds(USceneComponent* RootComponent, bool bMeshLocalSpace)
{
	if (!RootComponent)
		return FBoxSphereBounds(FVector::ZeroVector, FVector::ZeroVector, 0.0f);

	FBoxSphereBounds CombinedBounds;
	bool bFirstValidBounds = true;

	// Get all child components (including nested children)
	TArray<USceneComponent*> ChildComponents;
	RootComponent->GetChildrenComponents(true, ChildComponents);

	// Include root component if it's a static mesh
	if (UStaticMeshComponent* RootMesh = Cast<UStaticMeshComponent>(RootComponent))
	{
		if (RootMesh->GetStaticMesh())
		{
			if (bMeshLocalSpace)
			{
				CombinedBounds = RootMesh->GetStaticMesh()->GetBounds();
			}
			else
			{
				CombinedBounds = RootMesh->CalcBounds(RootMesh->GetComponentTransform());
			}
			bFirstValidBounds = false;
		}
	}

	// Process each child component
	for (USceneComponent* Child : ChildComponents)
	{
		if (UStaticMeshComponent* MeshComp = Cast<UStaticMeshComponent>(Child))
		{
			if (MeshComp->GetStaticMesh())
			{
				FBoxSphereBounds ChildBounds;

				if (bMeshLocalSpace)
				{
					ChildBounds = MeshComp->GetStaticMesh()->GetBounds();
				}
				else
				{
					ChildBounds = MeshComp->CalcBounds(MeshComp->GetComponentTransform());
				}
				
				if (bFirstValidBounds)
				{
					CombinedBounds = ChildBounds;
					bFirstValidBounds = false;
				}
				else
				{
					// TODO: Verify that adding bounds from mesh local doesnt get screwed up.
					CombinedBounds = CombinedBounds + ChildBounds;
				}
			}
		}
	}

	return CombinedBounds;
}