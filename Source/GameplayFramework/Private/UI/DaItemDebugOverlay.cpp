// Copyright Dream Awake Solutions LLC

#include "UI/DaItemDebugOverlay.h"

#include "GameplayFramework.h"
#include "HAL/IConsoleManager.h"

#define LOCTEXT_NAMESPACE "DaItemDebugOverlay"

#if !UE_BUILD_SHIPPING

#include "AbilitySystem/DaAbilitySystemComponent.h"
#include "CoreGameplayTags.h"
#include "DaPlayerState.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Equipment/DaEquipmentManagerComponent.h"
#include "GameFramework/PlayerController.h"
#include "Inventory/DaInventoryComponent.h"
#include "Inventory/DaInventoryEntry.h"
#include "Inventory/DaItemDefinition.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

namespace DaItemDebugOverlayPrivate
{
	/** How often the overlay re-reads the local player's state. Polling, not delegates: a debug
	 *  tool that misses a redraw is a nuisance, one that holds a stale binding to a respawned
	 *  pawn is a crash. */
	constexpr float RefreshPeriod = 0.25f;

	/** Everything a row's buttons act on, resolved fresh at click time rather than captured:
	 *  the pawn (and so the equipment manager) is replaced on every respawn. */
	struct FTargets
	{
		APlayerController* PC = nullptr;
		ADaPlayerState* PS = nullptr;
		UDaInventoryComponent* Inventory = nullptr;
		UDaEquipmentManagerComponent* Equipment = nullptr;
		UDaAbilitySystemComponent* ASC = nullptr;

		bool HasInventory() const { return PS != nullptr && Inventory != nullptr; }
	};

	FTargets Resolve(APlayerController* PC)
	{
		FTargets Out;
		if (!PC)
		{
			return Out;
		}
		Out.PC = PC;
		Out.PS = Cast<ADaPlayerState>(PC->PlayerState);
		if (Out.PS)
		{
			Out.Inventory = UDaInventoryComponent::GetInventoryFromActor(Out.PS);
			Out.ASC = Out.PS->FindComponentByClass<UDaAbilitySystemComponent>();
		}
		if (APawn* Pawn = PC->GetPawn())
		{
			Out.Equipment = UDaEquipmentManagerComponent::GetEquipmentFromActor(Pawn);
		}
		return Out;
	}

	FText BandText(EDaConditionBand Band)
	{
		switch (Band)
		{
		case EDaConditionBand::Broken:   return LOCTEXT("BandBroken", "BROKEN");
		case EDaConditionBand::Critical: return LOCTEXT("BandCritical", "Critical");
		case EDaConditionBand::Worn:     return LOCTEXT("BandWorn", "Worn");
		default:                         return LOCTEXT("BandNormal", "Normal");
		}
	}

	/** The leaf of a slot tag ("Equip.Slot.WeaponMain" -> "WeaponMain"). The full tag does not earn
	 *  its width here: every slot in a row shares the same two-segment prefix, and the panel has to
	 *  fit inside a game viewport nobody has resized for it. */
	FString SlotLeaf(const FGameplayTag& SlotTag)
	{
		FString Full = SlotTag.ToString();
		int32 Dot = INDEX_NONE;
		return Full.FindLastChar(TEXT('.'), Dot) ? Full.RightChop(Dot + 1) : Full;
	}

	FLinearColor BandColor(EDaConditionBand Band)
	{
		switch (Band)
		{
		case EDaConditionBand::Broken:   return FLinearColor(1.f, 0.25f, 0.25f);
		case EDaConditionBand::Critical: return FLinearColor(1.f, 0.5f, 0.2f);
		case EDaConditionBand::Worn:     return FLinearColor(1.f, 0.9f, 0.3f);
		default:                         return FLinearColor(0.7f, 1.f, 0.7f);
		}
	}

	/** One item's state, gathered before any widget is touched so the refresh can compare it
	 *  against what is already on screen and skip a rebuild that would change nothing (and would
	 *  otherwise yank a button out from under the cursor four times a second). */
	struct FRow
	{
		FGuid ItemID;
		int32 SlotIndex = INDEX_NONE;
		FString DefName;
		int32 StackCount = 1;
		bool bUsesCondition = false;
		int32 Grade = 0;
		int32 Condition = 0;
		int32 Cap = 0;
		EDaConditionBand Band = EDaConditionBand::Normal;
		bool bEquippable = false;
		bool bEquipped = false;
		FGameplayTag EquippedSlot;
		/** Which slot [Equip] would target: the definition's first allowed slot. */
		FGameplayTag EquipTargetSlot;

