// Copyright Dream Awake Solutions LLC


#include "UI/DaOverlayWidgetController.h"

#include "CoreGameplayTags.h"
#include "AbilitySystem/DaAbilitySystemComponent.h"
#include "UI/DaWidgetMessageData.h"

UDaOverlayWidgetController::UDaOverlayWidgetController()
{
	MessageParentTag = CoreGameplayTags::TAG_Message;
}

void UDaOverlayWidgetController::BindCallbacksToDependencies()
{
	Super::BindCallbacksToDependencies();
	
	Cast<UDaAbilitySystemComponent>(AbilitySystemComponent)->EffectAssetTags.AddLambda(
		[this](const FGameplayTagContainer& AssetTags)
	{
			for (const FGameplayTag& Tag: AssetTags)
			{
				if (Tag.MatchesTag(MessageParentTag))
				{
					if (MessageDataAsset)
					{
						FDaUIWidgetMessageData Data = MessageDataAsset->FindMessageDataForTag(Tag);
						if (Data.IsValid())
						{
							MessageWidgetDataDelegate.Broadcast(Data);
						}
					}
				}
			}
	});
}
