#include "Collectibles.h"

#define LOCTEXT_NAMESPACE "FCollectiblesModule"

void FCollectiblesModule::StartupModule()
{
	UE_LOG(LogTemp, Warning, TEXT("Collectibles submodule Loaded."));
}

void FCollectiblesModule::ShutdownModule()
{

}

#undef LOCTEXT_NAMESPACE
    
IMPLEMENT_MODULE(FCollectiblesModule, Collectibles)