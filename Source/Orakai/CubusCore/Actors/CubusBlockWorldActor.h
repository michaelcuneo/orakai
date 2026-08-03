#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CubusCore/Generation/CubusDensityEditField.h"
#include "CubusCore/Generation/CubusGenerationSeeds.h"
#include "CubusCore/Rendering/CubusVoxelRenderMode.h"

#include "CubusBlockWorldActor.generated.h"

class ACubusVoxelVolumeActor;
class ACubusWorldVegetationActor;
class APawn;
class USceneComponent;
class UCubusMaterialRegistry;
class UCubusGeologyProfile;
class UPCGGraphInterface;
class FCubusBlockChunkData;

UCLASS(
    BlueprintType,
    Blueprintable,
    Config = GameUserSettings,
    ClassGroup = "Cubus",
    meta = (DisplayName = "Cubus Block World")
)
class ORAKAI_API ACubusBlockWorldActor : public AActor
{
    GENERATED_BODY()

public:
    ACubusBlockWorldActor();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    void RegisterChunk(ACubusVoxelVolumeActor* ChunkActor);
    void UnregisterChunk(ACubusVoxelVolumeActor* ChunkActor);

    ACubusVoxelVolumeActor* FindChunk(
        const FIntVector& ChunkCoordinate
    ) const;

    void RebuildChunkAndNeighbours(
        const FIntVector& ChunkCoordinate
    );

    void QueueChunkForRebuild(const FIntVector& ChunkCoordinate);
    void QueueChunkAndFaceNeighboursForRebuild(
        const FIntVector& ChunkCoordinate
    );
    void QueueDensityEditDependenciesForRebuild(
        const FIntVector& ChunkCoordinate
    );

    FCubusDensityEditMap BuildDensityEditSnapshot(
        const FIntVector& ChunkCoordinate
    ) const;

    bool BuildBlockEditOverlayChunk(
        const FIntVector& ChunkCoordinate,
        FCubusBlockChunkData& OutChunk
    ) const;

    const FCubusGenerationSeeds GetGenerationSeeds() const
    {
        return FCubusGenerationSeeds::FromWorldSeed(WorldSeed);
    }

    UFUNCTION(BlueprintPure, Category = "Cubus|Generation|Seed")
    int64 GetWorldSeed() const
    {
        return WorldSeed;
    }

    UFUNCTION(BlueprintCallable, Category = "Cubus|Client Settings")
    void SetClientViewDistance(
        int32 InHorizontalViewRadius,
        int32 InVerticalViewRadius,
        bool bSaveSetting = true
    );

    UFUNCTION(BlueprintPure, Category = "Cubus|Client Settings")
    int32 GetClientHorizontalViewDistance() const
    {
        return HorizontalViewRadius;
    }

    UFUNCTION(BlueprintPure, Category = "Cubus|Client Settings")
    int32 GetClientVerticalViewDistance() const
    {
        return VerticalViewRadius;
    }

    UFUNCTION(BlueprintCallable, Category = "Cubus|Client Settings")
    void SetClientChunkLoadRate(
        int32 InMaxChunksGeneratedPerTick,
        bool bSaveSetting = true
    );

    UFUNCTION(BlueprintPure, Category = "Cubus|Client Settings")
    int32 GetClientChunkLoadRate() const
    {
        return MaxChunksGeneratedPerTick;
    }

    UFUNCTION(BlueprintPure, Category = "Cubus|Runtime Streaming|Spawn")
    bool IsInitialSpawnAreaReady() const
    {
        return bInitialSpawnAreaReady;
    }

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|World")
    void GenerateChunkGrid();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|World")
    void ClearGeneratedChunks();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|World")
    void RefreshChunkRegistry();

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|World")
    void RebuildAllChunks();

    /**
     * Returns the render-mode default authored on ChunkActorClass.
     *
     * Runtime chunks are spawned from BP_CubusVoxelPCGChunk (or another
     * configured chunk class), so the chunk Blueprint is the authoritative
     * Blocks / Density / Hybrid selector. The world actor deliberately owns no
     * separate render-mode property.
     */
    UFUNCTION(BlueprintPure, Category = "Cubus|Rendering")
    ECubusVoxelRenderMode GetVoxelRenderMode() const;

    UFUNCTION(BlueprintCallable, CallInEditor, Category = "Cubus|Terrain")
    void RegenerateTerrain();

    UFUNCTION(BlueprintCallable, Category = "Cubus|Edits")
    bool EditVoxelAtWorldVoxel(FIntVector WorldVoxel, int32 MaterialId, bool bIsWater);

