#pragma once

#include "CoreMinimal.h"
#include "CubusVegetationTypes.generated.h"

/** Stable generated-instance type IDs shared by generation and rendering. */
namespace CubusVegetationType
{
    constexpr int32 Grass = 1;
    constexpr int32 Shrub = 2;
    constexpr int32 BroadleafTree = 3;
    constexpr int32 Reeds = 4;
    constexpr int32 Alpine = 5;
    constexpr int32 ConiferTree = 6;
    constexpr int32 StoneClutter = 7;
    constexpr int32 OrganicClutter = 8;
    constexpr int32 Count = 9;
}

UENUM(BlueprintType, meta = (Bitflags))
enum class ECubusVegetationBiome : uint8
{
    None = 0 UMETA(Hidden),
    Plains = 1 << 0,
    Forest = 1 << 1,
    Rocky = 1 << 2,
    Wetland = 1 << 3
};

/**
 * Habitat envelope for one authored vegetation species.
 *
 * All environmental fields are normalized to 0..1 except slope. Defaults are
 * intentionally permissive so existing catalogs preserve their behaviour until
 * a species is given a narrower ecological niche.
 */
USTRUCT(BlueprintType)
struct FCubusVegetationHabitatEnvelope
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumMoisture = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumMoisture = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumTemperature = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumTemperature = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumSoilDepth = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumSoilDepth = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumDrainage = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumDrainage = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumRiverInfluence = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumRiverInfluence = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumRockExposure = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumRockExposure = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumExposure = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumExposure = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumFertility = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumFertility = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Elevation", meta = (ClampMin = "0.0", ClampMax = "1.5"))
    float MinimumElevationNormalized = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Elevation", meta = (ClampMin = "0.0", ClampMax = "1.5"))
    float MaximumElevationNormalized = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Elevation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumTreeLineWeight = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Elevation", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumTreeLineWeight = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Hydrology", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumSoilSaturation = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Hydrology", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumSoilSaturation = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Hydrology", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumGroundwaterPotential = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Hydrology", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumGroundwaterPotential = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Exposure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumSolarExposure = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Exposure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumSolarExposure = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Terrain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumErosion = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Terrain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumErosion = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Climate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumColdAirPooling = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Climate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumColdAirPooling = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Disturbance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumDisturbance = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Disturbance", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumDisturbance = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Structure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumCanopyPotential = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Structure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumCanopyPotential = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Soil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumSoilCoarseness = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Soil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumSoilCoarseness = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Soil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MinimumWaterHoldingCapacity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat|Soil", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MaximumWaterHoldingCapacity = 1.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "deg"))
    float MaximumSlopeDegrees = 90.0f;

    /** Soft transition width around normalized range boundaries. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat", meta = (ClampMin = "0.01", ClampMax = "0.5"))
    float TransitionSoftness = 0.12f;
};

USTRUCT(BlueprintType)
struct FCubusVegetationSpeciesCatalogEntry
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Catalog")
    FName SpeciesId = NAME_None;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Catalog",
        meta = (ToolTip = "1 Grass, 2 Shrub, 3 Broadleaf, 4 Reeds, 5 Alpine, 6 Conifer, 7 Stone clutter, 8 Organic clutter")
    )
    int32 TypeId = 0;

    /** Base abundance before habitat suitability is applied. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Catalog", meta = (ClampMin = "0.001"))
    float Weight = 1.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Catalog",
        meta = (Bitmask, BitmaskEnum = "/Script/Orakai.ECubusVegetationBiome")
    )
    int32 BiomeMask = static_cast<int32>(ECubusVegetationBiome::Forest);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Vegetation|Habitat")
    FCubusVegetationHabitatEnvelope Habitat;

    /**
     * The authoritative growth-stage assets.
     *
     * Static stages render directly. Skeletal stages render in the near world
     * and use deterministic generated _FarProxy meshes in the far-tree path.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Vegetation|Catalog",
        meta = (AllowedClasses = "/Script/Engine.StaticMesh,/Script/Engine.SkeletalMesh")
    )
    TArray<TSoftObjectPtr<UObject>> GrowthStageMeshes;

    TSoftClassPtr<AActor> HeroPveActorClassOverride;
    TSoftObjectPtr<UObject> HeroPveActorAssetOverride;
};