		FString Signature() const
		{
			return FString::Printf(TEXT("%s|%d|%s|%d|%d|%d|%d|%d|%d|%d|%s|%s;"),
				*ItemID.ToString(), SlotIndex, *DefName, StackCount, bUsesCondition ? 1 : 0,
				Grade, Condition, Cap, static_cast<int32>(Band), bEquipped ? 1 : 0,
				*EquippedSlot.ToString(), *EquipTargetSlot.ToString());
		}
	};

	UDaItemDefinition* ResolveDefinition(FPrimaryAssetId ItemDefinitionID)
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

	/** Rows drawn by whichever overlay refreshed most recently, for
	 *  UDaItemDebugLibrary::GetItemDebugOverlayRowCount. */
	int32 LastRefreshRowCount = 0;
}

/**
 * SDaItemDebugOverlay
 *
 * The overlay proper. Deliberately .cpp-local: nothing outside this file constructs it (the
 * console command and UDaItemDebugLibrary are the whole entry surface), and keeping the Slate
 * includes private matches this module's build settings, where Slate/SlateCore are private
 * dependencies.
 */
class SDaItemDebugOverlay : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SDaItemDebugOverlay) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, APlayerController* InOwner);

	/** Give the owning player its cursor settings back. Called before the overlay is removed. */
	void RestoreInput();

	int32 GetRowCount() const { return RowCount; }

private:

	EActiveTimerReturnType Poll(double InCurrentTime, float InDeltaTime);

	/** Read the local player's inventory into Rows; returns the header line as it should read. */
	FText Gather(TArray<DaItemDebugOverlayPrivate::FRow>& OutRows) const;

	void Rebuild(const TArray<DaItemDebugOverlayPrivate::FRow>& Rows);

	TSharedRef<SWidget> MakeRowWidget(const DaItemDebugOverlayPrivate::FRow& Row);

	/** Every button ends here: run Op against freshly resolved components, then force the next
	 *  poll to redraw (the op may not have landed yet — a client's write is a round trip away). */
	FReply RunOp(const TFunction<void(const DaItemDebugOverlayPrivate::FTargets&)>& Op);

	TWeakObjectPtr<APlayerController> OwnerPC;
	TSharedPtr<STextBlock> HeaderText;
	TSharedPtr<SVerticalBox> RowBox;
	TSharedPtr<STextBlock> EmptyText;

	FString LastSignature;
	int32 RowCount = 0;

	/** The owner's cursor state from before the overlay took it, so closing puts it back. */
	bool bRestoreCursorHidden = false;
};

