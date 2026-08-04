// Copyright Dream Awake Solutions LLC

#include "Equipment/DaConditionComponent.h"

#include "CoreGameplayTags.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameplayFramework.h"
#include "Components/MeshComponent.h"
#include "Equipment/DaEquipmentManagerComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "Inventory/DaInventoryComponent.h"
#include "Inventory/DaInventoryEntry.h"
#include "Inventory/DaItemDefinition.h"
#include "TimerManager.h"

const FName UDaConditionComponent::WearIntensityParameterName(TEXT("Da_Wear_Intensity"));
const FName UDaConditionComponent::WearSeedParameterName(TEXT("Da_Wear_Seed"));
const FName UDaConditionComponent::WearGradeParameterName(TEXT("Da_Wear_Grade"));

UDaConditionComponent::UDaConditionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	// Cosmetic only: nothing here is worth a replicated property, since the numbers are derived
	// from the inventory entry, which already replicates.
	SetIsReplicatedByDefault(false);
}

void UDaConditionComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!RefreshAndIsSettled())
	{
		StartResolveRetry();
	}
}

void UDaConditionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopResolveRetry();

	if (UDaInventoryComponent* Inventory = BoundInventory.Get())
	{
		Inventory->OnEntryChanged.RemoveDynamic(this, &UDaConditionComponent::OnInventoryEntryChanged);
	}
	BoundInventory.Reset();

	Super::EndPlay(EndPlayReason);
}

void UDaConditionComponent::SetItemID(FGuid NewItemID)
{
	ItemID = NewItemID;
	ResolvedItemID.Invalidate();
	if (RefreshAndIsSettled())
	{
		// A retry may already be running from BeginPlay; being told the answer ends the search.
		StopResolveRetry();
	}
	else
	{
		StartResolveRetry();
	}
}

void UDaConditionComponent::SetItem(UDaInventoryComponent* Inventory, FGuid NewItemID)
{
	ExplicitInventory = Inventory;
	SetItemID(NewItemID);
}

void UDaConditionComponent::SetExplicitWear(float Intensity, float Seed, float Grade)
{
	// The caller did the arithmetic, so there is nothing left to resolve: stop the search before it
	// spends its budget and then warns loudly about an item that was never missing. This is the path
	// a dropped pickup takes — the entry it came from has left the inventory, so no lookup could
	// ever succeed, and without this the actor would tick out its full retry timeout and complain.
	StopResolveRetry();

	WearIntensity = FMath::Clamp(Intensity, 0.f, 1.f);
	WearSeed = FMath::Clamp(Seed, 0.f, 1.f);
	WearGrade = FMath::Clamp(Grade, 0.f, 1.f);

	EnsureWearMaterials();
	PushWearParameters();
}

// ---------------------------------------------------------------------------
// The push
// ---------------------------------------------------------------------------

bool UDaConditionComponent::RefreshWearParameters()
{
	const FGuid Item = ResolveItemID();
	if (!Item.IsValid())
	{
		return false;
	}

	UDaInventoryComponent* Inventory = ResolveInventory();
	const FDaInventoryEntry* Entry = Inventory ? Inventory->FindEntryByItemID(Item) : nullptr;
	if (!Entry)
	{
		return false;
	}
	// Copy off the entry before anything else can touch the entry array.
	const FPrimaryAssetId EntryDefinitionID = Entry->ItemDefinitionID;
	Entry = nullptr;

	const UDaItemDefinition* Def = ResolveItemDefinition(EntryDefinitionID);
	if (!Def)
	{
		return false;
	}

	const FDaConditionConfig& Config = Def->ConditionConfig;
	const int32 Grade = Inventory->GetItemStat(Item, CoreGameplayTags::TAG_Item_Stat_Grade);
	const int32 Condition = Inventory->GetItemStat(Item, CoreGameplayTags::TAG_Item_Stat_Condition);
	const int32 Cap = Config.GetConditionCap(Grade);

	// An item that never opted into condition cannot wear: it keeps the material's pristine look,
	// but still gets its seed and grade, so a rack of identical items is not identical.
	WearIntensity = (Config.bUsesCondition && Cap > 0)
		? FMath::Clamp(1.f - static_cast<float>(Condition) / static_cast<float>(Cap), 0.f, 1.f)
		: 0.f;
	WearSeed = MakeWearSeed(Item);
	WearGrade = FMath::Clamp(Grade / 10.f, 0.f, 1.f);
	ResolvedItemID = Item;

	EnsureWearMaterials();
	PushWearParameters();

	// Only worth binding once there is an item to watch, and the inventory we found is the one
	// that owns it.
	EnsureInventoryBinding();
	return true;
}

