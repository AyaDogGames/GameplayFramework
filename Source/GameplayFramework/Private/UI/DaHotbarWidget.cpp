// Copyright Dream Awake Solutions LLC


#include "UI/DaHotbarWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/Texture2D.h"
#include "Equipment/DaEquipmentManagerComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayFramework.h"
#include "Inventory/DaInventoryComponent.h"
#include "Inventory/DaInventoryEntry.h"
#include "Inventory/DaInventoryItemBase.h"
#include "TimerManager.h"

#define LOCTEXT_NAMESPACE "DaHotbar"

namespace DaHotbarPrivate
{
	/** How often the row re-checks WHICH PlayerState and pawn it should be reading. Not a refresh
	 *  rate: the delegates drive the content, this only catches the objects being swapped. */
	constexpr float ResolvePeriod = 0.25f;

	/**
	 * A flat rounded rectangle in one colour.
	 *
	 * Not UBorder::SetBrushColor: that only tints the border's brush, and a default-constructed
	 * FSlateBrush is DrawAs=Image with NO resource object, so tinting it draws exactly nothing (the
	 * first version of this row shipped invisible slot backgrounds for that reason). RoundedBox is
	 * the draw type that needs no texture at all, which is what a class shipping no art wants.
	 */
	FSlateBrush SolidBrush(const FLinearColor& Color, float CornerRadius)
	{
		FSlateBrush Brush;
		Brush.DrawAs = ESlateBrushDrawType::RoundedBox;
		Brush.TintColor = FSlateColor(Color);
		Brush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
		Brush.OutlineSettings.CornerRadii = FVector4(CornerRadius, CornerRadius, CornerRadius, CornerRadius);
		return Brush;
	}

	/** Bar colour per band, matching the item debug overlay so the two never describe the same
	 *  item differently. */
	FLinearColor BandColor(EDaConditionBand Band)
	{
		switch (Band)
		{
		case EDaConditionBand::Broken:   return FLinearColor(1.f, 0.25f, 0.25f);
		case EDaConditionBand::Critical: return FLinearColor(1.f, 0.5f, 0.2f);
		case EDaConditionBand::Worn:     return FLinearColor(1.f, 0.9f, 0.3f);
		default:                         return FLinearColor(0.4f, 0.9f, 0.5f);
		}
	}
}

UDaHotbarWidget::UDaHotbarWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SetIsFocusable(false);
}

void UDaHotbarWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	// Before the Slate widgets are built, so anything constructed here is part of the tree.
	BuildDefaultWidgetTree();
}

void UDaHotbarWidget::NativeConstruct()
{
	Super::NativeConstruct();

	EnsureBindings();
	RefreshHotbar();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(ResolveTimerHandle, this, &UDaHotbarWidget::HandleResolveTick,
			DaHotbarPrivate::ResolvePeriod, true);
	}
}

void UDaHotbarWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ResolveTimerHandle);
	}

	BindInventory(nullptr);
	BindEquipment(nullptr);

	Super::NativeDestruct();
}

// ---------------------------------------------------------------------------
// Binding
// ---------------------------------------------------------------------------

bool UDaHotbarWidget::EnsureBindings()
{
	APlayerController* PC = GetOwningPlayer();

	UDaInventoryComponent* Inventory = nullptr;
	if (PC && PC->PlayerState)
	{
		Inventory = UDaInventoryComponent::GetInventoryFromActor(PC->PlayerState.Get());
	}

	UDaEquipmentManagerComponent* Equipment = nullptr;
	if (PC)
	{
		if (APawn* Pawn = PC->GetPawn())
		{
			Equipment = UDaEquipmentManagerComponent::GetEquipmentFromActor(Pawn);
		}
	}

	bool bRebound = false;
	if (BoundInventory.Get() != Inventory)
	{
		BindInventory(Inventory);
		bRebound = true;
	}
	if (BoundEquipment.Get() != Equipment)
	{
		BindEquipment(Equipment);
		bRebound = true;
	}
	return bRebound;
}

