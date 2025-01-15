// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "CommonGameViewportClient.h"
#include "DaCommonGameViewportClient.generated.h"

/**
 * 
 */
UCLASS()
class GAMEPLAYFRAMEWORK_API UDaCommonGameViewportClient : public UCommonGameViewportClient
{
	GENERATED_BODY()

public:
	UDaCommonGameViewportClient();

	virtual void Init(struct FWorldContext& WorldContext, UGameInstance* OwningGameInstance, bool bCreateNewAudioDevice = true) override;

};