void UDaConditionComponent::PushWearParameters()
{
	for (const TObjectPtr<UMaterialInstanceDynamic>& Material : WearMaterials)
	{
		if (UMaterialInstanceDynamic* MID = Material.Get())
		{
			MID->SetScalarParameterValue(WearIntensityParameterName, WearIntensity);
			MID->SetScalarParameterValue(WearSeedParameterName, WearSeed);
			MID->SetScalarParameterValue(WearGradeParameterName, WearGrade);
		}
	}
}

bool UDaConditionComponent::RefreshAndIsSettled()
{
	// Two things have to be true before there is nothing left to look for: an item to read, and at
	// least one material slot on the owner to push into. A mesh component whose mesh arrives after
	// BeginPlay reports zero slots, and the item resolving first would otherwise stop the search
	// while the visual is still un-driven.
	const bool bResolved = RefreshWearParameters();
	return bResolved && bWearMaterialsBuilt;
}

float UDaConditionComponent::MakeWearSeed(FGuid ForItemID)
{
	// GetItemSeed is a signed hash of the GUID; take the low 16 bits (the well-mixed end) as an
	// unsigned fraction so materials get a positive, exactly representable [0,1] value.
	const uint32 Hash = static_cast<uint32>(UDaInventoryComponent::GetItemSeed(ForItemID));
	return static_cast<float>(Hash & 0xFFFF) / 65535.f;
}

TArray<UMaterialInstanceDynamic*> UDaConditionComponent::GetWearMaterials() const
{
	TArray<UMaterialInstanceDynamic*> Result;
	Result.Reserve(WearMaterials.Num());
	for (const TObjectPtr<UMaterialInstanceDynamic>& Material : WearMaterials)
	{
		if (UMaterialInstanceDynamic* MID = Material.Get())
		{
			Result.Add(MID);
		}
	}
	return Result;
}

// ---------------------------------------------------------------------------
// Resolution
// ---------------------------------------------------------------------------

FGuid UDaConditionComponent::ResolveItemID() const
{
	if (ItemID.IsValid())
	{
		return ItemID;
	}

	APawn* Pawn = ResolveOwningPawn();
	const UDaEquipmentManagerComponent* Equipment =
		Pawn ? Pawn->FindComponentByClass<UDaEquipmentManagerComponent>() : nullptr;
	if (!Equipment)
	{
		return FGuid();
	}
	return Equipment->FindItemIDForSpawnedActor(GetOwner());
}

APawn* UDaConditionComponent::ResolveOwningPawn() const
{
	AActor* Actor = GetOwner();
	if (!Actor)
	{
		return nullptr;
	}

	for (AActor* Parent = Actor->GetAttachParentActor(); Parent; Parent = Parent->GetAttachParentActor())
	{
		if (APawn* Pawn = Cast<APawn>(Parent))
		{
			return Pawn;
		}
	}
	for (AActor* Owner = Actor->GetOwner(); Owner; Owner = Owner->GetOwner())
	{
		if (APawn* Pawn = Cast<APawn>(Owner))
		{
			return Pawn;
		}
	}
	return nullptr;
}

UDaInventoryComponent* UDaConditionComponent::ResolveInventory() const
{
	if (UDaInventoryComponent* Named = ExplicitInventory.Get())
	{
		return Named;
	}

	if (APawn* Pawn = ResolveOwningPawn())
	{
		if (APlayerState* PlayerState = Pawn->GetPlayerState())
		{
			if (UDaInventoryComponent* Inventory = UDaInventoryComponent::GetInventoryFromActor(PlayerState))
			{
				return Inventory;
			}
		}
		if (UDaInventoryComponent* Inventory = UDaInventoryComponent::GetInventoryFromActor(Pawn))
		{
			return Inventory;
		}
	}
	return UDaInventoryComponent::GetInventoryFromActor(GetOwner());
}

UDaItemDefinition* UDaConditionComponent::ResolveItemDefinition(FPrimaryAssetId ItemDefinitionID) const
{
	if (!ItemDefinitionID.IsValid())
	{
		return nullptr;
	}
	if (UDaItemDefinition* Loaded = Cast<UDaItemDefinition>(UAssetManager::Get().GetPrimaryAssetObject(ItemDefinitionID)))
	{
		return Loaded;
	}
	const FSoftObjectPath Path = UAssetManager::Get().GetPrimaryAssetPath(ItemDefinitionID);
	return Cast<UDaItemDefinition>(Path.TryLoad());
}

// ---------------------------------------------------------------------------
// Materials
// ---------------------------------------------------------------------------

