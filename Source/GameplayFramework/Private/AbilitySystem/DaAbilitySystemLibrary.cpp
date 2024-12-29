// Copyright Dream Awake Solutions LLC


#include "AbilitySystem/DaAbilitySystemLibrary.h"

#include "AbilitySystemComponent.h"
#include "CoreGameplayTags.h"
#include "DaPlayerState.h"
#include "AbilitySystem/DaAbilitySystemComponent.h"
#include "Inventory/DaInventoryWidgetController.h"
#include "Kismet/GameplayStatics.h"
#include "UI/DaHUD.h"
#include "UI/DaWidgetController.h"

int32 UDaAbilitySystemLibrary::GetAttributesWithParentTag(const FGameplayTag& ParentTag,
	const FGameplayTagContainer& FromContainer, FGameplayTagContainer& OutContainer)
{
	int32 FoundTags = 0;
	if (FromContainer.IsValid())
	{
		for (FGameplayTag Tag: FromContainer)
		{
			// "A.1".matches("A")
			if (Tag.MatchesTag(ParentTag))
			{
				OutContainer.AddTag(Tag);
				FoundTags++;
			}
		}
	}
	return FoundTags;
}

// Utility to get leaf tags from a parent tag if it exists in a tag container
FGameplayTag UDaAbilitySystemLibrary::GetLeafTag(const FGameplayTag& ParentTag, const FGameplayTagContainer& FromContainer)
{
	if (FromContainer.IsValid())
	{
		for (FGameplayTag Tag: FromContainer)
		{
			// if ("A.1".matches("A"))
			if (Tag.MatchesTag(ParentTag))
			{
				// return leaf tag "A.1"
				return Tag;
			}
		}
	}

	// If no matching tag is found, return an empty/default tag
	return FGameplayTag();
}

UDaOverlayWidgetController* UDaAbilitySystemLibrary::GetOverlayWidgetController(const UObject* WorldContextObject)
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0))
	{
		if (ADaHUD* HUD = Cast<ADaHUD>(PC->GetHUD()))
		{
			ADaPlayerState* PS = PC->GetPlayerState<ADaPlayerState>();
			UDaAbilitySystemComponent* ASC = Cast<UDaAbilitySystemComponent>(PS->GetAbilitySystemComponent());
			const FGameplayTagContainer SetTags = HUD->GetOverlayAttributeSetTags();
			const FWidgetControllerParams WidgetControllerParams(PC, PS, ASC, SetTags);
			return HUD->GetOverlayWidgetController(WidgetControllerParams);
		}
	}
	return nullptr;
}

UDaStatMenuWidgetController* UDaAbilitySystemLibrary::GetStatMenuWidgetController(const UObject* WorldContextObject)
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0))
	{
		if (ADaHUD* HUD = Cast<ADaHUD>(PC->GetHUD()))
		{
			ADaPlayerState* PS = PC->GetPlayerState<ADaPlayerState>();
			UDaAbilitySystemComponent* ASC = Cast<UDaAbilitySystemComponent>(PS->GetAbilitySystemComponent());
			const FGameplayTagContainer SetTags = HUD->GetStatMenuAttributeSetTags();
			const FWidgetControllerParams WidgetControllerParams(PC, PS, ASC, SetTags);
			return HUD->GetStatMenuWidgetController(WidgetControllerParams);
		}
	}
	return nullptr;
}

UDaInventoryWidgetController* UDaAbilitySystemLibrary::GetInventoryMenuWidgetController(
	const UObject* WorldContextObject)
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0))
	{
		if (ADaHUD* HUD = Cast<ADaHUD>(PC->GetHUD()))
		{
			ADaPlayerState* PS = PC->GetPlayerState<ADaPlayerState>();
			UDaAbilitySystemComponent* ASC = Cast<UDaAbilitySystemComponent>(PS->GetAbilitySystemComponent());
			const FGameplayTagContainer SetTags = HUD->GetInventoryAttributeSetTags();
			const FWidgetControllerParams WidgetControllerParams(PC, PS, ASC, SetTags);
			UDaInventoryWidgetController* Controller = HUD->GetInventoryWidgetController(WidgetControllerParams);
			Controller->InitializeInventory(PS);
			return Controller;
		}
	}
	return nullptr;
}
