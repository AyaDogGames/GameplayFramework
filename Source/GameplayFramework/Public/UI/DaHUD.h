// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "GameFramework/HUD.h"
#include "DaHUD.generated.h"

class UDaPrimaryGameLayout;
class UDaInventoryUIWidget;
class UDaInventoryWidgetController;
class UDaAbilitySystemComponent;
class UDaStatMenuWidgetController;
class UDaOverlayWidgetController;
class UDaUserWidgetBase;
struct FWidgetControllerParams;

/**
 * 
 */
UCLASS()
class GAMEPLAYFRAMEWORK_API ADaHUD : public AHUD
{
	GENERATED_BODY()

public:

	UDaPrimaryGameLayout* GetRootLayout() {return RootLayout;}
	
	UDaOverlayWidgetController* GetOverlayWidgetController(const FWidgetControllerParams& WCParams);
	UDaStatMenuWidgetController* GetStatMenuWidgetController(const FWidgetControllerParams& WCParams);
	UDaInventoryWidgetController* GetInventoryWidgetController(const FWidgetControllerParams& WCParams);

	void InitRootLayout(APlayerController* PC, APlayerState* PS, UDaAbilitySystemComponent* ASC);
	void InitOverlay(APlayerController* PC, APlayerState* PS, UDaAbilitySystemComponent* ASC);
	void RemoveOverlay();

	FORCEINLINE FGameplayTagContainer GetOverlayAttributeSetTags() { return OverlayWidgetAttributeSetTags; } 
	FORCEINLINE FGameplayTagContainer GetStatMenuAttributeSetTags() { return StatMenuWidgetAttributeSetTags; } 
	FORCEINLINE FGameplayTagContainer GetInventoryAttributeSetTags() { return InventoryWidgetAttributeSetTags; } 

protected:

	UPROPERTY(EditAnywhere, Category=UI)
	TSubclassOf<UDaPrimaryGameLayout> RootLayoutClass;
	
	UPROPERTY(EditAnywhere, Category=UI)
	TSubclassOf<UDaUserWidgetBase> OverlayWidgetClass;
	
	UPROPERTY(EditAnywhere, Category=UI)
	TSubclassOf<UDaOverlayWidgetController> OverlayWidgetControllerClass;
	
	UPROPERTY(EditAnywhere, Category=UI)
	FGameplayTagContainer OverlayWidgetAttributeSetTags;
	
	UPROPERTY(EditAnywhere, Category=UI)
	TSubclassOf<UDaStatMenuWidgetController> StatMenuWidgetControllerClass;

	UPROPERTY(EditAnywhere, Category=UI)
	FGameplayTagContainer StatMenuWidgetAttributeSetTags;
	
	UPROPERTY(EditAnywhere, Category=UI)
	TSubclassOf<UDaInventoryWidgetController> InventoryWidgetControllerClass;

	UPROPERTY(EditAnywhere, Category=UI)
	FGameplayTagContainer InventoryWidgetAttributeSetTags;
	
private:

	UPROPERTY()
	TObjectPtr<UDaPrimaryGameLayout> RootLayout;

	UPROPERTY()
	TObjectPtr<UDaUserWidgetBase> OverlayWidget;

	UPROPERTY()
	TObjectPtr<UDaOverlayWidgetController> OverlayWidgetController;

	UPROPERTY()
	TObjectPtr<UDaStatMenuWidgetController> StatMenuWidgetController;
	
	UPROPERTY()
	TObjectPtr<UDaInventoryWidgetController> InventoryWidgetController;
};
