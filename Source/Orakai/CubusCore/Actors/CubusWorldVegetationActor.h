#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "CubusWorldVegetationActor.generated.h"

class ACubusBlockWorldActor;
class UHierarchicalInstancedStaticMeshComponent;
class UInstancedSkinnedMeshComponent;
class UInstancedStaticMeshComponent;
class UMaterialParameterCollection;
class UPCGGraphInterface;
class USkeletalMeshComponent;
class USceneComponent;
class UObject;
class USkeletalMesh;
class UStaticMesh;
struct FCubusVegetationInstance;

UENUM(BlueprintType, meta = (Bitflags))
enum class ECubusVegetationBiome : uint8
{
    None = 0 UMETA(Hidden),
    Plains = 1 << 0,
    Forest = 1 << 1,
    Rocky = 1 << 2,
    Wetland = 1 << 3
};

USTRUCT(BlueprintType)
struct FCubusVegetationSpeciesCatalogEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Catalog")
    FName SpeciesId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Catalog")
    int32 TypeId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Catalog", meta = (ClampMin = "0.001"))
    float Weight = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Catalog", meta = (Bitmask, BitmaskEnum = "/Script/Orakai.ECubusVegetationBiome"))
    int32 BiomeMask = static_cast<int32>(ECubusVegetationBiome::Forest);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Catalog", meta = (AllowedClasses = "/Script/Engine.StaticMesh,/Script/Engine.SkeletalMesh"))
    TArray<TSoftObjectPtr<UObject>> GrowthStageMeshes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Catalog", meta = (AllowedClasses = "/Script/Engine.Actor"))
    TSoftClassPtr<AActor> HeroPveActorClassOverride;

    // Accepts data assets/blueprints that indirectly reference the runtime actor class.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Catalog")
    TSoftObjectPtr<UObject> HeroPveActorAssetOverride;
};