void SDaItemDebugOverlay::Construct(const FArguments& InArgs, APlayerController* InOwner)
{
	OwnerPC = InOwner;

	// The buttons need a cursor. Remember what the game had so closing the overlay hands
	// control straight back rather than leaving the player staring at a mouse pointer.
	if (InOwner)
	{
		bRestoreCursorHidden = !InOwner->bShowMouseCursor;
		InOwner->bShowMouseCursor = true;
		InOwner->SetInputMode(FInputModeGameAndUI().SetHideCursorDuringCapture(false));
	}

	const FSlateBrush* PanelBrush = FCoreStyle::Get().GetBrush("WhiteBrush");

	ChildSlot
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.Padding(12.f)
		[
			SNew(SBorder)
			.BorderImage(PanelBrush)
			.BorderBackgroundColor(FLinearColor(0.f, 0.f, 0.f, 0.75f))
			.Padding(8.f)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.FillWidth(1.f)
					.VAlign(VAlign_Center)
					[
						SAssignNew(HeaderText, STextBlock)
						.ColorAndOpacity(FLinearColor::White)
						.Text(LOCTEXT("Resolving", "Da.Debug.Items: resolving local player..."))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(8.f, 0.f, 0.f, 0.f)
					[
						SNew(SButton)
						.Text(LOCTEXT("AddCredits", "+100 credits"))
						.ToolTipText(LOCTEXT("AddCreditsTip",
							"ADaPlayerState::AdjustCredits is authority-only and silently ignores a client, "
							"so this is disabled on a client window."))
						.IsEnabled_Lambda([this]()
						{
							const DaItemDebugOverlayPrivate::FTargets T =
								DaItemDebugOverlayPrivate::Resolve(OwnerPC.Get());
							return T.PS != nullptr && T.PS->HasAuthority();
						})
						.OnClicked_Lambda([this]()
						{
							return RunOp([](const DaItemDebugOverlayPrivate::FTargets& T)
							{
								T.PS->AdjustCredits(100);
							});
						})
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4.f, 0.f, 0.f, 0.f)
					[
						SNew(SButton)
						.Text(LOCTEXT("Close", "Close"))
						.OnClicked_Lambda([this]()
						{
							UWorld* World = OwnerPC.IsValid() ? OwnerPC->GetWorld() : nullptr;
							UDaItemDebugLibrary::ToggleItemDebugOverlay(World);
							return FReply::Handled();
						})
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 6.f, 0.f, 0.f)
				.MaxHeight(600.f)
				[
					SNew(SScrollBox)
					+ SScrollBox::Slot()
					[
						SAssignNew(RowBox, SVerticalBox)
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(EmptyText, STextBlock)
					.ColorAndOpacity(FLinearColor(0.8f, 0.8f, 0.8f))
					.Visibility(EVisibility::Collapsed)
					.Text(LOCTEXT("Empty", "(inventory is empty)"))
				]
			]
		]
	];

	RegisterActiveTimer(DaItemDebugOverlayPrivate::RefreshPeriod,
		FWidgetActiveTimerDelegate::CreateSP(this, &SDaItemDebugOverlay::Poll));

	// Draw once immediately: the first poll is a quarter of a second away.
	TArray<DaItemDebugOverlayPrivate::FRow> Rows;
	HeaderText->SetText(Gather(Rows));
	Rebuild(Rows);
}

void SDaItemDebugOverlay::RestoreInput()
{
	if (APlayerController* PC = OwnerPC.Get())
	{
		if (bRestoreCursorHidden)
		{
			PC->bShowMouseCursor = false;
			PC->SetInputMode(FInputModeGameOnly());
		}
	}
}

EActiveTimerReturnType SDaItemDebugOverlay::Poll(double InCurrentTime, float InDeltaTime)
{
	TArray<DaItemDebugOverlayPrivate::FRow> Rows;
	const FText Header = Gather(Rows);
	HeaderText->SetText(Header);

	FString Signature = Header.ToString();
	for (const DaItemDebugOverlayPrivate::FRow& Row : Rows)
	{
		Signature += Row.Signature();
	}
	if (Signature != LastSignature)
	{
		LastSignature = MoveTemp(Signature);
		Rebuild(Rows);
	}

	DaItemDebugOverlayPrivate::LastRefreshRowCount = RowCount;
	return EActiveTimerReturnType::Continue;
}

