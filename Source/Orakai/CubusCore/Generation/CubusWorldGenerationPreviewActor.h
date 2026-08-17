#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CubusWorldGenerationPreviewActor.generated.h"

class ACubusWorldGenerationLoaderActor;
class UDirectionalLightComponent;
class UImage;
class UMaterialInterface;
class UProceduralMeshComponent;
class USceneCaptureComponent2D;
class USceneComponent;
class UTextureRenderTarget2D;

/**
 * Lightweight interactive 3D preview of the generated DEM.
 *
 * This actor never builds gameplay voxels or physics collision. It samples the
 * authoritative DEM into a small visual-only heightfield, captures that mesh to
 * a render target for UMG, and performs spawn picking directly against its cached
 * preview triangles instead of invoking Unreal collision/cooking.
 */
UCLASS(BlueprintType, Blueprintable, ClassGroup="Cubus", meta=(DisplayName="Cubus World Generation 3D Preview"))
class ORAKAI_API ACubusWorldGenerationPreviewActor : public AActor
{
    GENERATED_BODY()

public:
    ACubusWorldGenerationPreviewActor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    /** The generation loader to preview. If unset it is found automatically. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation|3D Preview")
    TObjectPtr<ACubusWorldGenerationLoaderActor> TargetLoader = nullptr;

    /** Coarse live visualization resolution; completely independent of gameplay terrain. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation|3D Preview", meta=(ClampMin="32", ClampMax="256"))
    int32 PreviewMeshResolution = 40;

    /** Render target size used by the UMG image. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation|3D Preview", meta=(ClampMin="256", ClampMax="2048"))
    int32 PreviewRenderResolution = 384;

    /** Maximum horizontal extent of the normalized preview model in Unreal units. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation|3D Preview", meta=(ClampMin="100.0", ClampMax="5000.0"))
    float PreviewHorizontalSize = 900.0f;

    /** Maximum displayed vertical relief. This is visual exaggeration only. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation|3D Preview", meta=(ClampMin="25.0", ClampMax="1000.0"))
    float PreviewVerticalRelief = 260.0f;

    /** Camera distance relative to PreviewHorizontalSize. Larger values show more empty margin around the whole model. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation|3D Preview", meta=(ClampMin="1.0", ClampMax="5.0"))
    float PreviewCameraDistanceMultiplier = 2.35f;

    /** Minimum delay between DEM resamples while the generator is changing. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation|3D Preview", meta=(ClampMin="0.02", ClampMax="2.0", Units="s"))
    float PreviewRefreshInterval = 0.50f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation|3D Preview")
    float OrbitDegreesPerPixel = 0.22f;

    /** Optional override. By default an Engine vertex-colour debug material is used. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation|3D Preview")
    TObjectPtr<UMaterialInterface> PreviewMaterial = nullptr;

    UFUNCTION(BlueprintPure, Category="Cubus|Generation|3D Preview", meta=(DisplayName="Get Generated Terrain 3D Preview Render Target"))
    UTextureRenderTarget2D* GetPreviewRenderTarget() const { return PreviewRenderTarget; }

    /** Assign the live render target directly to an existing UMG Image brush. */
    UFUNCTION(BlueprintCallable, Category="Cubus|Generation|3D Preview", meta=(DisplayName="Apply Generated Terrain 3D Preview To Image"))
    bool ApplyPreviewToImage(UImage* TargetImage);

    /** Orbit the displayed model. Intended to receive a UMG pointer CursorDelta. */
    UFUNCTION(BlueprintCallable, Category="Cubus|Generation|3D Preview", meta=(DisplayName="Rotate Generated Terrain 3D Preview"))
    void AddOrbitInput(FVector2D PointerDelta);

    UFUNCTION(BlueprintCallable, Category="Cubus|Generation|3D Preview", meta=(DisplayName="Reset Generated Terrain 3D Preview View"))
    void ResetOrbit();

    UFUNCTION(BlueprintCallable, Category="Cubus|Generation|3D Preview", meta=(DisplayName="Refresh Generated Terrain 3D Preview"))
    void RefreshPreviewNow();

    UFUNCTION(BlueprintPure, Category="Cubus|Generation|3D Preview")
    bool IsPreviewReady() const { return bHasRenderableTerrain; }

    /**
     * Convert a point on the rendered 3D preview to authoritative DEM preview UV.
     * Picking is done directly against cached visual triangles; no physics
     * collision or procedural-mesh collision cooking is ever required.
     */
    UFUNCTION(BlueprintCallable, Category="Cubus|Generation|Spawn", meta=(DisplayName="Set Generated Spawn From 3D Preview Position"))
    bool SetGeneratedSpawnFromPreviewPosition(
        FVector2D LocalPreviewPosition,
        FVector2D PreviewWidgetSize,
        FVector2D& OutPreviewUV,
        FVector2D& OutWorldMeters
    );

private:
    void ResolveLoader();
    void EnsureRenderTarget();
    void RebuildPreviewMesh();
    void CapturePreview();
    void ApplyOrbitTransform();
    bool BuildCaptureRay(FVector2D PreviewUV, FVector& OutOrigin, FVector& OutDirection) const;
    FLinearColor HeightColor(float NormalizedHeight) const;

    UPROPERTY(VisibleAnywhere, Category="Cubus|Generation|3D Preview")
    TObjectPtr<USceneComponent> Root = nullptr;

    UPROPERTY(VisibleAnywhere, Category="Cubus|Generation|3D Preview")
    TObjectPtr<USceneComponent> MeshPivot = nullptr;

    UPROPERTY(VisibleAnywhere, Category="Cubus|Generation|3D Preview")
    TObjectPtr<UProceduralMeshComponent> PreviewMesh = nullptr;

    UPROPERTY(VisibleAnywhere, Category="Cubus|Generation|3D Preview")
    TObjectPtr<USceneCaptureComponent2D> PreviewCapture = nullptr;

    UPROPERTY(VisibleAnywhere, Category="Cubus|Generation|3D Preview")
    TObjectPtr<UDirectionalLightComponent> PreviewLight = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UTextureRenderTarget2D> PreviewRenderTarget = nullptr;

    float PreviewYawDegrees = -28.0f;
    float PreviewPitchDegrees = 0.0f;
    float RefreshCountdown = 0.0f;
    float LastObservedOverallProgress = -1.0f;
    uint8 LastObservedStage = 255;
    bool bHasRenderableTerrain = false;
    FVector2D BuiltMeshDimensions = FVector2D(900.0, 900.0);

    /** Local-space visual geometry retained only for cheap click picking. */
    TArray<FVector> CachedPickVertices;
    TArray<int32> CachedPickTriangles;
};
