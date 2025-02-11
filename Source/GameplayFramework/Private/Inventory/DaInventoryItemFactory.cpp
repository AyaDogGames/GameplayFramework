// Copyright Dream Awake Solutions LLC


#include "Inventory/DaInventoryItemFactory.h"

#include "DaItemActor.h"
#include "DaPickup_Ability.h"
#include "DaRenderUtilLibrary.h"
#include "GameplayFramework.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Inventory/DaStackableInventoryItem.h"

static TAutoConsoleVariable<bool> CVarDebugRenderIconToDisk(TEXT("da.InventoryItemIconToDisk"), false, TEXT("Enable Dumping Generated Icon Textures to file at: C:/Temp/RenderTargetOutput.png."), ECVF_Cheat);

TSubclassOf<UDaInventoryItemBase> UDaBaseInventoryItemFactory::DetermineInventoryItemClassFromTags(
	const FGameplayTagContainer& Tags) const
{
	if (Tags.IsValid())
	{
		if (Tags.HasTagExact(CoreGameplayTags::InventoryItem_Stackable))
		{
			return UDaStackableInventoryItem::StaticClass();
		}

		if (Tags.HasTagExact(CoreGameplayTags::InventoryItem_Equipable))
		{
			// TODO: make an equipableItem class
			return UDaInventoryItemBase::StaticClass();
		}
		
		if (Tags.HasTagExact(CoreGameplayTags::InventoryItem))
		{
			return UDaInventoryItemBase::StaticClass(); 
		}
	}

	return nullptr;
}

// Add default functionality here for any IDaInventoryItemFactory functions that are not pure virtual.
TSubclassOf<UDaInventoryItemBase> UDaBaseInventoryItemFactory::DetermineInventoryItemClass(
	const UObject* SourceObject) const
{
	if (const IDaInventoryItemInterface* Pickup = Cast<IDaInventoryItemInterface>(SourceObject))
	{
		FGameplayTagContainer Tags;
		IDaInventoryItemInterface::Execute_GetItemTags(SourceObject, Tags);
		return DetermineInventoryItemClassFromTags(Tags);
	}

	return nullptr;
}

UDaInventoryItemBase* UDaBaseInventoryItemFactory::CreateInventoryItem(const UObject* SourceObject) const
{
	bool bDebugDraw = CVarDebugRenderIconToDisk.GetValueOnGameThread();

	if (const IDaInventoryItemInterface* Pickup = Cast<IDaInventoryItemInterface>(SourceObject))
	{
		FDaInventoryItemData Data;
		Data.ItemName = IDaInventoryItemInterface::Execute_GetItemName(SourceObject);
		Data.ItemDescription = IDaInventoryItemInterface::Execute_GetItemDescription(SourceObject);
		Data.AbilitySetToGrant = IDaInventoryItemInterface::Execute_GetAbilitySet(SourceObject);
		Data.ItemClass = DetermineInventoryItemClass(SourceObject);
		int32 NumTags = IDaInventoryItemInterface::Execute_GetItemTags(SourceObject, Data.Tags);
		
		if (!FGuid::Parse(Data.ItemName.ToString(), Data.ItemID))
		{
			LOG_WARNING("InventoryItemFactory::CreateInventoryItem: Failed to create FGuid ItemID");
		}
		
		FVector2d ImageSize = FVector2D(64, 64);
		if (UTexture2D* Image = IDaInventoryItemInterface::Execute_GetItemThumbnail(SourceObject))
		{
			// First check if there is already a thumbnail and if so, just use that and early out
			Data.ThumbnailBrush = UDaRenderUtilLibrary::CreateSlateBrushFromTexture2D(Image, ImageSize);
		}
		else
		{
			UTextureRenderTarget2D* ThumbnailRT = UDaRenderUtilLibrary::GenerateThumbnailWithRenderTarget(IDaInventoryItemInterface::Execute_GetMeshComponent(SourceObject), ImageSize, SourceObject->GetWorld());
			if (ThumbnailRT)
			{
				if (bDebugDraw)
				{
					FTextureRenderTargetResource* RTResource = ThumbnailRT->GameThread_GetRenderTargetResource();
					if (!RTResource)
					{
						LOG_ERROR("Render target resource is invalid.");
					}
					UKismetRenderingLibrary::ExportRenderTarget(SourceObject->GetWorld(), ThumbnailRT, TEXT("C:/Temp/"), TEXT("RenderTargetOutput.png"));
				}

				// check and see if there is a material set to create the thumbnail, and if not fallback to just using the render target directly
				USlateBrushAsset* ThumbnailBrush = nullptr;
				if (UMaterialInterface* BaseMaterial = IDaInventoryItemInterface::Execute_GetRenderTargetMaterial(SourceObject))
				{
					//UMaterialInterface* BaseMaterial = LoadObject<UMaterialInterface>(SourceObject->GetWorld(), TEXT("Material'/GameplayFramework/Materials/M_RenderTextureIcon.M_RenderTextureIcon'"));
					// Create a dynamic material instance
					UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, SourceObject->GetWorld());
					if (DynamicMaterial)
					{
						// Assign the render target to the material
						DynamicMaterial->SetTextureParameterValue(FName("DynamicTexture"), ThumbnailRT);
						ThumbnailBrush = UDaRenderUtilLibrary::CreateSlateBrushFromMaterial(DynamicMaterial, ImageSize);
					}
				}

				if (ThumbnailBrush == nullptr)
				{
					// Material method didnt work so fallback to rendertarget
					ThumbnailBrush = UDaRenderUtilLibrary::CreateSlateBrushFromRenderTarget(ThumbnailRT);
				}
			
				if (ThumbnailBrush)
				{
					Data.ThumbnailBrush = ThumbnailBrush;
				}
				else
				{
					LOG_WARNING("InventoryItemFactory::CreateInventoryItem: Failed to create ThumbnailBrush");
				}
			} else
			{
				LOG_WARNING("InventoryItemFactory::CreateInventoryItem: Failed to create ThumbnailRT");
			}
		}

		// Finally Create the Item and Save its source object					
		UDaInventoryItemBase* Item = UDaInventoryItemBase::CreateFromData(Data);
		Item->SetBaseObject(SourceObject);
		
		return Item;
	}

	return nullptr;
}