void UDaHotbarWidget::BindInventory(UDaInventoryComponent* Inventory)
{
	if (UDaInventoryComponent* Previous = BoundInventory.Get())
	{
		Previous->OnLoadoutChanged.RemoveDynamic(this, &UDaHotbarWidget::HandleLoadoutChanged);
		Previous->OnEntryAdded.RemoveDynamic(this, &UDaHotbarWidget::HandleEntryChanged);
		Previous->OnEntryChanged.RemoveDynamic(this, &UDaHotbarWidget::HandleEntryChanged);
		Previous->OnEntryRemoved.RemoveDynamic(this, &UDaHotbarWidget::HandleEntryChanged);
	}

	BoundInventory = Inventory;

	if (Inventory)
	{
		Inventory->OnLoadoutChanged.AddDynamic(this, &UDaHotbarWidget::HandleLoadoutChanged);
		// Add/Change/Remove all land in the same handler: a slot's icon, stack count and condition
		// all come off the entry, and the row is cheap enough to redraw whole.
		Inventory->OnEntryAdded.AddDynamic(this, &UDaHotbarWidget::HandleEntryChanged);
		Inventory->OnEntryChanged.AddDynamic(this, &UDaHotbarWidget::HandleEntryChanged);
		Inventory->OnEntryRemoved.AddDynamic(this, &UDaHotbarWidget::HandleEntryChanged);
	}
}

void UDaHotbarWidget::BindEquipment(UDaEquipmentManagerComponent* Equipment)
{
	if (UDaEquipmentManagerComponent* Previous = BoundEquipment.Get())
	{
		Previous->OnEquipped.RemoveDynamic(this, &UDaHotbarWidget::HandleEquipmentChanged);
		Previous->OnUnequipped.RemoveDynamic(this, &UDaHotbarWidget::HandleUnequipped);
		Previous->OnEquipmentChanged.RemoveDynamic(this, &UDaHotbarWidget::HandleEquipmentChanged);
	}

	BoundEquipment = Equipment;

	if (Equipment)
	{
		Equipment->OnEquipped.AddDynamic(this, &UDaHotbarWidget::HandleEquipmentChanged);
		Equipment->OnUnequipped.AddDynamic(this, &UDaHotbarWidget::HandleUnequipped);
		Equipment->OnEquipmentChanged.AddDynamic(this, &UDaHotbarWidget::HandleEquipmentChanged);
	}
}

void UDaHotbarWidget::HandleResolveTick()
{
	if (EnsureBindings())
	{
		RefreshHotbar();
	}
}

void UDaHotbarWidget::HandleLoadoutChanged()
{
	RefreshHotbar();
}

void UDaHotbarWidget::HandleEntryChanged(const FDaInventoryEntry& Entry, int32 SlotIndex)
{
	RefreshHotbar();
}

void UDaHotbarWidget::HandleEquipmentChanged(const FDaAppliedEquipmentEntry& Entry)
{
	RefreshHotbar();
}

void UDaHotbarWidget::HandleUnequipped(const FDaAppliedEquipmentEntry& Entry)
{
	UnequippingSlot = Entry.SlotTag;
	UnequippingItemID = Entry.ItemID;
	RefreshHotbar();
	UnequippingSlot = FGameplayTag();
	UnequippingItemID = FGuid();
}

// ---------------------------------------------------------------------------
// Refresh
// ---------------------------------------------------------------------------

void UDaHotbarWidget::RefreshHotbar()
{
	EnsureBindings();
	RebuildSlotStates();
	ApplySlotStatesToDefaultWidgets();
	OnHotbarRefreshed();
}

