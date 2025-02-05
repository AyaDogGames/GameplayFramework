// Copyright Dream Awake Solutions LLC


#include "DaRenderUtilLibrary.h"

#include "Kismet/KismetRenderingLibrary.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/StaticMesh.h"
#include "Slate/SlateBrushAsset.h"

void UDaRenderUtilLibrary::SetupSceneCaptureForThumbnail(USceneCaptureComponent2D* SceneCapture, UStaticMeshComponent* MeshComp)
{
    if (!SceneCapture || !MeshComp)
    {
        return;
    }

    // Set the scene capture to focus on the mesh
    SceneCapture->ShowOnlyComponent(MeshComp);
    
    SceneCapture->CaptureSource = ESceneCaptureSource::SCS_BaseColor;
    SceneCapture->bCaptureEveryFrame = false;
    SceneCapture->bCaptureOnMovement = false;
    SceneCapture->FOVAngle = 90.f;
    
    // Set a solid background color (black by default)
    SceneCapture->CompositeMode = ESceneCaptureCompositeMode::SCCM_Overwrite;
    SceneCapture->TextureTarget->ClearColor = FLinearColor::Transparent;

    // Adjust the transform to frame the mesh
    FVector MeshBounds = MeshComp->Bounds.BoxExtent;
    FVector MeshOrigin = MeshComp->Bounds.Origin;

    // Calculate the camera distance
    float CameraDistance = MeshBounds.Size() * 1.1f; // Adjust multiplier as needed

    // Calculate the camera location
    FVector CameraLocation = MeshOrigin + FVector(CameraDistance, 0.f, 0.f); // Default to capturing from the +X axis

    // Calculate the camera rotation
    FRotator CameraRotation = (MeshOrigin - CameraLocation).Rotation();
    
    SceneCapture->SetWorldLocationAndRotation(CameraLocation, CameraRotation); // Adjust rotation as needed
}

UTextureRenderTarget2D* UDaRenderUtilLibrary::GenerateThumbnailWithRenderTarget(UStaticMeshComponent* MeshComp, const FVector2D& ImageSize, UObject* WorldContextObject)
{
    if (!MeshComp || !WorldContextObject)
    {
        return nullptr;
    }

    // Create a render target
    UTextureRenderTarget2D* RenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(
        WorldContextObject, ImageSize.X, ImageSize.Y, ETextureRenderTargetFormat::RTF_RGBA8);

    if (!RenderTarget)
    {
        return nullptr;
    }

    // Create a temporary actor to hold the scene capture component
    AActor* TempActor = WorldContextObject->GetWorld()->SpawnActor<AActor>();
    USceneCaptureComponent2D* SceneCapture = NewObject<USceneCaptureComponent2D>(TempActor);
    SceneCapture->AttachToComponent(TempActor->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
    SceneCapture->TextureTarget = RenderTarget;

    // Setup the scene capture
    SetupSceneCaptureForThumbnail(SceneCapture, MeshComp);

    UKismetRenderingLibrary::ClearRenderTarget2D(WorldContextObject, SceneCapture->TextureTarget, FLinearColor::Transparent);

    // Disable post-processing effects
    SceneCapture->PostProcessSettings.bOverride_AutoExposureMethod = true;
    SceneCapture->PostProcessSettings.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
    SceneCapture->PostProcessSettings.bOverride_AutoExposureBias = true;
    SceneCapture->PostProcessSettings.AutoExposureBias = 1.0f;
    SceneCapture->PostProcessSettings.bOverride_BloomIntensity = true;
    SceneCapture->PostProcessSettings.BloomIntensity = 0.0f;
    
    // Trigger a one-time capture
    SceneCapture->CaptureScene();

    // Clean up the temporary actor
    TempActor->Destroy();
    
    return RenderTarget;
}

USlateBrushAsset* UDaRenderUtilLibrary::CreateSlateBrushFromRenderTarget(UTextureRenderTarget2D* RenderTarget)
{
    if (!RenderTarget)
    {
        return nullptr;
    }

    USlateBrushAsset* BrushAsset = NewObject<USlateBrushAsset>();
    BrushAsset->Brush.SetResourceObject(RenderTarget);
    BrushAsset->Brush.ImageSize = FVector2D(RenderTarget->SizeX, RenderTarget->SizeY);
    return BrushAsset;
}

USlateBrushAsset* UDaRenderUtilLibrary::CreateSlateBrushFromMaterial(UMaterialInstanceDynamic* Material, const FVector2D& ImageSize)
{
    if (!Material)
    {
        return nullptr;
    }

    USlateBrushAsset* BrushAsset = NewObject<USlateBrushAsset>();
    BrushAsset->Brush.SetResourceObject(Material);
    BrushAsset->Brush.ImageSize = ImageSize; //FVector2D(256.f, 256.f); // Match your render target size
    return BrushAsset;
}

USlateBrushAsset* UDaRenderUtilLibrary::CreateSlateBrushFromTexture2D(UTexture2D* Image, const FVector2D& ImageSize)
{
    if (!Image)
    {
        return nullptr;
    }

    USlateBrushAsset* BrushAsset = NewObject<USlateBrushAsset>();
    BrushAsset->Brush.SetResourceObject(Image);
    BrushAsset->Brush.ImageSize = ImageSize; //FVector2D(256.f, 256.f); // Match your render target size
    return BrushAsset;
}