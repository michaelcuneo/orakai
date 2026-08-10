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

/**
 * World-level owner for coarse visual terrain tiles.
 *
 * LOD0 gameplay chunks remain owned by ACubusBlockWorldActor. This actor only
 * renders coarse density terrain outside the detailed gameplay radius.
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

    /** LOD1 samples one point every four canonical voxels: 32 cells = 128 m. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD1", meta = (ClampMin = "2", ClampMax = "64"))
    int32 Lod1CanonicalVoxelStride = 4;

    /** Number of LOD1 tile centres omitted around the camera for LOD0 terrain. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD1", meta = (ClampMin = "0", ClampMax = "16"))
    int32 Lod1InnerRadiusTiles = 1;

    /** Conservative first-test radius; later tiers extend the horizon farther. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD1", meta = (ClampMin = "1", ClampMax = "32"))
    int32 Lod1OuterRadiusTiles = 4;

    /** One 128 m vertical tile is enough for the first surface-terrain test. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD1", meta = (ClampMin = "0", ClampMax = "4"))
    int32 Lod1VerticalRadiusTiles = 0;

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
    void CollectCompletedBuilds();
    void StartPendingBuilds();
    void UploadCompletedBuilds();
    void RemoveUnneededTiles();
    void ClearAllTiles();

    UProceduralMeshComponent* CreateTileComponent(
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

    TMap<FIntVector, TObjectPtr<UProceduralMeshComponent>> TileComponents;
    TSet<FIntVector> RequiredTiles;
    TSet<FIntVector> TilesBuilding;
    TArray<FIntVector> PendingTiles;
    TArray<FCubusTerrainLodTileBuild> ActiveBuilds;
    TArray<FCubusTerrainLodTileBuildResult> CompletedBuilds;

    FIntVector LastCentreTile = FIntVector(MAX_int32, MAX_int32, MAX_int32);
    float TimeUntilStreamingUpdate = 0.0f;
};
