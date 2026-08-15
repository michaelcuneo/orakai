#pragma once

#include "CoreMinimal.h"
#include "CubusCore/Data/CubusBiomeTypes.h"
#include "CubusCore/Generation/CubusHydrologyField.h"

class UCubusGeologyProfile;

/** Thread-safe settings copied from a geology profile for biome sampling. */
struct ORAKAI_API FCubusBiomeFieldSettings
{
    bool  bEnabled               = false;
    float Frequency              = 0.004f;
    float ForestThreshold        = 0.15f;
    float WetlandRiverDistance   = 0.10f;
    float RockySlopeThreshold    = 1.15f;
    float RockyMinimumWorldZ     = 48.0f; // Legacy asset compatibility only.

    int32 PlainsSurfaceMaterialId  = 1;
    int32 ForestSurfaceMaterialId  = 7;
    int32 RockySurfaceMaterialId   = 3;
    int32 WetlandSurfaceMaterialId = 8;

    int32 BiomeOffsetX = 0;
    int32 BiomeOffsetY = 0;

    bool bGenerateRivers = false;

    float RiverFrequency = 0.0025f;
    float RiverWarpAmplitude = 48.0f;
    float RiverWarpFrequency = 0.006f;
    int32 RiverOffsetX = 0;
    int32 RiverOffsetY = 0;

    /** Optional explicit nival/snow-line datum. <= sea level means derive it from mountain scale. */
    float NivalWorldZ = 0.0f;

    FCubusHydrologySettings HydrologySettings;
    TArray<FCubusBiomeDefinition> Definitions;
};

/** Slow-varying seeded climate backbone, safe to interpolate between nearby columns. */
struct ORAKAI_API FCubusBiomeClimateContext
{
    float ProvinceMoisture = 0.0f;
    float RegionalMoisture = 0.0f;
    float LocalHumidity = 0.0f;
    float ProvinceTemperature = 0.0f;
    float RegionalTemperature = 0.0f;
    float LocalTemperature = 0.0f;
    float CommunityPatch[4] = {0.5f, 0.5f, 0.5f, 0.5f};
    float ParentMaterial = 0.5f;
};

/** Terrain/environment context supplied by the authoritative density column. */
struct ORAKAI_API FCubusBiomeTerrainContext
{
    float Drainage = 0.0f;
    float RockExposure = 0.0f;
    float MountainCore = 0.0f;
    float FoothillWeight = 0.0f;
    float Ridge = 0.0f;
    FVector2D Gradient = FVector2D::ZeroVector;

    bool bHasTerrainFormSample = false;
    FCubusTerrainFormSample TerrainFormSample;

    bool bHasHydrologySample = false;
    FCubusHydrologySample HydrologySample;
};

/** Continuous environmental state and ecological community blend for one world column. */
struct ORAKAI_API FCubusBiomeSample
{
    static constexpr int32 MaxCommunityBlendCount = 4;

    // Compatibility outputs. New systems should prefer CommunityBlend and the fields below.
    ECubusBiomeKind DominantBiome = ECubusBiomeKind::Plains;
    FName BiomeName = TEXT("Plains");
    float BiomeStrength = 1.0f;
    float PlainsWeight = 1.0f;
    float ForestWeight = 0.0f;
    float RockyWeight = 0.0f;
    float WetlandWeight = 0.0f;

    // Broad climate.
    float Moisture = 0.5f;
    float Temperature = 0.5f;

    // Elevation structure.
    ECubusElevationZone ElevationZone = ECubusElevationZone::Lowland;
    float ElevationNormalized = 0.0f;
    float TreeLineWeight = 1.0f;
    float AlpineInfluence = 0.0f;
    float NivalInfluence = 0.0f;

    // Local landform position. These are continuous even though TerrainPosition is categorical.
    ECubusTerrainPosition TerrainPosition = ECubusTerrainPosition::Midslope;
    float FloodplainInfluence = 0.0f;
    float ValleyFloorInfluence = 0.0f;
    float LowerSlopeInfluence = 0.0f;
    float ShoulderInfluence = 0.0f;
    float RidgeInfluence = 0.0f;
    float ColdAirPooling = 0.0f;

    // Aspect and long-term exposure.
    float AspectRadians = 0.0f;
    float SolarExposure = 0.5f;
    float WindExposure = 0.0f;
    float WindShelter = 1.0f;
    float RainShadow = 0.0f;
    float Exposure = 0.0f;

    // Hydrology.
    float RiverDistance = 1.0f;
    float RiverInfluence = 0.0f;
    float Drainage = 0.0f;
    float FlowConvergence = 0.0f;
    float SurfaceWetness = 0.0f;
    float SoilSaturation = 0.0f;
    float GroundwaterPotential = 0.0f;

    // Soil/substrate.
    float SoilDepth = 0.5f;
    float SoilMoisture = 0.5f;
    float SoilPermeability = 0.5f;
    float OrganicMatter = 0.5f;
    float Fertility = 0.5f;
    float RockExposure = 0.0f;
    float Erosion = 0.0f;
    float Disturbance = 0.0f;
    float CanopyPotential = 0.0f;
    float CanopyOpenness = 1.0f;

    float SurfaceWorldZ = 0.0f;
    float Slope = 0.0f;
    int32 SurfaceMaterialId = 1;
    int32 BiomeDefinitionIndex = INDEX_NONE;

    int32 CommunityBlendCount = 0;
    FCubusBiomeCommunityBlend CommunityBlend[MaxCommunityBlendCount];
};

/** Deterministic continuous ecology engine for the density world. */
class ORAKAI_API FCubusBiomeField
{
public:
    static FCubusBiomeFieldSettings MakeSettings(const UCubusGeologyProfile* GeologyProfile, int32 BiomeSeed, int32 RiverSeed);

    static FCubusBiomeFieldSettings BindHydrology(
        const FCubusBiomeFieldSettings& BaseSettings,
        const FCubusHydrologySettings& HydrologySettings
    );

    static FCubusBiomeSample Sample(
        float WorldX,
        float WorldY,
        float SurfaceWorldZ,
        float Slope,
        const FCubusBiomeFieldSettings& Settings,
        const FCubusBiomeTerrainContext& TerrainContext = FCubusBiomeTerrainContext()
    );

    static float SampleRiverDistance(float WorldX, float WorldY, const FCubusBiomeFieldSettings& Settings);

private:
    static float SampleNoise(float WorldX, float WorldY, float Frequency);
    static float SampleFbm(float WorldX, float WorldY, float Frequency, int32 Octaves, float Gain);
    static float SmoothStep(float EdgeMinimum, float EdgeMaximum, float Value);
    static float RangeSuitability(float Value, float Minimum, float Maximum, float Softness);
};
