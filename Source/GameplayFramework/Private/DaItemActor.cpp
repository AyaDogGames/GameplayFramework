// Copyright Dream Awake Solutions LLC


#include "DaItemActor.h"

#include "AbilitySystemComponent.h"
#include "DaPlayerState.h"
#include "AbilitySystem/DaAbilitySet.h"
#include "AbilitySystem/DaAbilitySystemComponent.h"
#include "Components/SphereComponent.h"
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

void ADaItemActor::AddToInventory(APawn* InstigatorPawn, bool bDestroyActor)
{
	checkf(TypeTags.IsValid(), TEXT("ItemActor: TypeTags is not valid!"));
	
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

ADaItemActor* ADaItemActor::CreateFromInventoryItem(const FDaInventoryItemData& InventoryData)
{
	ADaItemActor* Actor = NewObject<ADaItemActor>();

	// TODO: Recreate this from data
	Actor->Name = InventoryData.ItemName;
	Actor->Description = InventoryData.ItemDescription;
	Actor->TypeTags = InventoryData.Tags;
	Actor->OwnedAbilitySet = InventoryData.AbilitySetToGrant;

	// TODO: Actor: set mesh, location and rotation
	
	return Actor;
}

void ADaItemActor::Interact_Implementation(APawn* InstigatorPawn)
{
	// Derived classes to implement
	AddToInventory(InstigatorPawn);
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

int32 ADaItemActor::GetItemTags_Implementation(FGameplayTagContainer& OutItemTags) const
{
	OutItemTags = TypeTags;
	return OutItemTags.IsValid() ? OutItemTags.Num() : 0;
}

void ADaItemActor::GrantSetToActor(UDaAbilitySystemComponent* ReceivingASC)
{
	if (ReceivingASC && OwnedAbilitySet)
	{
		ReceivingASC->GrantSet(OwnedAbilitySet);
	}
}

void ADaItemActor::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADaItemActor, Name);
	DOREPLIFETIME(ADaItemActor, Description);
	DOREPLIFETIME(ADaItemActor, TypeTags);
}

#undef LOCTEXT_NAMESPACE