#pragma once

#include "CoreMinimal.h"

#include "CubusCore/Generation/CubusDensityField.h"

/**
 * Terrain settings copied from a Cubus chunk before a density build.
 *
 * The density path deliberately keeps these values in a plain structure so
 * meshing does not depend on an Actor or UObject and can later move safely to
 * background jobs.
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

    int32 SurfaceMaterialId = 1;
    int32 SubsurfaceMaterialId = 2;
    int32 RockMaterialId = 3;
    int32 SnowMaterialId = 4;

    float RockSlopeThreshold = 1.25f;
    float SnowMinimumHeight = 34.0f;
    float SurfaceMaterialDepth = 2.0f;
};

/**
 * Continuous terrain scalar field used by the native density renderer.
 *
 * Unlike FCubusBlockDensityField, this field never converts generated block
 * occupancy into +1/-1 samples. It evaluates the terrain function directly,
 * preserves the fractional height, and returns:
 *
 *     Density = ContinuousSurfaceSampleZ - GlobalSampleZ
 *
 * The zero crossing therefore moves continuously between lattice samples
 * instead of remaining locked to the block staircase.
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

    float SampleNoise(
        float WorldX,
        float WorldY,
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

    static float SmoothStep(
        float EdgeMinimum,
        float EdgeMaximum,
        float Value
    );
};
