# Inventory System Refactor — FFastArraySerializer Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Replace the broken UObject-based inventory system with a server-authoritative, FFastArraySerializer-backed inventory that replicates efficiently, supports save/load, and exposes clean MVVM-ready interfaces.

**Architecture:** Items are replicated `USTRUCT`s inside an `FFastArraySerializer` on `UDaInventoryComponent`. The server is the sole authority — clients request changes via Server RPCs and receive granular per-item delta replication. The factory interface (`IDaInventoryItemInterface`) is kept for sourcing item data from world actors. An `IDaInventoryChangeListener` interface enables clean decoupling for UI (MVVM later) and gameplay systems.

**Tech Stack:** UE 5.7, Gameplay Abilities, GameplayTags, FFastArraySerializer, Enhanced Input, CommonUI (MVVM layer later)

---

## Architecture Overview

```
World Actors (IDaInventoryItemInterface)
    │
    ▼
UDaInventoryComponent (ActorComponent on PlayerState)
    ├── FDaInventoryList : FFastArraySerializer
    │     └── TArray<FDaInventoryEntry> Entries  (delta-replicated)
    │
    ├── Server_RequestAddItem(ItemDefID, SlotHint)    ← Client RPC
    ├── Server_RequestRemoveItem(SlotIndex)            ← Client RPC
    ├── Server_RequestMoveItem(FromSlot, ToSlot)       ← Client RPC
    ├── Server_RequestStackItems(FromSlot, ToSlot)     ← Client RPC
    │
    ├── OnEntryAdded / OnEntryRemoved / OnEntryChanged ← Delegates (for UI/MVVM)
    │
    └── SaveInventory / LoadInventory                  ← Persistence hooks
```

### Key Design Decisions

1. **Struct-only items** — No UObject inventory items. `FDaInventoryEntry` is a replicated USTRUCT containing all item state. This eliminates subobject replication complexity.

2. **Definition/Instance split** — Item *definitions* (name, description, mesh, icon, tags, ability set) live in `UDaItemDefinition` (a UDataAsset). Item *instances* (`FDaInventoryEntry`) reference the definition by `FPrimaryAssetId` and store only per-instance state (stack count, GUID, slot index, runtime tags).

3. **Server authority, no client prediction** — Clients send requests, server validates and mutates. Clients receive updates via FastArray delta replication. Simpler, safer, no rollback bugs.

4. **Factory kept but simplified** — `IDaInventoryItemInterface` remains on world actors. Instead of creating UObjects, it now returns an `FPrimaryAssetId` pointing to the item definition. The component looks up the definition and creates an `FDaInventoryEntry`.

5. **MVVM-ready** — The component exposes `FOnInventoryEntryAdded`, `FOnInventoryEntryRemoved`, `FOnInventoryEntryChanged` delegates with slot index and entry data. A ViewModel layer can subscribe to these later without touching the component.

6. **Save/Load** — `FDaInventoryEntry` is a plain USTRUCT with `UPROPERTY()` serialization. `SaveInventory()` returns `TArray<FDaInventoryEntry>` for the save system. `LoadInventory()` accepts the same array. Integrates into existing `FPlayerSaveData`.

---

## File Plan

### Files to CREATE (new system):
- `Public/Inventory/DaItemDefinition.h` — UDataAsset for item templates
- `Public/Inventory/DaInventoryEntry.h` — USTRUCT for replicated item instance
- `Public/Inventory/DaInventoryList.h` — FFastArraySerializer wrapper

### Files to REWRITE (keeping the filename, replacing contents):
- `Public/Inventory/DaInventoryComponent.h` — New component API
- `Private/Inventory/DaInventoryComponent.cpp` — New implementation
- `Public/Inventory/DaInventoryItemInterface.h` — Simplified to return asset IDs
- `Private/Inventory/DaInventoryItemInterface.cpp` — Updated defaults

