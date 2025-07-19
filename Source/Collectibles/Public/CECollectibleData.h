// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "CECollectibleData.generated.h"

class UCECollectibleViewModel;
class ACECollectibleBase;

UENUM(BlueprintType)
enum ECollectibleSpawnLocations
{
	UnsortedInventoryLocation UMETA(DisplayName = "Unsorted"),
	KeepInventoryLocation UMETA(DisplayName = "Keep"),
	SellInventoryLocation UMETA(DisplayName = "Sell"),
	OtherInventoryLocation UMETA(DisplayName = "Other"),
	MaxLocations UMETA(Hidden)
};

constexpr uint8 max_locations_uint8 = static_cast<uint8>(ECollectibleSpawnLocations::MaxLocations);

USTRUCT()
struct FCECollectibleTemplateBaseRef : public FTableRowBase
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collectible Template Base")
	FName Name = FName();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collectible Template Base")
	FName Subtype = FName();
		
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Collectible Template Base")
	int Year = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collectible Template Base")
	FName Issuer = FName();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collectible Template Base")
	FName Edition = FName();
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collectible Template Base")
	TSoftObjectPtr<UTexture2D> Image = nullptr;

	// Any Special Tags for this template. Will be added to final instance Special Tags
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Collectible Template Base", meta = (Categories = "Collectibles.Special"))
	FGameplayTagContainer SpecialTags = FGameplayTagContainer();
};

USTRUCT(BlueprintType)
struct FCECoinCoreDataRef : public FCECollectibleTemplateBaseRef
{
	GENERATED_BODY()
	
	// Unique CID for this coin template
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Coin Template")
	FName MintMark = FName();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Coin Template")
	FName Material = FName();
};

USTRUCT(BlueprintType)
struct FCECardCoreDataRef : public FCECollectibleTemplateBaseRef
{
	GENERATED_BODY()
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Card Template")
	FPrimaryAssetId EditionTemplateAssetId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Card Template")
	int Variation = -1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Card Template")
	FPrimaryAssetId WearTemplateAssetId;
};

USTRUCT(BlueprintType)
struct FCECollectibleDataDef : public FTableRowBase
{
	GENERATED_BODY()

	// Unique UUID Representing this Collectible
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collectibles")
	FName Name = FName();
	
	// Human Readable Name representing this Collectible.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collectibles")
	FName CollectibleName = FName();

	// Maps to a Collectible Template DataTable Row	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collectibles")
	FName CID = FName();
	
	// The Grade of the Collectible
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Collectibles")
	int Grade = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Collectibles")
	TEnumAsByte<ECollectibleSpawnLocations> SpawnLocation = UnsortedInventoryLocation;

	// Core definition of a Collectible. Should have a Class assigned in SpawnManager
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Collectibles", meta = (Categories = "Collectibles.Core"))
	FGameplayTag CoreTag = FGameplayTag();

	// Any special identifying features of the Collectible.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Collectibles", meta = (Categories = "Collectibles.Special"))
	FGameplayTagContainer SpecialTags = FGameplayTagContainer();

	bool InitializedWithData = false;
	virtual void OnPostDataImport(const UDataTable* InDataTable, const FName InRowName, TArray<FString>& OutCollectedImportProblems) override
	{
		InitializedWithData = true;
	}
	bool IsInitialized() const { return InitializedWithData; } 
};

USTRUCT(BlueprintType)
struct FCECollectiblePlayerAppraisalData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appraisal")
	FName CollectibleUUID = FName();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appraisal")
	int32 PlayerSetGrade = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appraisal")
	int32 PlayerSetYear = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Appraisal")
	int32 AppraisedValue = 0;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Collectibles", meta = (Categories = "Collectibles.Special"))
	FGameplayTagContainer PlayerSetSpecialTags = FGameplayTagContainer();

	bool IsAppraisalValid() const { return PlayerSetGrade > 0 && PlayerSetYear > 0; }
};


UCLASS(BlueprintType)
class COLLECTIBLES_API UCECollectibleData : public UObject
{
	GENERATED_BODY()

public:
	UCECollectibleData();

	static UCECollectibleData* CreateCoinData(UObject* Outer, const FCECollectibleDataDef* DataRef, const FCECoinCoreDataRef* TemplateDataRef, TSubclassOf<UCECollectibleViewModel> CollectibleViewModelClass, ECollectibleSpawnLocations LocationType)
	{
		UCECollectibleData* Data = NewObject<UCECollectibleData>(Outer);
		Data->CollectibleDataRef = *DataRef;
		Data->CoreTypeTag = DataRef->CoreTag;
		Data->CoinTemplateData = *TemplateDataRef;
		Data->CurrentLocationType = LocationType;
		Data->CollectibleViewModeClass = CollectibleViewModelClass;
		Data->SetupViewModel();
		return Data;
	}

	static UCECollectibleData* CreateCardData(UObject* Outer, const FCECollectibleDataDef* DataRef, const FCECardCoreDataRef* TemplateDataRef, TSubclassOf<UCECollectibleViewModel> CollectibleViewModelClass, ECollectibleSpawnLocations LocationType)
	{
		UCECollectibleData* Data = NewObject<UCECollectibleData>(Outer);
		Data->CollectibleDataRef = *DataRef;
		Data->CoreTypeTag = DataRef->CoreTag;
		Data->CardTemplateData = *TemplateDataRef;
		Data->CurrentLocationType = LocationType;
		Data->CollectibleViewModeClass = CollectibleViewModelClass;
		Data->SetupViewModel();
		return Data;
	}
	
	UFUNCTION(BlueprintCallable, Category = "Collectible")
	void SetPlayerSelectedYear(int32 Year);

	UFUNCTION(BlueprintCallable, Category = "Collectible")
	void SetPlayerSelectedGrade(int32 Grade);

	UFUNCTION(BlueprintCallable, Category = "Collectible")
	void AddPlayerSelectedSpecialTag(const FGameplayTag& InTag);

	UFUNCTION(BlueprintCallable, Category = "Collectible")
	void RemovePlayerSelectedSpecialTag(const FGameplayTag& InTag);
	
	UFUNCTION(BlueprintCallable, Category = "Collectible")
	void SetAppraisedValue(int32 Value);
	
	UPROPERTY(BlueprintReadOnly, Category = "Collectible")
	TSubclassOf<UCECollectibleViewModel> CollectibleViewModeClass;
	
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Collectible")
	TObjectPtr<UCECollectibleViewModel> CollectibleViewModel = nullptr;
	
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Collectible")
	FCECollectibleDataDef CollectibleDataRef;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Collectible")
	FCECollectiblePlayerAppraisalData AppraisalData;
	
	UPROPERTY(SaveGame, BlueprintReadWrite, Category = "Collectible")
	TEnumAsByte<ECollectibleSpawnLocations> CurrentLocationType = UnsortedInventoryLocation;
	
	UFUNCTION(BlueprintCallable, Category = "Collectible")
	bool IsAppraised() const { return AppraisalData.IsAppraisalValid(); }

	UFUNCTION(BlueprintCallable, Category = "Collectible")
	void SetupViewModel();
	
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Collectible")
	FCECoinCoreDataRef CoinTemplateData;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "Collectible")
	FCECardCoreDataRef CardTemplateData;

	UFUNCTION(BlueprintCallable, Category = "Collectible")
	UTexture2D* GetIcon() const;
	
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "Collectible")
	FGameplayTag CoreTypeTag = FGameplayTag();
};
