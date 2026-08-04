// Copyright Dream Awake Solutions LLC


#include "DaItemActor.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/DaAbilitySet.h"
#include "AbilitySystem/DaAbilitySystemComponent.h"
#include "Components/SphereComponent.h"
#include "CoreGameplayTags.h"
#include "Engine/AssetManager.h"
#include "Equipment/DaConditionComponent.h"
#include "GameplayFramework.h"
#include "GameFramework/PlayerState.h"
#include "Inventory/DaInventoryComponent.h"
#include "Inventory/DaItemDefinition.h"
#include "Materials/MaterialInstanceDynamic.h"
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

	if (!bHasDroppedWear && ItemDefinitionID.IsValid())
	{
		// A hand-placed pickup was never dropped by anyone, so there is no instance to read wear
		// from: it is a factory-fresh example of its type. Saying that explicitly is what stops a
		// wear driver on the pickup from spending its whole retry budget looking for an inventory
		// entry that does not exist, and then warning about it.
		const UDaItemDefinition* Def = ResolveItemDefinition(ItemDefinitionID);
		DroppedWearGrade = Def
			? FMath::Clamp(Def->ConditionConfig.DefaultGrade / 10.f, 0.f, 1.f)
			: 0.f;
		DroppedWearSeed = 0.f;
		DroppedWearIntensity = 0.f;
		bHasDroppedWear = true;
	}

	// A drop sets its wear in InitializeDroppedItem, which runs before FinishSpawning when components
	// may not be registered yet, so the push is repeated here where the mesh is certainly live.
	// Idempotent.
	ApplyDroppedWear();
}

void ADaItemActor::AddToInventory_Implementation(APawn* InstigatorPawn, bool bDestroyActor)
{
	if (!InstigatorPawn)
		return;

	// Already handed its contents over and waiting to be destroyed. Destroy() only marks the actor
	// for teardown at the end of the frame, so a second interact in the SAME frame still reaches
	// this function — and without this gate it would take the AddItem fallback below and mint a
	// pristine duplicate of the item that was just restored.
	if (bPickedUp || IsActorBeingDestroyed())
	{
		return;
	}

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
		if (bAdded)
		{
			// One snapshot, one restore. Spent the moment it succeeds, so nothing can restore this
			// ItemID a second time — into this inventory (RestoreEntry would refuse, and the AddItem
			// fallback would then hand out a free pristine copy) or into another player's, which is
			// how a race between two interactors could clone the instance.
			bHasDroppedSnapshot = false;
			// And stop being interactable at all while the destroy is pending.
			SetActorEnableCollision(false);
		}
		else
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
		bPickedUp = true;
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

	// No instance behind this drop (a hand-placed pickup, or the partial-stack case whose identity
	// stayed with the entry left behind), so it is pristine by definition. Saying so explicitly —
	// rather than leaving the numbers unset — is what keeps a condition component on the pickup from
	// spending its whole retry budget hunting for an inventory entry that does not exist.
	DroppedWearIntensity = 0.f;
	DroppedWearSeed = 0.f;
	DroppedWearGrade = 0.f;
	bHasDroppedWear = true;
	ApplyDroppedWear();
}

void ADaItemActor::InitializeDroppedItem(const FDaInventoryEntry& SourceEntry, UStaticMesh* DisplayMesh)
{
	InitializeDroppedItem(SourceEntry.ItemDefinitionID, DisplayMesh);
	DroppedEntrySnapshot = SourceEntry;
	bHasDroppedSnapshot = SourceEntry.IsValid();

	// A worn sword has to hit the ground looking worn. The snapshot is the only place the numbers
	// still exist — the entry has already left the inventory, so nothing on this actor could look
	// them up afterwards — so compute the contract values here and hand them straight to the visual.
	const int32 Grade = SourceEntry.GetStatCount(CoreGameplayTags::TAG_Item_Stat_Grade);
	DroppedWearGrade = FMath::Clamp(Grade / 10.f, 0.f, 1.f);
	DroppedWearSeed = UDaConditionComponent::MakeWearSeed(SourceEntry.ItemID);
	DroppedWearIntensity = 0.f;

	if (const UDaItemDefinition* Def = ResolveItemDefinition(SourceEntry.ItemDefinitionID))
	{
		const FDaConditionConfig& Config = Def->ConditionConfig;
		const int32 Cap = Config.GetConditionCap(Grade);
		if (Config.bUsesCondition && Cap > 0)
		{
			// Same formula as UDaConditionComponent::RefreshWearParameters, so a dropped item and
			// the equipped one it came from read identically.
			const int32 Condition = SourceEntry.GetStatCount(CoreGameplayTags::TAG_Item_Stat_Condition);
			DroppedWearIntensity = FMath::Clamp(
				1.f - static_cast<float>(Condition) / static_cast<float>(Cap), 0.f, 1.f);
		}
	}

	bHasDroppedWear = true;
	ApplyDroppedWear();
}

void ADaItemActor::ApplyDroppedWear()
{
	if (!bHasDroppedWear)
	{
		return;
	}

	// A pickup class that carries the wear driver gets told the answer directly — which also stops
	// the driver's resolve retry, since the item it would look for is no longer in any inventory.
	if (UDaConditionComponent* Condition = FindComponentByClass<UDaConditionComponent>())
	{
		Condition->SetExplicitWear(DroppedWearIntensity, DroppedWearSeed, DroppedWearGrade);
		return;
	}

	// Plain ADaItemActor (or any pickup class without the component): drive the display mesh's MIDs
	// directly, using the component's own "did this material opt in" rule so materials that know
	// nothing about wear are left alone.
	if (!MeshComp)
	{
		return;
	}
	const int32 NumMaterials = MeshComp->GetNumMaterials();
	for (int32 Index = 0; Index < NumMaterials; ++Index)
	{
		if (!UDaConditionComponent::ImplementsWearContract(MeshComp->GetMaterial(Index)))
		{
			continue;
		}
		if (UMaterialInstanceDynamic* MID = MeshComp->CreateAndSetMaterialInstanceDynamic(Index))
		{
			MID->SetScalarParameterValue(UDaConditionComponent::WearIntensityParameterName, DroppedWearIntensity);
			MID->SetScalarParameterValue(UDaConditionComponent::WearSeedParameterName, DroppedWearSeed);
			MID->SetScalarParameterValue(UDaConditionComponent::WearGradeParameterName, DroppedWearGrade);
		}
	}
}

const UDaItemDefinition* ADaItemActor::ResolveItemDefinition(const FPrimaryAssetId& InItemDefinitionID)
{
	if (!InItemDefinitionID.IsValid())
	{
		return nullptr;
	}
	if (const UDaItemDefinition* Loaded = Cast<UDaItemDefinition>(UAssetManager::Get().GetPrimaryAssetObject(InItemDefinitionID)))
	{
		return Loaded;
	}
	const FSoftObjectPath Path = UAssetManager::Get().GetPrimaryAssetPath(InItemDefinitionID);
	return Cast<UDaItemDefinition>(Path.TryLoad());
}

void ADaItemActor::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADaItemActor, Name);
	DOREPLIFETIME(ADaItemActor, Description);
	DOREPLIFETIME(ADaItemActor, TypeTags);
}

#undef LOCTEXT_NAMESPACE