void UDaHotbarWidget::RebuildSlotStates()
{
	const int32 Count = FMath::Clamp(SlotCount, 1, 4);
	SlotStates.Reset(Count);

	UDaInventoryComponent* Inventory = BoundInventory.Get();
	UDaEquipmentManagerComponent* Equipment = BoundEquipment.Get();

	for (int32 Number = 1; Number <= Count; ++Number)
	{
		FDaHotbarSlotState& State = SlotStates.AddDefaulted_GetRef();
		State.SlotNumber = Number;
		State.SlotTag = UDaEquipmentManagerComponent::GetItemSlotTag(Number);

		if (!Inventory || !State.SlotTag.IsValid())
		{
			continue;
		}

		const FGuid ItemID = Inventory->GetLoadoutItemID(State.SlotTag);
		const FDaInventoryEntry* Entry = ItemID.IsValid() ? Inventory->FindEntryByItemID(ItemID) : nullptr;
		if (!Entry)
		{
			// An assignment naming an item the inventory no longer holds draws as empty. The
			// inventory clears those itself (ClearLoadoutForItem on removal); this is the belt to
			// that braces, and it is what makes "drop the sword, the slot goes empty" true even in
			// the frame before the cleared loadout replicates.
			continue;
		}

		// Everything the slot shows comes off the same view-model the inventory panel uses, so the
		// two views of one item cannot disagree.
		if (UDaInventoryItemBase* Item = UDaInventoryItemBase::CreateFromEntry(*Entry, this))
		{
			State.ItemID = ItemID;
			State.bAssigned = true;
			State.ItemName = FText::FromName(Item->Name);
			State.StackCount = Item->StackCount;
			State.bEquippable = Item->bIsEquippable;
			State.bUsesCondition = Item->bUsesCondition;
			State.Condition = Item->Condition;
			State.ConditionCap = Item->ConditionCap;
			State.ConditionBand = Item->ConditionBand;
			State.ConditionFraction = Item->GetConditionFraction();
			State.Icon = Item->Icon;
			State.bHasIcon = !Item->Icon.IsNull();
			const bool bLeavingThisSlot = State.SlotTag == UnequippingSlot && ItemID == UnequippingItemID;
			State.bEquipped = Equipment
				&& !bLeavingThisSlot
				&& Equipment->GetEquippedItemID(State.SlotTag) == ItemID;
		}
	}
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

bool UDaHotbarWidget::IsValidSlotNumber(int32 SlotNumber) const
{
	return SlotStates.IsValidIndex(SlotNumber - 1);
}

FDaHotbarSlotState UDaHotbarWidget::GetSlotState(int32 SlotNumber) const
{
	return IsValidSlotNumber(SlotNumber) ? SlotStates[SlotNumber - 1] : FDaHotbarSlotState();
}

bool UDaHotbarWidget::IsSlotAssigned(int32 SlotNumber) const
{
	return IsValidSlotNumber(SlotNumber) && SlotStates[SlotNumber - 1].bAssigned;
}

bool UDaHotbarWidget::IsSlotEquipped(int32 SlotNumber) const
{
	return IsValidSlotNumber(SlotNumber) && SlotStates[SlotNumber - 1].bEquipped;
}

FGuid UDaHotbarWidget::GetSlotItemID(int32 SlotNumber) const
{
	return IsValidSlotNumber(SlotNumber) ? SlotStates[SlotNumber - 1].ItemID : FGuid();
}

float UDaHotbarWidget::GetSlotConditionFraction(int32 SlotNumber) const
{
	return IsValidSlotNumber(SlotNumber) ? SlotStates[SlotNumber - 1].ConditionFraction : 0.f;
}

int32 UDaHotbarWidget::GetSlotStackCount(int32 SlotNumber) const
{
	return IsValidSlotNumber(SlotNumber) ? SlotStates[SlotNumber - 1].StackCount : 0;
}

bool UDaHotbarWidget::SlotHasIcon(int32 SlotNumber) const
{
	return IsValidSlotNumber(SlotNumber) && SlotStates[SlotNumber - 1].bHasIcon;
}

// ---------------------------------------------------------------------------
// Activation
// ---------------------------------------------------------------------------

bool UDaHotbarWidget::ActivateSlot(int32 SlotNumber)
{
	const FGameplayTag SlotTag = UDaEquipmentManagerComponent::GetItemSlotTag(SlotNumber);
	APlayerController* PC = GetOwningPlayer();
	APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn || !SlotTag.IsValid())
	{
		return false;
	}

	return UDaEquipmentManagerComponent::ActivateItemSlotForPawn(Pawn, SlotTag);
}

void UDaHotbarWidget::HandleSlot1Clicked() { ActivateSlot(1); }
void UDaHotbarWidget::HandleSlot2Clicked() { ActivateSlot(2); }
void UDaHotbarWidget::HandleSlot3Clicked() { ActivateSlot(3); }
void UDaHotbarWidget::HandleSlot4Clicked() { ActivateSlot(4); }

// ---------------------------------------------------------------------------
// Default widget tree
// ---------------------------------------------------------------------------

