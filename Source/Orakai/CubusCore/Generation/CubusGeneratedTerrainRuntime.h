#pragma once

#include "CoreMinimal.h"

#include "CubusCore/Generation/CubusLandscapeEvolution.h"
#include "CubusCore/Generation/CubusTerrainRaster.h"
#include "CubusCore/Generation/CubusTerrainForm.h"

/**
 * Process-local handoff from the generation page to the runtime density world.
 *
 * The generated DEM is authoritative for the runtime voxel world. The loader
 * can publish either legacy raster tiles or the production 500 km global DEM.
 * Gameplay samples the exact same immutable surface while building density
 * chunks at whatever local voxel LOD is required.
 */
class ORAKAI_API FCubusGeneratedTerrainRuntime
{
public:
    static void Configure(
        int32 WorldSeed,
        const FCubusTerrainRasterSettings& RasterSettings
    );

    static void StoreTile(const FCubusTerrainRasterTile& Tile);
    static void StoreTileIfActive(const FCubusTerrainRasterTile& Tile);

    /** Publish the exact production global DEM used by the generation preview. */
    static void StoreGlobalDem(const TSharedPtr<const CubusLandscapeEvolution::FGlobalDem, ESPMode::ThreadSafe>& GlobalDem);
    static bool HasGlobalDem();

    static void StorePreviewSnapshot(
        int32 Resolution,
        const FBox2D& BoundsMeters,
        const TArray<FColor>& Pixels
    );

    static bool GetPreviewSnapshot(
        int32& OutResolution,
        FBox2D& OutBoundsMeters,
        TArray<FColor>& OutPixels
    );

    /** Lightweight access for the gameplay spawn page and 3D DEM preview. */
    static bool GetPreviewBoundsMeters(FBox2D& OutBoundsMeters);
    static bool TrySampleHeightMeters(const FVector2D& WorldMeters, float& OutHeightMeters);

    static void SetProposedSpawnFromPreviewUV(const FVector2D& PreviewUV);
    static bool GetProposedSpawnWorldMeters(FVector2D& OutWorldMeters);
    static bool ConfirmProposedSpawn();
    static bool HasConfirmedSpawn();
    static bool GetConfirmedSpawnWorldMeters(FVector2D& OutWorldMeters);
    static bool TryGetConfirmedSpawnSurfaceHeightMeters(float& OutHeightMeters);

    static void Reset();

    static bool IsActive();
    static int32 GetWorldSeed();
    static int32 GetTileCount();

    static bool TrySampleTerrainForm(
        float WorldX,
        float WorldY,
        float VoxelSizeCm,
        FCubusTerrainFormSample& OutSample
    );

private:
    struct FState
    {
        int32 WorldSeed = 0;
        FIntPoint TerrainDomainOffsetVoxels = FIntPoint::ZeroValue;
        FCubusTerrainRasterSettings RasterSettings;
        TMap<FIntPoint, FCubusTerrainRasterTile> Tiles;
        TSharedPtr<const CubusLandscapeEvolution::FGlobalDem, ESPMode::ThreadSafe> GlobalDem;

        int32 PreviewResolution = 0;
        FBox2D PreviewBoundsMeters;
        TArray<FColor> PreviewPixels;

        FVector2D ProposedSpawnWorldMeters = FVector2D::ZeroVector;
        FVector2D ConfirmedSpawnWorldMeters = FVector2D::ZeroVector;
        bool bHasProposedSpawn = false;
        bool bHasConfirmedSpawn = false;
        bool bActive = false;
    };

    static bool TrySampleHeightMetersLocked(
        const FVector2D& WorldMeters,
        float& OutHeightMeters
    );

    static FRWLock StateLock;
    static FState State;
};
