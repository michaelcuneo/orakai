#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CubusCore/Data/CubusVegetationInstance.h"
#include "Tasks/Task.h"
#include "CubusCore/Vegetation/CubusVegetationTypes.h"
#include "CubusCore/Vegetation/CubusVegetationCatalog.h"
#include "CubusCore/Vegetation/CubusVegetationRenderer.h"
#include "CubusCore/Vegetation/CubusVegetationPlacement.h"

#include "CubusWorldVegetationActor.generated.h"

struct FCubusFarVegetationCellBuildResult
{
    FIntPoint CellCoordinate = FIntPoint::ZeroValue;
    int32 RefinementPass = 0;
    TArray<FCubusVegetationInstance> Trees;
};

struct FCubusFarVegetationCellBuild
{
    FIntPoint CellCoordinate = FIntPoint::ZeroValue;
    int32 RefinementPass = 0;
    UE::Tasks::TTask<FCubusFarVegetationCellBuildResult> Task;
};

class ACubusBlockWorldActor;
class UInstancedStaticMeshComponent;
class UInstancedSkinnedMeshComponent;
class UMaterialParameterCollection;
class USkeletalMeshComponent;
class USceneComponent;
class UObject;
struct FCubusVegetationInstance;

/**
 * World vegetation owner.
 *
 * The block world owns chunk residency. This actor owns the shared vegetation
 * render batches and the independent far-tree cells. Dense ground detail is a
 * separate local-only path and is not part of tree residency or tree budgets.
 */
UCLASS(
    BlueprintType,
    Blueprintable,
    ClassGroup = "Cubus",
    meta = (DisplayName = "Cubus World Vegetation")
)
class ORAKAI_API ACubusWorldVegetationActor : public AActor
{
    GENERATED_BODY()

public:
    ACubusWorldVegetationActor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    void ConfigureForWorld(ACubusBlockWorldActor* InBlockWorld);

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Vegetation")
    void RebuildWorldVegetation();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Vegetation")
    void ClearWorldVegetation();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Vegetation|Far Proxies")
    void BakeFarVegetationProxies();

    bool EnsureFarVegetationProxyAssets(bool bSaveGeneratedAssets);

    void InvalidateDynamicWindBridgeTargets();