void UDaHotbarWidget::BuildDefaultWidgetTree()
{
	if (!WidgetTree)
	{
		return;
	}

	const int32 Count = FMath::Clamp(SlotCount, 1, 4);

	if (!SlotRow)
	{
		SlotRow = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("DaHotbarRow"));

		// Nothing authored at all, or the empty canvas root a Widget Blueprint arrives with: either
		// way the row goes bottom centre on a canvas, sized to its contents.
		UCanvasPanel* Canvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
		if (!WidgetTree->RootWidget)
		{
			Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DaHotbarCanvas"));
			WidgetTree->RootWidget = Canvas;
		}

		if (Canvas)
		{
			if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Canvas->AddChild(SlotRow)))
			{
				CanvasSlot->SetAnchors(FAnchors(0.5f, 1.f, 0.5f, 1.f));
				CanvasSlot->SetAlignment(FVector2D(0.5f, 1.f));
				CanvasSlot->SetAutoSize(true);
				CanvasSlot->SetPosition(RowOffsetFromBottom);
			}
		}
		else if (UPanelWidget* RootPanel = Cast<UPanelWidget>(WidgetTree->RootWidget))
		{
			RootPanel->AddChild(SlotRow);
		}
		else
		{
			// A themed root that is not a panel: leave it alone and let the theme drive the display
			// from OnHotbarRefreshed. The state queries still work.
			LOG_WARNING("UDaHotbarWidget: root widget %s is not a panel, so no default hotbar row was built — "
				"drive the visuals from OnHotbarRefreshed, or bind a SlotRow container",
				*GetNameSafe(WidgetTree->RootWidget));
			SlotRow = nullptr;
			return;
		}
	}
	else if (SlotRow->GetChildrenCount() > 0)
	{
		// The theme authored its own slots; do not add ours underneath them.
		return;
	}

	SlotWidgets.Reset(Count);
	for (int32 Number = 1; Number <= Count; ++Number)
	{
		FDaHotbarSlotWidgets& Widgets = SlotWidgets.AddDefaulted_GetRef();

		USizeBox* SizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
			*FString::Printf(TEXT("DaHotbarSize%d"), Number));
		SizeBox->SetWidthOverride(SlotSize);
		SizeBox->SetHeightOverride(SlotSize);

		Widgets.Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),
			*FString::Printf(TEXT("DaHotbarButton%d"), Number));
		SizeBox->AddChild(Widgets.Button);

		UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(),
			*FString::Printf(TEXT("DaHotbarOverlay%d"), Number));
		Widgets.Button->AddChild(Overlay);

		// Background: also the "empty slot" look, so an unassigned slot is visibly a slot.
		UBorder* Background = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
			*FString::Printf(TEXT("DaHotbarBackground%d"), Number));
		Background->SetBrush(DaHotbarPrivate::SolidBrush(EmptySlotColor, 4.f));
		Background->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UOverlaySlot* OverlaySlot = Overlay->AddChildToOverlay(Background))
		{
			OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
			OverlaySlot->SetVerticalAlignment(VAlign_Fill);
		}

		Widgets.IconImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(),
			*FString::Printf(TEXT("DaHotbarIcon%d"), Number));
		Widgets.IconImage->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* OverlaySlot = Overlay->AddChildToOverlay(Widgets.IconImage))
		{
			OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
			OverlaySlot->SetVerticalAlignment(VAlign_Fill);
			OverlaySlot->SetPadding(FMargin(4.f));
		}

		// Equipped tint over the icon rather than a ring: a ring needs a brush asset, and this
		// class ships no art.
		Widgets.EquippedBorder = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(),
			*FString::Printf(TEXT("DaHotbarEquipped%d"), Number));
		Widgets.EquippedBorder->SetBrush(DaHotbarPrivate::SolidBrush(EquippedTintColor, 4.f));
		Widgets.EquippedBorder->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* OverlaySlot = Overlay->AddChildToOverlay(Widgets.EquippedBorder))
		{
			OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
			OverlaySlot->SetVerticalAlignment(VAlign_Fill);
		}

		Widgets.SlotLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
			*FString::Printf(TEXT("DaHotbarSlotLabel%d"), Number));
		Widgets.SlotLabel->SetText(FText::AsNumber(Number));
		{
			FSlateFontInfo Font = Widgets.SlotLabel->GetFont();
			Font.Size = 14;
			Widgets.SlotLabel->SetFont(Font);
		}
		Widgets.SlotLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UOverlaySlot* OverlaySlot = Overlay->AddChildToOverlay(Widgets.SlotLabel))
		{
			OverlaySlot->SetHorizontalAlignment(HAlign_Left);
			OverlaySlot->SetVerticalAlignment(VAlign_Top);
			OverlaySlot->SetPadding(FMargin(4.f, 2.f, 0.f, 0.f));
		}

		Widgets.CountLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
			*FString::Printf(TEXT("DaHotbarCount%d"), Number));
		{
			FSlateFontInfo Font = Widgets.CountLabel->GetFont();
			Font.Size = 14;
			Widgets.CountLabel->SetFont(Font);
		}
		Widgets.CountLabel->SetVisibility(ESlateVisibility::Collapsed);
		if (UOverlaySlot* OverlaySlot = Overlay->AddChildToOverlay(Widgets.CountLabel))
		{
			OverlaySlot->SetHorizontalAlignment(HAlign_Right);
			OverlaySlot->SetVerticalAlignment(VAlign_Bottom);
			OverlaySlot->SetPadding(FMargin(0.f, 0.f, 4.f, 8.f));
		}

		USizeBox* BarBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
			*FString::Printf(TEXT("DaHotbarBarBox%d"), Number));
		BarBox->SetHeightOverride(6.f);
		BarBox->SetVisibility(ESlateVisibility::HitTestInvisible);
		Widgets.ConditionBar = WidgetTree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(),
			*FString::Printf(TEXT("DaHotbarCondition%d"), Number));
		Widgets.ConditionBar->SetVisibility(ESlateVisibility::Collapsed);
		BarBox->AddChild(Widgets.ConditionBar);
		if (UOverlaySlot* OverlaySlot = Overlay->AddChildToOverlay(BarBox))
		{
			OverlaySlot->SetHorizontalAlignment(HAlign_Fill);
			OverlaySlot->SetVerticalAlignment(VAlign_Bottom);
			OverlaySlot->SetPadding(FMargin(2.f, 0.f, 2.f, 2.f));
		}

		if (UHorizontalBoxSlot* RowSlot = SlotRow->AddChildToHorizontalBox(SizeBox))
		{
			RowSlot->SetPadding(FMargin(Number == 1 ? 0.f : SlotSpacing, 0.f, 0.f, 0.f));
		}
	}

	// Buttons are wired after the loop so the handler list reads as a table.
	if (SlotWidgets.IsValidIndex(0) && SlotWidgets[0].Button)
	{
		SlotWidgets[0].Button->OnClicked.AddDynamic(this, &UDaHotbarWidget::HandleSlot1Clicked);
	}
	if (SlotWidgets.IsValidIndex(1) && SlotWidgets[1].Button)
	{
		SlotWidgets[1].Button->OnClicked.AddDynamic(this, &UDaHotbarWidget::HandleSlot2Clicked);
	}
	if (SlotWidgets.IsValidIndex(2) && SlotWidgets[2].Button)
	{
		SlotWidgets[2].Button->OnClicked.AddDynamic(this, &UDaHotbarWidget::HandleSlot3Clicked);
	}
	if (SlotWidgets.IsValidIndex(3) && SlotWidgets[3].Button)
	{
		SlotWidgets[3].Button->OnClicked.AddDynamic(this, &UDaHotbarWidget::HandleSlot4Clicked);
	}

	bOwnsDefaultWidgets = true;
}

