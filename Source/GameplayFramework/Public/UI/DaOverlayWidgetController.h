// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "DaWidgetController.h"
#include "GameplayTagContainer.h"

#include "DaOverlayWidgetController.generated.h"

USTRUCT(BlueprintType)
struct FUIWidgetRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag MessageTag = FGameplayTag();

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Message = FText();

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<class UDaUserWidgetBase> MessageWidget;
	
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UTexture2D* Image = nullptr;
};

class UDaWidgetMessageData;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMessageWidgetDataSignature, const FDaUIWidgetMessageData&, Data);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMessageWidgetRowSignature, FUIWidgetRow, Row);

/**
 * 
 */
UCLASS(BlueprintType, Blueprintable)
class GAMEPLAYFRAMEWORK_API UDaOverlayWidgetController : public UDaWidgetController
{
	GENERATED_BODY()
	
	UPROPERTY(BlueprintAssignable, Category="UI Messages")
	FMessageWidgetRowSignature MessageWidgetRowDelegate;

	UPROPERTY(BlueprintAssignable, Category="UI Messages")
	FMessageWidgetDataSignature MessageWidgetDataDelegate;
	
public:
	UDaOverlayWidgetController();
	
	virtual void BindCallbacksToDependencies() override;

protected:

	// Messages will be broadcast from datatable if they match the message parent tag, can use either MessageWidgetDataTable or MessageDataAsset
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI Data")
	TObjectPtr<UDataTable> MessageWidgetDataTable;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI Data")
	FGameplayTag MessageParentTag;

	// Messages will be broadcast from data asset if they match the message parent tag, can use either MessageWidgetDataTable or MessageDataAsset
	UPROPERTY(EditDefaultsOnly, Category="UI Data")
	TObjectPtr<UDaWidgetMessageData> MessageDataAsset;
	
	template<typename T>
	T* GetDataTableRowByTag(UDataTable* DataTable, const FGameplayTag& Tag);
	
};

template <typename T>
T* UDaOverlayWidgetController::GetDataTableRowByTag(UDataTable* DataTable, const FGameplayTag& Tag)
{
	return DataTable->FindRow<T>(Tag.GetTagName(), TEXT(""));
}