### Files to DELETE:
- `Public/Inventory/DaInventoryItemBase.h` — Replaced by FDaInventoryEntry
- `Private/Inventory/DaInventoryItemBase.cpp`
- `Public/Inventory/DaStackableInventoryItem.h` — Stacking is now built into FDaInventoryEntry
- `Private/Inventory/DaStackableInventoryItem.cpp`
- `Public/Inventory/DaInventoryItemFactory.h` — Factory UObject pattern removed
- `Private/Inventory/DaInventoryItemFactory.cpp`
- `Public/Inventory/DaEquipmentInventoryComponent.h` — Equipment is now tag-based slots on the base component
- `Private/Inventory/DaEquipmentInventoryComponent.cpp`
- `Public/Inventory/DaInventoryBlueprintLibrary.h` — Merged into component API
- `Private/Inventory/DaInventoryBlueprintLibrary.cpp`
- `Public/Inventory/DaInventoryUIWidget.h` — Will be rebuilt with MVVM later
- `Private/Inventory/DaInventoryUIWidget.cpp`
- `Public/Inventory/DaInventoryWidgetController.h` — Will be rebuilt with MVVM later
- `Private/Inventory/DaInventoryWidgetController.cpp`

### Files to UPDATE (minimal changes):
- `Public/DaPlayerState.h` — Change `UDaInventoryComponent*` type (stays the same, new internals)
- `Private/DaPlayerState.cpp` — Update SavePlayerState/LoadPlayerState to use new Save/Load API
- `Public/DaSaveGame.h` — Add `TArray<FDaInventoryEntry> SavedInventory` to `FPlayerSaveData`
- `Public/DaItemActor.h` — Update `IDaInventoryItemInterface` implementation
- `Private/DaItemActor.cpp` — Return `FPrimaryAssetId` instead of constructing UObjects
- `Public/CoreGameplayTags.h` — No changes needed (existing tags are fine)
- `GameplayFramework.Build.cs` — No changes needed

### Collectibles module updates:
- `Collectibles/Public/CEInventoryItemFactory.h` — DELETE
- `Collectibles/Private/CEInventoryItemFactory.cpp` — DELETE
- `Collectibles/Public/CEInventoryItem.h` — DELETE
- `Collectibles/Private/CEInventoryItem.cpp` — DELETE
- `Collectibles/Private/CECollectibleActorBase.cpp` — Update `AddToInventory` to use new component API
- `Collectibles/Private/Collectibles.cpp` — Remove factory registration from module startup

---

## Tasks

### Task 1: Create FDaInventoryEntry and FDaInventoryList

**Files:**
- Create: `Source/GameplayFramework/Public/Inventory/DaInventoryEntry.h`
- Create: `Source/GameplayFramework/Public/Inventory/DaInventoryList.h`

**Step 1: Create `DaInventoryEntry.h`**

This is the core replicated item struct. It stores per-instance state and references a definition asset.

```cpp
// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "DaInventoryEntry.generated.h"

class UDaItemDefinition;
struct FDaInventoryList;

USTRUCT(BlueprintType)
struct GAMEPLAYFRAMEWORK_API FDaInventoryEntry : public FFastArraySerializerItem
{
    GENERATED_BODY()

    FDaInventoryEntry()
        : SlotIndex(INDEX_NONE)
        , StackCount(1)
        , MaxStackCount(1)
    {}

    // Unique ID for this specific item instance (persists across save/load)
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FGuid ItemID;

    // Reference to the item definition data asset
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FPrimaryAssetId ItemDefinitionID;

    // Which slot this item occupies
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int32 SlotIndex;

    // Current stack count (1 for non-stackable)
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int32 StackCount;

    // Max stack size (copied from definition on creation)
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    int32 MaxStackCount;

    // Tags describing this item instance (copied from definition, can be modified at runtime)
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FGameplayTagContainer Tags;

    // Optional: ability set to grant when equipped (asset path from definition)
    UPROPERTY(BlueprintReadOnly, Category="Inventory")
    FPrimaryAssetId AbilitySetID;

    bool IsValid() const { return ItemID.IsValid(); }
    bool IsStackable() const { return MaxStackCount > 1; }
    bool CanStackWith(const FDaInventoryEntry& Other) const;

    void PreReplicatedRemove(const FDaInventoryList& InArraySerializer);
    void PostReplicatedAdd(const FDaInventoryList& InArraySerializer);
    void PostReplicatedChange(const FDaInventoryList& InArraySerializer);
};
```

**Step 2: Create `DaInventoryList.h`**

The FFastArraySerializer wrapper that owns the array and forwards callbacks to the component.

