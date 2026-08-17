#pragma once

#include "CoreMinimal.h"
#include "CubusCore/Generation/CubusTerrainStructure.h"

/**
 * Settings for the pre-voxel terrain domain.
 *
 * The raster is authored in physical metres and deliberately has no knowledge
 * of the final density voxel size. The default source is a 1 m DEM-like grid
 * split into 512 m tiles with a retained interpolation/processing halo.
 */
struct ORAKAI_API FCubusTerrainRasterSettings
{
    /** Horizontal spacing between authored terrain samples. */
    float SampleSpacingMeters = 1.0f;

    /** Requested interior tile width. Resolved to an integer number of cells. */
    float TileSizeMeters = 512.0f;

    /** Samples retained outside each tile edge for seamless reconstruction/passes. */
    int32 HaloSamples = 2;

    /** Deterministic seed-domain displacement expressed in metres, not voxels. */
    FVector2D DomainOffsetMeters = FVector2D::ZeroVector;

    /** Broad pre-drainage structural landscape sampled into the 1 m raster. */
    FCubusTerrainStructureSettings Structure;
};

/** One deterministic high-resolution height tile in physical world metres. */
class ORAKAI_API FCubusTerrainRasterTile
{
public:
    FCubusTerrainRasterTile() = default;

    bool IsValid() const;

    const FIntPoint& GetTileCoordinate() const { return TileCoordinate; }
    int32 GetInteriorCellCount() const { return InteriorCellCount; }
    int32 GetInteriorSampleCount() const { return InteriorCellCount + 1; }
    int32 GetStorageSampleCount() const { return StorageSampleCount; }
    int32 GetHaloSamples() const { return HaloSamples; }
    float GetSampleSpacingMeters() const { return SampleSpacingMeters; }
    double GetTileSizeMeters() const { return TileSizeMeters; }
    FVector2D GetWorldMinimumMeters() const;
    FVector2D GetWorldMaximumMeters() const;

    /**
     * Fetch one authored grid sample. Grid coordinates are relative to the
     * tile interior and may address the retained halo in [-Halo, Cells+Halo].
     */
    float GetHeightSampleMeters(int32 GridX, int32 GridY) const;

    /**
     * Seamless bounded bicubic reconstruction of the authored terrain.
     * The query is in physical metres and may lie anywhere inside this tile.
     */
    float SampleHeightMeters(double WorldXmeters, double WorldYmeters) const;

private:
    friend class FCubusTerrainRasterBuilder;
    friend class FCubusTerrainCarving;
    friend class FCubusTerrainErosion;

    static float CubicInterpolate(float P0, float P1, float P2, float P3, float Alpha);
    int32 StorageIndex(int32 GridX, int32 GridY) const;

    FIntPoint TileCoordinate = FIntPoint::ZeroValue;
    int32 InteriorCellCount = 0;
    int32 StorageSampleCount = 0;
    int32 HaloSamples = 0;
    float SampleSpacingMeters = 1.0f;
    double TileSizeMeters = 0.0;
    TArray<float> HeightMeters;
};

/**
 * Builds deterministic high-resolution terrain tiles before any voxelisation.
 */
class ORAKAI_API FCubusTerrainRasterBuilder
{
public:
    static FCubusTerrainRasterTile BuildTile(
        const FIntPoint& TileCoordinate,
        const FCubusTerrainRasterSettings& Settings
    );

    static float SampleStructuralHeightMeters(
        double WorldXmeters,
        double WorldYmeters,
        const FCubusTerrainRasterSettings& Settings
    );

    static FIntPoint WorldToTileCoordinate(
        double WorldXmeters,
        double WorldYmeters,
        const FCubusTerrainRasterSettings& Settings
    );

    static int32 ResolveInteriorCellCount(const FCubusTerrainRasterSettings& Settings);
    static double ResolveTileSizeMeters(const FCubusTerrainRasterSettings& Settings);
};
