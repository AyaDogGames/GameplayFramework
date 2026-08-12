// Copyright Dream Awake Solutions LLC


#include "UI/DaInventoryPanelWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameplayFramework.h"
#include "Inventory/DaInventoryComponent.h"
#include "Inventory/DaInventoryItemBase.h"
#include "Inventory/DaInventoryWidgetController.h"

#define LOCTEXT_NAMESPACE "DaInventoryPanel"

TArray<TWeakObjectPtr<UDaInventoryPanelWidget>> UDaInventoryPanelWidget::ActivePanels;

// ---------------------------------------------------------------------------
// UDaInventoryRowWidget
// ---------------------------------------------------------------------------

void UDaInventoryRowWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	if (!WidgetTree || WidgetTree->RootWidget)
	{
		return;
	}

	UHorizontalBox* Row = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(), TEXT("Row"));
	WidgetTree->RootWidget = Row;

	SelectButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("SelectButton"));
	NameText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("NameText"));
	{
		FSlateFontInfo Font = NameText->GetFont();
		Font.Size = 14;
		NameText->SetFont(Font);
	}
	SelectButton->AddChild(NameText);
	if (UHorizontalBoxSlot* RowSlot = Row->AddChildToHorizontalBox(SelectButton))
	{
		// The name takes whatever width is left; the buttons are fixed.
		RowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
		RowSlot->SetVerticalAlignment(VAlign_Center);
	}

	ConditionText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ConditionText"));
	{
		FSlateFontInfo Font = ConditionText->GetFont();
		Font.Size = 12;
		ConditionText->SetFont(Font);
	}
	ConditionText->SetVisibility(ESlateVisibility::Collapsed);
	if (UHorizontalBoxSlot* RowSlot = Row->AddChildToHorizontalBox(ConditionText))
	{
		RowSlot->SetPadding(FMargin(8.f, 0.f));
		RowSlot->SetVerticalAlignment(VAlign_Center);
	}

	// [1][2][3][4]. Built for the maximum and hidden down to HotbarSlotCount by SetItem, so the
	// per-button handler wiring stays a straight-line table.
	AssignButtons.Reset(4);
	for (int32 Number = 1; Number <= 4; ++Number)
	{
		USizeBox* Box = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),
			*FString::Printf(TEXT("AssignBox%d"), Number));
		Box->SetWidthOverride(28.f);
		Box->SetHeightOverride(24.f);

		UButton* Button = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),
			*FString::Printf(TEXT("AssignButton%d"), Number));
		UTextBlock* Label = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
			*FString::Printf(TEXT("AssignLabel%d"), Number));
		Label->SetText(FText::AsNumber(Number));
		{
			FSlateFontInfo Font = Label->GetFont();
			Font.Size = 12;
			Label->SetFont(Font);
		}
		Button->AddChild(Label);
		Box->AddChild(Button);
		AssignButtons.Add(Button);

		if (UHorizontalBoxSlot* RowSlot = Row->AddChildToHorizontalBox(Box))
		{
			RowSlot->SetPadding(FMargin(2.f, 0.f));
			RowSlot->SetVerticalAlignment(VAlign_Center);
		}
	}

	SelectButton->OnClicked.AddDynamic(this, &UDaInventoryRowWidget::HandleSelectClicked);
	AssignButtons[0]->OnClicked.AddDynamic(this, &UDaInventoryRowWidget::HandleAssign1Clicked);
	AssignButtons[1]->OnClicked.AddDynamic(this, &UDaInventoryRowWidget::HandleAssign2Clicked);
	AssignButtons[2]->OnClicked.AddDynamic(this, &UDaInventoryRowWidget::HandleAssign3Clicked);
	AssignButtons[3]->OnClicked.AddDynamic(this, &UDaInventoryRowWidget::HandleAssign4Clicked);
}

