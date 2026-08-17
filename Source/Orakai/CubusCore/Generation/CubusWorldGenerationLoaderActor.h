#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CubusCore/Generation/CubusTerrainCarving.h"
#include "CubusCore/Generation/CubusTerrainErosion.h"
#include "CubusCore/Generation/CubusTerrainDeposition.h"
#include "CubusCore/Generation/CubusGeneratedTerrainRuntime.h"
#include "CubusWorldGenerationLoaderActor.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class ECubusGenerationLoaderStage : uint8
{
    WaitingForSeed UMETA(DisplayName="Waiting for Seed"),
    StructuralDEM UMETA(DisplayName="Structural DEM"),
    Drainage UMETA(DisplayName="Drainage"),
    TerrainCarving UMETA(DisplayName="Terrain Carving"),
    Erosion UMETA(DisplayName="Coarse Erosion"),
    Deposition UMETA(DisplayName="Alluvial Deposition"),
    FineErosion UMETA(DisplayName="Fine Erosion"),
    Complete UMETA(DisplayName="Current Pipeline Complete"),
    Failed UMETA(DisplayName="Failed")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FCubusGenerationStageChanged,
    ECubusGenerationLoaderStage,
    NewStage
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
    FCubusGenerationProgressChanged,
    ECubusGenerationLoaderStage,
    Stage,
    float,
    StageProgress
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCubusGenerationFinished);

/**
 * Blueprint-facing orchestrator for the world-generation loader level.
 *
 * The loader owns the current pre-voxel generation session and exposes the real
 * generated DEM as a progressive top-down preview. Diagnostic overlays exist
 * only while their corresponding stage is running; they never become part of
 * the authored terrain.
 */
UCLASS(BlueprintType, Blueprintable)
class ORAKAI_API ACubusWorldGenerationLoaderActor : public AActor
{
    GENERATED_BODY()

public:
    ACubusWorldGenerationLoaderActor();

    virtual void Tick(float DeltaSeconds) override;

    UFUNCTION(BlueprintCallable, Category="Cubus|Generation")
    void StartGeneration(int32 InWorldSeed);

    UFUNCTION(BlueprintCallable, Category="Cubus|Generation")
    void CancelGeneration();

    UFUNCTION(BlueprintPure, Category="Cubus|Generation")
    ECubusGenerationLoaderStage GetGenerationStage() const { return Stage; }

    UFUNCTION(BlueprintPure, Category="Cubus|Generation")
    float GetStageProgress() const { return StageProgress; }

    UFUNCTION(BlueprintPure, Category="Cubus|Generation")
    float GetOverallProgress() const { return OverallProgress; }

    UFUNCTION(BlueprintPure, Category="Cubus|Generation")
    int32 GetWorldSeed() const { return WorldSeed; }

    UFUNCTION(BlueprintPure, Category="Cubus|Generation")
    FText GetStageDisplayName() const;

    UFUNCTION(BlueprintPure, Category="Cubus|Generation|Preview")
    UTexture2D* GetPreviewTexture() const { return PreviewTexture; }

    /** Preserve the final DEM preview across OpenLevel for the runtime spawn picker. */
    UFUNCTION(BlueprintCallable, Category="Cubus|Generation|Preview", meta=(DisplayName="Publish Generated DEM Preview"))
    void PublishPreviewSnapshotToRuntime()
    {
        FCubusGeneratedTerrainRuntime::StorePreviewSnapshot(
            FMath::Clamp(PreviewTextureResolution, 64, 2048),
            GetGenerationBoundsMeters(),
            PreviewPixels
        );
    }

    /**
     * Set the proposed spawn from a normalized point on the DEM preview.
     * PreviewUV.X and PreviewUV.Y are expected in the 0..1 range where
     * (0,0) is the top-left of the preview image and (1,1) is bottom-right.
     */
    UFUNCTION(BlueprintCallable, Category="Cubus|Generation|Spawn", meta=(DisplayName="Set Generated Spawn From Preview UV"))
    bool SetGeneratedSpawnFromPreviewUV(FVector2D PreviewUV, FVector2D& OutWorldMeters)
    {
        if (Stage != ECubusGenerationLoaderStage::Complete)
        {
            OutWorldMeters = FVector2D::ZeroVector;
            return false;
        }

        FCubusGeneratedTerrainRuntime::SetProposedSpawnFromPreviewUV(PreviewUV);
        return FCubusGeneratedTerrainRuntime::GetProposedSpawnWorldMeters(OutWorldMeters);
    }

