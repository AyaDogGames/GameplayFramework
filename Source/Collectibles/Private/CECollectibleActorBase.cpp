// Fill out your copyright notice in the Description page of Project Settings.

#include "CECollectibleActorBase.h"

#include "CoreGameplayTags.h"
#include "DaCharacter.h"
#include "DaInspectableComponent.h"
#include "DaPlayerState.h"
#include "Inventory/DaInventoryComponent.h"
#include "UI/DaCommonUIExtensions.h"

#define LOCTEXT_NAMESPACE "CollectibleItems"

// Sets default values
ACECollectibleActorBase::ACECollectibleActorBase()
{
	// Set this actor to call Tick() every frame
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	RootSceneComp = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComp"));
	RootComponent = RootSceneComp;
	
	PreviewMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PreviewMesh"));
	PreviewMeshComp->SetupAttachment(RootComponent);
	PreviewMeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	DetailMeshComp = CreateDefaultSubobject<UStaticMeshComponent>("DetailedMesh");
	DetailMeshComp->SetupAttachment(RootComponent);
	DetailMeshComp->SetVisibility(false);
	DetailMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	InspectableComp = CreateDefaultSubobject<UDaInspectableComponent>("InspectableComponent");
}

// Called when the game starts or when spawned
void ACECollectibleActorBase::BeginPlay()
{
	Super::BeginPlay();
}

// ICECollectibleItemInterface Implementation
int32 ACECollectibleActorBase::GetItemCoreTags_Implementation(FGameplayTagContainer& OutItemTags) const
{
	if (CollectibleData && CollectibleData->CollectibleDataRef.CoreTag.GetTagName().IsValid())
	{
		OutItemTags.AddTag(CollectibleData->CollectibleDataRef.CoreTag);
	}

	return OutItemTags.Num();
}

USlateBrushAsset* ACECollectibleActorBase::GetItemBrush_Implementation() const
{
	// TODO: Implement getting item brush
	return nullptr;
}

UTexture2D* ACECollectibleActorBase::GetItemIcon_Implementation() const
{
	if (CollectibleData)
	{
		return CollectibleData->GetIcon();
	}
	return nullptr;
}

FCECollectibleDataDef ACECollectibleActorBase::GetDataRef_Implementation() const
{
	return CollectibleData ? CollectibleData->CollectibleDataRef : FCECollectibleDataDef();
}

void ACECollectibleActorBase::SetCollectibleData_Implementation(UCECollectibleData* Data)
{
	CollectibleData = Data;

	// In Blueprint we were subclassing the CECollectibleViewModel and casting it and storing a pointer here.
	// TODO: Move all ViewModel code to C++ and dont cast.

	if (!CollectibleData->IsAppraised())
	{
		PreviewMeshComp->SetCustomDepthStencilValue(DepthStencilValue_NotAppraised);
		PreviewMeshComp->SetRenderCustomDepth(true);
	}
}

FString ACECollectibleActorBase::GetCollectibleDisplayNames_Implementation(bool bPlayerDesignated)
{
	// TODO: Implement getting collectible display names
	return FString();
}

void ACECollectibleActorBase::SaveCollectible_Implementation(bool bSaveToDisk)
{
	// TODO: Implement saving collectible
}

// IDaInventoryItemInterface Implementation
FName ACECollectibleActorBase::GetItemName_Implementation() const
{
	if (CollectibleData)
	{
		return CollectibleData->CollectibleDataRef.Name;
	}
	return FName();
}

FName ACECollectibleActorBase::GetItemDescription_Implementation() const
{
	if (CollectibleData)
	{
		return CollectibleData->CollectibleDataRef.Name;
	}
	return FName();
}

int32 ACECollectibleActorBase::GetItemTags_Implementation(FGameplayTagContainer& OutItemTags) const
{
	if (CollectibleData && CollectibleData->CollectibleDataRef.CoreTag.GetTagName().IsValid())
	{
		OutItemTags.AddTag(CollectibleData->CollectibleDataRef.CoreTag);
	}
	return OutItemTags.Num();
}

UTexture2D* ACECollectibleActorBase::GetItemThumbnail_Implementation() const
{
	if (CollectibleData)
	{
		return CollectibleData->GetIcon();
	}
	return nullptr;
}

