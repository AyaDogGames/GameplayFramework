// Copyright Dream Awake Solutions LLC. All Rights Reserved.

#include "DaProcGenModule.h"

#define LOCTEXT_NAMESPACE "FDaProcGenModule"

DEFINE_LOG_CATEGORY(DA_ProcGen);

void FDaProcGenModule::StartupModule()
{
	UE_LOG(DA_ProcGen, Log, TEXT("DaProcGen module loaded."));
}

void FDaProcGenModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDaProcGenModule, DaProcGen)
