// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"
#include "Inventory/DaConditionBand.h"
#include "DaHotbarWidget.generated.h"

class UBorder;
class UButton;
class UDaEquipmentManagerComponent;
class UDaInventoryComponent;
class UHorizontalBox;
class UImage;
class UPanelWidget;
class UProgressBar;
class UTextBlock;
class UTexture2D;
struct FDaAppliedEquipmentEntry;
struct FDaInventoryEntry;

/**
 * FDaHotbarSlotState
 *
 * What one hotbar slot currently shows, cached at the last refresh. The BlueprintPure queries on
 * UDaHotbarWidget read THIS, not the live inventory, on purpose: a test (or a Blueprint) asking
 * "does slot 1 show the sword as equipped?" must get the answer the widget drew, so that a refresh
 * the widget never received shows up as a failure instead of being papered over by a fresh read.
 */
USTRUCT(BlueprintType)
struct GAMEPLAYFRAMEWORK_API FDaHotbarSlotState
{
	GENERATED_BODY()

	/** Equip.Slot.ItemN this slot draws. */
	UPROPERTY(BlueprintReadOnly, Category="Hotbar")
	FGameplayTag SlotTag;

	/** 1-based button number, as labelled on screen and pressed on the keyboard. */
	UPROPERTY(BlueprintReadOnly, Category="Hotbar")
	int32 SlotNumber = 0;

	/** The assigned item, or an invalid Guid for an empty slot. */
	UPROPERTY(BlueprintReadOnly, Category="Hotbar")
	FGuid ItemID;

	/** True when the loadout names an item for this slot AND that item is still in the inventory. */
	UPROPERTY(BlueprintReadOnly, Category="Hotbar")
	bool bAssigned = false;

	/** True when the pawn's equipment manager currently has this slot filled with this item. */
	UPROPERTY(BlueprintReadOnly, Category="Hotbar")
	bool bEquipped = false;

	/** True when a press would equip-toggle rather than use. */
	UPROPERTY(BlueprintReadOnly, Category="Hotbar")
	bool bEquippable = false;

	UPROPERTY(BlueprintReadOnly, Category="Hotbar")
	FText ItemName;

	UPROPERTY(BlueprintReadOnly, Category="Hotbar")
	int32 StackCount = 0;

	UPROPERTY(BlueprintReadOnly, Category="Hotbar")
	bool bUsesCondition = false;

	/** Condition as 0..1 of the item's grade-derived cap; 0 for items with no condition model. */
	UPROPERTY(BlueprintReadOnly, Category="Hotbar")
	float ConditionFraction = 0.f;

	UPROPERTY(BlueprintReadOnly, Category="Hotbar")
	int32 Condition = 0;

	UPROPERTY(BlueprintReadOnly, Category="Hotbar")
	int32 ConditionCap = 0;

	UPROPERTY(BlueprintReadOnly, Category="Hotbar")
	EDaConditionBand ConditionBand = EDaConditionBand::Normal;

	/** True when the assigned item's definition names an Icon texture. */
	UPROPERTY(BlueprintReadOnly, Category="Hotbar")
	bool bHasIcon = false;

	/** The definition's icon, for a themed slot to load however it likes. */
	UPROPERTY(BlueprintReadOnly, Category="Hotbar")
	TSoftObjectPtr<UTexture2D> Icon;
};

/**
 * Slate widgets making up one default-built slot. Only populated when this class builds its own
 * widget tree; a UMG theme that supplies its own visuals leaves these null and drives the display
 * from OnHotbarRefreshed + the BlueprintPure queries instead.
 */
USTRUCT()
struct FDaHotbarSlotWidgets
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TObjectPtr<UButton> Button;

	UPROPERTY(Transient)
	TObjectPtr<UImage> IconImage;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> EquippedBorder;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SlotLabel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CountLabel;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> ConditionBar;
};

