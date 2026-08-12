// Copyright Dream Awake Solutions LLC


#include "UI/DaHUD.h"

#include "CoreGameplayTags.h"
#include "GameplayFramework.h"
#include "Kismet/GameplayStatics.h"
#include "Inventory/DaInventoryWidgetController.h"
#include "UI/DaHotbarWidget.h"
#include "UI/DaOverlayWidgetController.h"
#include "UI/DaStatMenuWidgetController.h"
#include "UI/DaUserWidgetBase.h"
#include "UI/DaPrimaryGameLayout.h"
#include "UI/DaUILevelData.h"

UDaWidgetController* ADaHUD::GetWidgetController(const TSubclassOf<UDaWidgetController>& WidgetControllerClass, const FWidgetControllerParams& WCParams)
{
	UDaWidgetController* WidgetController = NewObject<UDaWidgetController>(this, WidgetControllerClass);
	WidgetController->SetWidgetControllerParams(WCParams);
	WidgetController->BindCallbacksToDependencies();
	
	return WidgetController;
}

UDaOverlayWidgetController* ADaHUD::GetOverlayWidgetController(const FWidgetControllerParams& WCParams)
{
	if (OverlayWidgetController == nullptr)
	{
		OverlayWidgetController = Cast<UDaOverlayWidgetController>(GetWidgetController(OverlayWidgetControllerClass, WCParams));
	}
	return OverlayWidgetController;
}

UDaStatMenuWidgetController* ADaHUD::GetStatMenuWidgetController(const FWidgetControllerParams& WCParams)
{
	if (StatMenuWidgetController == nullptr)
	{
		StatMenuWidgetController = Cast<UDaStatMenuWidgetController>(GetWidgetController(StatMenuWidgetControllerClass, WCParams));
	}
	return StatMenuWidgetController;
}

UDaInventoryWidgetController* ADaHUD::GetInventoryWidgetController(const FWidgetControllerParams& WCParams)
{
	if (InventoryWidgetController == nullptr)
	{
		InventoryWidgetController = Cast<UDaInventoryWidgetController>(GetWidgetController(InventoryWidgetControllerClass, WCParams));
	}
	return InventoryWidgetController;
}

void ADaHUD::InitRootLayout(APlayerController* PC)
{
	checkf(RootLayoutClass, TEXT("RootLayoutClass uninitialized, fill out in HUD blueprint class defaults."));

	UUserWidget* Widget = CreateWidget<UUserWidget>(PC, RootLayoutClass);
	RootLayout = Cast<UDaPrimaryGameLayout>(Widget);
	Widget->AddToViewport();

	// The hotbar goes up with the root layout, NOT with the overlay. It has no dependency on the
	// overlay's widget controller, its attribute tags or the per-level UI data — it resolves the
	// local player's inventory itself — and hooking it to InitOverlay cost a client its hotbar
	// entirely: in GlitchShaper a network client reaches InitRootLayout but never InitOverlay, so
	// the row existed on the host and nowhere else.
	InitHotbar(PC);

	// notify we are loaded and ready to have widgets pushed onto layers
	NativeRootLayoutLoaded();
}

void ADaHUD::NativeRootLayoutLoaded()
{
	OnPrimaryGameLayoutLoaded.Broadcast();
}

void ADaHUD::InitOverlay(APlayerController* PC, APlayerState* PS, UDaAbilitySystemComponent* ASC)
{
	// look for per-level overlay and controller overrides  
	bool bFound = false;
	TSubclassOf<UDaUserWidgetBase> OverlayWidgetClassToLoad = nullptr;
	
	for (UDaUILevelData* LevelData : OverlayWidgetLevelData)
	{
		if (LevelData->Level != nullptr)
		{
			if (LevelData->Level.GetAssetName().Equals(UGameplayStatics::GetCurrentLevelName(this)))
			{
				checkf(LevelData->WidgetClass, TEXT("LevelData.WidgetClass uninitialized, fill out in DaUILevelData DataAsset class defaults set on HUD LevelWidgetData."));
				checkf(LevelData->WidgetControllerClass, TEXT("LevelData.WidgetClass uninitialized,  fill out in DaUILevelData DataAsset class defaults set on HUD LevelWidgetData."));
				checkf(LevelData->WidgetGameplayAttributeSetTags.IsValid(), TEXT("LevelData.WidgetClass empty,  fill out in DaUILevelData DataAsset class defaults set on HUD LevelWidgetData."));
				
				// Load Overlay
				const FWidgetControllerParams WidgetControllerParams(PC, PS, ASC, LevelData->WidgetGameplayAttributeSetTags);
				OverlayWidgetController = Cast<UDaOverlayWidgetController>(GetWidgetController(LevelData->WidgetControllerClass, WidgetControllerParams));
				OverlayWidgetClassToLoad = LevelData->WidgetClass;
				bFound = true;
				break;
			}
		}
	}

	// We didnt find any per-level data overrides so load default overlay and controller
	if (!bFound)
	{
		checkf(OverlayWidgetClass, TEXT("OverlayWidgetClass uninitialized, either fill out in DaUILevelData DataAsset class defaults set on HUD LevelWidgetData."));
		checkf(OverlayWidgetControllerClass, TEXT("OverlayWidgetControllerClass uninitialized, either fill out in DaUILevelData DataAsset class defaults set on HUD LevelWidgetData."));
		checkf(OverlayWidgetAttributeSetTags.IsValid(), TEXT("OverlayWidgetAttributeSetTags empty, either fill out in DaUILevelData DataAsset class defaults set on HUD LevelWidgetData."));

		// Load Overlay
		const FWidgetControllerParams WidgetControllerParams(PC, PS, ASC, OverlayWidgetAttributeSetTags);
		OverlayWidgetController = GetOverlayWidgetController(WidgetControllerParams);
		OverlayWidgetClassToLoad = OverlayWidgetClass;
		bFound = true;
	}

	if (bFound)
	{
		RootLayout->PushWidgetToLayerStack<UDaUserWidgetBase>(CoreGameplayTags::TAG_UI_Layer_Game, OverlayWidgetClassToLoad, [this](UDaUserWidgetBase& Widget)
		{
				OverlayWidget = &Widget;
				OverlayWidget->SetWidgetController(OverlayWidgetController);
				OverlayWidgetController->BroadcastInitialValues();
		});
	}
	else
	{
		LOG_ERROR("DaHud: No Game Overlay Class or OverlayController Class found");
	}
}

void ADaHUD::InitHotbar(APlayerController* PC)
{
	if (!HotbarWidgetClass || !PC)
	{
		return;
	}

	if (HotbarWidget)
	{
		// InitOverlay can run again (level change, re-init); one row is enough.
		return;
	}

	HotbarWidget = CreateWidget<UDaHotbarWidget>(PC, HotbarWidgetClass);
	if (!HotbarWidget)
	{
		LOG_ERROR("DaHud: failed to create hotbar widget of class %s", *GetNameSafe(HotbarWidgetClass));
		return;
	}

	HotbarWidget->AddToPlayerScreen(HotbarZOrder);
}

void ADaHUD::RemoveHotbar()
{
	if (HotbarWidget)
	{
		HotbarWidget->RemoveFromParent();
		HotbarWidget = nullptr;
	}
}

void ADaHUD::RemoveOverlay()
{
	RemoveHotbar();

	if (OverlayWidget)
	{
		RootLayout->FindAndRemoveWidgetFromLayer(OverlayWidget);
		OverlayWidget = nullptr;
	}

	if (OverlayWidgetController)
	{
		OverlayWidgetController->MarkAsGarbage();
		OverlayWidgetController = nullptr;
	}
}