    /** Convenience node for UMG: pass the mouse/marker local position and image size directly. */
    UFUNCTION(BlueprintCallable, Category="Cubus|Generation|Spawn", meta=(DisplayName="Set Generated Spawn From Preview Position"))
    bool SetGeneratedSpawnFromPreviewPosition(
        FVector2D LocalPreviewPosition,
        FVector2D PreviewSize,
        FVector2D& OutPreviewUV,
        FVector2D& OutWorldMeters
    )
    {
        if (PreviewSize.X <= UE_SMALL_NUMBER || PreviewSize.Y <= UE_SMALL_NUMBER)
        {
            OutPreviewUV = FVector2D::ZeroVector;
            OutWorldMeters = FVector2D::ZeroVector;
            return false;
        }

        OutPreviewUV = FVector2D(
            FMath::Clamp(LocalPreviewPosition.X / PreviewSize.X, 0.0, 1.0),
            FMath::Clamp(LocalPreviewPosition.Y / PreviewSize.Y, 0.0, 1.0)
        );
        return SetGeneratedSpawnFromPreviewUV(OutPreviewUV, OutWorldMeters);
    }

    UFUNCTION(BlueprintPure, Category="Cubus|Generation|Spawn", meta=(DisplayName="Get Proposed Generated Spawn"))
    bool GetProposedGeneratedSpawn(FVector2D& OutWorldMeters) const
    {
        return FCubusGeneratedTerrainRuntime::GetProposedSpawnWorldMeters(OutWorldMeters);
    }

    UFUNCTION(BlueprintCallable, Category="Cubus|Generation|Spawn", meta=(DisplayName="Confirm Generated Spawn"))
    bool ConfirmGeneratedSpawn(FVector2D& OutWorldMeters)
    {
        if (!FCubusGeneratedTerrainRuntime::ConfirmProposedSpawn())
        {
            OutWorldMeters = FVector2D::ZeroVector;
            return false;
        }

        return FCubusGeneratedTerrainRuntime::GetConfirmedSpawnWorldMeters(OutWorldMeters);
    }

    UFUNCTION(BlueprintPure, Category="Cubus|Generation|Spawn", meta=(DisplayName="Has Confirmed Generated Spawn"))
    bool HasConfirmedGeneratedSpawn() const
    {
        return FCubusGeneratedTerrainRuntime::HasConfirmedSpawn();
    }

    UFUNCTION(BlueprintPure, Category="Cubus|Generation|Spawn", meta=(DisplayName="Get Confirmed Generated Spawn"))
    bool GetConfirmedGeneratedSpawn(FVector2D& OutWorldMeters) const
    {
        return FCubusGeneratedTerrainRuntime::GetConfirmedSpawnWorldMeters(OutWorldMeters);
    }

    UFUNCTION(BlueprintPure, Category="Cubus|Generation|Spawn", meta=(DisplayName="Get Confirmed Generated Spawn Height"))
    bool GetConfirmedGeneratedSpawnHeight(float& OutHeightMeters) const
    {
        return FCubusGeneratedTerrainRuntime::TryGetConfirmedSpawnSurfaceHeightMeters(OutHeightMeters);
    }

    UFUNCTION(BlueprintPure, Category="Cubus|Generation|Diagnostics")
    int32 GetGeneratedTerrainTileCount() const { return TerrainTiles.Num(); }

    UFUNCTION(BlueprintPure, Category="Cubus|Generation|Diagnostics")
    int32 GetDrainageSegmentCount() const { return DrainageSegments.Num(); }

    UFUNCTION(BlueprintCallable, Category="Cubus|Generation|Diagnostics")
    bool GetGeneratedHeightMeters(double WorldXmeters, double WorldYmeters, float& OutHeightMeters) const;

    UPROPERTY(BlueprintAssignable, Category="Cubus|Generation|Events")
    FCubusGenerationStageChanged OnStageChanged;

    UPROPERTY(BlueprintAssignable, Category="Cubus|Generation|Events")
    FCubusGenerationProgressChanged OnProgressChanged;

