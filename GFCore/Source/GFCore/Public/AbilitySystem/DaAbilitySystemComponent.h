// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "DaAbilitySystemComponent.generated.h"

class UDaBaseAttributeSet;
class UDaAbilitySet;
struct FInputActionValue;
class UDaInputConfig;
struct FDaAbilitySet_GrantedHandles;
class UDaPawnData;

DECLARE_MULTICAST_DELEGATE_OneParam(FEffectAssetTags, const FGameplayTagContainer& /*AssetTags*/)
/**
 * 
 */
UCLASS()
class GFCORE_API UDaAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

protected:

	UPROPERTY()
	TArray<FDaAbilitySet_GrantedHandles> OutGrantedAbilityHandlesArray;
	// TODO: Make this a map so we can remove individual AbilitySets later ex. TSet<FDaAbilitySet_GrantedHandles, FGameplayTag> AbilitySets;

	UFUNCTION(Client, Reliable)
	void ClientEffectApplied(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayEffectSpec& EffectSpec, FActiveGameplayEffectHandle ActiveEffectHandle);
	
public:
	UFUNCTION(BlueprintPure, Category = "DA|AbilitySystem")
	static UDaAbilitySystemComponent* FindAbilitySystemComponent(const AActor* Actor) { return (Actor ? Actor->FindComponentByClass<UDaAbilitySystemComponent>() : nullptr); }

	FEffectAssetTags EffectAssetTags;
	
	void InitAbilitiesWithPawnData(const UDaPawnData* DataAsset);
	void ClearAbilitySets();

	void UpgradeAttribute(const FGameplayTag& AttributeTag, int32 Amount);

	UFUNCTION(Server, Reliable)
	void ServerUpgradeAttribute(const FGameplayTag& AttributeTag, int32 Amount);
	
	void GrantSet(const UDaAbilitySet* AbilitySet);
	
	void AbilityInputTagPressed(const FInputActionValue& Value, const FGameplayTag& InputTag);
	void AbilityInputTagHeld(const FGameplayTag InputTag);
	void AbilityInputTagReleased(const FGameplayTag InputTag);

	void AbilityActorInfoSet();
	
	UDaBaseAttributeSet* GetAttributeSetForTag(const FGameplayTag& SetIdentifierTag) const;
};