void UDaConditionComponent::EnsureWearMaterials()
{
	if (bWearMaterialsBuilt)
	{
		return;
	}
	AActor* Actor = GetOwner();
	if (!Actor)
	{
		return;
	}

	TArray<UMeshComponent*> Meshes;
	Actor->GetComponents(Meshes);

	int32 SlotsSeen = 0;
	for (UMeshComponent* Mesh : Meshes)
	{
		const int32 NumMaterials = Mesh->GetNumMaterials();
		SlotsSeen += NumMaterials;
		for (int32 Index = 0; Index < NumMaterials; ++Index)
		{
			if (!ImplementsWearContract(Mesh->GetMaterial(Index)))
			{
				continue;
			}
			if (UMaterialInstanceDynamic* MID = Mesh->CreateAndSetMaterialInstanceDynamic(Index))
			{
				WearMaterials.Add(MID);
			}
		}
	}

	// Latch only once there was something to scan. A mesh component can report zero material slots
	// because its mesh has not been assigned yet — anything that fills a mesh after BeginPlay
	// (spawn-then-configure, a late async load) lands here — and latching on that would leave the
	// visual permanently un-driven. An actor WITH slots but no wear-capable material among them is a
	// legitimate no-op and does latch. (A dropped ADaItemActor is not an example of the late case:
	// InitializeDroppedItem sets its mesh before FinishSpawning, so BeginPlay already sees slots.)
	bWearMaterialsBuilt = SlotsSeen > 0;
}

bool UDaConditionComponent::ImplementsWearContract(const UMaterialInterface* Material)
{
	if (!Material)
	{
		return false;
	}

	TArray<FMaterialParameterInfo> ParameterInfos;
	TArray<FGuid> ParameterIds;
	Material->GetAllScalarParameterInfo(ParameterInfos, ParameterIds);
	for (const FMaterialParameterInfo& Info : ParameterInfos)
	{
		if (Info.Name == WearIntensityParameterName
			|| Info.Name == WearSeedParameterName
			|| Info.Name == WearGradeParameterName)
		{
			return true;
		}
	}
	return false;
}

// ---------------------------------------------------------------------------
// Staying current
// ---------------------------------------------------------------------------

void UDaConditionComponent::EnsureInventoryBinding()
{
	UDaInventoryComponent* Inventory = ResolveInventory();
	if (!Inventory || BoundInventory.Get() == Inventory)
	{
		return;
	}

	if (UDaInventoryComponent* Previous = BoundInventory.Get())
	{
		Previous->OnEntryChanged.RemoveDynamic(this, &UDaConditionComponent::OnInventoryEntryChanged);
	}
	Inventory->OnEntryChanged.AddDynamic(this, &UDaConditionComponent::OnInventoryEntryChanged);
	BoundInventory = Inventory;
}

void UDaConditionComponent::OnInventoryEntryChanged(const FDaInventoryEntry& Entry, int32 SlotIndex)
{
	if (Entry.ItemID == ResolvedItemID)
	{
		RefreshWearParameters();
	}
}

void UDaConditionComponent::StartResolveRetry()
{
	UWorld* World = GetWorld();
	if (!World || ResolveTimerHandle.IsValid() || ResolveRetryTimeout <= 0.f)
	{
		return;
	}
	ResolveElapsed = 0.f;
	World->GetTimerManager().SetTimer(ResolveTimerHandle, this, &UDaConditionComponent::RetryResolve,
		ResolveRetryInterval, /*bLoop=*/true);
}

void UDaConditionComponent::StopResolveRetry()
{
	if (UWorld* World = GetWorld(); World && ResolveTimerHandle.IsValid())
	{
		World->GetTimerManager().ClearTimer(ResolveTimerHandle);
	}
	ResolveTimerHandle.Invalidate();
}

void UDaConditionComponent::RetryResolve()
{
	if (RefreshAndIsSettled())
	{
		StopResolveRetry();
		return;
	}

	ResolveElapsed += ResolveRetryInterval;
	if (ResolveElapsed >= ResolveRetryTimeout)
	{
		StopResolveRetry();
		// Loud, because a wear visual that never found its item is silently pristine forever, and
		// that reads as "the wear system is broken" rather than "this actor has no item".
		if (ResolvedItemID.IsValid())
		{
			LOG_WARNING("[%s] UDaConditionComponent resolved item %s but saw no material slots on the "
				"owner after %.1fs — nothing to push wear into", *GetNameSafe(GetOwner()),
				*ResolvedItemID.ToString(), ResolveRetryTimeout);
		}
		else
		{
			LOG_WARNING("[%s] UDaConditionComponent gave up resolving its item after %.1fs — no "
				"explicit ItemID and no equipment entry claims this actor (use SetItem for visuals "
				"nobody equips, or SetExplicitWear when the numbers are already known)",
				*GetNameSafe(GetOwner()), ResolveRetryTimeout);
		}
	}
}