```cpp
// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "DaInventoryEntry.h"
#include "DaInventoryList.generated.h"

class UDaInventoryComponent;

USTRUCT(BlueprintType)
struct GAMEPLAYFRAMEWORK_API FDaInventoryList : public FFastArraySerializer
{
    GENERATED_BODY()

    FDaInventoryList()
        : OwnerComponent(nullptr)
    {}

    // The actual replicated item array
    UPROPERTY()
    TArray<FDaInventoryEntry> Entries;

    // Owning component (set during init, not replicated)
    UPROPERTY(NotReplicated)
    TObjectPtr<UDaInventoryComponent> OwnerComponent;

    // FFastArraySerializer contract
    bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
    {
        return FFastArraySerializer::FastArrayDeltaSerialize<FDaInventoryEntry, FDaInventoryList>(
            Entries, DeltaParms, *this);
    }

    // Mutation helpers (server-only)
    void AddEntry(const FDaInventoryEntry& NewEntry);
    void RemoveEntry(int32 SlotIndex);
    void UpdateEntry(int32 SlotIndex, const FDaInventoryEntry& UpdatedEntry);

    // Query helpers
    int32 FindFirstEmptySlot(int32 MaxSize) const;
    int32 FindStackableSlot(const FDaInventoryEntry& ForEntry) const;
    const FDaInventoryEntry* FindBySlot(int32 SlotIndex) const;
    FDaInventoryEntry* FindBySlotMutable(int32 SlotIndex);
    int32 GetCount() const { return Entries.Num(); }
};

template<>
struct TStructOpsTypeTraits<FDaInventoryList> : public TStructOpsTypeTraitsBase2<FDaInventoryList>
{
    enum { WithNetDeltaSerializer = true };
};
```

**Step 3: Commit**

```bash
git add Source/GameplayFramework/Public/Inventory/DaInventoryEntry.h \
        Source/GameplayFramework/Public/Inventory/DaInventoryList.h
git commit -m "feat(inventory): add FDaInventoryEntry and FDaInventoryList structs"
```

---

### Task 2: Create UDaItemDefinition Data Asset

**Files:**
- Create: `Source/GameplayFramework/Public/Inventory/DaItemDefinition.h`
- Create: `Source/GameplayFramework/Private/Inventory/DaItemDefinition.cpp`

**Step 1: Create `DaItemDefinition.h`**

Static item template — designers create these as data assets. One per item type.

```cpp
// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "DaItemDefinition.generated.h"

class UDaAbilitySet;

UCLASS(BlueprintType, Blueprintable)
class GAMEPLAYFRAMEWORK_API UDaItemDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:

    // Display name shown in UI
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
    FText DisplayName;

    // Description shown in UI
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
    FText Description;

    // Icon for UI display
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|UI")
    TSoftObjectPtr<UTexture2D> Icon;

    // Mesh to display in world or inspect mode
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|Visual")
    TSoftObjectPtr<UStaticMesh> DisplayMesh;

    // Tags that categorize this item (stackable, equipable, type, etc.)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item")
    FGameplayTagContainer ItemTags;

    // Max stack size (1 = not stackable)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|Stacking", meta=(ClampMin="1"))
    int32 MaxStackCount = 1;

    // Ability set granted when this item is equipped
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|Abilities")
    TSoftObjectPtr<UDaAbilitySet> AbilitySetToGrant;

    // For equipment: which slot(s) this item can go in
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item|Equipment")
    FGameplayTagContainer EquipSlotTags;

    // Primary Asset ID support for async loading and save references
    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
};
```

**Step 2: Create `DaItemDefinition.cpp`**

```cpp
// Copyright Dream Awake Solutions LLC

#include "Inventory/DaItemDefinition.h"

FPrimaryAssetId UDaItemDefinition::GetPrimaryAssetId() const
{
    return FPrimaryAssetId("ItemDefinition", GetFName());
}
```

**Step 3: Commit**

```bash
git add Source/GameplayFramework/Public/Inventory/DaItemDefinition.h \
        Source/GameplayFramework/Private/Inventory/DaItemDefinition.cpp
git commit -m "feat(inventory): add UDaItemDefinition data asset"
```

---

### Task 3: Implement FDaInventoryEntry and FDaInventoryList

**Files:**
- Create: `Source/GameplayFramework/Private/Inventory/DaInventoryEntry.cpp`
- Create: `Source/GameplayFramework/Private/Inventory/DaInventoryList.cpp`

**Step 1: Implement `DaInventoryEntry.cpp`**

