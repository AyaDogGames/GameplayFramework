// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "UI/DaUserWidgetBase.h"
#include "DaInventoryPanelWidget.generated.h"

class UButton;
class UDaInventoryComponent;
class UDaInventoryItemBase;
class UDaInventoryWidgetController;
class UPanelWidget;
class UScrollBox;
class UTextBlock;
class UVerticalBox;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDaInventoryRowAssignRequested, FGuid, ItemID, int32, SlotNumber);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDaInventoryRowSelected, FGuid, ItemID);

/**
 * UDaInventoryRowWidget
 *
 * One line of the inventory panel: the item's name and count, its condition when it has one, and
 * the hotbar-assign buttons [1][2][3][4].
 *
 * It exists as its own widget class for one reason: a dynamic delegate carries no payload, so a
 * shared handler cannot tell which row's "3" was clicked. Giving each row an object to be gives
 * every button an obvious owner, and it is the natural extension point for a themed row later.
 */
UCLASS()
class GAMEPLAYFRAMEWORK_API UDaInventoryRowWidget : public UUserWidget
{
	GENERATED_BODY()

public:

	virtual void NativeOnInitialized() override;

	/** Fill this row in from a view-model. Safe to call repeatedly (a refresh reuses rows). */
	void SetItem(UDaInventoryItemBase* Item, int32 InHotbarSlotCount);

	void SetSelected(bool bInSelected);

	UFUNCTION(BlueprintPure, Category="Inventory|UI")
	FGuid GetRowItemID() const { return RowItemID; }

	/** The text this row is showing, so a caller (or a smoke) can read what the player sees
	 *  without walking the widget tree. */
	UFUNCTION(BlueprintPure, Category="Inventory|UI")
	FText GetRowLabel() const;

	/** Fires when one of the row's [1]..[4] buttons is clicked. */
	FOnDaInventoryRowAssignRequested OnAssignRequested;

	/** Fires when the row's name is clicked. */
	FOnDaInventoryRowSelected OnRowSelected;

private:

	UFUNCTION()
	void HandleSelectClicked();
	UFUNCTION()
	void HandleAssign1Clicked();
	UFUNCTION()
	void HandleAssign2Clicked();
	UFUNCTION()
	void HandleAssign3Clicked();
	UFUNCTION()
	void HandleAssign4Clicked();

	void RequestAssign(int32 SlotNumber);

	FGuid RowItemID;

	UPROPERTY(Transient)
	TObjectPtr<UButton> SelectButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ConditionText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> AssignButtons;
};

/**
 * UDaInventoryPanelWidget
 *
 * Theme-neutral C++ base for the inventory panel: one row per inventory entry showing name, stack
 * count and condition, with per-row hotbar-assign buttons.
 *
 * It is a UDaActivatableWidget (through UDaUserWidgetBase), so it is pushed and popped on a CommonUI
 * layer — see UDaGameplayAbility_ToggleUILayer, which is how a key opens it.
 *
 * It feeds itself: if nothing set a WidgetController before construction it creates a
 * UDaInventoryWidgetController over the local player's PlayerState inventory, which is also what
 * assignment routes through (UDaInventoryWidgetController::AssignToHotbarSlot). Everything the rows
 * display comes from the same UDaInventoryItemBase view-models the hotbar reads, so the panel and
 * the hotbar cannot describe one item two ways.
 *
 * Assignment UX is per-row [1][2][3][4] buttons rather than "select a row, then press a number
 * key". Two reasons: the number keys are already the hotbar's own buttons (holding this panel open
 * with game input live, which is deliberate so the toggle key can close it, means a 1 keypress
 * would fire quick-slot 1 as well), and a button is a thing a test can click. Row selection still
 * exists (click the name) for a themed subclass that wants it, with AssignSelectedToSlot.
 */
UCLASS(Blueprintable)
class GAMEPLAYFRAMEWORK_API UDaInventoryPanelWidget : public UDaUserWidgetBase
{
	GENERATED_BODY()

public:

	UDaInventoryPanelWidget(const FObjectInitializer& ObjectInitializer);

	virtual void NativeOnInitialized() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Rebuild the rows from the controller's current view-models. */
	UFUNCTION(BlueprintCallable, Category="Inventory|UI")
	void RefreshPanel();

	/** Assign ItemID to hotbar slot 1..4 through the widget controller. */
	UFUNCTION(BlueprintCallable, Category="Inventory|UI")
	bool AssignToSlot(FGuid ItemID, int32 SlotNumber);

	/** Assign whatever row is selected; false when nothing is selected. */
	UFUNCTION(BlueprintCallable, Category="Inventory|UI")
	bool AssignSelectedToSlot(int32 SlotNumber);

	UFUNCTION(BlueprintPure, Category="Inventory|UI")
	int32 GetRowCount() const { return Rows.Num(); }

	/** Text of row RowIndex (0-based), empty when out of range. What the player is reading. */
	UFUNCTION(BlueprintPure, Category="Inventory|UI")
	FText GetRowLabel(int32 RowIndex) const;

	UFUNCTION(BlueprintPure, Category="Inventory|UI")
	FGuid GetSelectedItemID() const { return SelectedItemID; }

	UFUNCTION(BlueprintPure, Category="Inventory|UI")
	UDaInventoryWidgetController* GetInventoryController() const { return InventoryController; }

	/**
	 * The panel currently constructed in the world WorldContextObject belongs to, or null.
	 *
	 * A static registry rather than a lookup through the toggling ability: the ability instance is
	 * not reachable from outside the ASC, and a multi-window PIE session has one panel per world,
	 * so the world is the thing that has to disambiguate them.
	 */
	UFUNCTION(BlueprintPure, Category="Inventory|UI", meta=(WorldContext="WorldContextObject"))
	static UDaInventoryPanelWidget* GetActivePanel(const UObject* WorldContextObject);

protected:

	/** Fires at the end of every refresh. */
	UFUNCTION(BlueprintImplementableEvent, Category="Inventory|UI")
	void OnPanelRefreshed();

	/** How many hotbar buttons each row offers (Equip.Slot.Item1..N). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Inventory|UI", meta=(ClampMin="1", ClampMax="4"))
	int32 HotbarSlotCount = 4;

	/** Container the default-built rows go into. Bind it in a UMG subclass to place the list
	 *  yourself; leave it unbound and this class creates a scrolling one. */
	UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional), Category="Inventory|UI")
	TObjectPtr<UPanelWidget> RowContainer;

private:

	/** Resolve (or create) the controller and bind it. Idempotent. */
	void EnsureController();

	void BuildDefaultWidgetTree();

	UFUNCTION()
	void HandleInventoryChanged(const TArray<UDaInventoryItemBase*>& Items);

	UFUNCTION()
	void HandleLoadoutChanged();

	UFUNCTION()
	void HandleRowAssignRequested(FGuid ItemID, int32 SlotNumber);

	UFUNCTION()
	void HandleRowSelected(FGuid ItemID);

	UPROPERTY(Transient)
	TObjectPtr<UDaInventoryWidgetController> InventoryController;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDaInventoryRowWidget>> Rows;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EmptyText;

	FGuid SelectedItemID;

	/** Inventory whose OnLoadoutChanged this panel is bound to. */
	TWeakObjectPtr<UDaInventoryComponent> BoundInventory;

	/** Every constructed panel, for GetActivePanel. Weak, and swept on every lookup. */
	static TArray<TWeakObjectPtr<UDaInventoryPanelWidget>> ActivePanels;
};