void UDaInventoryRowWidget::SetItem(UDaInventoryItemBase* Item, int32 InHotbarSlotCount)
{
	RowItemID = Item ? Item->ItemID : FGuid();

	if (NameText)
	{
		if (Item)
		{
			// Name and count in one string: the panel's job is to be readable, not to be a grid.
			const FText NameOnly = FText::FromName(Item->Name);
			NameText->SetText(Item->StackCount > 1
				? FText::Format(LOCTEXT("RowNameWithCount", "{0}  x{1}"), NameOnly, FText::AsNumber(Item->StackCount))
				: NameOnly);
		}
		else
		{
			NameText->SetText(FText::GetEmpty());
		}
	}

	if (ConditionText)
	{
		if (Item && Item->bUsesCondition)
		{
			ConditionText->SetText(FText::Format(LOCTEXT("RowCondition", "{0}/{1}"),
				FText::AsNumber(Item->Condition), FText::AsNumber(Item->ConditionCap)));
			ConditionText->SetVisibility(ESlateVisibility::HitTestInvisible);
		}
		else
		{
			ConditionText->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	const int32 Shown = FMath::Clamp(InHotbarSlotCount, 1, AssignButtons.Num());
	for (int32 Index = 0; Index < AssignButtons.Num(); ++Index)
	{
		if (UButton* Button = AssignButtons[Index])
		{
			Button->SetVisibility(Index < Shown ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	}
}

void UDaInventoryRowWidget::SetSelected(bool bInSelected)
{
	if (NameText)
	{
		NameText->SetColorAndOpacity(bInSelected
			? FSlateColor(FLinearColor(0.3f, 0.95f, 1.f))
			: FSlateColor(FLinearColor::White));
	}
}

FText UDaInventoryRowWidget::GetRowLabel() const
{
	if (!NameText)
	{
		return FText::GetEmpty();
	}
	if (ConditionText && ConditionText->GetVisibility() != ESlateVisibility::Collapsed)
	{
		return FText::Format(LOCTEXT("RowLabelWithCondition", "{0}  {1}"),
			NameText->GetText(), ConditionText->GetText());
	}
	return NameText->GetText();
}

void UDaInventoryRowWidget::HandleSelectClicked()
{
	OnRowSelected.Broadcast(RowItemID);
}

void UDaInventoryRowWidget::HandleAssign1Clicked() { RequestAssign(1); }
void UDaInventoryRowWidget::HandleAssign2Clicked() { RequestAssign(2); }
void UDaInventoryRowWidget::HandleAssign3Clicked() { RequestAssign(3); }
void UDaInventoryRowWidget::HandleAssign4Clicked() { RequestAssign(4); }

void UDaInventoryRowWidget::RequestAssign(int32 SlotNumber)
{
	OnAssignRequested.Broadcast(RowItemID, SlotNumber);
}

// ---------------------------------------------------------------------------
// UDaInventoryPanelWidget
// ---------------------------------------------------------------------------

UDaInventoryPanelWidget::UDaInventoryPanelWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Game input stays live on purpose: the key that opened this panel has to be able to close it,
	// and it is a gameplay input action bound through the player controller. NoCapture so the
	// cursor can reach the assign buttons.
	InputConfig = EDaWidgetInputMode::GameAndMenu;
	GameMouseCaptureMode = EMouseCaptureMode::NoCapture;
}

void UDaInventoryPanelWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();

	BuildDefaultWidgetTree();
}

void UDaInventoryPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	ActivePanels.Add(this);

	EnsureController();
	RefreshPanel();
}

void UDaInventoryPanelWidget::NativeDestruct()
{
	ActivePanels.Remove(this);

	if (InventoryController)
	{
		InventoryController->OnInventoryChanged.RemoveDynamic(this, &UDaInventoryPanelWidget::HandleInventoryChanged);
	}
	if (UDaInventoryComponent* Inventory = BoundInventory.Get())
	{
		Inventory->OnLoadoutChanged.RemoveDynamic(this, &UDaInventoryPanelWidget::HandleLoadoutChanged);
	}
	BoundInventory = nullptr;

	Super::NativeDestruct();
}

UDaInventoryPanelWidget* UDaInventoryPanelWidget::GetActivePanel(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;

	for (int32 Index = ActivePanels.Num() - 1; Index >= 0; --Index)
	{
		UDaInventoryPanelWidget* Panel = ActivePanels[Index].Get();
		if (!Panel)
		{
			ActivePanels.RemoveAt(Index);
			continue;
		}
		// No world context means "any panel", which is what a single-world caller wants.
		if (!World || Panel->GetWorld() == World)
		{
			return Panel;
		}
	}
	return nullptr;
}

void UDaInventoryPanelWidget::EnsureController()
{
	// Honour a controller somebody already handed us (the ADaHUD/UDaUserWidgetBase pattern).
	if (!InventoryController)
	{
		InventoryController = Cast<UDaInventoryWidgetController>(WidgetController);
	}
	if (!InventoryController)
	{
		InventoryController = NewObject<UDaInventoryWidgetController>(this);
		WidgetController = InventoryController;
	}

	APlayerController* PC = GetOwningPlayer();
	if (PC && PC->PlayerState && !InventoryController->GetInventoryComponent())
	{
		InventoryController->InitializeInventory(PC->PlayerState.Get());
	}

	if (!InventoryController->OnInventoryChanged.IsAlreadyBound(this, &UDaInventoryPanelWidget::HandleInventoryChanged))
	{
		InventoryController->OnInventoryChanged.AddDynamic(this, &UDaInventoryPanelWidget::HandleInventoryChanged);
	}

	// The loadout is not part of the entry stream, so the controller does not re-broadcast it.
	UDaInventoryComponent* Inventory = InventoryController->GetInventoryComponent();
	if (BoundInventory.Get() != Inventory)
	{
		if (UDaInventoryComponent* Previous = BoundInventory.Get())
		{
			Previous->OnLoadoutChanged.RemoveDynamic(this, &UDaInventoryPanelWidget::HandleLoadoutChanged);
		}
		BoundInventory = Inventory;
		if (Inventory)
		{
			Inventory->OnLoadoutChanged.AddDynamic(this, &UDaInventoryPanelWidget::HandleLoadoutChanged);
		}
	}
}

void UDaInventoryPanelWidget::HandleInventoryChanged(const TArray<UDaInventoryItemBase*>& Items)
{
	RefreshPanel();
}

void UDaInventoryPanelWidget::HandleLoadoutChanged()
{
	RefreshPanel();
}

void UDaInventoryPanelWidget::RefreshPanel()
{
	EnsureController();

	TArray<UDaInventoryItemBase*> Items;
	if (InventoryController)
	{
		Items = InventoryController->GetItems();
	}

	if (RowContainer)
	{
		// One row per item, rebuilt from scratch: an inventory is 20 slots at most, and reusing
		// rows would mean reconciling selection against a list that may have reordered.
		RowContainer->ClearChildren();
		Rows.Reset(Items.Num());

		for (UDaInventoryItemBase* Item : Items)
		{
			if (!Item)
			{
				continue;
			}
			UDaInventoryRowWidget* Row = CreateWidget<UDaInventoryRowWidget>(this, UDaInventoryRowWidget::StaticClass());
			if (!Row)
			{
				continue;
			}
			Row->SetItem(Item, HotbarSlotCount);
			Row->SetSelected(Item->ItemID == SelectedItemID);
			Row->OnAssignRequested.AddDynamic(this, &UDaInventoryPanelWidget::HandleRowAssignRequested);
			Row->OnRowSelected.AddDynamic(this, &UDaInventoryPanelWidget::HandleRowSelected);
			RowContainer->AddChild(Row);
			Rows.Add(Row);
		}
	}

	if (EmptyText)
	{
		EmptyText->SetVisibility(Rows.Num() == 0 ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	OnPanelRefreshed();
}

FText UDaInventoryPanelWidget::GetRowLabel(int32 RowIndex) const
{
	return Rows.IsValidIndex(RowIndex) && Rows[RowIndex] ? Rows[RowIndex]->GetRowLabel() : FText::GetEmpty();
}

bool UDaInventoryPanelWidget::AssignToSlot(FGuid ItemID, int32 SlotNumber)
{
	if (!InventoryController)
	{
		LOG_WARNING("UDaInventoryPanelWidget::AssignToSlot: no inventory controller");
		return false;
	}
	return InventoryController->AssignToHotbarSlot(ItemID, SlotNumber);
}

bool UDaInventoryPanelWidget::AssignSelectedToSlot(int32 SlotNumber)
{
	if (!SelectedItemID.IsValid())
	{
		return false;
	}
	return AssignToSlot(SelectedItemID, SlotNumber);
}

void UDaInventoryPanelWidget::HandleRowAssignRequested(FGuid ItemID, int32 SlotNumber)
{
	AssignToSlot(ItemID, SlotNumber);
}

void UDaInventoryPanelWidget::HandleRowSelected(FGuid ItemID)
{
	SelectedItemID = ItemID;
	for (UDaInventoryRowWidget* Row : Rows)
	{
		if (Row)
		{
			Row->SetSelected(Row->GetRowItemID() == SelectedItemID);
		}
	}
}

void UDaInventoryPanelWidget::BuildDefaultWidgetTree()
{
	if (!WidgetTree || RowContainer)
	{
		return;
	}

	UCanvasPanel* Canvas = Cast<UCanvasPanel>(WidgetTree->RootWidget);
	if (!WidgetTree->RootWidget)
	{
		Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("PanelCanvas"));
		WidgetTree->RootWidget = Canvas;
	}
	if (!Canvas)
	{
		// A themed root that is not a canvas: the theme owns the layout, and RowContainer was not
		// bound, so there is nowhere to put rows. Say so rather than silently showing nothing.
		LOG_WARNING("UDaInventoryPanelWidget: root widget %s is not a CanvasPanel and no RowContainer is bound — "
			"bind a RowContainer in the Blueprint", *GetNameSafe(WidgetTree->RootWidget));
		return;
	}

	UBorder* Frame = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PanelFrame"));
	// RoundedBox, not SetBrushColor on the default brush: that brush has no texture resource, so
	// tinting it draws nothing (see the note in DaHotbarWidget.cpp).
	FSlateBrush FrameBrush;
	FrameBrush.DrawAs = ESlateBrushDrawType::RoundedBox;
	FrameBrush.TintColor = FSlateColor(FLinearColor(0.015f, 0.02f, 0.03f, 0.94f));
	FrameBrush.OutlineSettings.RoundingType = ESlateBrushRoundingType::FixedRadius;
	FrameBrush.OutlineSettings.CornerRadii = FVector4(8.f, 8.f, 8.f, 8.f);
	Frame->SetBrush(FrameBrush);
	Frame->SetPadding(FMargin(16.f));
	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Canvas->AddChild(Frame)))
	{
		CanvasSlot->SetAnchors(FAnchors(0.5f, 0.5f, 0.5f, 0.5f));
		CanvasSlot->SetAlignment(FVector2D(0.5f, 0.5f));
		CanvasSlot->SetOffsets(FMargin(0.f, 0.f, 520.f, 420.f));
	}

	UVerticalBox* Column = WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("PanelColumn"));
	Frame->AddChild(Column);

	UTextBlock* Title = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PanelTitle"));
	Title->SetText(LOCTEXT("PanelTitle", "Inventory"));
	Title->SetVisibility(ESlateVisibility::HitTestInvisible);
	Column->AddChildToVerticalBox(Title);

	UTextBlock* Hint = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PanelHint"));
	Hint->SetText(LOCTEXT("PanelHint", "Click 1-4 on an item to put it on that hotbar slot."));
	{
		FSlateFontInfo Font = Hint->GetFont();
		Font.Size = 12;
		Hint->SetFont(Font);
	}
	Hint->SetColorAndOpacity(FSlateColor(FLinearColor(0.7f, 0.7f, 0.75f)));
	Hint->SetVisibility(ESlateVisibility::HitTestInvisible);
	if (UVerticalBoxSlot* ColumnSlot = Column->AddChildToVerticalBox(Hint))
	{
		ColumnSlot->SetPadding(FMargin(0.f, 2.f, 0.f, 10.f));
	}

	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("PanelRows"));
	RowContainer = Scroll;
	if (UVerticalBoxSlot* ColumnSlot = Column->AddChildToVerticalBox(Scroll))
	{
		ColumnSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	}

	EmptyText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("PanelEmpty"));
	EmptyText->SetText(LOCTEXT("PanelEmpty", "(empty)"));
	EmptyText->SetVisibility(ESlateVisibility::Collapsed);
	Column->AddChildToVerticalBox(EmptyText);
}

#undef LOCTEXT_NAMESPACE