/**
 * UDaHotbarWidget
 *
 * Theme-neutral C++ base for the hotbar row over Equip.Slot.Item1..4. It reads the local player's
 * loadout (on the PlayerState's UDaInventoryComponent) and equipped state (on the pawn's
 * UDaEquipmentManagerComponent), and refreshes from their delegates — OnLoadoutChanged for
 * assignment, OnEntryAdded/Changed/Removed for stack count and condition, OnEquipped/OnUnequipped
 * for the highlight. All of those fire on clients as well as the host, so a client's row shows the
 * CLIENT's own loadout.
 *
 * Two ways to use it:
 *  - Subclass it in UMG and author the visuals. Bind `SlotRow` to your own container (optional
 *    bind) and drive the display from OnHotbarRefreshed and the BlueprintPure queries.
 *  - Subclass it and author nothing. NativeOnInitialized then builds a plain default row — icon,
 *    stack count, condition bar, slot number, equipped tint — so a bare Blueprint renders
 *    something real instead of an empty rectangle.
 *
 * Clicking a slot runs UDaEquipmentManagerComponent::ActivateItemSlotForPawn, the same function
 * UDaGameplayAbility_QuickSlot runs for a key press, so the mouse and the keyboard cannot drift.
 */
UCLASS(Blueprintable)
class GAMEPLAYFRAMEWORK_API UDaHotbarWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	UDaHotbarWidget(const FObjectInitializer& ObjectInitializer);

	//~UUserWidget
	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	//~End UUserWidget

	/** Re-resolve the local player's inventory/equipment if needed, then rebuild every slot's
	 *  cached state and (when this class owns the visuals) redraw it. Cheap and idempotent —
	 *  every delegate this widget listens to lands here. */
	UFUNCTION(BlueprintCallable, Category="Hotbar")
	void RefreshHotbar();

	/** Press slot 1..4: use the assigned consumable, or equip-toggle the assigned equippable.
	 *  Same path as the quick-slot ability. False when the slot is empty or unresolvable. */
	UFUNCTION(BlueprintCallable, Category="Hotbar")
	bool ActivateSlot(int32 SlotNumber);

	// ----- State queries (what the row currently SHOWS) -----

	UFUNCTION(BlueprintPure, Category="Hotbar")
	int32 GetSlotCount() const { return SlotStates.Num(); }

	/** Whole cached state of slot 1..4; a default-constructed struct for an out-of-range number. */
	UFUNCTION(BlueprintPure, Category="Hotbar")
	FDaHotbarSlotState GetSlotState(int32 SlotNumber) const;

	UFUNCTION(BlueprintPure, Category="Hotbar")
	bool IsSlotAssigned(int32 SlotNumber) const;

	UFUNCTION(BlueprintPure, Category="Hotbar")
	bool IsSlotEquipped(int32 SlotNumber) const;

	UFUNCTION(BlueprintPure, Category="Hotbar")
	FGuid GetSlotItemID(int32 SlotNumber) const;

	UFUNCTION(BlueprintPure, Category="Hotbar")
	float GetSlotConditionFraction(int32 SlotNumber) const;

	UFUNCTION(BlueprintPure, Category="Hotbar")
	int32 GetSlotStackCount(int32 SlotNumber) const;

	/** True when the slot resolved and is drawing the definition's Icon texture. Lets a smoke
	 *  assert the icon arrived without reading pixels. */
	UFUNCTION(BlueprintPure, Category="Hotbar")
	bool SlotHasIcon(int32 SlotNumber) const;

	/** The inventory this row is bound to; null until a PlayerState with one exists. */
	UFUNCTION(BlueprintPure, Category="Hotbar")
	UDaInventoryComponent* GetBoundInventory() const { return BoundInventory.Get(); }

protected:

	/** Fires at the end of every refresh, after SlotStates is up to date. The hook a UMG theme
	 *  redraws from. */
	UFUNCTION(BlueprintImplementableEvent, Category="Hotbar")
	void OnHotbarRefreshed();

	/** How many buttons the row draws, 1..4 (Equip.Slot.Item1..N). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hotbar", meta=(ClampMin="1", ClampMax="4"))
	int32 SlotCount = 4;

	/** Container the default-built slots go into. Optional: bind it in a UMG subclass to place the
	 *  row yourself, or leave it unbound and this class creates one. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Hotbar")
	TObjectPtr<UHorizontalBox> SlotRow;

	// ----- Look of the default-built row (ignored when a UMG theme owns the visuals) -----

	UPROPERTY(EditAnywhere, Category="Hotbar|Default Style")
	float SlotSize = 64.f;

	UPROPERTY(EditAnywhere, Category="Hotbar|Default Style")
	float SlotSpacing = 6.f;

	/** Where the default-built row sits, relative to the bottom centre of the screen. Clear of the
	 *  gameplay overlay's vitals bars by default — at the very bottom the two draw over each other. */
	UPROPERTY(EditAnywhere, Category="Hotbar|Default Style")
	FVector2D RowOffsetFromBottom = FVector2D(0.f, -160.f);

	UPROPERTY(EditAnywhere, Category="Hotbar|Default Style")
	FLinearColor EmptySlotColor = FLinearColor(0.02f, 0.02f, 0.03f, 0.65f);

	UPROPERTY(EditAnywhere, Category="Hotbar|Default Style")
	FLinearColor EquippedTintColor = FLinearColor(0.2f, 0.85f, 1.f, 0.35f);

