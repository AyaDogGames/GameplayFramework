// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "DaLevelData.generated.h"

USTRUCT(BlueprintType)
struct FDaPOIDataRef
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "Level.POI"), Category="POI")
	FGameplayTagContainer InfoTags;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="POI")
	FTransform Transform;
};

/**
 * 
 */
UCLASS(BlueprintType, Const)
class GAMEPLAYFRAMEWORK_API UDaLevelData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta = (Categories = "Level.ID"), Category = "POI")
	FGameplayTag LevelDataIDTag;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(TitleProperty="Name"), Category = "POI")
	TArray<FDaPOIDataRef> POILocations;
	
	virtual FPrimaryAssetId GetPrimaryAssetId() const override
	{
		return FPrimaryAssetId("LevelPOIData", GetFName());
	}
};