```cpp
// Copyright Dream Awake Solutions LLC

#include "Inventory/DaInventoryEntry.h"
#include "Inventory/DaInventoryList.h"
#include "Inventory/DaInventoryComponent.h"

bool FDaInventoryEntry::CanStackWith(const FDaInventoryEntry& Other) const
{
    return IsStackable()
        && ItemDefinitionID == Other.ItemDefinitionID
        && StackCount < MaxStackCount;
}

void FDaInventoryEntry::PreReplicatedRemove(const FDaInventoryList& InArraySerializer)
{
    if (InArraySerializer.OwnerComponent)
    {
        InArraySerializer.OwnerComponent->OnEntryRemovedInternal(*this);
    }
}

void FDaInventoryEntry::PostReplicatedAdd(const FDaInventoryList& InArraySerializer)
{
    if (InArraySerializer.OwnerComponent)
    {
        InArraySerializer.OwnerComponent->OnEntryAddedInternal(*this);
    }
}

void FDaInventoryEntry::PostReplicatedChange(const FDaInventoryList& InArraySerializer)
{
    if (InArraySerializer.OwnerComponent)
    {
        InArraySerializer.OwnerComponent->OnEntryChangedInternal(*this);
    }
}
```

**Step 2: Implement `DaInventoryList.cpp`**

```cpp
// Copyright Dream Awake Solutions LLC

#include "Inventory/DaInventoryList.h"

void FDaInventoryList::AddEntry(const FDaInventoryEntry& NewEntry)
{
    Entries.Add(NewEntry);
    MarkItemDirty(Entries.Last());
}

void FDaInventoryList::RemoveEntry(int32 SlotIndex)
{
    for (int32 i = Entries.Num() - 1; i >= 0; --i)
    {
        if (Entries[i].SlotIndex == SlotIndex)
        {
            Entries.RemoveAt(i);
            MarkArrayDirty();
            return;
        }
    }
}

void FDaInventoryList::UpdateEntry(int32 SlotIndex, const FDaInventoryEntry& UpdatedEntry)
{
    if (FDaInventoryEntry* Found = FindBySlotMutable(SlotIndex))
    {
        *Found = UpdatedEntry;
        MarkItemDirty(*Found);
    }
}

int32 FDaInventoryList::FindFirstEmptySlot(int32 MaxSize) const
{
    TSet<int32> OccupiedSlots;
    for (const FDaInventoryEntry& Entry : Entries)
    {
        OccupiedSlots.Add(Entry.SlotIndex);
    }

    for (int32 i = 0; i < MaxSize; ++i)
    {
        if (!OccupiedSlots.Contains(i))
        {
            return i;
        }
    }
    return INDEX_NONE;
}

int32 FDaInventoryList::FindStackableSlot(const FDaInventoryEntry& ForEntry) const
{
    for (const FDaInventoryEntry& Entry : Entries)
    {
        if (Entry.CanStackWith(ForEntry))
        {
            return Entry.SlotIndex;
        }
    }
    return INDEX_NONE;
}

const FDaInventoryEntry* FDaInventoryList::FindBySlot(int32 SlotIndex) const
{
    for (const FDaInventoryEntry& Entry : Entries)
    {
        if (Entry.SlotIndex == SlotIndex)
        {
            return &Entry;
        }
    }
    return nullptr;
}

FDaInventoryEntry* FDaInventoryList::FindBySlotMutable(int32 SlotIndex)
{
    for (FDaInventoryEntry& Entry : Entries)
    {
        if (Entry.SlotIndex == SlotIndex)
        {
            return &Entry;
        }
    }
    return nullptr;
}
```

**Step 3: Commit**

```bash
git add Source/GameplayFramework/Private/Inventory/DaInventoryEntry.cpp \
        Source/GameplayFramework/Private/Inventory/DaInventoryList.cpp
git commit -m "feat(inventory): implement FDaInventoryEntry and FDaInventoryList"
```

---

### Task 4: Rewrite UDaInventoryComponent

This is the core task. Replace the entire component with the new FastArray-backed implementation.

**Files:**
- Rewrite: `Source/GameplayFramework/Public/Inventory/DaInventoryComponent.h`
- Rewrite: `Source/GameplayFramework/Private/Inventory/DaInventoryComponent.cpp`

**Step 1: Rewrite the header**

The new component owns an `FDaInventoryList`, exposes server RPCs for mutation, and broadcasts delegates for UI/gameplay listeners.