FText SDaItemDebugOverlay::Gather(TArray<DaItemDebugOverlayPrivate::FRow>& OutRows) const
{
	using namespace DaItemDebugOverlayPrivate;

	const FTargets T = Resolve(OwnerPC.Get());
	if (!T.HasInventory())
	{
		return LOCTEXT("NoInventory", "Da.Debug.Items: no ADaPlayerState inventory on the local player");
	}

	for (const FDaInventoryEntry& Entry : T.Inventory->GetAllEntries())
	{
		FRow Row;
		Row.ItemID = Entry.ItemID;
		Row.SlotIndex = Entry.SlotIndex;
		Row.StackCount = Entry.StackCount;
		Row.DefName = Entry.ItemDefinitionID.PrimaryAssetName.ToString();

		if (const UDaItemDefinition* Def = ResolveDefinition(Entry.ItemDefinitionID))
		{
			Row.bEquippable = !Def->EquipSlotTags.IsEmpty();
			if (Row.bEquippable)
			{
				Row.EquipTargetSlot = Def->EquipSlotTags.First();
			}
			Row.bUsesCondition = Def->ConditionConfig.bUsesCondition;
			if (Row.bUsesCondition)
			{
				Row.Grade = T.Inventory->GetItemStat(Entry.ItemID, CoreGameplayTags::TAG_Item_Stat_Grade);
				Row.Condition = T.Inventory->GetItemStat(Entry.ItemID, CoreGameplayTags::TAG_Item_Stat_Condition);
				Row.Cap = Def->ConditionConfig.GetConditionCap(Row.Grade);
				// The authority's own banding function, so the overlay cannot disagree with the
				// penalties actually applied.
				Row.Band = UDaEquipmentManagerComponent::ComputeConditionBand(
					Def->ConditionConfig, Row.Condition, Row.Grade);
			}

			if (T.Equipment && Row.bEquippable)
			{
				for (const FGameplayTag& Candidate : Def->EquipSlotTags)
				{
					if (T.Equipment->GetEquippedItemID(Candidate) == Entry.ItemID)
					{
						Row.bEquipped = true;
						Row.EquippedSlot = Candidate;
						break;
					}
				}
			}
		}

		OutRows.Add(MoveTemp(Row));
	}

	// The ASC's band tags are per-WEARER, not per-item, so they belong in the header next to
	// credits rather than on a row: they are what the penalty effects actually granted.
	FString Bands;
	if (T.ASC)
	{
		if (T.ASC->HasMatchingGameplayTag(CoreGameplayTags::TAG_Item_Condition_Critical))
		{
			Bands = TEXT(" [ASC: Item.Condition.Critical]");
		}
		else if (T.ASC->HasMatchingGameplayTag(CoreGameplayTags::TAG_Item_Condition_Worn))
		{
			Bands = TEXT(" [ASC: Item.Condition.Worn]");
		}
	}

	return FText::FromString(FString::Printf(TEXT("Items  |  %s  |  Credits %d  |  %d/%d slots%s"),
		T.PS->HasAuthority() ? TEXT("SERVER") : TEXT("CLIENT"),
		T.PS->GetCredits(),
		T.Inventory->GetFilledSlotCount(),
		T.Inventory->GetMaxSlots(),
		*Bands));
}

void SDaItemDebugOverlay::Rebuild(const TArray<DaItemDebugOverlayPrivate::FRow>& Rows)
{
	RowBox->ClearChildren();
	for (const DaItemDebugOverlayPrivate::FRow& Row : Rows)
	{
		RowBox->AddSlot()
		.AutoHeight()
		.Padding(0.f, 1.f)
		[
			MakeRowWidget(Row)
		];
	}
	RowCount = Rows.Num();
	EmptyText->SetVisibility(RowCount == 0 ? EVisibility::Visible : EVisibility::Collapsed);
	DaItemDebugOverlayPrivate::LastRefreshRowCount = RowCount;
}

