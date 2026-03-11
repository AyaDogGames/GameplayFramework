// Copyright Dream Awake Solutions LLC

#include "DaInspectableItem.h"

#include "DaCharacter.h"
#include "DaInspectableComponent.h"
#include "GameplayFramework.h"
#include "Components/SphereComponent.h"
#include "Inventory/DaInventoryComponent.h"

#define LOCTEXT_NAMESPACE "InspectableItems"

ADaInspectableItem::ADaInspectableItem()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	
	SphereComp = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComp"));
	RootComponent = SphereComp;
	
	PreviewMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewMesh"));
	PreviewMeshComponent->SetupAttachment(RootComponent);
	PreviewMeshComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	DetailedMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("DetailedMesh");
	DetailedMeshComponent->SetupAttachment(RootComponent);
	DetailedMeshComponent->SetVisibility(false);
	DetailedMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	InspectableComponent = CreateDefaultSubobject<UDaInspectableComponent>("InspectableComponent");

	// Default properties
	bCanBeAddedToInventory = true;
}

void ADaInspectableItem::BeginPlay()
{
	Super::BeginPlay();

	InteractionText = FText::Format(LOCTEXT("Inspectable_HighlightText", "{0}"), FText::FromName(Name));
	
	// Ensure detailed mesh starts hidden
	if (DetailedMeshComponent)
	{
		DetailedMeshComponent->SetVisibility(false);
	}
}

// IDaInspectableInterface Implementation
UStaticMeshComponent* ADaInspectableItem::GetPreviewMeshComponent_Implementation() const
{
	return PreviewMeshComponent;
}

UStaticMeshComponent* ADaInspectableItem::GetDetailedMeshComponent_Implementation() const
{
	return DetailedMeshComponent;
}

void ADaInspectableItem::OnInspectionStarted_Implementation(APawn* InstigatorPawn)
{
	// Blueprint implementable event - can be extended in BP
}

void ADaInspectableItem::OnInspectionEnded_Implementation(APawn* InstigatorPawn)
{
	// Blueprint implementable event - can be extended in BP
}


void ADaInspectableItem::Interact_Implementation(APawn* InstigatorPawn)
{
	if (ADaCharacter* PlayerCharacter = Cast<ADaCharacter>(InstigatorPawn))
	{
		// Do this to give Gameplay Abilities access to the currently inspected item through the avatar or owning actor 
		PlayerCharacter->SetInspectedItem(this);
	}
	
	if (InspectableComponent)
	{
		InspectableComponent->Inspect(InstigatorPawn);
	}
}

void ADaInspectableItem::SecondaryInteract_Implementation(APawn* InstigatorPawn)
{
	// TODO: Also check if its already in inventory before adding
	if (bCanBeAddedToInventory)
	{
		Execute_AddToInventory(this, InstigatorPawn, true);
	}
}

FText ADaInspectableItem::GetInteractText_Implementation(APawn* InstigatorPawn)
{
	return InteractionText;
}

void ADaInspectableItem::AddToInventory_Implementation(APawn* InstigatorPawn, bool bDestroyActor)
{
	if (!InstigatorPawn)
		return;

	APlayerState* PS = InstigatorPawn->GetPlayerState();
	if (PS)
	{
		UDaInventoryComponent* InvComp = UDaInventoryComponent::GetInventoryFromActor(PS);
		if (InvComp && InvComp->AddItem(ItemDefinitionID))
		{
			// dont allow it to be added again
			bCanBeAddedToInventory = false;

			if (bDestroyActor && HasAuthority())
			{
				Destroy();
			}
		}
	}
}


#undef LOCTEXT_NAMESPACE