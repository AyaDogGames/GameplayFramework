// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "Modules/ModuleManager.h"

// Define alias to easily keep track of custom channels in C++ (must match what is specified in Project Settings > Collision 
#define COLLISION_PROJECTILE ECC_GameTraceChannel1
#define TRACE_INTERACT ECC_GameTraceChannel2

DECLARE_STATS_GROUP(TEXT("DA_GameplayFramework"), STATGROUP_DAGF, STATCAT_Advanced);

GAMEPLAYFRAMEWORK_API DECLARE_LOG_CATEGORY_EXTERN(DA_GameplayFramework, Log, All);

// custom log macro
#define LOG(x, ...) UE_LOG(DA_GameplayFramework, Log, TEXT(x), ##__VA_ARGS__)
#define LOG_WARNING(x, ...) UE_LOG(DA_GameplayFramework, Warning, TEXT(x), ##__VA_ARGS__)
#define LOG_ERROR(x, ...) UE_LOG(DA_GameplayFramework, Error, TEXT(x), ##__VA_ARGS__)

// maps static delegates or functions to a static C++ function pointers.
// example: TStaticFuncPtr<float(int32, float)> FuncPtr;
//			static float Foo(int32 i, float f) { return i*f; }
//			FuncPtr = Foo;
//			float val = FuncPtr(4.f, 3.14f);
template<class T>
using TStaticFuncPtr = typename TBaseStaticDelegateInstance<T, FDefaultDelegateUserPolicy>::FFuncPtr;

static void LogOnScreen(UObject* WorldContext, FString Msg, bool ConsoleLogAlso = true, FColor Color = FColor::White, float Duration = 5.0f, int32 Key = -1)
{
	if (!ensure(WorldContext))
	{
		return;
	}

	UWorld* World = WorldContext->GetWorld();
	if (!ensure(World))
	{
		return;
	}

	FString NetPrefix = World->IsNetMode(NM_Client) ? "[CLIENT] " : "[SERVER] ";
	FString FinalMsg = NetPrefix + Msg;
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(Key, Duration, Color, FinalMsg);
	}

	if (ConsoleLogAlso)
	{
		LOG("%s", *FinalMsg);
	}
}

class FGameplayFrameworkModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