    UFUNCTION(BlueprintCallable, Category = "Cubus|Edits")
    int32 EditBlockSphereAtWorldVoxel(
        FIntVector CentreWorldVoxel,
        int32 BrushRadius,
        int32 MaterialId,
        bool bIsWater = false
    );

    UFUNCTION(BlueprintCallable, Category = "Cubus|Edits")
    int32 EditDensitySphereAtWorldSample(
        FIntVector CentreWorldSample,
        int32 BrushRadius,
        float DensityDelta,
        int32 MaterialId = 1
    );

    UFUNCTION(BlueprintCallable, Category = "Cubus|Edits")
    bool ClearVoxelEditAtWorldVoxel(FIntVector WorldVoxel);

    UFUNCTION(BlueprintCallable, Category = "Cubus|Edits")
    void RecordFoliageEditAtWorldVoxel(
        FIntVector WorldVoxel,
        int32 TypeId,
        float RotationYaw,
        float Scale
    );

    UFUNCTION(BlueprintCallable, Category = "Cubus|Edits")
    void RemoveFoliageAtWorldVoxel(FIntVector WorldVoxel);

    UFUNCTION(BlueprintCallable, Category = "Cubus|Gameplay")
    bool HarvestTreeAlongRay(
        FVector TraceStart,
        FVector TraceEnd,
        float SelectionRadius,
        FIntVector& OutTreeWorldVoxel
    );

    void ReleaseHeldPawnAtLocation(
        APawn* PlayerPawn,
        const FVector& ReleaseLocation
    );

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cubus|Components")
    TObjectPtr<USceneComponent> WorldRoot;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Generation|Seed")
    int64 WorldSeed = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Persistence")
    bool bConnectToSpacetimeDB = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Persistence")
    FString SpacetimeServerUri = TEXT("127.0.0.1:3000");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Persistence")
    FString SpacetimeDatabaseName = TEXT("orakai");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Persistence")
    FString SpacetimeTokenFilePath = TEXT(".spacetime_orakai");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Generation")
    FIntVector GridDimensions = FIntVector(2, 2, 1);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Generation")
    FIntVector GridOrigin = FIntVector::ZeroValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Generation", meta = (ClampMin = "1.0", Units = "cm"))
    float GeneratedVoxelSize = 100.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Generation")
    TSubclassOf<ACubusVoxelVolumeActor> ChunkActorClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Runtime Streaming")
    bool bEnableRuntimeStreaming = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation")
    bool bEnableWorldVegetation = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation")
    TSubclassOf<ACubusWorldVegetationActor> WorldVegetationActorClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Runtime Streaming", meta = (ClampMin = "0", UIMax = "16"))
    int32 InitialLoadRadius = 2;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Cubus|Runtime Streaming", meta = (ClampMin = "0", UIMax = "32"))
    int32 HorizontalViewRadius = 8;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Cubus|Runtime Streaming", meta = (ClampMin = "1", UIMax = "16"))
    int32 VerticalViewRadius = 3;

    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Cubus|Runtime Streaming", meta = (ClampMin = "1", UIMax = "16"))
    int32 MaxChunksGeneratedPerTick = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Runtime Streaming", meta = (ClampMin = "1", UIMax = "32"))
    int32 MaxChunksRemovedPerTick = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Runtime Streaming", meta = (ClampMin = "1", UIMax = "32"))
    int32 MaxDirtyChunksRebuiltPerTick = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Runtime Streaming", meta = (ClampMin = "0.05", Units = "s"))
    float StreamingUpdateInterval = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Runtime Streaming|Spawn")
    bool bHoldPawnUntilInitialAreaReady = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Runtime Streaming|Spawn", meta = (ClampMin = "0.0", Units = "cm"))
    float SpawnHeightOffset = 200.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Runtime Streaming|Spawn", meta = (ClampMin = "0.0", Units = "s"))
    float SpawnHoldTimeoutSeconds = 3.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain")
    bool bUseHeightTerrain = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain")
    int32 TerrainSurfaceWorldZ = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain")
    int32 TerrainBaseHeight = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.0"))
    float TerrainContinentAmplitude = 18.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.000001"))
    float TerrainContinentFrequency = 0.003f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.0"))
    float TerrainHillAmplitude = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.000001"))
    float TerrainHillFrequency = 0.015f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.0"))
    float TerrainDetailAmplitude = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.000001"))
    float TerrainDetailFrequency = 0.08f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.0"))
    float TerrainRidgeAmplitude = 16.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.000001"))
    float TerrainRidgeFrequency = 0.012f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.0"))
    float TerrainValleyDepth = 14.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.000001"))
    float TerrainValleyFrequency = 0.006f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float TerrainValleyWidth = 0.08f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.001", ClampMax = "1.0"))
    float TerrainValleyFalloff = 0.22f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.0"))
    float TerrainValleyWarpAmplitude = 24.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Shape", meta = (ClampMin = "0.000001"))
    float TerrainValleyWarpFrequency = 0.004f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Regions", meta = (ClampMin = "0.000001"))
    float TerrainRegionFrequency = 0.0025f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Regions", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
    float TerrainPlainsThreshold = -0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Regions", meta = (ClampMin = "0.001", ClampMax = "1.0"))
    float TerrainPlainsBlend = 0.18f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Regions", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
    float TerrainMountainThreshold = 0.30f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Regions", meta = (ClampMin = "0.001", ClampMax = "1.0"))
    float TerrainMountainBlend = 0.20f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Rendering")
    TObjectPtr<UCubusMaterialRegistry> MaterialRegistry = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Geology")
    TObjectPtr<UCubusGeologyProfile> GeologyProfile = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Materials", meta = (ClampMin = "1"))
    int32 TerrainRockMaterialId = 3;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Materials", meta = (ClampMin = "1"))
    int32 TerrainSnowMaterialId = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Materials", meta = (ClampMin = "0.0"))
    float TerrainRockSlopeThreshold = 1.25f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Materials")
    int32 TerrainSnowMinimumHeight = 34;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Water")
    bool bGenerateWater = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Water")
    int32 TerrainWaterLevel = 8;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Water", meta = (ClampMin = "1"))
    int32 TerrainWaterMaterialId = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Materials", meta = (ClampMin = "1"))
    int32 TerrainSurfaceMaterialId = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain|Materials", meta = (ClampMin = "1"))
    int32 TerrainSubsurfaceMaterialId = 2;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Diagnostics")
    int32 RegisteredChunkCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Diagnostics")
    int32 GeneratedChunkCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Diagnostics")
    int32 PendingRuntimeChunkCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Diagnostics")
    bool bInitialSpawnAreaReady = false;

private:
    UPCGGraphInterface* VegetationPCGGraph = nullptr;
    bool bGenerateVegetationPCG = false;

