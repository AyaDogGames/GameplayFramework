// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayFramework.h"

#define LOCTEXT_NAMESPACE "FGameplayFrameworkModule"

DEFINE_LOG_CATEGORY(DA_GameplayFramework);

void FGameplayFrameworkModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	LOG_WARNING("Gameplay framework module loaded.");
}

void FGameplayFrameworkModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FGameplayFrameworkModule, GameplayFramework)