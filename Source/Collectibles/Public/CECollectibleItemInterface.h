// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CECollectibleData.h"
#include "GameplayTagContainer.h"
#include "UObject/Interface.h"
#include "CECollectibleItemInterface.generated.h"

UENUM()
enum ECollectibleSetProperties
{
	None,
	Edition,
	Issuer,
	SpecialTags,
	Grade,
	All
};

class USlateBrushAsset;
// This class does not need to be modified.
UINTERFACE(Blueprintable)
class COLLECTIBLES_API UCECollectibleItemInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class COLLECTIBLES_API ICECollectibleItemInterface
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Collectible")
	int32 GetItemCoreTags(FGameplayTagContainer& OutItemTags) const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Collectible")
	USlateBrushAsset* GetItemBrush() const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Collectible")
	UTexture2D* GetItemIcon() const;
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Collectible")
	FCECollectibleDataDef GetDataRef() const;

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Collectible")
	FCECollectibleTemplateBaseRef GetTemplateDataRef() const;
	
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Collectible")
	void SetCollectibleData(UCECollectibleData* Data);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Collectible")
	FString GetCollectibleDisplayNames(bool bPlayerDesignated);
	
	// Saves to GameInstance for persisting data across levels but will not save to disk by default. Save to disk by setting bSaveToDisk to true
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "Collectible")
	void SaveCollectible(bool bSaveToDisk=false);
};