private:

	/** Idempotent, compare-and-rebind: resolve the local player's inventory (PlayerState) and
	 *  equipment manager (pawn) and move the delegate subscriptions if either changed. Returns
	 *  true when anything was rebound, so the caller knows a redraw is due.
	 *
	 *  Compare-and-rebind rather than bind-once because both objects are replaced under us: the
	 *  PlayerState arrives after the widget on a client, and the pawn (and its equipment manager)
	 *  is a new actor after every respawn. */
	bool EnsureBindings();

	void BindInventory(UDaInventoryComponent* Inventory);
	void BindEquipment(UDaEquipmentManagerComponent* Equipment);

	/** Rebuild SlotStates from the bound components. */
	void RebuildSlotStates();

	/** Push SlotStates into the default-built slot widgets. No-op when this class did not build them. */
	void ApplySlotStatesToDefaultWidgets();

	/** Create the plain row when no UMG theme supplied one. */
	void BuildDefaultWidgetTree();

	/** Low-frequency safety net: re-resolves the PlayerState/pawn this row reads. The delegates do
	 *  the actual work; this only notices that the objects carrying them were swapped (possession,
	 *  respawn, a late-arriving PlayerState on a client). */
	void HandleResolveTick();

	// Delegate targets. All of them just refresh: the row is four items wide, so working out
	// which slot a change touched would cost more than redrawing all of them.
	UFUNCTION()
	void HandleLoadoutChanged();

	UFUNCTION()
	void HandleEntryChanged(const FDaInventoryEntry& Entry, int32 SlotIndex);

	UFUNCTION()
	void HandleEquipmentChanged(const FDaAppliedEquipmentEntry& Entry);

	/**
	 * Its own handler because OnUnequipped fires BEFORE the entry is torn down — deliberately, so
	 * listeners can still see the spawned actors and grants, and so the host's timing matches a
	 * client's PreReplicatedRemove. A plain refresh from here would ask the equipment manager
	 * "what is in this slot?" and be told, correctly and uselessly, "the item that is about to
	 * leave" — leaving the highlight on until something else happened to refresh the row. So the
	 * departing slot/item pair is masked out for the duration of this one refresh.
	 */
	UFUNCTION()
	void HandleUnequipped(const FDaAppliedEquipmentEntry& Entry);

	// One handler per button: a dynamic delegate carries no payload, and four is not enough
	// repetition to justify a wrapper widget class.
	UFUNCTION()
	void HandleSlot1Clicked();
	UFUNCTION()
	void HandleSlot2Clicked();
	UFUNCTION()
	void HandleSlot3Clicked();
	UFUNCTION()
	void HandleSlot4Clicked();

	/** True when SlotNumber is 1..SlotStates.Num(). */
	bool IsValidSlotNumber(int32 SlotNumber) const;

	UPROPERTY(Transient)
	TArray<FDaHotbarSlotState> SlotStates;

	UPROPERTY(Transient)
	TArray<FDaHotbarSlotWidgets> SlotWidgets;

	TWeakObjectPtr<UDaInventoryComponent> BoundInventory;
	TWeakObjectPtr<UDaEquipmentManagerComponent> BoundEquipment;

	/** True once this class built its own row, so refreshes know they own the visuals. */
	bool bOwnsDefaultWidgets = false;

	/** Slot/item being unequipped right now, treated as already gone. See HandleUnequipped. */
	FGameplayTag UnequippingSlot;
	FGuid UnequippingItemID;

	FTimerHandle ResolveTimerHandle;
};