void UDaHotbarWidget::ApplySlotStatesToDefaultWidgets()
{
	if (!bOwnsDefaultWidgets)
	{
		return;
	}

	for (const FDaHotbarSlotState& State : SlotStates)
	{
		const int32 Index = State.SlotNumber - 1;
		if (!SlotWidgets.IsValidIndex(Index))
		{
			continue;
		}
		const FDaHotbarSlotWidgets& Widgets = SlotWidgets[Index];

		if (Widgets.IconImage)
		{
			// Synchronous load: an icon is a small texture, the row is four wide, and this only
			// runs when an assignment changes. Worth revisiting with the async plan that already
			// covers the equip path's LoadSynchronous.
			UTexture2D* IconTexture = (State.bAssigned && State.bHasIcon)
				? State.Icon.LoadSynchronous()
				: nullptr;

			if (IconTexture)
			{
				Widgets.IconImage->SetBrushFromTexture(IconTexture, false);
				Widgets.IconImage->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				Widgets.IconImage->SetVisibility(ESlateVisibility::Collapsed);
			}
		}

		if (Widgets.EquippedBorder)
		{
			Widgets.EquippedBorder->SetVisibility(State.bEquipped
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
		}

		if (Widgets.CountLabel)
		{
			// A count only earns screen space when there is more than one.
			if (State.bAssigned && State.StackCount > 1)
			{
				Widgets.CountLabel->SetText(FText::Format(LOCTEXT("StackCountFormat", "x{0}"),
					FText::AsNumber(State.StackCount)));
				Widgets.CountLabel->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				Widgets.CountLabel->SetVisibility(ESlateVisibility::Collapsed);
			}
		}

		if (Widgets.ConditionBar)
		{
			if (State.bAssigned && State.bUsesCondition)
			{
				Widgets.ConditionBar->SetPercent(State.ConditionFraction);
				Widgets.ConditionBar->SetFillColorAndOpacity(DaHotbarPrivate::BandColor(State.ConditionBand));
				Widgets.ConditionBar->SetVisibility(ESlateVisibility::HitTestInvisible);
			}
			else
			{
				Widgets.ConditionBar->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
