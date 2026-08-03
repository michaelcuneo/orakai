#pragma once

#include "CoreMinimal.h"
#include "CubusCore/Data/CubusBiomeTypes.h"

class UCubusGeologyProfile;

/** Thread-safe settings copied from a geology profile for biome sampling. */
struct ORAKAI_API FCubusBiomeFieldSettings
{
    bool bEnabled = false;
    float Frequency = 0.004f;
    float ForestThreshold = 0.15f;
    float WetlandRiverDistance = 0.10f;
    float RockySlopeThreshold = 4.0f;
    float RockyMinimumWorldZ = 48.0f;

    int32 PlainsSurfaceMaterialId = 1;
    int32 ForestSurfaceMaterialId = 7;
    int32 RockySurfaceMaterialId = 3;
    int32 WetlandSurfaceMaterialId = 8;

    int32 BiomeOffsetX = 0;
    int32 BiomeOffsetY = 0;

    bool bGenerateRivers = false;
    float RiverFrequency = 0.0025f;
    float RiverWarpAmplitude = 48.0f;
    float RiverWarpFrequency = 0.006f;
    int32 RiverOffsetX = 0;
    int32 RiverOffsetY = 0;

    TArray<FCubusBiomeDefinition> Definitions;
};

/** Continuous climate/terrain classification at one world column. */
struct ORAKAI_API FCubusBiomeSample
{
    ECubusBiomeKind DominantBiome = ECubusBiomeKind::Plains;
    float PlainsWeight = 1.0f;
    float ForestWeight = 0.0f;
    float RockyWeight = 0.0f;
    float WetlandWeight = 0.0f;
    float Moisture = 0.5f;
    float Temperature = 0.5f;
    float RiverDistance = 1.0f;
    int32 SurfaceMaterialId = 1;
    int32 BiomeDefinitionIndex = INDEX_NONE;
};

/**
 * Shared deterministic biome field for block and density terrain.
 *
 * Biomes are derived from broad, domain-warped moisture and temperature
 * fields plus actual terrain slope, elevation and drainage. The continuous
 * weights keep transitions spatially coherent even though block surfaces
 * ultimately choose a discrete material.
 */
class ORAKAI_API FCubusBiomeField
{
public:
    static FCubusBiomeFieldSettings MakeSettings(
        const UCubusGeologyProfile* GeologyProfile,
        int32 BiomeSeed,
        int32 RiverSeed
    );

    static FCubusBiomeSample Sample(
        float WorldX,
        float WorldY,
        float SurfaceWorldZ,
        float Slope,
        const FCubusBiomeFieldSettings& Settings
    );

    /** Normalized distance-like field shared by carving and wetland rules. */
    static float SampleRiverDistance(
        float WorldX,
        float WorldY,
        const FCubusBiomeFieldSettings& Settings
    );

private:
    static float SampleNoise(
        float WorldX,
        float WorldY,
        float Frequency
    );

    static float SampleFbm(
        float WorldX,
        float WorldY,
        float Frequency,
        int32 Octaves,
        float Gain
    );

    static float SmoothStep(
        float EdgeMinimum,
        float EdgeMaximum,
        float Value
    );
};