UMaterialInterface* ACECollectibleActorBase::GetRenderTargetMaterial_Implementation() const
{
	return RenderTargetMaterial;
}

UStaticMeshComponent* ACECollectibleActorBase::GetMeshComponent_Implementation() const
{
	return PreviewMeshComp;
}

UDaAbilitySet* ACECollectibleActorBase::GetAbilitySet_Implementation() const
{
	// TODO: Implement getting ability set
	return nullptr;
}

void ACECollectibleActorBase::AddToInventory_Implementation(APawn* InstigatorPawn, bool bDestroyActor)
{
	if (APlayerState* PS = InstigatorPawn->GetPlayerState())
	{
		if (UDaInventoryComponent* InventoryComponent = Cast<ADaPlayerState>(PS)->GetInventoryComponent())
		{
			if (InventoryComponent->AddItem(this))
			{
				Execute_ItemAddedToInventory(this);
			}
		}
	}
}

// IDaInspectableInterface Implementation
void ACECollectibleActorBase::OnInspectionStarted_Implementation(APawn* InstigatorPawn)
{
	UStaticMeshComponent* MeshComp = GetPhysicsMesh();
	if (MeshComp)
	{
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}
	
	APlayerController* PlayerController = Cast<APlayerController>(InstigatorPawn->GetController());
	AppraisalWidget = UDaCommonUIExtensions::PushContentToLayer_ForPlayer(PlayerController, CoreGameplayTags::TAG_UI_Layer_Game, AppraisalWidgetClass);

}

void ACECollectibleActorBase::OnInspectionEnded_Implementation(APawn* InstigatorPawn)
{
	UStaticMeshComponent* MeshComp = GetPhysicsMesh();
	if (MeshComp)
	{
		MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	
	UDaCommonUIExtensions::PopContentFromLayer(AppraisalWidget);
}

UStaticMeshComponent* ACECollectibleActorBase::GetPreviewMeshComponent_Implementation() const
{
	return PreviewMeshComp;
}

UStaticMeshComponent* ACECollectibleActorBase::GetDetailedMeshComponent_Implementation() const
{
	return DetailMeshComp;
}

void ACECollectibleActorBase::Interact_Implementation(APawn* InstigatorPawn)
{
	if (InspectableComp == nullptr)
		return;
	
	if (InspectableComp->IsInspecting())
	{
		// If already inspecting, end inspection
		InspectableComp->HideDetailedMesh();
		return;
	}
	
	if (ADaCharacter* PlayerCharacter = Cast<ADaCharacter>(InstigatorPawn))
	{
		// Do this to give Gameplay Abilities access to the currently inspected item through the avatar or owning actor 
		PlayerCharacter->SetInspectedItem(this);
	}
	
	InspectableComp->Inspect(InstigatorPawn);
	
}

void ACECollectibleActorBase::SecondaryInteract_Implementation(APawn* InstigatorPawn)
{
	if (!InspectableComp->IsInspecting() && !bIsMoving)
	{
		Execute_AddToInventory(this, InstigatorPawn, true);
	}
}

FText ACECollectibleActorBase::GetInteractText_Implementation(APawn* InstigatorPawn)
{
	return FText::Format(LOCTEXT("Collectible InteractText", "{0}"), FText::FromName(CollectibleData->CollectibleDataRef.Name));
}

void ACECollectibleActorBase::HighlightActor_Implementation()
{
	if (CollectibleData && CollectibleData->IsAppraised())
	{
		PreviewMeshComp->SetCustomDepthStencilValue(DepthStencilValue_AppraisedHover);
	}
	else
	{
		PreviewMeshComp->SetCustomDepthStencilValue(DepthStencilValue_NotAppraisedHover);
	}
	
	PreviewMeshComp->SetRenderCustomDepth(true);
}

void ACECollectibleActorBase::UnHighlightActor_Implementation()
{
	if (CollectibleData && CollectibleData->IsAppraised())
	{
		if (!bIsMoving)
			PreviewMeshComp->SetRenderCustomDepth(false);
	}
	else
	{
		PreviewMeshComp->SetCustomDepthStencilValue(DepthStencilValue_NotAppraised);
	}
}

#undef LOCTEXT_NAMESPACE
