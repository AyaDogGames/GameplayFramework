// Copyright Dream Awake Solutions LLC


#include "UI/DaHUD.h"

#include "CoreGameplayTags.h"
#include "DaPlayerState.h"
#include "Inventory/DaInventoryUIWidget.h"
#include "Inventory/DaInventoryWidgetController.h"
#include "UI/DaCommonUIExtensions.h"
#include "UI/DaOverlayWidgetBase.h"
#include "UI/DaOverlayWidgetController.h"
#include "UI/DaStatMenuWidgetController.h"
#include "UI/DaUserWidgetBase.h"
#include "UI/DaPrimaryGameLayout.h"

UDaOverlayWidgetController* ADaHUD::GetOverlayWidgetController(const FWidgetControllerParams& WCParams)
{
	if (OverlayWidgetController == nullptr)
	{
		OverlayWidgetController = NewObject<UDaOverlayWidgetController>(this, OverlayWidgetControllerClass);
		OverlayWidgetController->SetWidgetControllerParams(WCParams);
		OverlayWidgetController->BindCallbacksToDependencies();
	}
	return OverlayWidgetController;
}

UDaStatMenuWidgetController* ADaHUD::GetStatMenuWidgetController(const FWidgetControllerParams& WCParams)
{
	if (StatMenuWidgetController == nullptr)
	{
		StatMenuWidgetController = NewObject<UDaStatMenuWidgetController>(this, StatMenuWidgetControllerClass);
		StatMenuWidgetController->SetWidgetControllerParams(WCParams);
		StatMenuWidgetController->BindCallbacksToDependencies();
	}
	return StatMenuWidgetController;
}

UDaInventoryWidgetController* ADaHUD::GetInventoryWidgetController(const FWidgetControllerParams& WCParams)
{
	if (InventoryWidgetController == nullptr)
	{
		InventoryWidgetController = NewObject<UDaInventoryWidgetController>(this, InventoryWidgetControllerClass);
		InventoryWidgetController->SetWidgetControllerParams(WCParams);
		InventoryWidgetController->BindCallbacksToDependencies();
	}
	return InventoryWidgetController;
}

void ADaHUD::InitRootLayout(APlayerController* PC, APlayerState* PS, UDaAbilitySystemComponent* ASC)
{
	checkf(RootLayoutClass, TEXT("RootLayoutClass uninitialized, fill out in HUD blueprint class defaults."));

	UUserWidget* Widget = CreateWidget<UUserWidget>(PC, RootLayoutClass);
	RootLayout = Cast<UDaPrimaryGameLayout>(Widget);
	
	Widget->AddToViewport();

	OnPrimaryGameLayoutLoaded.Broadcast();
}

void ADaHUD::InitOverlay(APlayerController* PC, APlayerState* PS, UDaAbilitySystemComponent* ASC)
{
	checkf(OverlayWidgetClass, TEXT("OverlayWidgetClass uninitialized, fill out in HUD blueprint class defaults."));
	checkf(OverlayWidgetControllerClass, TEXT("OverlayWidgetControllerClass uninitialized, fill out in HUD blueprint class defaults."));
	checkf(OverlayWidgetAttributeSetTags.IsValid(), TEXT("OverlayWidgetAttributeSetTags empty, Fill out AttributeSet Tags in HUD blueprint class defaults."));
	
	const FWidgetControllerParams WidgetControllerParams(PC, PS, ASC, OverlayWidgetAttributeSetTags);
	UDaOverlayWidgetController* WidgetController = GetOverlayWidgetController(WidgetControllerParams);
	
	RootLayout->PushWidgetToLayerStack<UDaOverlayWidgetBase>(CoreGameplayTags::TAG_UI_Layer_Game, OverlayWidgetClass, [this, WidgetController](UDaOverlayWidgetBase& Widget)
	{
		OverlayWidget = Widget;
		OverlayWidget->SetWidgetController(WidgetController);
		WidgetController->BroadcastInitialValues();
	});
}

void ADaHUD::RemoveOverlay()
{
	if (OverlayWidget)
	{
		RootLayout->FindAndRemoveWidgetFromLayer(OverlayWidget);
	}

	if (OverlayWidgetController)
	{
		OverlayWidgetController->MarkAsGarbage();
		OverlayWidgetController = nullptr;
	}
}
