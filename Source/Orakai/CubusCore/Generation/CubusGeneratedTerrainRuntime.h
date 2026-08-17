#pragma once

#include "CoreMinimal.h"

#include "CubusCore/Generation/CubusTerrainRaster.h"
#include "CubusCore/Generation/CubusTerrainForm.h"

/**
 * Process-local handoff from the loader DEM pipeline to the runtime density world.
 *
 * The loader authors terrain in physical metres while Cubus density operates in
 * canonical voxel coordinates. This bridge keeps one thread-safe copy of the
 * currently generated raster tiles and performs the coordinate conversion at
 * sample time. It deliberately does not create a second voxel terrain store.
 *
 * Generation writes occur before gameplay level handoff. Runtime density workers
 * then read the finished tiles concurrently through the same immutable sampling
 * interface used by streaming surface prediction.
 */
class ORAKAI_API FCubusGeneratedTerrainRuntime
{
public:
    /** Start or continue staging terrain for one loader generation session. */
    static void Configure(
        int32 WorldSeed,
        const FCubusTerrainRasterSettings& RasterSettings
    );

    /** Store the latest version of one authored raster tile. */
    static void StoreTile(const FCubusTerrainRasterTile& Tile);

    /** Replace a tile only when a generated-terrain session is already active. */
    static void StoreTileIfActive(const FCubusTerrainRasterTile& Tile);

    /** Clear the process-local generated terrain handoff. */
    static void Reset();

    static bool IsActive();
    static int32 GetWorldSeed();
    static int32 GetTileCount();

    /**
     * Sample the generated DEM using the coordinate convention received by
     * FCubusTerrainForm::Sample(). WorldX/Y already contain the legacy seeded
     * terrain-domain offset, so the bridge removes that offset before converting
     * canonical voxels to physical metres.
     */
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
        bool bActive = false;
    };

    static FRWLock StateLock;
    static FState State;
};