/**
 * One world-level vegetation owner for all currently streamed Cubus chunks.
 * Chunks only generate deterministic placement records; this actor owns the
 * fixed set of shared species batches used to render those records.
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

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    void ConfigureForWorld(ACubusBlockWorldActor* InBlockWorld);

    // Temporary source-compatibility overload for call sites compiled against
    // the removed PCG configuration API. The graph and bool are ignored.
    void ConfigureForWorld(
        ACubusBlockWorldActor* InBlockWorld,
        UPCGGraphInterface* InVegetationGraph,
        bool bInEnableRuntimeVegetation
    );

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Vegetation")
    void RebuildWorldVegetation();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Vegetation")
    void ClearWorldVegetation();

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cubus|Components")
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation")
    TObjectPtr<ACubusBlockWorldActor> BlockWorld = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Rendering")
    bool bRenderWorldPlantBatches = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Wind")
    bool bBridgeUdwToDynamicWind = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Wind")
    TSoftObjectPtr<UMaterialParameterCollection> DynamicWindCollectionOverride;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Wind")
    bool bEnableHeroSkeletalWindMode = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Wind")
    bool bBridgeUdwToGlobalFoliageActor = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Wind", meta = (ClampMin = "0.01"))
    float UdwWindIntensityMax = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Wind", meta = (ClampMin = "0.0"))
    float GlobalFoliageWindSpeedMax = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Wind", meta = (ClampMin = "0.1", UIMin = "0.5", UIMax = "3.0"))
    float GlobalFoliageWindResponseExponent = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Wind")
    bool bGlobalFoliageFlipWindDirection = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Wind", meta = (ClampMin = "-180.0", ClampMax = "180.0", Units = "deg"))
    float GlobalFoliageWindDirectionYawOffset = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Wind", meta = (EditCondition = "bEnableHeroSkeletalWindMode", ClampMin = "0"))
    int32 MaxHeroSkeletalWindComponents = 32;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Wind", meta = (EditCondition = "bEnableHeroSkeletalWindMode", ClampMin = "0", Units = "cm"))
    float HeroSkeletalWindMaxDistance = 10000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Wind", meta = (EditCondition = "bEnableHeroSkeletalWindMode"))
    bool bUseInstancedSkeletalFallbackBeyondHeroDistance = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Wind", meta = (EditCondition = "bEnableHeroSkeletalWindMode && bUseInstancedSkeletalFallbackBeyondHeroDistance", ClampMin = "0"))
    int32 MaxInstancedSkeletalFallbackInstances = 256;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Wind")
    bool bForceMegaplantFoliageMaterialOverride = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Wind", meta = (EditCondition = "bEnableHeroSkeletalWindMode"))
    bool bUseHeroPveActorWindMode = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Wind", meta = (EditCondition = "bEnableHeroSkeletalWindMode"))
    bool bForceHeroWindVisualSway = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Wind", meta = (EditCondition = "bEnableHeroSkeletalWindMode && bForceHeroWindVisualSway", ClampMin = "0.0", ClampMax = "25.0", Units = "deg"))
    float HeroWindSwayMaxDegrees = 6.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Wind", meta = (EditCondition = "bEnableHeroSkeletalWindMode && bForceHeroWindVisualSway", ClampMin = "0.01", ClampMax = "10.0"))
    float HeroWindSwayFrequency = 0.8f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Wind", meta = (EditCondition = "bEnableHeroSkeletalWindMode && bForceHeroWindVisualSway", ClampMin = "0.01", ClampMax = "4.0"))
    float HeroWindSwayIntensityScale = 0.35f;

    // DynamicWind foliage should participate in the world's dynamic shadows.
    // Disable only as an explicit performance tradeoff.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Rendering")
    bool bCastWorldPlantShadows = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Catalog")
    bool bAutoSeedCatalogDefaults = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Catalog")
    TArray<FCubusVegetationSpeciesCatalogEntry> SpeciesCatalog;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Families")
    bool bClusterTreeFamilies = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Families", meta = (EditCondition = "bClusterTreeFamilies", ClampMin = "4", UIMin = "6", UIMax = "32"))
    int32 TreeFamilyCellSizeVoxels = 12;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Families", meta = (EditCondition = "bClusterTreeFamilies", ClampMin = "0.0", ClampMax = "0.4"))
    float TreeFamilyCenterJitterFraction = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Families", meta = (EditCondition = "bClusterTreeFamilies", ClampMin = "0.02", ClampMax = "0.4"))
    float MatureTreeCoreRadius = 0.16f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Families", meta = (EditCondition = "bClusterTreeFamilies", ClampMin = "0.1", ClampMax = "0.8"))
    float YoungTreeRingRadius = 0.42f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Families", meta = (EditCondition = "bClusterTreeFamilies", ClampMin = "0.2", ClampMax = "1.0"))
    float SaplingTreeRingRadius = 0.72f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Families", meta = (EditCondition = "bClusterTreeFamilies", ClampMin = "0.0", ClampMax = "0.3"))
    float TreeFamilyGrowthNoise = 0.08f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Rendering", meta = (ClampMin = "0", Units = "cm"))
    int32 PlantStartCullDistance = 30000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Rendering", meta = (ClampMin = "0", Units = "cm"))
    int32 PlantEndCullDistance = 120000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Runtime Scale", meta = (ClampMin = "0.01", UIMin = "0.25", UIMax = "2.0"))
    float GlobalPlantScaleMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Runtime Scale")
    bool bEnablePerTypeScaleOverrides = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Runtime Scale", meta = (EditCondition = "bEnablePerTypeScaleOverrides", ClampMin = "0.01", UIMin = "0.25", UIMax = "2.0"))
    float BroadleafScaleMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Runtime Scale", meta = (EditCondition = "bEnablePerTypeScaleOverrides", ClampMin = "0.01", UIMin = "0.25", UIMax = "2.0"))
    float ConiferScaleMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Runtime Scale", meta = (EditCondition = "bEnablePerTypeScaleOverrides", ClampMin = "0.01", UIMin = "0.25", UIMax = "2.0"))
    float ShrubScaleMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Runtime Scale", meta = (EditCondition = "bEnablePerTypeScaleOverrides", ClampMin = "0.01", UIMin = "0.25", UIMax = "2.0"))
    float GrassScaleMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Runtime Scale", meta = (EditCondition = "bEnablePerTypeScaleOverrides", ClampMin = "0.01", UIMin = "0.25", UIMax = "2.0"))
    float ReedsScaleMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Runtime Scale", meta = (EditCondition = "bEnablePerTypeScaleOverrides", ClampMin = "0.01", UIMin = "0.25", UIMax = "2.0"))
    float AlpineScaleMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Runtime Prune")
    bool bEnableHeightPruneFilter = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Runtime Prune", meta = (EditCondition = "bEnableHeightPruneFilter", ClampMin = "-1000000", ClampMax = "1000000", Units = "cm"))
    float PruneMinWorldZ = -100000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Runtime Prune", meta = (EditCondition = "bEnableHeightPruneFilter", ClampMin = "-1000000", ClampMax = "1000000", Units = "cm"))
    float PruneMaxWorldZ = 100000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Runtime Randomization")
    bool bEnableRuntimeRandomization = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Runtime Randomization", meta = (EditCondition = "bEnableRuntimeRandomization"))
    int32 RuntimeRandomizationSeed = 1337;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Runtime Randomization", meta = (EditCondition = "bEnableRuntimeRandomization", ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float RandomPruneProbability = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Runtime Randomization", meta = (EditCondition = "bEnableRuntimeRandomization", ClampMin = "0.01", UIMin = "0.5", UIMax = "1.5"))
    float RandomScaleJitterMin = 0.85f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Runtime Randomization", meta = (EditCondition = "bEnableRuntimeRandomization", ClampMin = "0.01", UIMin = "0.5", UIMax = "1.5"))
    float RandomScaleJitterMax = 1.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Runtime Randomization", meta = (EditCondition = "bEnableRuntimeRandomization", ClampMin = "0.0", ClampMax = "0.49", UIMin = "0.0", UIMax = "0.49"))
    float RandomPositionJitterVoxelFraction = 0.38f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Runtime Randomization", meta = (EditCondition = "bEnableRuntimeRandomization", ClampMin = "0.0", ClampMax = "180.0", UIMin = "0.0", UIMax = "90.0", Units = "deg"))
    float RandomYawJitterDegrees = 35.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Rendering", meta = (ClampMin = "0"))
    int32 MaximumRenderedPlants = 4096;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Streaming", meta = (ClampMin = "0.1", Units = "s"))
    float RefreshInterval = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Streaming")
    bool bCullByCameraChunkRadius = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Streaming", meta = (ClampMin = "0", UIMax = "24", EditCondition = "bCullByCameraChunkRadius"))
    int32 CameraChunkHorizontalRadius = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Streaming", meta = (ClampMin = "0", UIMax = "12", EditCondition = "bCullByCameraChunkRadius"))
    int32 CameraChunkVerticalRadius = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Streaming", meta = (ClampMin = "0"))
    int32 MaximumPublishedPoints = 20000;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Debug")
    TObjectPtr<UStaticMesh> MarkerMesh = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Debug")
    bool bShowDebugMarkers = false;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Vegetation|Diagnostics")
    int32 LoadedChunkCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Vegetation|Diagnostics")
    int32 PublishedPointCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Vegetation|Diagnostics")
    int32 RenderedPlantCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Vegetation|Diagnostics")
    int64 PublishedPlacementHash = 0;

private:
    struct FCubusRuntimeRandomizationSample
    {
        bool bPruned = false;
        float ScaleMultiplier = 1.0f;
        FVector2f PositionJitterUnit = FVector2f::ZeroVector;
        float YawJitterUnit = 0.0f;
    };

    UPROPERTY(Transient)
    TObjectPtr<UInstancedStaticMeshComponent> GrassPoints = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UInstancedStaticMeshComponent> ShrubPoints = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UInstancedStaticMeshComponent> TreePoints = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UInstancedStaticMeshComponent> ConiferTreePoints = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UInstancedStaticMeshComponent> ReedsPoints = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UInstancedStaticMeshComponent> AlpinePoints = nullptr;

    UPROPERTY(Transient)
    TMap<int64, TObjectPtr<UHierarchicalInstancedStaticMeshComponent>> CatalogStaticBatchComponents;

    UPROPERTY(Transient)
    TMap<int64, TObjectPtr<UInstancedSkinnedMeshComponent>> CatalogSkeletalBatchComponents;

    UPROPERTY(Transient)
    TArray<TObjectPtr<USkeletalMeshComponent>> HeroSkeletalWindComponents;

    UPROPERTY(Transient)
    TArray<TObjectPtr<AActor>> HeroPveWindActors;

    UPROPERTY(Transient)
    TArray<FTransform> HeroSkeletalWindBaseLocalTransforms;

    UPROPERTY(Transient)
    TObjectPtr<AActor> CachedUltraDynamicWeatherActor = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<AActor> CachedGlobalFoliageActor = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialParameterCollection> CachedDynamicWindCollection = nullptr;

    FVector LastBridgedWindDirection = FVector::ZeroVector;
    float LastBridgedWindIntensity = -1.0f;
    bool bLoggedHeroMaterialWindBridge = false;
    bool bLoggedInstancedMaterialWindBridge = false;
    bool bLoggedSpawnedSkeletalWindPropertyScan = false;

    TMap<uint64, FCubusRuntimeRandomizationSample> RuntimeRandomizationSamplesByPlant;
    TMap<FIntVector, uint32> PublishedChunkVegetationSignatures;
    FRandomStream RuntimeRandomizationStream;
    bool bRuntimeRandomizationStreamInitialized = false;
    int32 RuntimeRandomizationSeedSnapshot = 0;
    uint32 RuntimeRandomizationSettingsHashSnapshot = 0;
    uint32 PublishedVegetationSettingsHash = 0;

    TMap<int32, TArray<int32>> CatalogSpeciesIndicesByType;
    TMap<int32, float> CatalogTotalWeightByType;

    float TimeUntilRefresh = 0.0f;
    float HeroWindSwayTime = 0.0f;

    void ResolveBlockWorld();
    void EnsurePointCarriers();
    void EnsurePlantBatches();
    void BuildDefaultSpeciesCatalogIfNeeded();
    void RebuildCatalogLookups();
    int32 SelectCatalogSpeciesIndex(const FCubusVegetationInstance& Instance) const;
    int32 ResolveGrowthStageIndex(const FCubusVegetationInstance& Instance, int32 StageCount) const;
    void UpdateDynamicWindBridge();
    void ApplyWindParametersToHeroMaterials();
    void ApplyWindParametersToSpawnedSkinnedMaterials();
    void ApplyHeroWindVisualSway(float DeltaSeconds);
    uint32 CalculateLoadedPlacementHash(int32& OutLoadedChunkCount) const;
    uint32 CalculateVegetationSettingsHash() const;
    uint32 CalculateRuntimeRandomizationSettingsHash() const;

    UInstancedStaticMeshComponent* CreatePointCarrier(
        FName ComponentName,
        FName ComponentTag
    );

    UHierarchicalInstancedStaticMeshComponent* CreatePlantBatch(FName ComponentName);
    UInstancedSkinnedMeshComponent* CreateSkeletalPlantBatch(FName ComponentName);
    USkeletalMeshComponent* CreateHeroSkeletalWindComponent(FName ComponentName);
    UInstancedStaticMeshComponent* ResolveCarrierForType(int32 TypeId) const;
};