TSharedRef<SWidget> SDaItemDebugOverlay::MakeRowWidget(const DaItemDebugOverlayPrivate::FRow& Row)
{
	using namespace DaItemDebugOverlayPrivate;

	const FGuid ItemID = Row.ItemID;
	const int32 SlotIndex = Row.SlotIndex;
	const FGameplayTag EquipTarget = Row.EquipTargetSlot;
	const FGameplayTag EquippedSlot = Row.EquippedSlot;
	const bool bEquipped = Row.bEquipped;

	const FString Condition = Row.bUsesCondition
		? FString::Printf(TEXT("%d/%d"), Row.Condition, Row.Cap)
		: FString(TEXT("---"));
	const FString Grade = Row.bUsesCondition ? FString::Printf(TEXT("g%d"), Row.Grade) : FString(TEXT("--"));
	const FString Equipped = bEquipped
		? FString::Printf(TEXT("[E] %s"), *SlotLeaf(EquippedSlot))
		: FString(TEXT(""));

	TSharedRef<SHorizontalBox> Line = SNew(SHorizontalBox);

	auto AddText = [&Line](const FString& Value, float Width, const FLinearColor& Color)
	{
		Line->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.Padding(0.f, 0.f, 6.f, 0.f)
		[
			SNew(SBox)
			.WidthOverride(Width)
			[
				SNew(STextBlock)
				.ColorAndOpacity(Color)
				.Text(FText::FromString(Value))
			]
		];
	};

	// Fixed column widths so the rows line up into a table. They are deliberately tight: the whole
	// panel has to fit inside a game viewport at its normal size, and an overlay whose last button
	// is off the right edge is not a usable debug tool.
	AddText(FString::Printf(TEXT("%d"), SlotIndex), 20.f, FLinearColor(0.7f, 0.7f, 0.7f));
	AddText(Row.DefName, 170.f, FLinearColor::White);
	AddText(Grade, 28.f, FLinearColor(0.8f, 0.85f, 1.f));
	AddText(Condition, 56.f, FLinearColor::White);
	AddText(Row.bUsesCondition ? BandText(Row.Band).ToString() : FString(TEXT("---")), 60.f,
		Row.bUsesCondition ? BandColor(Row.Band) : FLinearColor(0.6f, 0.6f, 0.6f));
	AddText(FString::Printf(TEXT("x%d"), Row.StackCount), 28.f, FLinearColor(0.85f, 0.85f, 0.85f));
	AddText(Equipped, 110.f, FLinearColor(0.6f, 1.f, 0.8f));

	auto AddButton = [&Line, this](const FText& Label, const FText& Tip, bool bEnabled,
		TFunction<void(const FTargets&)> Op)
	{
		Line->AddSlot()
		.AutoWidth()
		.Padding(2.f, 0.f)
		[
			SNew(SButton)
			.Text(Label)
			.ToolTipText(Tip)
			.IsEnabled(bEnabled)
			.OnClicked_Lambda([this, Op]() { return RunOp(Op); })
		];
	};

	// Condition cheats only make sense for a condition user; a non-condition item's Condition
	// stat would be an invented one the rest of the system ignores.
	AddButton(LOCTEXT("Wear10", "Wear-10"),
		LOCTEXT("Wear10Tip", "AddItemStat(Item.Stat.Condition, -10)"),
		Row.bUsesCondition,
		[ItemID](const FTargets& T)
		{
			T.Inventory->AddItemStat(ItemID, CoreGameplayTags::TAG_Item_Stat_Condition, -10);
		});

	// SetItemStat to 0 (not AddItemStat down to 0) so one press breaks the item whatever it was
	// on. A count of 0 drops the stat from StatTags, which is exactly how "absent" is stored —
	// and both gates that matter read it that way: ComputeConditionBand treats Condition <= 0 as
	// Broken, and Internal_EquipItem refuses a condition user whose GetItemStat is <= 0.
	AddButton(LOCTEXT("Break", "Break"),
		LOCTEXT("BreakTip", "SetItemStat(Item.Stat.Condition, 0) — the item goes inert and auto-unequips"),
		Row.bUsesCondition,
		[ItemID](const FTargets& T)
		{
			T.Inventory->SetItemStat(ItemID, CoreGameplayTags::TAG_Item_Stat_Condition, 0);
		});

	AddButton(LOCTEXT("Repair20", "Repair+20"),
		LOCTEXT("Repair20Tip", "RepairItem(20) — refused outright when the player cannot afford all 20 points"),
		Row.bUsesCondition,
		[ItemID](const FTargets& T)
		{
			T.Inventory->RepairItem(ItemID, 20);
		});

	AddButton(LOCTEXT("RepairFull", "Repair Full"),
		LOCTEXT("RepairFullTip", "RepairItem(0) — buys as many points as the purse allows"),
		Row.bUsesCondition,
		[ItemID](const FTargets& T)
		{
			T.Inventory->RepairItem(ItemID, 0);
		});

	AddButton(bEquipped ? LOCTEXT("Unequip", "Unequip") : LOCTEXT("Equip", "Equip"),
		bEquipped
			? LOCTEXT("UnequipTip", "UnequipSlot(the slot this item occupies)")
			: LOCTEXT("EquipTip", "EquipItem(the definition's first allowed Equip.Slot.*)"),
		Row.bEquippable,
		[ItemID, EquipTarget, EquippedSlot, bEquipped](const FTargets& T)
		{
			if (!T.Equipment)
			{
				LOG_WARNING("Da.Debug.Items: no equipment manager on the local pawn");
				return;
			}
			if (bEquipped)
			{
				T.Equipment->UnequipSlot(EquippedSlot);
			}
			else
			{
				T.Equipment->EquipItem(ItemID, EquipTarget);
			}
		});

	AddButton(LOCTEXT("Drop", "Drop"),
		LOCTEXT("DropTip", "DropItem(slot, 1)"),
		SlotIndex != INDEX_NONE,
		[SlotIndex](const FTargets& T)
		{
			T.Inventory->DropItem(SlotIndex, 1);
		});

	return Line;
}

