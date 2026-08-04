// Copyright Dream Awake Solutions LLC


#include "DaItemActor.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/DaAbilitySet.h"
#include "AbilitySystem/DaAbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "GameplayFramework.h"
#include "GameFramework/PlayerState.h"
#include "Inventory/DaInventoryComponent.h"
#include "Net/UnrealNetwork.h"

#define LOCTEXT_NAMESPACE "ItemActor"

ADaItemActor::ADaItemActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SphereComp = CreateDefaultSubobject<USphereComponent>("SphereComp");
	RootComponent = SphereComp;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>("MeshComp");
	MeshComp->SetupAttachment(RootComponent);
	MeshComp->SetCustomDepthStencilValue(250.f);

	AbilitySystemComponent = CreateDefaultSubobject<UDaAbilitySystemComponent>("AbilitySystemComp");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	bReplicates = true;
}

void ADaItemActor::BeginPlay()
{
	Super::BeginPlay();

	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	if (OwnedAbilitySet)
	{
		AbilitySystemComponent->GrantSet(OwnedAbilitySet);
	}
}

void ADaItemActor::AddToInventory_Implementation(APawn* InstigatorPawn, bool bDestroyActor)
{
	if (!InstigatorPawn)
		return;

	APlayerState* PS = InstigatorPawn->GetPlayerState();
	if (!PS)
	{
		return;
	}

	UDaInventoryComponent* InvComp = UDaInventoryComponent::GetInventoryFromActor(PS);
	if (!InvComp)
	{
		return;
	}

	// A dropped item carries its own entry, so picking it back up returns the very same instance
	// (ItemID, stats, tags) rather than a fresh one. Only the authority holds a snapshot.
	bool bAdded = false;
	if (bHasDroppedSnapshot && HasAuthority())
	{
		bAdded = InvComp->RestoreEntry(DroppedEntrySnapshot);
		if (!bAdded)
		{
			LOG_WARNING("AddToInventory: restoring dropped item %s failed — falling back to a new instance",
				*DroppedEntrySnapshot.ItemID.ToString());
		}
	}

	if (!bAdded)
	{
		bAdded = InvComp->AddItem(ItemDefinitionID);
	}

	if (bAdded && bDestroyActor && HasAuthority())
	{
		Destroy();
	}
}

void ADaItemActor::Interact_Implementation(APawn* InstigatorPawn)
{
	// Default behavior: items with a valid definition go into the instigator's inventory.
	// Derived classes can override for other interact behavior.
	if (ItemDefinitionID.IsValid())
	{
		Execute_AddToInventory(this, InstigatorPawn, true);
	}
}

void ADaItemActor::SecondaryInteract_Implementation(APawn* InstigatorPawn)
{
	// Derived classes to implement
}

FText ADaItemActor::GetInteractText_Implementation(APawn* InstigatorPawn)
{
	if (!OwnedAbilitySet)
	{
		return FText();
	}

	return FText::Format(LOCTEXT("ItemActor", "ItemActor: {0}"), FText::FromName(OwnedAbilitySet->GetSetIdentityTag().GetTagName()));
}

void ADaItemActor::HighlightActor_Implementation()
{
	bHighlighted = true;
	MeshComp->SetRenderCustomDepth(true);
}

void ADaItemActor::UnHighlightActor_Implementation()
{
	bHighlighted = false;
	MeshComp->SetRenderCustomDepth(false);
}

UAbilitySystemComponent* ADaItemActor::GetAbilitySystemComponent() const
{
	return Cast<UAbilitySystemComponent>(AbilitySystemComponent);
}

void ADaItemActor::GrantSetToActor(UDaAbilitySystemComponent* ReceivingASC)
{
	if (ReceivingASC && OwnedAbilitySet)
	{
		ReceivingASC->GrantSet(OwnedAbilitySet);
	}
}

void ADaItemActor::InitializeDroppedItem(const FPrimaryAssetId& InItemDefinitionID, UStaticMesh* DisplayMesh)
{
	ItemDefinitionID = InItemDefinitionID;
	if (DisplayMesh)
	{
		MeshComp->SetStaticMesh(DisplayMesh);
	}
}

void ADaItemActor::InitializeDroppedItem(const FDaInventoryEntry& SourceEntry, UStaticMesh* DisplayMesh)
{
	InitializeDroppedItem(SourceEntry.ItemDefinitionID, DisplayMesh);
	DroppedEntrySnapshot = SourceEntry;
	bHasDroppedSnapshot = SourceEntry.IsValid();
}

void ADaItemActor::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADaItemActor, Name);
	DOREPLIFETIME(ADaItemActor, Description);
	DOREPLIFETIME(ADaItemActor, TypeTags);
}

#undef LOCTEXT_NAMESPACE
