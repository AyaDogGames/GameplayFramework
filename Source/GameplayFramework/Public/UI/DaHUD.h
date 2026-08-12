// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/HUD.h"
#include "DaHUD.generated.h"

class UDaWidgetController;
class UDaUILevelData;
class UDaPrimaryGameLayout;
class UDaAbilitySystemComponent;
class UDaStatMenuWidgetController;
class UDaOverlayWidgetController;
class UDaInventoryWidgetController;
class UDaHotbarWidget;
class UDaUserWidgetBase;
struct FWidgetControllerParams;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnOnPrimaryGameLayoutLoaded);

/**
 * 
 */
UCLASS()
class GAMEPLAYFRAMEWORK_API ADaHUD : public AHUD
{
	GENERATED_BODY()

public:

	UDaPrimaryGameLayout* GetRootLayout() {return RootLayout;}

	UDaWidgetController* GetWidgetController(const TSubclassOf<UDaWidgetController>& WidgetControllerClass, const FWidgetControllerParams& WCParams);
	
	UDaOverlayWidgetController* GetOverlayWidgetController(const FWidgetControllerParams& WCParams);
	UDaStatMenuWidgetController* GetStatMenuWidgetController(const FWidgetControllerParams& WCParams);
	UDaInventoryWidgetController* GetInventoryWidgetController(const FWidgetControllerParams& WCParams);

	UFUNCTION(BlueprintCallable)
	void InitRootLayout(APlayerController* PC);

	// subclasses can use this to do things after root layout has been loaded. Base class calls multicast delegate OnPrimaryGameLayoutLoaded
	virtual void NativeRootLayoutLoaded();
	
	UFUNCTION(BlueprintCallable)
	void InitOverlay(APlayerController* PC, APlayerState* PS, UDaAbilitySystemComponent* ASC);

	UFUNCTION(BlueprintCallable)
	void RemoveOverlay();

	/**
	 * Put the hotbar row on screen. Called from InitRootLayout, and a no-op when HotbarWidgetClass
	 * is unset — which is how projects with no hotbar stay unaffected.
	 *
	 * Two placement decisions, both learned from getting them wrong:
	 *  - It goes straight to the PLAYER SCREEN, not onto the UI.Layer.Game stack that carries the
	 *    overlay. An activatable-widget stack shows only its top entry, so pushing the hotbar there
	 *    would hide the overlay (and vice versa). A hotbar is a HUD element, not a screen; nothing
	 *    about it wants CommonUI's activation lifecycle or input routing.
	 *  - It goes up with the ROOT LAYOUT, not with the overlay. A network client in GlitchShaper
	 *    reaches InitRootLayout but never InitOverlay, and hooking the hotbar to the latter left
	 *    every client without one.
	 */
	UFUNCTION(BlueprintCallable)
	void InitHotbar(APlayerController* PC);

	UFUNCTION(BlueprintCallable)
	void RemoveHotbar();

	UFUNCTION(BlueprintPure, Category="UI|Hotbar")
	UDaHotbarWidget* GetHotbarWidget() const { return HotbarWidget; }

	FORCEINLINE FGameplayTagContainer GetOverlayAttributeSetTags() { return OverlayWidgetAttributeSetTags; }
	FORCEINLINE FGameplayTagContainer GetStatMenuAttributeSetTags() { return StatMenuWidgetAttributeSetTags; }
	FORCEINLINE FGameplayTagContainer GetInventoryAttributeSetTags() { return InventoryWidgetAttributeSetTags; }

protected:

	// HUD Loads this before anything else. See: Blueprint Asset WBP_PrimaryLayout which loads 4 UI.Layer activatable widget Containers. EWhen this class is loaded the event OnPrimaryGameLayoutLoaded will get fired.
	UPROPERTY(EditAnywhere, Category=UI)
	TSubclassOf<UDaPrimaryGameLayout> RootLayoutClass;

	// Runtime RootLayout instance pointer
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category=UI)
	TObjectPtr<UDaPrimaryGameLayout> RootLayout;
	
	// Overlay
	
	// The Default OverlayWidget to use if a level data is not found in CurrentLevelData for the current level.
	UPROPERTY(EditAnywhere, Category="UI|Overlay")
	TSubclassOf<UDaUserWidgetBase> OverlayWidgetClass;

	// Runtime DefaultOverlayWidget instance pointer
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI|Overlay")
	TObjectPtr<UDaUserWidgetBase> OverlayWidget;
	
	// The Default OverlayWidgetController to use if a level data is not found in CurrentLevelData for the current level.
	UPROPERTY(EditAnywhere, Category="UI|Overlay")
	TSubclassOf<UDaOverlayWidgetController> OverlayWidgetControllerClass;

	// Runtime Default OverlayWidgetController instance pointer
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI|Overlay")
	TObjectPtr<UDaOverlayWidgetController> OverlayWidgetController;
	
	// Default Overlay Attribute Set Tags
	UPROPERTY(EditAnywhere, Category="UI|Overlay")
	FGameplayTagContainer OverlayWidgetAttributeSetTags;

	// Per-Level Overlay Overrides - Data Assets which map Root "UI.Layer.Game" Overlay Widgets and Controllers
	UPROPERTY(EditDefaultsOnly, Category="UI|Overlay")
	TArray<TObjectPtr<UDaUILevelData>> OverlayWidgetLevelData;
	
	// Hotbar

	// Row of Equip.Slot.Item1..4 quick slots, drawn over the overlay. Leave unset for no hotbar.
	UPROPERTY(EditAnywhere, Category="UI|Hotbar")
	TSubclassOf<UDaHotbarWidget> HotbarWidgetClass;

	// Z order the hotbar is added to the player screen with. Above the root layout (which goes in
	// at 0) so the row is not painted over by the overlay.
	UPROPERTY(EditAnywhere, Category="UI|Hotbar")
	int32 HotbarZOrder = 1;

	// Runtime hotbar instance pointer
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI|Hotbar")
	TObjectPtr<UDaHotbarWidget> HotbarWidget;

	// Stats

	// Widget Controller for all GAS attributes in a given AttributeSet array
	UPROPERTY(EditAnywhere, Category="UI|Stats")
	TSubclassOf<UDaStatMenuWidgetController> StatMenuWidgetControllerClass;

	// Stat Menu Attribute Set GameplayTags 
	UPROPERTY(EditAnywhere, Category="UI|Stats")
	FGameplayTagContainer StatMenuWidgetAttributeSetTags;

	// Runtime StatWidgetController instance pointer
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI|Stats")
	TObjectPtr<UDaStatMenuWidgetController> StatMenuWidgetController;

	// Inventory

	// Widget Controller setup to respond to a given InventoryComponent
	UPROPERTY(EditAnywhere, Category="UI|Inventory")
	TSubclassOf<UDaInventoryWidgetController> InventoryWidgetControllerClass;

	// Inventory Attribute set GameplayTags
	UPROPERTY(EditAnywhere, Category="UI|Inventory")
	FGameplayTagContainer InventoryWidgetAttributeSetTags;

	// Runtime Inventory WidgetController instance pointer
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="UI|Inventory")
	TObjectPtr<UDaInventoryWidgetController> InventoryWidgetController;

	// OnPrimaryGameLayoutLoaded Event
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnOnPrimaryGameLayoutLoaded OnPrimaryGameLayoutLoaded;
};
