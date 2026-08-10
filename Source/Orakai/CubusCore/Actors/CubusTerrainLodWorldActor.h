#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Tasks/Task.h"

#include "CubusCore/Generation/CubusTerrainDensityField.h"
#include "CubusCore/Meshing/CubusMeshData.h"

#include "CubusTerrainLodWorldActor.generated.h"

class ACubusBlockWorldActor;
class UProceduralMeshComponent;
class USceneComponent;
class UMaterialInterface;

struct FCubusTerrainLodTileBuildInput
{
    FCubusTerrainDensitySettings DensitySettings;
    FIntVector TileCoordinate = FIntVector::ZeroValue;
    int32 CanonicalVoxelStride = 4;
    float CanonicalVoxelSize = 100.0f;
    float IsoLevel = 0.0f;
};

struct FCubusTerrainLodTileBuildResult
{
    FIntVector TileCoordinate = FIntVector::ZeroValue;
    TMap<int32, FCubusMeshData> MaterialMeshes;
    int32 GeneratedTriangleCount = 0;
    double BuildTimeMilliseconds = 0.0;
};

struct FCubusTerrainLodTileBuild
{
    FIntVector TileCoordinate = FIntVector::ZeroValue;
    UE::Tasks::TTask<FCubusTerrainLodTileBuildResult> Task;
};

struct FCubusTerrainLodTierRuntime
{
    int32 LodLevel = 1;
    int32 CanonicalVoxelStride = 4;
    int32 InnerRadiusTiles = 0;
    int32 OuterRadiusTiles = 4;
    int32 VerticalRadiusTiles = 0;
    int32 OverlapTiles = 1;

    TMap<FIntVector, TObjectPtr<UProceduralMeshComponent>> TileComponents;
    TSet<FIntVector> RequiredTiles;
    TSet<FIntVector> TilesBuilding;
    TArray<FIntVector> PendingTiles;
    TArray<FCubusTerrainLodTileBuild> ActiveBuilds;
    TArray<FCubusTerrainLodTileBuildResult> CompletedBuilds;

    FIntVector LastCentreTile =
        FIntVector(MAX_int32, MAX_int32, MAX_int32);
};

/**
 * World-level owner for coarse visual terrain tiles.
 *
 * LOD0 gameplay chunks remain owned by ACubusBlockWorldActor. This actor owns
 * visual-only coarse density tiers outside the detailed gameplay radius.
 */
UCLASS(
    BlueprintType,
    Blueprintable,
    ClassGroup = "Cubus",
    meta = (DisplayName = "Cubus Terrain LOD World")
)
class ORAKAI_API ACubusTerrainLodWorldActor : public AActor
{
    GENERATED_BODY()

public:
    ACubusTerrainLodWorldActor();

    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cubus|Components")
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD")
    bool bEnableTerrainLod = true;

    /** LOD1 samples one point every four canonical voxels. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD1", meta = (ClampMin = "2", ClampMax = "64"))
    int32 Lod1CanonicalVoxelStride = 4;

    /** Number of LOD1 tiles allowed to overlap the outer edge of LOD0. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD1", meta = (ClampMin = "0", ClampMax = "2"))
    int32 Lod1OverlapTiles = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD1", meta = (ClampMin = "1", ClampMax = "32"))
    int32 Lod1OuterRadiusTiles = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD1", meta = (ClampMin = "0", ClampMax = "4"))
    int32 Lod1VerticalRadiusTiles = 0;

    /** LOD2 samples one point every sixteen canonical voxels. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD2", meta = (ClampMin = "4", ClampMax = "256"))
    int32 Lod2CanonicalVoxelStride = 16;

    /** Number of LOD2 tiles allowed to overlap the outer edge of LOD1. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD2", meta = (ClampMin = "0", ClampMax = "2"))
    int32 Lod2OverlapTiles = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD2", meta = (ClampMin = "1", ClampMax = "32"))
    int32 Lod2OuterRadiusTiles = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD2", meta = (ClampMin = "0", ClampMax = "4"))
    int32 Lod2VerticalRadiusTiles = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|Streaming", meta = (ClampMin = "1", ClampMax = "16"))
    int32 MaxConcurrentLodBuilds = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|Streaming", meta = (ClampMin = "1", ClampMax = "16"))
    int32 MaxLodBuildStartsPerTick = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|Streaming", meta = (ClampMin = "1", ClampMax = "16"))
    int32 MaxLodUploadsPerTick = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|Streaming", meta = (ClampMin = "0.05", Units = "s"))
    float LodStreamingUpdateInterval = 0.25f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Terrain LOD|Diagnostics")
    int32 LoadedLodTileCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Terrain LOD|Diagnostics")
    int32 BuildingLodTileCount = 0;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Transient, Category = "Cubus|Terrain LOD|Diagnostics")
    int32 PendingLodTileCount = 0;

private:
    void ResolveBlockWorld();
    void UpdateStreaming();
    void UpdateTierStreaming(
        FCubusTerrainLodTierRuntime& Tier,
        const FVector& CameraGridLocation,
        float CanonicalChunkWorldSize,
        double PreviousTierHalfExtentCanonicalChunks,
        int32 CanonicalVoxelStride,
        int32 OverlapTiles,
        int32 OuterRadiusTiles,
        int32 VerticalRadiusTiles
    );
    void CollectCompletedBuilds();
    void CollectCompletedBuildsForTier(FCubusTerrainLodTierRuntime& Tier);
    void StartPendingBuilds();
    void UploadCompletedBuilds();
    bool UploadOneCompletedBuild(
        FCubusTerrainLodTierRuntime& Tier,
        float CanonicalVoxelSize,
        UMaterialInterface* TerrainMaterial,
        double& WorkerMilliseconds
    );
    void RemoveUnneededTiles(FCubusTerrainLodTierRuntime& Tier);
    void ClearAllTiles();
    void ClearTier(FCubusTerrainLodTierRuntime& Tier);

    UProceduralMeshComponent* CreateTileComponent(
        FCubusTerrainLodTierRuntime& Tier,
        const FIntVector& TileCoordinate,
        float TileWorldSize
    );

    UMaterialInterface* ResolveTerrainMaterial() const;
    FVector ResolveWorldGridOrigin(float CanonicalChunkWorldSize) const;

    static FCubusTerrainLodTileBuildResult BuildTile(
        const FCubusTerrainLodTileBuildInput& Input
    );

    UPROPERTY(Transient)
    TObjectPtr<ACubusBlockWorldActor> BlockWorld;

    FCubusTerrainLodTierRuntime Lod1Runtime;
    FCubusTerrainLodTierRuntime Lod2Runtime;

    float TimeUntilStreamingUpdate = 0.0f;
};
