// Copyright Dream Awake Solutions LLC

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DaCharacterInterface.generated.h"

// This class does not need to be modified.
UINTERFACE(MinimalAPI, BlueprintType)
class UDaCharacterInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */

class GFCORE_API IDaCharacterInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:

	virtual int32 GetCharacterLevel();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	FVector GetProjectileSocketLocation();

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	UAnimMontage* GetAttackMontage();

	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void UpdateFacingTarget(const FVector& Target);

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void SaveProgress(const FName& CheckpointTag);
};
