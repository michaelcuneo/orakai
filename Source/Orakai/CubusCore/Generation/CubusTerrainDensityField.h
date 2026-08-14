#pragma once

#include "CoreMinimal.h"

#include "CubusCore/Generation/CubusBiomeField.h"
#include "CubusCore/Generation/CubusDensityField.h"
#include "CubusCore/Generation/CubusLandmarkField.h"
#include "CubusCore/Generation/CubusTerrainForm.h"

/** Plain, immutable-at-build-time settings for the native terrain density field. */
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

    /*
     * Volumetric geology is evaluated in continuous canonical world space.
     * These controls describe rock behaviour rather than another height-noise
     * stack: hardness preserves shelves, fractures remove material, folds
     * change strata attitude, and mass noise produces coherent outcrops.
     */
    bool bGenerateVolumetricGeology = true;
    float GeologySurfaceBand = 24.0f;
    float GeologyCliffSlopeStart = 0.45f;
    float GeologyCliffSlopeFull = 1.35f;
    float GeologyStrataFrequency = 0.055f;
    float GeologyStrataDip = 0.012f;
    float GeologyShelfStrength = 5.0f;
    float GeologyUndercutStrength = 4.0f;
    float GeologyRockWarpFrequency = 0.035f;
    float GeologyRockWarpStrength = 1.5f;
    float GeologyHardnessFrequency = 0.0028f;
    float GeologyFractureFrequency = 0.017f;
    float GeologyFoldFrequency = 0.006f;
    float GeologyMassFrequency = 0.021f;
    float GeologyMassStrength = 3.5f;
    float GeologyOverhangStrength = 7.0f;
    float GeologyFractureStrength = 2.75f;

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
    int32 RiverSeed = 0;

    /*
     * Caves use deterministic graph-like tunnel segments in XY with smoothly
     * varying depth. 3D noise only perturbs the walls; it no longer decides
     * the topology of the cave system.
     */
    bool bGenerateCaves = false;
    int32 CaveMinimumWorldZ = -256;
    int32 CaveMaximumWorldZ = 24;
    int32 CaveSurfaceClearance = 5;
    float CavePrimaryFrequency = 0.035f;
    float CaveSecondaryFrequency = 0.07f;
    float CaveThreshold = 0.16f;
    float CaveSurfaceSharpness = 8.0f;
    float CaveNetworkCellSize = 36.0f;
    float CaveTunnelRadius = 3.0f;
    float CaveChamberChance = 0.08f;
    float CaveChamberRadius = 7.5f;
    float CaveWallWarpStrength = 0.75f;
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
    float RockMaterialDepth = 7.0f;

    int32 BiomeSubsurfaceMaterialId = 2;
    int32 BiomeRockMaterialId = 3;
    int32 BiomeSnowMaterialId = 4;
    float BiomeSnowMinimumHeight = 34.0f;

    FCubusBiomeFieldSettings BiomeSettings;
    FCubusLandmarkFieldSettings LandmarkSettings;
};

/**
 * Continuous scalar field for the generated density world.
 *
 * The field is deliberately independent of mesh sampling resolution. A caller
 * may evaluate the same terrain at 4 m, 1 m, 25 cm or 10 cm spacing without
 * creating another terrain representation or changing persisted edits.
 *
 * This object is also the authoritative density-biome context. Consumers that
 * need ecology information must query SampleSurfaceBiome() rather than
 * reconstructing climate or hydrology independently.
 */
class ORAKAI_API FCubusTerrainDensityField final : public ICubusDensityField
{
public:
    explicit FCubusTerrainDensityField(const FCubusTerrainDensitySettings& InSettings);

    virtual FCubusDensitySample Sample(const FIntVector& GlobalSampleCoordinate) const override;
    virtual FCubusDensitySample SampleContinuous(const FVector& GlobalSampleCoordinate) const override;

    float SampleSurfaceVoxelHeight(float WorldX, float WorldY) const;

    /** Authoritative biome/environment sample for one density-world column. */
    FCubusBiomeSample SampleSurfaceBiome(float WorldX, float WorldY) const;

private:
    struct FTerrainRegionWeights
    {
        float Plains = 0.0f;
        float Rolling = 1.0f;
        float Mountains = 0.0f;
    };

    struct FSurfaceData
    {
        float SurfaceVoxelHeight = 0.0f;
        FCubusTerrainFormSample FormSample;
    };

    struct FColumnData
    {
        float SurfaceVoxelHeight = 0.0f;
        float SurfaceSampleZ = 1.0f;
        float Slope = 0.0f;
        FVector2D Gradient = FVector2D::ZeroVector;
        float RockHardness = 0.5f;
        float Fracture = 0.0f;
        float StrataTilt = 0.0f;
        float RockExposure = 0.0f;
        FCubusTerrainFormSample FormSample;
        FCubusBiomeSample BiomeSample;
        int32 SurfaceMaterialId = 1;
    };

    FCubusTerrainDensitySettings Settings;
    FCubusTerrainFormSettings TerrainFormSettings;
    FCubusHydrologySettings HydrologySettings;

    static constexpr float CoordinateCacheScale = 20.0f;

    mutable TMap<FIntPoint, FSurfaceData> SurfaceCache;
    mutable TMap<FIntPoint, FColumnData> ColumnCache;

    static FIntPoint MakeCoordinateCacheKey(float WorldX, float WorldY);

    const FSurfaceData& GetCachedSurfaceData(float WorldX, float WorldY) const;
    float GetCachedSurfaceVoxelHeight(float WorldX, float WorldY) const;
    const FColumnData& GetColumnData(float WorldX, float WorldY) const;

    FTerrainRegionWeights SampleTerrainRegions(float WorldX, float WorldY) const;

    float SampleNoise2D(float WorldX, float WorldY, float Frequency) const;
    float SampleNoise3D(float WorldX, float WorldY, float WorldZ, float Frequency) const;
    float SampleRidgedNoise(float WorldX, float WorldY, float Frequency) const;
    float SampleRidgedNoise3D(float WorldX, float WorldY, float WorldZ, float Frequency) const;
    float SampleValleyMask(float WorldX, float WorldY) const;
    float SampleRiverDistance(float WorldX, float WorldY) const;
    float ApplyRiverLowering(float SurfaceHeight, float WorldX, float WorldY) const;

    float SampleGeologicalDensity(const FVector& GlobalSampleCoordinate, const FColumnData& Column, float BaseTerrainDensity) const;
    float SampleCaveDensity(const FVector& GlobalSampleCoordinate, float SurfaceVoxelHeight, float SurfaceSlope) const;

    static float SmoothStep(float EdgeMinimum, float EdgeMaximum, float Value);
};