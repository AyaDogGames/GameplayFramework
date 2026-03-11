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

	UFUNCTION(BlueprintCallable)
	void InitRootLayout(APlayerController* PC);

	// subclasses can use this to do things after root layout has been loaded. Base class calls multicast delegate OnPrimaryGameLayoutLoaded
	virtual void NativeRootLayoutLoaded();
	
	UFUNCTION(BlueprintCallable)
	void InitOverlay(APlayerController* PC, APlayerState* PS, UDaAbilitySystemComponent* ASC);

	UFUNCTION(BlueprintCallable)
	void RemoveOverlay();

	FORCEINLINE FGameplayTagContainer GetOverlayAttributeSetTags() { return OverlayWidgetAttributeSetTags; }
	FORCEINLINE FGameplayTagContainer GetStatMenuAttributeSetTags() { return StatMenuWidgetAttributeSetTags; }

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
	
	// OnPrimaryGameLayoutLoaded Event
	UPROPERTY(BlueprintAssignable, Category="Events")
	FOnOnPrimaryGameLayoutLoaded OnPrimaryGameLayoutLoaded;
};