    bool FindInteractiveTreeAlongRay(
        const FVector& TraceStart,
        const FVector& TraceEnd,
        float SelectionRadius,
        FIntVector& OutWorldVoxel
    );

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cubus|Components")
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation")
    TObjectPtr<ACubusBlockWorldActor> BlockWorld = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Rendering")
    bool bRenderWorldPlantBatches = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Rendering")
    bool bCastWorldPlantShadows = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Catalog")
    bool bAutoSeedCatalogDefaults = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Catalog")
    TArray<FCubusVegetationSpeciesCatalogEntry> SpeciesCatalog;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Far Proxies")
    FString FarProxyPackageRoot = TEXT("/Game/OrakaiGenerated/Vegetation/Far");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Families")
    bool bClusterTreeFamilies = true;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Families",
        meta = (EditCondition = "bClusterTreeFamilies", ClampMin = "4", UIMin = "6", UIMax = "32")
    )
    int32 TreeFamilyCellSizeVoxels = 12;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Families",
        meta = (EditCondition = "bClusterTreeFamilies", ClampMin = "0.0", ClampMax = "0.4")
    )
    float TreeFamilyCenterJitterFraction = 0.2f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Families",
        meta = (EditCondition = "bClusterTreeFamilies", ClampMin = "0.02", ClampMax = "0.4")
    )
    float MatureTreeCoreRadius = 0.16f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Families",
        meta = (EditCondition = "bClusterTreeFamilies", ClampMin = "0.1", ClampMax = "0.8")
    )
    float YoungTreeRingRadius = 0.42f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Families",
        meta = (EditCondition = "bClusterTreeFamilies", ClampMin = "0.2", ClampMax = "1.0")
    )
    float SaplingTreeRingRadius = 0.72f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Families",
        meta = (EditCondition = "bClusterTreeFamilies", ClampMin = "0.0", ClampMax = "0.3")
    )
    float TreeFamilyGrowthNoise = 0.08f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Runtime Scale",
        meta = (ClampMin = "0.01", UIMin = "0.25", UIMax = "2.0")
    )
    float GlobalPlantScaleMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Runtime Scale")
    bool bEnablePerTypeScaleOverrides = false;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Runtime Scale",
        meta = (EditCondition = "bEnablePerTypeScaleOverrides", ClampMin = "0.01", UIMin = "0.25", UIMax = "2.0")
    )
    float BroadleafScaleMultiplier = 1.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Runtime Scale",
        meta = (EditCondition = "bEnablePerTypeScaleOverrides", ClampMin = "0.01", UIMin = "0.25", UIMax = "2.0")
    )
    float ConiferScaleMultiplier = 1.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Runtime Scale",
        meta = (EditCondition = "bEnablePerTypeScaleOverrides", ClampMin = "0.01", UIMin = "0.25", UIMax = "2.0")
    )
    float ShrubScaleMultiplier = 1.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Runtime Scale",
        meta = (EditCondition = "bEnablePerTypeScaleOverrides", ClampMin = "0.01", UIMin = "0.25", UIMax = "2.0")
    )
    float GrassScaleMultiplier = 1.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Runtime Scale",
        meta = (EditCondition = "bEnablePerTypeScaleOverrides", ClampMin = "0.01", UIMin = "0.25", UIMax = "2.0")
    )
    float ReedsScaleMultiplier = 1.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Runtime Scale",
        meta = (EditCondition = "bEnablePerTypeScaleOverrides", ClampMin = "0.01", UIMin = "0.25", UIMax = "2.0")
    )
    float AlpineScaleMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Runtime Prune")
    bool bEnableHeightPruneFilter = false;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Runtime Prune",
        meta = (EditCondition = "bEnableHeightPruneFilter", ClampMin = "-1000000", ClampMax = "1000000", Units = "cm")
    )
    float PruneMinWorldZ = -100000.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Runtime Prune",
        meta = (EditCondition = "bEnableHeightPruneFilter", ClampMin = "-1000000", ClampMax = "1000000", Units = "cm")
    )
    float PruneMaxWorldZ = 100000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Runtime Randomization")
    bool bEnableRuntimeRandomization = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Runtime Randomization")
    int32 RuntimeRandomizationSeed = 1337;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Runtime Randomization",
        meta = (EditCondition = "bEnableRuntimeRandomization", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0")
    )
    float RandomPruneProbability = 0.15f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Runtime Randomization",
        meta = (EditCondition = "bEnableRuntimeRandomization", ClampMin = "0.01", UIMin = "0.5", UIMax = "1.5")
    )
    float RandomScaleJitterMin = 0.85f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Runtime Randomization",
        meta = (EditCondition = "bEnableRuntimeRandomization", ClampMin = "0.01", UIMin = "0.5", UIMax = "1.5")
    )
    float RandomScaleJitterMax = 1.2f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Runtime Randomization",
        meta = (EditCondition = "bEnableRuntimeRandomization", ClampMin = "0.0", ClampMax = "0.49", UIMin = "0.0", UIMax = "0.49")
    )
    float RandomPositionJitterVoxelFraction = 0.38f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Runtime Randomization",
        meta = (EditCondition = "bEnableRuntimeRandomization", ClampMin = "0.0", ClampMax = "180.0", UIMin = "0.0", UIMax = "90.0", Units = "deg")
    )
    float RandomYawJitterDegrees = 35.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Streaming",
        meta = (ClampMin = "0.1", Units = "s")
    )
    float RefreshInterval = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Far Streaming")
    bool bEnableFarVegetation = true;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Far Streaming",
        meta = (ClampMin = "1", ClampMax = "32")
    )
    int32 FarVegetationCellSizeChunks = 12;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Far Streaming",
        meta = (ClampMin = "1", ClampMax = "32")
    )
    int32 FarVegetationRadiusCells = 24;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Far Streaming",
        meta = (ClampMin = "2", ClampMax = "64")
    )
    int32 FarTreeSampleStrideVoxels = 3;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Far Streaming",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float FarTreeDensityScale = 1.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Far Streaming",
        meta = (ClampMin = "1", ClampMax = "16")
    )
    int32 MaxConcurrentFarVegetationBuilds = 8;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Far Streaming",
        meta = (ClampMin = "1", ClampMax = "16")
    )
    int32 MaxFarVegetationBuildStartsPerTick = 8;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Far Streaming",
        meta = (ClampMin = "0.1", ClampMax = "5.0", Units = "s")
    )
    float FarVegetationPublishInterval = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Far Streaming")
    bool bCastFarVegetationShadows = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Wind")
    bool bBridgeUdwToDynamicWind = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Wind")
    TSoftObjectPtr<UMaterialParameterCollection> DynamicWindCollectionOverride;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Wind")
    bool bBridgeUdwToGlobalFoliageActor = true;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Wind",
        meta = (ClampMin = "0.01")
    )
    float UdwWindIntensityMax = 10.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Wind",
        meta = (ClampMin = "0.0")
    )
    float GlobalFoliageWindSpeedMax = 50.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Wind",
        meta = (ClampMin = "0.1", UIMin = "0.5", UIMax = "3.0")
    )
    float GlobalFoliageWindResponseExponent = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Wind")
    bool bGlobalFoliageFlipWindDirection = false;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Wind",
        meta = (ClampMin = "-180.0", ClampMax = "180.0", Units = "deg")
    )
    float GlobalFoliageWindDirectionYawOffset = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Wind")
    bool bForceMegaplantFoliageMaterialOverride = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Vegetation|Diagnostics")
    int32 LoadedFarVegetationCellCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Vegetation|Diagnostics")
    int32 RenderedFarTreeCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Vegetation|Diagnostics")
    int32 LoadedChunkCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Vegetation|Diagnostics")
    int32 RenderedPlantCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Vegetation|Diagnostics")
    int64 PublishedPlacementHash = 0;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Placement",
        meta = (ClampMin = "0.0", ClampMax = "89.0", Units = "deg")
    )
    float MaximumTreeSlopeDegrees = 32.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Placement",
        meta = (ClampMin = "0.0", ClampMax = "89.0", Units = "deg")
    )
    float MaximumGrassSlopeDegrees = 30.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Ground Detail",
        meta = (ClampMin = "1", ClampMax = "16")
    )
    int32 GrassInstancesPerPlacement = 6;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Ground Detail",
        meta = (ClampMin = "0.0", Units = "cm")
    )
    float GrassScatterRadius = 80.0f;