Key API:
- `AddItem(FPrimaryAssetId, int32 SlotHint)` — Local call, routes to server
- `RemoveItem(int32 SlotIndex)` — Local call, routes to server
- `MoveItem(int32 From, int32 To)` — Local call, routes to server
- `GetEntries()` — Read-only access to current items
- `GetEntryAtSlot(int32)` — Single item lookup
- `SaveInventory() / LoadInventory()` — Persistence

Delegates:
- `FOnInventoryEntryEvent OnEntryAdded`
- `FOnInventoryEntryEvent OnEntryRemoved`
- `FOnInventoryEntryEvent OnEntryChanged`

Internal methods called by FastArray callbacks:
- `OnEntryAddedInternal` / `OnEntryRemovedInternal` / `OnEntryChangedInternal`

Equipment is handled by tag-based slot filtering on the same component — no separate equipment component needed. `AddItem` with a `SlotHint` that has an equipment tag validates via `EquipSlotTags` on the definition.

**Step 2: Implement the cpp**

Server RPCs:
- `Server_AddItem_Implementation` — Validates authority, finds slot (stack or empty), creates entry, calls `InventoryList.AddEntry()`. For stackable items, finds existing stack and calls `UpdateEntry()`.
- `Server_RemoveItem_Implementation` — Validates slot exists, calls `InventoryList.RemoveEntry()`.
- `Server_MoveItem_Implementation` — Validates both slots, swaps entries.

The component must call `GetLifetimeReplicatedProps` with `DOREPLIFETIME(ThisClass, InventoryList)`.

`BeginPlay` sets `InventoryList.OwnerComponent = this`.

**Step 3: Compile and test**

