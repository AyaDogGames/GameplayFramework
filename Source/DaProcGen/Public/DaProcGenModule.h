// Copyright Dream Awake Solutions LLC. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * Log category for the procedural-placement module. Kept separate from DA_GameplayFramework so a
 * project can quiet (or verbose) generation chatter without touching the rest of the framework.
 */
DAPROCGEN_API DECLARE_LOG_CATEGORY_EXTERN(DA_ProcGen, Log, All);

class FDaProcGenModule : public IModuleInterface
{
public:
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
