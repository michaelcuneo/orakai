#pragma once

#include "CoreMinimal.h"

#include "CubusBiomeTypes.generated.h"

/** Broad habitat behaviour retained for compatibility with vegetation/gameplay rules. */
UENUM(BlueprintType)
enum class ECubusBiomeKind : uint8
{
    Plains,
    Forest,
    Rocky,
    Wetland
};

/** Broad, slow-varying climate province independent of local slope and drainage. */
UENUM(BlueprintType)
enum class ECubusClimateProvince : uint8
{
    HumidCool,
    HumidTemperate,
    TemperateTransition,
    ContinentalInterior,
    DryWarm,
    DryCool
};

/** Structural elevation zone derived from sea level, treeline and the alpine/nival scale. */
UENUM(BlueprintType)
enum class ECubusElevationZone : uint8
{
    Lowland,
    Foothill,
    Montane,
    Subalpine,
    Alpine,
    Nival
};

/** Topographic position within the local landform, independent of named biome. */
UENUM(BlueprintType)
enum class ECubusTerrainPosition : uint8
{
    Floodplain,
    ValleyFloor,
    LowerSlope,
    Midslope,
    Shoulder,
    Ridge
};

/** One weighted ecological community retained in the top-four local ecotone blend. */
USTRUCT(BlueprintType)
struct ORAKAI_API FCubusBiomeCommunityBlend
{
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cubus|Biomes|Blend")
    int32 DefinitionIndex = INDEX_NONE;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cubus|Biomes|Blend")
    FName Name = NAME_None;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cubus|Biomes|Blend")
    ECubusBiomeKind Archetype = ECubusBiomeKind::Plains;

    /** Normalized share of this community in the local ecotone. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cubus|Biomes|Blend", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Weight = 0.0f;

    /** Raw environmental suitability before competition with other communities. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cubus|Biomes|Blend", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float Suitability = 0.0f;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Cubus|Biomes|Blend", meta = (ClampMin = "1"))
    int32 SurfaceMaterialId = 1;
};

/**
 * One client-authored ecological community definition.
 *
 * Archetype is deliberately broad and retained for legacy vegetation masks.
 * The real community is selected from climate, relative elevation, terrain,
 * hydrology, soil and exposure. Multiple matching definitions are retained in
 * the runtime top-four blend rather than throwing away every non-winner.
 */
USTRUCT(BlueprintType)
struct ORAKAI_API FCubusBiomeDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes")
    FName Name = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes")
    ECubusBiomeKind Archetype = ECubusBiomeKind::Plains;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes", meta = (ClampMin = "1"))
    int32 SurfaceMaterialId = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Climate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float TargetMoisture = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Climate", meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float MoistureTolerance = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Climate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float TargetTemperature = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Climate", meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float TemperatureTolerance = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Elevation")
    float MinimumWorldZ = -100000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Elevation")
    float MaximumWorldZ = 100000.0f;

    /** Relative 0..1 position from sea level to the nival/snow-line datum. Values above 1 are summit terrain. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Elevation", meta = (ClampMin = "0.0", ClampMax = "1.5"))
    float MinimumElevationNormalized = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Elevation", meta = (ClampMin = "0.0", ClampMax = "1.5"))
    float MaximumElevationNormalized = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Terrain", meta = (ClampMin = "0.0"))
    float MaximumSlope = 2.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Soil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumSoilDepth = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Soil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumSoilDepth = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Hydrology", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumDrainage = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Hydrology", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumDrainage = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Hydrology", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumRiverInfluence = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Hydrology", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumRiverInfluence = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Hydrology", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumSoilSaturation = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Hydrology", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumSoilSaturation = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Hydrology", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumGroundwaterPotential = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Hydrology", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumGroundwaterPotential = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Exposure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumRockExposure = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Exposure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumRockExposure = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Exposure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumExposure = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Exposure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumExposure = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Exposure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumSolarExposure = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Exposure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumSolarExposure = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Climate|Topography", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumOrographicLift = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Climate|Topography", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumOrographicLift = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Climate|Topography", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumRainShadow = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Climate|Topography", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumRainShadow = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Climate|Topography", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumSolarOcclusion = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Climate|Topography", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumSolarOcclusion = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Soil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumFertility = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Soil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumFertility = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Soil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumOrganicMatter = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Soil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumOrganicMatter = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Substrate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumSubstrateHardness = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Substrate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumSubstrateHardness = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Substrate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumFractureDensity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Substrate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumFractureDensity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Soil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumSoilCoarseness = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Soil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumSoilCoarseness = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Soil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumWaterHoldingCapacity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Soil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumWaterHoldingCapacity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Terrain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumErosion = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Terrain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumErosion = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Terrain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumValleyFloorInfluence = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Terrain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumValleyFloorInfluence = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Climate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumColdAirPooling = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Climate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumColdAirPooling = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Disturbance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumDisturbance = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Disturbance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumDisturbance = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Structure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumCanopyPotential = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Structure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumCanopyPotential = 1.0f;

    /** Coherent stand/opening modulation applied only after habitat suitability is established. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Structure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float PatchStrength = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes", meta = (ClampMin = "0.01", ClampMax = "0.5"))
    float TransitionSoftness = 0.12f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes", meta = (ClampMin = "0.01"))
    float Priority = 1.0f;
};
