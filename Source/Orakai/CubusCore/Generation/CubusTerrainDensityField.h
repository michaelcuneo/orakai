#pragma once

#include "CoreMinimal.h"

#include "CubusCore/Generation/CubusDensityField.h"

/**
 * Plain, immutable-at-build-time settings for the native terrain density
 * field. Keeping this independent of Actors and UObjects allows density
 * sampling and meshing to move to worker threads later.
 */
struct ORAKAI_API FCubusTerrainDensitySettings
{
    bool bUseHeightTerrain = true;

    float FlatSurfaceWorldZ = 8.0f;
    float BaseHeight = 8.0f;

    float ContinentAmplitude = 18.0f;
    float ContinentFrequency = 0.003f;

    float HillAmplitude = 10.0f;
    float HillFrequency = 0.015f;

    float DetailAmplitude = 2.0f;
    float DetailFrequency = 0.08f;

    float RidgeAmplitude = 16.0f;
    float RidgeFrequency = 0.012f;

    float ValleyDepth = 14.0f;
    float ValleyFrequency = 0.006f;
    float ValleyWidth = 0.08f;
    float ValleyFalloff = 0.22f;
    float ValleyWarpAmplitude = 24.0f;
    float ValleyWarpFrequency = 0.004f;

    float RegionFrequency = 0.0025f;
    float PlainsThreshold = -0.25f;
    float PlainsBlend = 0.18f;
    float MountainThreshold = 0.30f;
    float MountainBlend = 0.20f;

    /** Same whole-chunk terrain-domain offset used by block generation. */
    int32 TerrainOffsetX = 0;
    int32 TerrainOffsetY = 0;

    bool bGenerateRivers = false;
    float RiverFrequency = 0.0025f;
    float RiverChannelWidth = 0.055f;
    float RiverValleyWidth = 0.22f;
    float RiverValleyDepth = 7.0f;
    float RiverChannelDepth = 4.0f;
    float RiverWarpAmplitude = 48.0f;
    float RiverWarpFrequency = 0.006f;
    int32 RiverOffsetX = 0;
    int32 RiverOffsetY = 0;

    bool bGenerateCaves = false;
    int32 CaveMinimumWorldZ = -256;
    int32 CaveMaximumWorldZ = 24;
    int32 CaveSurfaceClearance = 5;
    float CavePrimaryFrequency = 0.035f;
    float CaveSecondaryFrequency = 0.07f;
    float CaveThreshold = 0.16f;
    float CaveSurfaceSharpness = 8.0f;
    int32 CaveOffsetX = 0;
    int32 CaveOffsetY = 0;
    int32 CaveOffsetZ = 0;

    int32 SurfaceMaterialId = 1;
    int32 SubsurfaceMaterialId = 2;
    int32 RockMaterialId = 3;
    int32 SnowMaterialId = 4;

    float RockSlopeThreshold = 1.25f;
    float SnowMinimumHeight = 34.0f;
    float SurfaceMaterialDepth = 2.0f;
};

/**
 * Continuous scalar field for Cubus terrain.
 *
 * The field evaluates the same seeded two-dimensional terrain domain as the
 * block generator, applies continuous river lowering, and intersects the
 * result with a smooth three-dimensional cave field. Its zero crossing is not
 * quantized to block occupancy.
 */
class ORAKAI_API FCubusTerrainDensityField final : public ICubusDensityField
{
public:
    explicit FCubusTerrainDensityField(
        const FCubusTerrainDensitySettings& InSettings
    );

    virtual FCubusDensitySample Sample(
        const FIntVector& GlobalSampleCoordinate
    ) const override;

    float SampleSurfaceVoxelHeight(
        float WorldX,
        float WorldY
    ) const;

private:
    struct FTerrainRegionWeights
    {
        float Plains = 0.0f;
        float Rolling = 1.0f;
        float Mountains = 0.0f;
    };

    struct FColumnData
    {
        float SurfaceVoxelHeight = 0.0f;
        float SurfaceSampleZ = 1.0f;
        float Slope = 0.0f;
        int32 SurfaceMaterialId = 1;
    };

    FCubusTerrainDensitySettings Settings;

    mutable TMap<FIntPoint, float> HeightCache;
    mutable TMap<FIntPoint, FColumnData> ColumnCache;

    float GetCachedSurfaceVoxelHeight(
        int32 WorldX,
        int32 WorldY
    ) const;

    const FColumnData& GetColumnData(
        int32 WorldX,
        int32 WorldY
    ) const;

    FTerrainRegionWeights SampleTerrainRegions(
        float WorldX,
        float WorldY
    ) const;

    float SampleNoise2D(
        float WorldX,
        float WorldY,
        float Frequency
    ) const;

    float SampleNoise3D(
        float WorldX,
        float WorldY,
        float WorldZ,
        float Frequency
    ) const;

    float SampleRidgedNoise(
        float WorldX,
        float WorldY,
        float Frequency
    ) const;

    float SampleValleyMask(
        float WorldX,
        float WorldY
    ) const;

    float SampleRiverDistance(
        float WorldX,
        float WorldY
    ) const;

    float ApplyRiverLowering(
        float SurfaceHeight,
        float WorldX,
        float WorldY
    ) const;

    float SampleCaveDensity(
        const FIntVector& GlobalSampleCoordinate,
        float SurfaceVoxelHeight
    ) const;

    static float SmoothStep(
        float EdgeMinimum,
        float EdgeMaximum,
        float Value
    );
};