FReply SDaItemDebugOverlay::RunOp(const TFunction<void(const DaItemDebugOverlayPrivate::FTargets&)>& Op)
{
	const DaItemDebugOverlayPrivate::FTargets T = DaItemDebugOverlayPrivate::Resolve(OwnerPC.Get());
	if (!T.HasInventory())
	{
		LOG_WARNING("Da.Debug.Items: no local PlayerState inventory to act on");
		return FReply::Handled();
	}
	Op(T);

	// Force the next poll to redraw even if the op has not landed yet: on a client every one of
	// these is a Server_* round trip, so the state that comes back is a frame or more away and
	// the signature would otherwise still match.
	LastSignature.Reset();
	return FReply::Handled();
}

namespace DaItemDebugOverlayPrivate
{
	/** One overlay per game viewport, so a multi-window PIE session can show the host's items in
	 *  one window and a client's in another (the statics are shared by every world in the
	 *  process, which a single global overlay would collapse into one). */
	TMap<TWeakObjectPtr<UGameViewportClient>, TSharedPtr<SDaItemDebugOverlay>> Overlays;

	void DropDeadViewports()
	{
		for (auto It = Overlays.CreateIterator(); It; ++It)
		{
			if (!It.Key().IsValid() || !It.Value().IsValid())
			{
				It.RemoveCurrent();
			}
		}
	}

	bool Toggle(UWorld* World)
	{
		DropDeadViewports();

		UGameViewportClient* Viewport = World ? World->GetGameViewport() : nullptr;
		if (!Viewport)
		{
			LOG_WARNING("Da.Debug.Items: no game viewport in this world — is the game running?");
			return false;
		}

		if (TSharedPtr<SDaItemDebugOverlay> Existing = Overlays.FindRef(Viewport))
		{
			Existing->RestoreInput();
			Viewport->RemoveViewportWidgetContent(Existing.ToSharedRef());
			Overlays.Remove(Viewport);
			LastRefreshRowCount = 0;
			return false;
		}

		APlayerController* PC = World->GetFirstPlayerController();
		if (!PC)
		{
			LOG_WARNING("Da.Debug.Items: no local player controller in this world");
			return false;
		}

		TSharedRef<SDaItemDebugOverlay> Overlay = SNew(SDaItemDebugOverlay, PC);
		// Above the game HUD, below anything modal a game puts at the top of the stack.
		Viewport->AddViewportWidgetContent(Overlay, 1000);
		Overlays.Add(Viewport, Overlay);
		return true;
	}
}

#endif // !UE_BUILD_SHIPPING

// ----- Console command (registered in every configuration; the overlay itself is non-shipping) -----

static void DaItemDebugToggleCommand(UWorld* World)
{
#if !UE_BUILD_SHIPPING
	const bool bVisible = DaItemDebugOverlayPrivate::Toggle(World);
	LOG("Da.Debug.Items: overlay %s", bVisible ? TEXT("shown") : TEXT("hidden"));
#else
	LOG_WARNING("Da.Debug.Items: the item debug overlay is not compiled into shipping builds");
#endif
}

static FAutoConsoleCommandWithWorld GDaDebugItemsCommand(
	TEXT("Da.Debug.Items"),
	TEXT("Toggle the item / condition debug overlay for this world's first local player."),
	FConsoleCommandWithWorldDelegate::CreateStatic(&DaItemDebugToggleCommand));

// ----- UDaItemDebugLibrary -----

bool UDaItemDebugLibrary::ToggleItemDebugOverlay(const UObject* WorldContextObject)
{
#if !UE_BUILD_SHIPPING
	UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
		: nullptr;
	return DaItemDebugOverlayPrivate::Toggle(World);
#else
	return false;
#endif
}

bool UDaItemDebugLibrary::IsItemDebugOverlayVisible()
{
#if !UE_BUILD_SHIPPING
	DaItemDebugOverlayPrivate::DropDeadViewports();
	return DaItemDebugOverlayPrivate::Overlays.Num() > 0;
#else
	return false;
#endif
}

int32 UDaItemDebugLibrary::GetItemDebugOverlayRowCount()
{
#if !UE_BUILD_SHIPPING
	return IsItemDebugOverlayVisible() ? DaItemDebugOverlayPrivate::LastRefreshRowCount : 0;
#else
	return 0;
#endif
}

#undef LOCTEXT_NAMESPACE
