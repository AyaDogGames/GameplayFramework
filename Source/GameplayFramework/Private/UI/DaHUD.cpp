// Copyright Dream Awake Solutions LLC


#include "UI/DaHUD.h"

#include "CoreGameplayTags.h"
#include "GameplayFramework.h"
#include "Kismet/GameplayStatics.h"
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

void ADaHUD::InitRootLayout(APlayerController* PC)
{
	checkf(RootLayoutClass, TEXT("RootLayoutClass uninitialized, fill out in HUD blueprint class defaults."));

	UUserWidget* Widget = CreateWidget<UUserWidget>(PC, RootLayoutClass);
	RootLayout = Cast<UDaPrimaryGameLayout>(Widget);
	Widget->AddToViewport();

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

void ADaHUD::RemoveOverlay()
{
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