    UPROPERTY(BlueprintAssignable, Category="Cubus|Generation|Events")
    FCubusGenerationFinished OnGenerationFinished;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation")
    bool bTravelToGameplayWhenComplete = true;

    /** Gameplay map opened once the generated DEM has been published to runtime density. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation", meta=(EditCondition="bTravelToGameplayWhenComplete"))
    FName GameplayLevelName = TEXT("Lvl_ThirdPerson");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation", meta=(ClampMin="0.5", UIMin="1.0"))
    FVector2D GenerationSizeKm = FVector2D(4.0, 4.0);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation")
    int32 WorkItemsPerTick = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation|Preview", meta=(ClampMin="64", ClampMax="2048"))
    int32 PreviewTextureResolution = 768;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation|Preview")
    bool bPreviewHillshade = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation|Preview", meta=(ClampMin="0.0", ClampMax="1.0"))
    float PreviewHillshadeStrength = 0.72f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation|Terrain", meta=(ClampMin="0.25"))
    float DEMSampleSpacingMeters = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation|Terrain", meta=(ClampMin="64.0"))
    float DEMTileSizeMeters = 512.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation|Drainage", meta=(ClampMin="2.0"))
    float DrainageCellSizeMeters = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation|Drainage", meta=(ClampMin="0.001"))
    float StreamSourceAreaSquareKm = 0.20f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation|Erosion", meta=(ClampMin="1", ClampMax="12"))
    int32 ErosionIterations = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation|Erosion", meta=(ClampMin="5.0", ClampMax="60.0"))
    float ErosionTalusAngleDegrees = 33.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation|Deposition", meta=(ClampMin="1", ClampMax="8"))
    int32 DepositionIterations = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation|Fine Erosion", meta=(ClampMin="1", ClampMax="8"))
    int32 FineErosionIterations = 3;

protected:
    virtual void BeginPlay() override;

private:
    void ResetSession();
    void BuildSettings();
    void BuildWorkQueues();
    void BeginStage(ECubusGenerationLoaderStage NewStage);
    void FinishCurrentPipeline();

    void ProcessStructuralDEM();
    void ProcessDrainage();
    void ProcessTerrainCarving();
    void ProcessErosion();
    void ProcessDeposition();
    void ProcessFineErosion();

    FBox2D GetGenerationBoundsMeters() const;
    void SetWorkProgress(int32 CompletedItems, int32 TotalItems);

    void EnsurePreviewTexture();
    void ClearPreview();
    void UploadPreview();
    void PaintTileToPreview(const FCubusTerrainRasterTile& Tile);
    void RebuildTerrainPreview(bool bOverlayDrainage);
    void DrawDrainageSegmentsToPreview(const TArray<FCubusTerrainDrainageSegment>& Segments);
    FColor HeightToPreviewColor(float NormalizedHeight, float Hillshade) const;
    FIntPoint WorldToPreviewPixel(int32 PixelX, int32 PixelY) const;
    FVector2D PreviewPixelToWorld(int32 PixelX, int32 PixelY) const;
    void DrawPreviewLine(FIntPoint Start, FIntPoint End, const FColor& Color, int32 RadiusPixels);

    ECubusGenerationLoaderStage Stage = ECubusGenerationLoaderStage::WaitingForSeed;
    float StageProgress = 0.0f;
    float OverallProgress = 0.0f;
    int32 WorldSeed = 0;
    int32 CurrentWorkIndex = 0;

    FCubusTerrainRasterSettings RasterSettings;
    FCubusTerrainDrainageSettings DrainageSettings;
    FCubusTerrainCarvingSettings CarvingSettings;
    FCubusTerrainErosionSettings ErosionSettings;
    FCubusTerrainDepositionSettings DepositionSettings;
    FCubusTerrainErosionSettings FineErosionSettings;

    TArray<FIntPoint> TerrainTileQueue;
    TArray<FIntPoint> DrainageRegionQueue;
    TMap<FIntPoint, FCubusTerrainRasterTile> TerrainTiles;
    TArray<FCubusTerrainDrainageSegment> DrainageSegments;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> PreviewTexture = nullptr;

    TArray<float> PreviewHeightMeters;
    TArray<FColor> PreviewPixels;
};
