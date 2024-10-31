// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "DaAIController.generated.h"

class UBehaviorTree;

UCLASS()
class GAMEPLAYFRAMEWORK_API ADaAIController : public AAIController
{
	GENERATED_BODY()

protected:
	
	UPROPERTY(EditDefaultsOnly, Category="AI")
	TObjectPtr<UBehaviorTree> BehaviorTree;
	
	virtual void BeginPlay() override;
	
};
