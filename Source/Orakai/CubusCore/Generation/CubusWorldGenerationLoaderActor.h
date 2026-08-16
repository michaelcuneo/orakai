#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CubusCore/Generation/CubusTerrainCarving.h"
#include "CubusWorldGenerationLoaderActor.generated.h"

class UTexture2D;

UENUM(BlueprintType)
enum class ECubusGenerationLoaderStage : uint8
{
    WaitingForSeed UMETA(DisplayName="Waiting for Seed"),
    StructuralDEM UMETA(DisplayName="Structural DEM"),
    Drainage UMETA(DisplayName="Drainage"),
    TerrainCarving UMETA(DisplayName="Terrain Carving"),
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
 * This actor owns the current pre-voxel generation session. It creates real
 * 1 m structural DEM tiles, solves drainage region-by-region, then replaces
 * those structural tiles with their drainage-carved versions. A lightweight
 * top-down texture is updated from the same data so the loader can visualize
 * progress without rendering the full-resolution DEM as geometry.
 *
 * Density streaming, gameplay chunks and player spawning are intentionally not
 * touched here yet. Later erosion, morphology and voxel-cache stages can extend
 * this state machine without changing the loader-level Blueprint contract.
 */
UCLASS(BlueprintType, Blueprintable)
class ORAKAI_API ACubusWorldGenerationLoaderActor : public AActor
{
    GENERATED_BODY()

public:
    ACubusWorldGenerationLoaderActor();

    virtual void Tick(float DeltaSeconds) override;

    /** Begin a new deterministic generation session from one world seed. */
    UFUNCTION(BlueprintCallable, Category="Cubus|Generation")
    void StartGeneration(int32 InWorldSeed);

    /** Cancel the current session and return to the seed-entry state. */
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

    UFUNCTION(BlueprintPure, Category="Cubus|Generation|Diagnostics")
    int32 GetGeneratedTerrainTileCount() const { return TerrainTiles.Num(); }

    UFUNCTION(BlueprintPure, Category="Cubus|Generation|Diagnostics")
    int32 GetDrainageSegmentCount() const { return DrainageSegments.Num(); }

    /** Sample the latest generated DEM if its tile is already resident. */
    UFUNCTION(BlueprintCallable, Category="Cubus|Generation|Diagnostics")
    bool GetGeneratedHeightMeters(double WorldXmeters, double WorldYmeters, float& OutHeightMeters) const;

    UPROPERTY(BlueprintAssignable, Category="Cubus|Generation|Events")
    FCubusGenerationStageChanged OnStageChanged;

    UPROPERTY(BlueprintAssignable, Category="Cubus|Generation|Events")
    FCubusGenerationProgressChanged OnProgressChanged;

    UPROPERTY(BlueprintAssignable, Category="Cubus|Generation|Events")
    FCubusGenerationFinished OnGenerationFinished;

    /** Size of the authored DEM region, centred on world XY zero. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation", meta=(ClampMin="0.5", UIMin="1.0"))
    FVector2D GenerationSizeKm = FVector2D(4.0, 4.0);

    /** Number of expensive work items allowed per game-thread tick. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation", meta=(ClampMin="1", ClampMax="8"))
    int32 WorkItemsPerTick = 1;

    /** Square preview texture resolution. This does not change DEM resolution. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation|Preview", meta=(ClampMin="64", ClampMax="2048"))
    int32 PreviewTextureResolution = 512;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation|Preview")
    float PreviewMinimumElevationMeters = -300.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation|Preview")
    float PreviewMaximumElevationMeters = 2200.0f;

    /** 1 m DEM settings used by the real structural tiles. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation|Terrain", meta=(ClampMin="0.25"))
    float DEMSampleSpacingMeters = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation|Terrain", meta=(ClampMin="64.0"))
    float DEMTileSizeMeters = 512.0f;

    /** Coarser routing grid used only for hydrology topology. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation|Drainage", meta=(ClampMin="2.0"))
    float DrainageCellSizeMeters = 8.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Cubus|Generation|Drainage", meta=(ClampMin="0.001"))
    float StreamSourceAreaSquareKm = 0.20f;

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

    FBox2D GetGenerationBoundsMeters() const;
    void SetWorkProgress(int32 CompletedItems, int32 TotalItems);

    void EnsurePreviewTexture();
    void ClearPreview();
    void UploadPreview();
    void PaintTileToPreview(const FCubusTerrainRasterTile& Tile);
    void DrawDrainageSegmentsToPreview(const TArray<FCubusTerrainDrainageSegment>& Segments);
    FColor HeightToPreviewColor(float HeightMeters) const;
    FIntPoint WorldToPreviewPixel(const FVector2D& WorldMeters) const;
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

    TArray<FIntPoint> TerrainTileQueue;
    TArray<FIntPoint> DrainageRegionQueue;
    TMap<FIntPoint, FCubusTerrainRasterTile> TerrainTiles;
    TArray<FCubusTerrainDrainageSegment> DrainageSegments;

    UPROPERTY(Transient)
    TObjectPtr<UTexture2D> PreviewTexture = nullptr;

    TArray<FColor> PreviewPixels;
};
