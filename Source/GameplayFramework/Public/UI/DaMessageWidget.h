// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DaMessageWidget.generated.h"

/**
 * 
 */
UCLASS()
class GAMEPLAYFRAMEWORK_API UDaMessageWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	
	void NativeSetImageAndText(UTexture2D* NewImage, const FText& NewText) { Image = NewImage; Text = NewText; };

protected:

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UTexture2D> Image;

	UPROPERTY(BlueprintReadOnly)
	FText Text;
	
};