    TMap<FIntVector, TWeakObjectPtr<ACubusVoxelVolumeActor>> ChunksByCoordinate;

    UPROPERTY(Transient)
    TArray<TObjectPtr<ACubusVoxelVolumeActor>> GeneratedChunks;

    TArray<FIntVector> PendingChunkGeneration;
    TArray<FIntVector> PendingChunkRemoval;
    TSet<FIntVector> DirtyChunkCoordinates;
    TSet<FIntVector> RequiredChunkCoordinates;
    TSet<FIntVector> InitialRequiredCoordinates;
    FCubusDensityEditMap DensityEdits;

    TWeakObjectPtr<APawn> TrackedPawn;
    TWeakObjectPtr<ACubusWorldVegetationActor> WorldVegetationActor;
    FIntVector LastTrackedChunk = FIntVector(MAX_int32, MAX_int32, MAX_int32);
    FVector HeldPawnLocation = FVector::ZeroVector;
    bool bPawnHeldForStreaming = false;
    bool bSpawnTimeoutReported = false;
    float HeldPawnElapsedSeconds = 0.0f;
    float TimeUntilStreamingUpdate = 0.0f;

    void RemoveInvalidChunks();
    void RebuildChunkAtCoordinate(const FIntVector& ChunkCoordinate);
    void PublishWorldConfig();
    void RestoreDensityEdits();
    void ApplyPersistedEditsToChunk(ACubusVoxelVolumeActor& ChunkActor);
    void RecordTrackedPawnCoordinate();

    ACubusVoxelVolumeActor* SpawnChunkAtCoordinate(
        const FIntVector& Coordinate,
        bool bGenerateVegetation
    );

    void UpdateRuntimeStreaming(bool bForce);
    void ProcessRuntimeQueues();
    void ProcessDirtyChunkQueue();
    void BuildRequiredCoordinates(
        const FIntVector& CentreCoordinate,
        int32 HorizontalRadius,
        int32 VerticalRadius,
        TSet<FIntVector>& OutCoordinates
    ) const;

    FIntVector WorldLocationToChunkCoordinate(
        const FVector& WorldLocation
    ) const;

    void HoldPawnForInitialStreaming();
    void TryReleasePawnToTerrain();
    void EnsureWorldVegetationActor();
};
