// Copyright Dream Awake Solutions LLC


#include "AbilitySystem/DaAbilitySystemComponent.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/DaAbilitySet.h"
#include "DaPawnData.h"
#include "AbilitySystem/Attributes/DaBaseAttributeSet.h"

void UDaAbilitySystemComponent::InitAbilitiesWithPawnData(const UDaPawnData* DataAsset)
{
	if (DataAsset && GetOwner()->HasAuthority())
	{
		TArray<TObjectPtr<UDaAbilitySet>> AbilitySets = DataAsset->AbilitySets;
		for (auto AbilitySet : AbilitySets)
		{
			FDaAbilitySet_GrantedHandles Handle = FDaAbilitySet_GrantedHandles();
			OutGrantedAbilityHandlesArray.Add(Handle);
			AbilitySet->GiveToAbilitySystem(this, &Handle);
		}
	}
}

void UDaAbilitySystemComponent::GrantSet(const UDaAbilitySet* AbilitySet)
{
	if (AbilitySet && GetOwner()->HasAuthority())
	{
		FDaAbilitySet_GrantedHandles Handle = FDaAbilitySet_GrantedHandles();
		OutGrantedAbilityHandlesArray.Add(Handle);
		AbilitySet->GiveToAbilitySystem(this, &Handle);
	}
}

void UDaAbilitySystemComponent::ClearAbilitySets()
{
	if (GetOwner()->HasAuthority())
	{
		for (auto AbilitySetHandle: OutGrantedAbilityHandlesArray)
		{
			AbilitySetHandle.TakeFromAbilitySystem(this);
		}
	}
}

void UDaAbilitySystemComponent::AbilityInputTagPressed(const FInputActionValue& Value, const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid()) return;
	
	for (auto& AbilitySpec: GetActivatableAbilities())
	{
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			AbilitySpecInputPressed(AbilitySpec);
			if (AbilitySpec.IsActive())
			{
				FPredictionKey Key;
				if(UGameplayAbility* Instance = AbilitySpec.GetPrimaryInstance())
				{
					Key = Instance->GetCurrentActivationInfo().GetActivationPredictionKey();
				}
				InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, AbilitySpec.Handle, Key);
			}
		}
	}
}

void UDaAbilitySystemComponent::AbilityInputTagHeld(const FGameplayTag InputTag)
{
	if (!InputTag.IsValid()) return;

	for (auto& AbilitySpec: GetActivatableAbilities())
	{
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			AbilitySpecInputPressed(AbilitySpec);
			if (!AbilitySpec.IsActive())
			{
				TryActivateAbility(AbilitySpec.Handle);
			}
		}
	}
}

void UDaAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag InputTag)
{
	if (!InputTag.IsValid()) return;

	for (auto& AbilitySpec: GetActivatableAbilities())
	{
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag)  && AbilitySpec.IsActive())
		{
			AbilitySpecInputReleased(AbilitySpec);

			FPredictionKey Key;
			if(UGameplayAbility* Instance = AbilitySpec.GetPrimaryInstance())
			{
				Key = Instance->GetCurrentActivationInfo().GetActivationPredictionKey();
			}
			InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, AbilitySpec.Handle, Key);
		}
	}
}

void UDaAbilitySystemComponent::AbilityActorInfoSet()
{
	OnGameplayEffectAppliedDelegateToSelf.AddUObject(this, &UDaAbilitySystemComponent::ClientEffectApplied);
}

UDaBaseAttributeSet* UDaAbilitySystemComponent::GetAttributeSetForTag(const FGameplayTag& SetIdentifierTag) const
{
	TArray<UAttributeSet*> AttributeSets = GetSpawnedAttributes();
	for (UAttributeSet* Set : AttributeSets)
	{
		if(UDaBaseAttributeSet* DaAttributeSet = Cast<UDaBaseAttributeSet>(Set))
		{
			// (A.1).MatchesTag(A) Check if Attributes.Stats.ANYTHING matches Attributes.Stats
			FGameplayTag SetAssignedTag = DaAttributeSet->GetSetIdentifierTag();
			if (SetAssignedTag.IsValid() && SetAssignedTag.MatchesTag(SetIdentifierTag))
			{
				return DaAttributeSet;
			}
		}
	}

	return nullptr;
}

void UDaAbilitySystemComponent::ClientEffectApplied_Implementation(UAbilitySystemComponent* AbilitySystemComponent,
                                                                   const FGameplayEffectSpec& EffectSpec, FActiveGameplayEffectHandle ActiveEffectHandle)
{
	FGameplayTagContainer TagContainer;
	EffectSpec.GetAllAssetTags(TagContainer);

	if (TagContainer.IsValid())
		EffectAssetTags.Broadcast(TagContainer);
}

void UDaAbilitySystemComponent::UpgradeAttribute(const FGameplayTag& AttributeTag, int32 Amount)
{
	ServerUpgradeAttribute(AttributeTag, Amount);
}

void UDaAbilitySystemComponent::ServerUpgradeAttribute_Implementation(const FGameplayTag& AttributeTag, int32 Amount)
{
	FGameplayEventData Payload;
	Payload.EventTag = AttributeTag;
	Payload.EventMagnitude = Amount;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(GetAvatarActor(), AttributeTag, Payload);
}