private:
    /*
     * Retired hero/fallback and manual distance-policy values are deliberately
     * no longer reflected settings. They remain fixed here only until the dead
     * implementation branches are physically removed from the cpp file.
     * This prevents old Blueprint/level serialization from changing runtime
     * behavior while keeping this cleanup source-compatible in one commit.
     */
    static constexpr bool bEnableHeroSkeletalWindMode = false;
    static constexpr int32 MaxHeroSkeletalWindComponents = 0;
    static constexpr float HeroSkeletalWindMaxDistance = 0.0f;
    static constexpr float HeroRepresentationRefreshDistance = 500.0f;
    static constexpr bool bUseInstancedSkeletalFallbackBeyondHeroDistance = false;
    static constexpr int32 MaxInstancedSkeletalFallbackInstances = 0;
    static constexpr bool bUseHeroPveActorWindMode = false;

    static constexpr int32 PlantStartCullDistance = 0;
    static constexpr int32 PlantEndCullDistance = 0;
    static constexpr int32 MaximumRenderedPlants = 0;

    static constexpr bool bCullByCameraChunkRadius = false;
    static constexpr int32 CameraChunkHorizontalRadius = 0;
    static constexpr int32 CameraChunkVerticalRadius = 0;
    static constexpr float VegetationRecenterDistance = 3200.0f;

    /*
     * Far-tree overlap begins before the near streamed forest ends. The old
     * explicit outer cull is effectively disabled; far-cell residency is the
     * outer boundary.
     */
    static constexpr float FarVegetationInnerRadius = 10000.0f;
    static constexpr float FarVegetationEndCullDistance = 1000000000.0f;
    static constexpr int32 MaximumFarRenderedTrees = MAX_int32;

    UPROPERTY(Transient)
    TMap<int64, TObjectPtr<UInstancedStaticMeshComponent>> CatalogGrassBatchComponents;

    UPROPERTY(Transient)
    TMap<int64, TObjectPtr<UInstancedStaticMeshComponent>> CatalogStaticBatchComponents;

    UPROPERTY(Transient)
    TMap<int64, TObjectPtr<UInstancedStaticMeshComponent>> FarCatalogStaticBatchComponents;

    UPROPERTY(Transient)
    TMap<int64, TObjectPtr<UInstancedSkinnedMeshComponent>> CatalogSkeletalBatchComponents;

    UPROPERTY(Transient)
    TArray<TObjectPtr<USkeletalMeshComponent>> HeroSkeletalWindComponents;

    UPROPERTY(Transient)
    TArray<TObjectPtr<AActor>> HeroPveWindActors;

    UPROPERTY(Transient)
    TObjectPtr<AActor> CachedUltraDynamicWeatherActor = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<AActor> CachedGlobalFoliageActor = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialParameterCollection> CachedDynamicWindCollection = nullptr;

    FVector LastBridgedWindDirection = FVector::ZeroVector;
    float LastBridgedWindIntensity = -1.0f;

    TMap<FIntVector, uint32> PublishedChunkVegetationSignatures;
    uint32 PublishedVegetationSettingsHash = 0;

    FVector LastFullVegetationBuildCameraLocation = FVector::ZeroVector;
    bool bHasLastFullVegetationBuildCameraLocation = false;
    bool bPublishedVegetationBudgetSaturated = false;

    float TimeUntilRefresh = 0.0f;

    FIntPoint LastFarVegetationCentreCell = FIntPoint(MAX_int32, MAX_int32);
    TSet<FIntPoint> RequiredFarVegetationCells;
    TSet<FIntPoint> FarVegetationCellsBuilding;
    TArray<FIntPoint> PendingFarVegetationCells;
    TArray<FCubusFarVegetationCellBuild> FarVegetationBuilds;
    TMap<FIntPoint, TArray<FCubusVegetationInstance>> FarVegetationCellCache;
    TMap<FIntPoint, int32> FarVegetationCellRefinementPasses;
    int32 FarVegetationRefinementPass = 0;
    float TimeUntilFarVegetationPublish = 0.0f;
    bool bFarVegetationRenderDirty = false;

    FCubusVegetationCatalog VegetationCatalog;
    FCubusVegetationRenderer VegetationRenderer;
    FCubusVegetationPlacement VegetationPlacement;

    struct FCubusVegetationDistancePolicy
    {
        int32 NearStartCullDistance = 0;
        int32 NearEndCullDistance = 0;
        float FarInnerRadius = 0.0f;
        float NearResidentRadius = 0.0f;
    };

    FCubusVegetationDistancePolicy ResolveVegetationDistancePolicy(float VoxelSize) const;
    void ApplyVegetationDistancePolicy(float VoxelSize);

    void ResolveBlockWorld();
    void RefreshVegetationBatches();
    void UpdateDynamicWindBridge();
    void RefreshFarVegetationBatches();
    void UpdateFarVegetationStreaming(float DeltaSeconds);
    void PublishFarVegetation(const FVector& CameraLocation, float VoxelSize);
    void ClearFarVegetation();

    uint32 CalculateLoadedPlacementHash(int32& OutLoadedChunkCount) const;
    uint32 CalculateVegetationSettingsHash() const;
};