Build the project to ensure the new component compiles. Temporarily leave old files in place (they'll be deleted in Task 6).

**Step 4: Commit**

```bash
git add Source/GameplayFramework/Public/Inventory/DaInventoryComponent.h \
        Source/GameplayFramework/Private/Inventory/DaInventoryComponent.cpp
git commit -m "feat(inventory): rewrite DaInventoryComponent with FFastArraySerializer"
```

---

### Task 5: Update IDaInventoryItemInterface

**Files:**
- Rewrite: `Source/GameplayFramework/Public/Inventory/DaInventoryItemInterface.h`
- Update: `Source/GameplayFramework/Private/Inventory/DaInventoryItemInterface.cpp`

Simplify the interface. World actors now return:
- `GetItemDefinitionID() → FPrimaryAssetId` (points to the UDaItemDefinition asset)
- `GetStackCount() → int32` (how many of this item the pickup represents, default 1)
- `AddToInventory(APawn*, bool bDestroy)` (unchanged)

Remove: `GetItemName`, `GetItemDescription`, `GetItemTags`, `GetItemThumbnail`, `GetRenderTargetMaterial`, `GetMeshComponent`, `GetAbilitySet` — all of this now lives on `UDaItemDefinition`.

**Step 1: Rewrite the header**

```cpp
UINTERFACE(Blueprintable)
class GAMEPLAYFRAMEWORK_API UDaInventoryItemInterface : public UInterface
{
    GENERATED_BODY()
};

class GAMEPLAYFRAMEWORK_API IDaInventoryItemInterface
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Inventory")
    FPrimaryAssetId GetItemDefinitionID() const;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Inventory")
    int32 GetStackCount() const;

    UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category="Inventory")
    void AddToInventory(APawn* InstigatorPawn, bool bDestroyActor = true);
};
```

**Step 2: Commit**

```bash
git add Source/GameplayFramework/Public/Inventory/DaInventoryItemInterface.h \
        Source/GameplayFramework/Private/Inventory/DaInventoryItemInterface.cpp
git commit -m "refactor(inventory): simplify IDaInventoryItemInterface to return asset IDs"
```

---

### Task 6: Delete Old Files and Update Build

**Files:**
- Delete all files listed in the "Files to DELETE" section above
- Update: `Source/GameplayFramework/GameplayFramework.Build.cs` if needed (likely no changes)

**Step 1: Delete old inventory files**

```bash
git rm Source/GameplayFramework/Public/Inventory/DaInventoryItemBase.h \
      Source/GameplayFramework/Private/Inventory/DaInventoryItemBase.cpp \
      Source/GameplayFramework/Public/Inventory/DaStackableInventoryItem.h \
      Source/GameplayFramework/Private/Inventory/DaStackableInventoryItem.cpp \
      Source/GameplayFramework/Public/Inventory/DaInventoryItemFactory.h \
      Source/GameplayFramework/Private/Inventory/DaInventoryItemFactory.cpp \
      Source/GameplayFramework/Public/Inventory/DaEquipmentInventoryComponent.h \
      Source/GameplayFramework/Private/Inventory/DaEquipmentInventoryComponent.cpp \
      Source/GameplayFramework/Public/Inventory/DaInventoryBlueprintLibrary.h \
      Source/GameplayFramework/Private/Inventory/DaInventoryBlueprintLibrary.cpp \
      Source/GameplayFramework/Public/Inventory/DaInventoryUIWidget.h \
      Source/GameplayFramework/Private/Inventory/DaInventoryUIWidget.cpp \
      Source/GameplayFramework/Public/Inventory/DaInventoryWidgetController.h \
      Source/GameplayFramework/Private/Inventory/DaInventoryWidgetController.cpp
```

**Step 2: Fix any remaining #include references**

Search for `#include "Inventory/DaInventoryItemBase.h"` and similar across the codebase and update or remove.

**Step 3: Compile to verify clean build**

```bash
# From project root
"C:/Program Files/Epic Games/UE_5.7/Engine/Build/BatchFiles/Build.bat" GlitchShaperEditor Win64 Development -Project="C:/Source/GlitchShaper/GlitchShaper.uproject"
```

**Step 4: Commit**

```bash
git add -A
git commit -m "refactor(inventory): delete old UObject-based inventory files"
```

---

### Task 7: Update DaItemActor and World Actor Integration

**Files:**
- Update: `Source/GameplayFramework/Public/DaItemActor.h`
- Update: `Source/GameplayFramework/Private/DaItemActor.cpp`

Update `ADaItemActor` to implement the new simplified `IDaInventoryItemInterface`:
- Add `UPROPERTY(EditDefaultsOnly) FPrimaryAssetId ItemDefinitionID` — set by designers in Blueprint defaults
- `GetItemDefinitionID_Implementation()` returns the asset ID
- `GetStackCount_Implementation()` returns 1
- `AddToInventory_Implementation()` gets the inventory component from the pawn's player state and calls `AddItem(ItemDefinitionID)`
- Remove old fields: `Name`, `Description`, `TypeTags`, `RenderTargetMaterial` (these now live on the definition asset)
- Remove `CreateFromInventoryItem` static method (dead code)

**Step 1: Commit**

```bash
git add Source/GameplayFramework/Public/DaItemActor.h \
        Source/GameplayFramework/Private/DaItemActor.cpp
git commit -m "refactor(inventory): update DaItemActor to new inventory interface"
```

---

### Task 8: Update Collectibles Module

**Files:**
- Delete: `Source/Collectibles/Public/CEInventoryItemFactory.h`
- Delete: `Source/Collectibles/Private/CEInventoryItemFactory.cpp`
- Delete: `Source/Collectibles/Public/CEInventoryItem.h`
- Delete: `Source/Collectibles/Private/CEInventoryItem.cpp`
- Update: `Source/Collectibles/Private/CECollectibleActorBase.cpp` — `AddToInventory` now calls `InventoryComponent->AddItem(ItemDefinitionID)`
- Update: `Source/Collectibles/Private/Collectibles.cpp` — Remove factory registration from `StartupModule`
- Update: `Source/Collectibles/Collectibles.Build.cs` — Remove dependency on deleted factory classes if needed

Each collectible type gets a `UDaItemDefinition` data asset. `ACECollectibleActorBase` stores a `FPrimaryAssetId` pointing to it and returns it from the interface.

**Step 1: Commit**

```bash
git add -A
git commit -m "refactor(inventory): update Collectibles module to new inventory system"
```

---

### Task 9: Add Save/Load Integration

**Files:**
- Update: `Source/GameplayFramework/Public/DaSaveGame.h` — Add `TArray<FDaInventoryEntry> SavedInventory` to `FPlayerSaveData`
- Update: `Source/GameplayFramework/Private/DaPlayerState.cpp` — Call `InventoryComp->SaveInventory()` and `LoadInventory()` in save/load hooks

**Step 1: Add to FPlayerSaveData**

```cpp
// In FPlayerSaveData:
UPROPERTY()
TArray<FDaInventoryEntry> SavedInventory;
```

**Step 2: Wire up in PlayerState**

```cpp
void ADaPlayerState::SavePlayerState_Implementation(UDaSaveGame* SaveObject)
{
    // ... existing save logic ...
    PlayerData->SavedInventory = InventoryComp->SaveInventory();
}

void ADaPlayerState::LoadPlayerState_Implementation(UDaSaveGame* SaveObject)
{
    // ... existing load logic ...
    InventoryComp->LoadInventory(PlayerData->SavedInventory);
}
```

**Step 3: Commit**

```bash
git add Source/GameplayFramework/Public/DaSaveGame.h \
        Source/GameplayFramework/Private/DaPlayerState.cpp
git commit -m "feat(inventory): add save/load integration for inventory"
```

---

### Task 10: Update GameplayFramework Module Startup

**Files:**
- Update: `Source/GameplayFramework/Private/GameplayFramework.cpp` — Remove factory registration from `StartupModule`

The static `UDaInventoryItemBase::Factories` array no longer exists. Remove any references to it in module startup.

Also register `"ItemDefinition"` as a primary asset type so the asset manager can find `UDaItemDefinition` assets:

```cpp
// In DefaultGame.ini or via code:
// +PrimaryAssetTypesToScan=(PrimaryAssetType="ItemDefinition",AssetBaseClass="/Script/GameplayFramework.DaItemDefinition",...)
```

**Step 1: Commit**

```bash
git add Source/GameplayFramework/Private/GameplayFramework.cpp
git commit -m "refactor(inventory): clean up module startup, register ItemDefinition asset type"
```

---

### Task 11: Final Compile, Test, and Cleanup

**Step 1: Full rebuild**

Close the editor and do a clean build:

```bash
"C:/Program Files/Epic Games/UE_5.7/Engine/Build/BatchFiles/Build.bat" GlitchShaperEditor Win64 Development -Project="C:/Source/GlitchShaper/GlitchShaper.uproject"
```

**Step 2: Verify in editor**

1. Open the editor
2. Create a test `UDaItemDefinition` data asset (Content Browser → Miscellaneous → Data Asset → DaItemDefinition)
3. Set up a test pickup actor with the new `ItemDefinitionID` reference
4. PIE and pick up the item
5. Verify the log shows the item being added server-side
6. Verify no replication errors

**Step 3: Verify save/load**

1. Pick up items in PIE
2. Trigger save
3. Stop PIE, restart, trigger load
4. Verify items are restored

**Step 4: Final commit**

```bash
git add -A
git commit -m "feat(inventory): complete inventory system refactor to FFastArraySerializer"
```

---

## Migration Notes for Blueprints

After completing the C++ refactor:

1. **Create `UDaItemDefinition` data assets** for each item type (potions, collectibles, equipment, etc.) in the Content Browser
2. **Update pickup Blueprints** (`BP_ItemActor`, collectible actors) to reference the new `ItemDefinitionID` property instead of setting Name/Description/Tags individually
3. **Register `ItemDefinition`** in `DefaultGame.ini` under `PrimaryAssetTypesToScan` so the asset manager can discover and load them
4. **Equipment** — Equipment slots are now tags on the component. Set `EquipSlotTags` on the definition and the component's inventory tags to match
5. **UI will be rebuilt** with MVVM in a follow-up task. The component delegates (`OnEntryAdded`, `OnEntryRemoved`, `OnEntryChanged`) are the integration points

## MVVM Preparation

The component exposes three delegates that a ViewModel will subscribe to:

```cpp
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnInventoryEntryEvent, const FDaInventoryEntry&, Entry, int32, SlotIndex);

UPROPERTY(BlueprintAssignable) FOnInventoryEntryEvent OnEntryAdded;
UPROPERTY(BlueprintAssignable) FOnInventoryEntryEvent OnEntryRemoved;
UPROPERTY(BlueprintAssignable) FOnInventoryEntryEvent OnEntryChanged;
```

A future `UDaInventoryViewModel : UMVVMViewModelBase` will:
1. Subscribe to these delegates in `Initialize()`
2. Expose `TArray<FDaInventoryEntry>` as a field viewmodel property
3. Push per-slot updates to bound widgets via `UE_MVVM_SET_PROPERTY_VALUE`
