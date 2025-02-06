// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "DaInventoryItemBase.generated.h"

class USlateBrushAsset;
class IDaInventoryItemFactory;
class UDaAbilitySet;
class UDaInventoryComponent;

USTRUCT(BlueprintType)
struct FDaInventoryItemData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName ItemName = FName();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FName ItemDescription = FName();
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGuid ItemID = FGuid();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TSubclassOf<class UDaInventoryItemBase> ItemClass = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTagContainer Tags = FGameplayTagContainer();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<USlateBrushAsset> ThumbnailBrush = nullptr;
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TObjectPtr<UDaAbilitySet> AbilitySetToGrant = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 NestedInventorySize = 0;
	
	UPROPERTY(BlueprintReadOnly)
	int32 ItemCount = 1;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryItemDataRemoved, const FDaInventoryItemData&, itemData);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnInventoryItemUpdated, UDaInventoryItemBase*, item);

/**
 * 
 */
UCLASS(Blueprintable, BlueprintType)
class GAMEPLAYFRAMEWORK_API UDaInventoryItemBase : public UObject
{
	GENERATED_BODY()

public:

	UDaInventoryItemBase();

	static UDaInventoryItemBase* CreateFromData(const FDaInventoryItemData& Data);
	
	FGameplayTagContainer GetTags() const { return InventoryItemTags; }

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Inventory")
	bool bIsEmptySlot = true;

	UPROPERTY(Replicated, BlueprintReadOnly, Category="Inventory")
	FName Name;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Inventory")
	FName Description;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Inventory")
	FGuid ItemID;
	
	virtual void PopulateWithData(const FDaInventoryItemData& Data);

	virtual FDaInventoryItemData ToData() const;

	virtual void ClearData();

	static TArray<TScriptInterface<IDaInventoryItemFactory>> Factories;

	// Delegate to notify listeners when inventory changes
	UPROPERTY(BlueprintAssignable, Category="Inventory")
	FOnInventoryItemDataRemoved OnInventoryItemRemoved;

	UPROPERTY(BlueprintAssignable, Category="Inventory")
	FOnInventoryItemUpdated OnInventoryItemUpdated;
	
protected:

	// Tags the item has if !bIsEmptySlot
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category="Inventory")
	FGameplayTagContainer InventoryItemTags;

	// Tags that define this inventory slot (and what can be put in it)
	UPROPERTY(Replicated, EditAnywhere, BlueprintReadWrite, Category="Inventory")
	FGameplayTagContainer SlotTags;

	UPROPERTY(EditDefaultsOnly, Category="Inventory")
	TObjectPtr<UDaAbilitySet> AbilitySetToGrant;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory")
	TObjectPtr<USlateBrushAsset> ThumbnailBrush;
	
	UPROPERTY(ReplicatedUsing="OnRep_NestedInventory", BlueprintReadOnly, Category = "Inventory")
	TObjectPtr<UDaInventoryComponent> NestedInventory;
	
	UFUNCTION(BlueprintNativeEvent, Category="Inventory")
	void OnRep_NestedInventory();
	
public:

	UFUNCTION(BlueprintCallable, Category="AbilitySystem")
	FGameplayTag GetType() const;
	
	UFUNCTION(BlueprintCallable, Category = "Inventory")
	virtual bool CanMergeWith(const UDaInventoryItemBase* OtherItem) const;

	UFUNCTION(BlueprintCallable, Category = "Inventory")
	virtual void MergeWith(UDaInventoryItemBase* OtherItem);
	
	// Tries to activate an ability on this inventory item with the tag InventoryItem.EquipAbility
	UFUNCTION(BlueprintCallable, Category="AbilitySystem")
	void ActivateEquipAbility();

	UFUNCTION(BlueprintCallable, Category="AbilitySystem")
	void EndEquipAbility();
	
	// If the item itself can host an inventory
	UFUNCTION(BlueprintCallable, Category="Inventory")
	UDaInventoryComponent* GetNestedInventory() const;

	// This may or may not be valid if the object was destroyed when adding to inventory. Always check IsValid or not null before using.
	UFUNCTION(BlueprintCallable, Category="Inventory")
	UObject* GetBaseObject() const { return BaseObject.Get(); }
	
	void SetBaseObject(const UObject* Object) { BaseObject = const_cast<UObject*>(Object); }
	
protected:

	UPROPERTY(Transient)
	TWeakObjectPtr<UObject> BaseObject;
};