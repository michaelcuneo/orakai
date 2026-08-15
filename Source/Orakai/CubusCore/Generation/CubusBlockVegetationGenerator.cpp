#include "CubusCore/Generation/CubusBlockVegetationGenerator.h"

#include "CubusCore/Vegetation/CubusVegetationTypes.h"
#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Data/CubusBlockVoxel.h"
#include "CubusCore/Data/CubusGeologyProfile.h"
#include "CubusCore/Data/CubusVegetationInstance.h"
#include "CubusCore/Generation/CubusGenerationSeeds.h"
#include "CubusCore/Generation/CubusTerrainDensityField.h"

namespace CubusBlockVegetationGenerator
{
    struct FEcologyWeights
    {
        int32 BiomeMask = CubusVegetationBiome::All;
        float TreeDensity = 0.0f;
        float BroadleafScore = 0.0f;
        float ConiferScore = 0.0f;
        float GrassDensity = 0.0f;
        float ShrubDensity = 0.0f;
        float ReedDensity = 0.0f;
        float AlpineDensity = 0.0f;
    };

    int32 WholeChunkOffset(const int32 VoxelOffset)
    {
        return (VoxelOffset / Cubus::ChunkSize) * Cubus::ChunkSize;
    }

    bool IsLandmarkColumn(
        const int32 WorldX,
        const int32 WorldY,
        const int32 TerrainOffsetX,
        const int32 TerrainOffsetY,
        const FCubusLandmarkFieldSettings& LandmarkSettings
    )
    {
        return FCubusLandmarkField::Sample(
            static_cast<float>(WorldX + TerrainOffsetX),
            static_cast<float>(WorldY + TerrainOffsetY),
            LandmarkSettings
        ).IsInside();
    }

    float DominantEcologyStrength(const FCubusBiomeSample& BiomeSample)
    {
        return FMath::Clamp(BiomeSample.BiomeStrength, 0.0f, 1.0f);
    }

    float RangeSuitability(const float Value, const float Minimum, const float Maximum, const float Softness = 0.12f)
    {
        const float MinValue = FMath::Min(Minimum, Maximum);
        const float MaxValue = FMath::Max(Minimum, Maximum);
        const float Enter = MinValue <= 0.0f
            ? 1.0f
            : FMath::SmoothStep(FMath::Max(0.0f, MinValue - Softness), MinValue, Value);
        const float Exit = MaxValue >= 1.0f
            ? 1.0f
            : 1.0f - FMath::SmoothStep(MaxValue, FMath::Min(1.0f, MaxValue + Softness), Value);
        return FMath::Clamp(Enter * Exit, 0.0f, 1.0f);
    }

    int32 BuildBiomeMask(const FCubusBiomeSample& BiomeSample)
    {
        int32 Mask = 0;
        constexpr float EcotoneThreshold = 0.14f;
        if (BiomeSample.PlainsWeight >= EcotoneThreshold) Mask |= CubusVegetationBiome::Plains;
        if (BiomeSample.ForestWeight >= EcotoneThreshold) Mask |= CubusVegetationBiome::Forest;
        if (BiomeSample.RockyWeight >= EcotoneThreshold) Mask |= CubusVegetationBiome::Rocky;
        if (BiomeSample.WetlandWeight >= EcotoneThreshold) Mask |= CubusVegetationBiome::Wetland;

        if (Mask != 0)
        {
            return Mask;
        }

        switch (BiomeSample.DominantBiome)
        {
            case ECubusBiomeKind::Forest: return CubusVegetationBiome::Forest;
            case ECubusBiomeKind::Rocky: return CubusVegetationBiome::Rocky;
            case ECubusBiomeKind::Wetland: return CubusVegetationBiome::Wetland;
            case ECubusBiomeKind::Plains:
            default: return CubusVegetationBiome::Plains;
        }
    }

    FCubusVegetationHabitatSample MakeHabitatSample(const FCubusBiomeSample& BiomeSample)
    {
        FCubusVegetationHabitatSample Habitat;
        Habitat.Moisture = FCubusVegetationHabitatSample::QuantizeUnit(BiomeSample.Moisture);
        Habitat.Temperature = FCubusVegetationHabitatSample::QuantizeUnit(BiomeSample.Temperature);
        Habitat.SoilDepth = FCubusVegetationHabitatSample::QuantizeUnit(BiomeSample.SoilDepth);
        Habitat.Drainage = FCubusVegetationHabitatSample::QuantizeUnit(BiomeSample.Drainage);
        Habitat.RiverInfluence = FCubusVegetationHabitatSample::QuantizeUnit(BiomeSample.RiverInfluence);
        Habitat.RockExposure = FCubusVegetationHabitatSample::QuantizeUnit(BiomeSample.RockExposure);
        Habitat.Exposure = FCubusVegetationHabitatSample::QuantizeUnit(BiomeSample.Exposure);
        Habitat.Fertility = FCubusVegetationHabitatSample::QuantizeUnit(BiomeSample.Fertility);
        Habitat.ElevationNormalized = FCubusVegetationHabitatSample::QuantizeUnit(BiomeSample.ElevationNormalized / 1.5f);
        Habitat.TreeLineWeight = FCubusVegetationHabitatSample::QuantizeUnit(BiomeSample.TreeLineWeight);
        Habitat.SoilSaturation = FCubusVegetationHabitatSample::QuantizeUnit(BiomeSample.SoilSaturation);
        Habitat.GroundwaterPotential = FCubusVegetationHabitatSample::QuantizeUnit(BiomeSample.GroundwaterPotential);
        Habitat.SolarExposure = FCubusVegetationHabitatSample::QuantizeUnit(BiomeSample.SolarExposure);
        Habitat.Erosion = FCubusVegetationHabitatSample::QuantizeUnit(BiomeSample.Erosion);
        Habitat.ColdAirPooling = FCubusVegetationHabitatSample::QuantizeUnit(BiomeSample.ColdAirPooling);
        Habitat.Disturbance = FCubusVegetationHabitatSample::QuantizeUnit(BiomeSample.Disturbance);
        Habitat.CanopyPotential = FCubusVegetationHabitatSample::QuantizeUnit(BiomeSample.CanopyPotential);
        Habitat.SoilCoarseness = FCubusVegetationHabitatSample::QuantizeUnit(BiomeSample.SoilCoarseness);
        Habitat.WaterHoldingCapacity = FCubusVegetationHabitatSample::QuantizeUnit(BiomeSample.WaterHoldingCapacity);
        Habitat.SlopeDegrees = FCubusVegetationHabitatSample::QuantizeSlopeDegrees(
            FMath::RadiansToDegrees(FMath::Atan(BiomeSample.Slope))
        );
        return Habitat;
    }

    FEcologyWeights CalculateEcology(
        const FCubusBiomeSample& BiomeSample,
        const FCubusVegetationGenerationSettings& Settings
    )
    {
        FEcologyWeights Result;
        Result.BiomeMask = BuildBiomeMask(BiomeSample);

        if (!Settings.bUseConfiguredBiomes)
        {
            Result.BiomeMask = CubusVegetationBiome::Forest;
            Result.TreeDensity = FMath::Clamp(Settings.FallbackTreeDensity, 0.0f, 1.0f);
            Result.BroadleafScore = 0.72f;
            Result.ConiferScore = 0.28f;
            return Result;
        }

        const float Moisture = FMath::Clamp(BiomeSample.Moisture, 0.0f, 1.0f);
        const float Temperature = FMath::Clamp(BiomeSample.Temperature, 0.0f, 1.0f);
        const float Soil = FMath::Clamp(BiomeSample.SoilDepth, 0.0f, 1.0f);
        const float River = FMath::Clamp(BiomeSample.RiverInfluence, 0.0f, 1.0f);
        const float Rock = FMath::Clamp(BiomeSample.RockExposure, 0.0f, 1.0f);
        const float Exposure = FMath::Clamp(BiomeSample.Exposure, 0.0f, 1.0f);
        const float Fertility = FMath::Clamp(BiomeSample.Fertility, 0.0f, 1.0f);
        const float Elevation = FMath::Clamp(BiomeSample.ElevationNormalized, 0.0f, 1.5f);
        const float TreeLine = FMath::Clamp(BiomeSample.TreeLineWeight, 0.0f, 1.0f);
        const float Saturation = FMath::Clamp(BiomeSample.SoilSaturation, 0.0f, 1.0f);
        const float Groundwater = FMath::Clamp(BiomeSample.GroundwaterPotential, 0.0f, 1.0f);
        const float Solar = FMath::Clamp(BiomeSample.SolarExposure, 0.0f, 1.0f);
        const float Erosion = FMath::Clamp(BiomeSample.Erosion, 0.0f, 1.0f);
        const float ColdPool = FMath::Clamp(BiomeSample.ColdAirPooling, 0.0f, 1.0f);
        const float Disturbance = FMath::Clamp(BiomeSample.Disturbance, 0.0f, 1.0f);
        const float Canopy = FMath::Clamp(BiomeSample.CanopyPotential, 0.0f, 1.0f);
        const float CanopyOpen = 1.0f - Canopy;
        const float Coarseness = FMath::Clamp(BiomeSample.SoilCoarseness, 0.0f, 1.0f);
        const float WaterHolding = FMath::Clamp(BiomeSample.WaterHoldingCapacity, 0.0f, 1.0f);
        const float Alpine = FMath::Clamp(BiomeSample.AlpineInfluence, 0.0f, 1.0f);
        const float Nival = FMath::Clamp(BiomeSample.NivalInfluence, 0.0f, 1.0f);
        const float SlopeDegrees = FMath::RadiansToDegrees(FMath::Atan(BiomeSample.Slope));
        const float Gentle = 1.0f - FMath::SmoothStep(24.0f, 48.0f, SlopeDegrees);
        const float VeryGentle = 1.0f - FMath::SmoothStep(10.0f, 28.0f, SlopeDegrees);
        const float EcologyStrength = DominantEcologyStrength(BiomeSample);
        const float VisualTreeCover = FMath::Clamp(BiomeSample.VisualTreeCover, 0.0f, 1.0f);
        const float VisualConiferPreference = FMath::Clamp(BiomeSample.VisualConiferPreference, 0.0f, 1.0f);
        const float VisualShrubCover = FMath::Clamp(BiomeSample.VisualShrubCover, 0.0f, 1.0f);
        const float VisualHerbCover = FMath::Clamp(BiomeSample.VisualHerbCover, 0.0f, 1.0f);
        const float VisualReedCover = FMath::Clamp(BiomeSample.VisualReedCover, 0.0f, 1.0f);
        const float VisualAlpineCover = FMath::Clamp(BiomeSample.VisualAlpineCover, 0.0f, 1.0f);

        const float ForestTreeBase = Settings.ForestTreeDensity * BiomeSample.ForestWeight *
            FMath::Lerp(0.65f, 1.25f, FMath::Clamp(Settings.ForestGroveCoverage, 0.05f, 1.0f));
        const float WetlandTreeBase = Settings.WetlandTreeDensity * BiomeSample.WetlandWeight;
        const float PlainsTreeBase = Settings.PlainsTreeDensity * BiomeSample.PlainsWeight;
        const float RockyConiferBase = Settings.ForestTreeDensity * BiomeSample.RockyWeight * 0.22f;

        const float BroadleafHabitat =
            RangeSuitability(Moisture, 0.34f, 0.92f) *
            RangeSuitability(Temperature, 0.28f, 0.88f) *
            FMath::Lerp(0.25f, 1.0f, Soil) *
            FMath::Lerp(0.35f, 1.0f, Fertility) *
            FMath::Lerp(0.55f, 1.10f, WaterHolding) *
            FMath::Lerp(1.0f, 0.52f, Coarseness) *
            FMath::Lerp(1.0f, 0.35f, Exposure) *
            FMath::Lerp(1.0f, 0.30f, Rock) *
            RangeSuitability(Saturation, 0.02f, 0.82f) *
            RangeSuitability(Elevation, 0.0f, 0.76f) *
            TreeLine * (1.0f - Alpine * 0.78f) *
            Gentle;

        const float ConiferHabitat =
            RangeSuitability(Moisture, 0.22f, 0.88f) *
            RangeSuitability(Temperature, 0.08f, 0.74f) *
            FMath::Lerp(0.45f, 1.0f, Soil) *
            FMath::Lerp(1.0f, 0.72f, Exposure) *
            FMath::Lerp(1.0f, 0.70f, Rock) *
            RangeSuitability(Saturation, 0.01f, 0.88f) *
            RangeSuitability(Elevation, 0.12f, 0.94f) *
            FMath::Sqrt(TreeLine) * (1.0f - Nival) *
            (1.0f - FMath::SmoothStep(34.0f, 48.0f, SlopeDegrees));

        const float LegacyTreeBase = ForestTreeBase + WetlandTreeBase + PlainsTreeBase + RockyConiferBase;
        const float PhenotypeTreeBase = Settings.ForestTreeDensity * FMath::Lerp(0.04f, 1.85f, VisualTreeCover);
        const float TreeBase = FMath::Lerp(LegacyTreeBase, PhenotypeTreeBase, 0.78f);
        const float ArchetypeBroadleafFraction = FMath::Clamp(
            BiomeSample.ForestWeight * Settings.ForestBroadleafFraction +
            BiomeSample.WetlandWeight * 0.86f +
            BiomeSample.PlainsWeight * 0.88f +
            BiomeSample.RockyWeight * 0.15f,
            0.05f,
            0.95f
        );

        const float BroadleafFraction = FMath::Clamp(
            FMath::Lerp(ArchetypeBroadleafFraction, 1.0f - VisualConiferPreference, 0.82f),
            0.02f,
            0.98f
        );
        Result.BroadleafScore = BroadleafFraction * BroadleafHabitat;
        Result.ConiferScore = (1.0f - BroadleafFraction) * ConiferHabitat +
            BiomeSample.RockyWeight * ConiferHabitat * 0.18f;
        const float TreeHabitat = FMath::Clamp(Result.BroadleafScore + Result.ConiferScore, 0.0f, 1.25f);
        Result.TreeDensity = FMath::Clamp(
            TreeBase *
            FMath::Lerp(0.55f, 1.30f, EcologyStrength) *
            TreeHabitat *
            FMath::Lerp(0.48f, 1.18f, Canopy) *
            FMath::Lerp(1.0f, 0.42f, Disturbance),
            0.0f,
            1.0f
        );

        const float ReedHabitat =
            FMath::Max(FMath::SmoothStep(0.25f, 0.78f, River), FMath::SmoothStep(0.52f, 0.82f, Groundwater)) *
            FMath::SmoothStep(0.48f, 0.82f, Moisture) *
            FMath::SmoothStep(0.42f, 0.76f, Saturation) *
            FMath::Lerp(0.55f, 1.0f, Fertility) *
            FMath::Lerp(0.72f, 1.08f, WaterHolding) * VeryGentle * (1.0f - Nival);
        Result.ReedDensity = FMath::Clamp(
            Settings.WetlandReedDensity *
            FMath::Lerp(0.08f, 1.90f, VisualReedCover) * ReedHabitat,
            0.0f,
            1.0f
        );

        const float AlpineHabitat =
            Alpine * (1.0f - Nival) *
            RangeSuitability(Elevation, 0.62f, 1.02f) *
            RangeSuitability(Temperature, 0.0f, 0.58f) *
            FMath::Lerp(0.55f, 1.0f, Exposure) *
            FMath::Lerp(0.45f, 1.0f, FMath::Max(Rock, BiomeSample.RockyWeight)) *
            FMath::Lerp(0.82f, 1.12f, Coarseness) *
            (1.0f - River * 0.8f);
        Result.AlpineDensity = FMath::Clamp(
            Settings.RockyAlpineDensity *
            FMath::Lerp(0.08f, 2.10f, VisualAlpineCover) * AlpineHabitat,
            0.0f,
            1.0f
        );

        const float LegacyGroundBase = Settings.PlainsGroundCoverDensity *
            (BiomeSample.PlainsWeight + BiomeSample.ForestWeight * 0.35f + BiomeSample.RockyWeight * 0.16f);
        const float ModerateDisturbance = 1.0f - FMath::Clamp(FMath::Abs(Disturbance - 0.45f) / 0.55f, 0.0f, 1.0f);
        const float ShrubHabitat =
            RangeSuitability(Moisture, 0.12f, 0.76f) *
            FMath::Lerp(0.75f, 1.15f, Exposure) *
            FMath::Lerp(0.82f, 1.12f, Solar) *
            FMath::Lerp(1.1f, 0.62f, Soil) *
            FMath::Lerp(1.0f, 0.58f, Erosion) *
            FMath::Lerp(0.76f, 1.14f, CanopyOpen) *
            FMath::Lerp(0.86f, 1.10f, ModerateDisturbance) *
            (1.0f - River * 0.72f) * (1.0f - Nival) *
            (1.0f - FMath::SmoothStep(38.0f, 52.0f, SlopeDegrees));
        const float GrassHabitat =
            RangeSuitability(Moisture, 0.20f, 0.90f) *
            FMath::Lerp(0.42f, 1.0f, Soil) *
            FMath::Lerp(0.48f, 1.0f, Fertility) *
            FMath::Lerp(1.0f, 0.55f, Rock) *
            FMath::Lerp(1.0f, 0.66f, Erosion) *
            FMath::Lerp(0.58f, 1.12f, CanopyOpen) *
            FMath::Lerp(0.92f, 1.06f, ColdPool) *
            FMath::Lerp(0.82f, 1.08f, WaterHolding) *
            (1.0f - Nival) * Gentle;

        const float ShrubFraction = FMath::Clamp(Settings.PlainsShrubFraction, 0.0f, 1.0f);
        const float PhenotypeShrubBase = Settings.PlainsGroundCoverDensity * FMath::Lerp(0.04f, 1.75f, VisualShrubCover);
        const float PhenotypeHerbBase = Settings.PlainsGroundCoverDensity * FMath::Lerp(0.04f, 1.85f, VisualHerbCover);
        const float ShrubBase = FMath::Lerp(LegacyGroundBase * FMath::Lerp(0.24f, 0.78f, ShrubFraction), PhenotypeShrubBase, 0.82f);
        const float HerbBase = FMath::Lerp(LegacyGroundBase * FMath::Lerp(1.0f, 0.52f, ShrubFraction), PhenotypeHerbBase, 0.82f);
        Result.ShrubDensity = FMath::Clamp(ShrubBase * ShrubHabitat, 0.0f, 1.0f);
        Result.GrassDensity = FMath::Clamp(HerbBase * GrassHabitat, 0.0f, 1.0f);

        return Result;
    }
}

FCubusVegetationGenerationSettings
FCubusBlockVegetationGenerator::CaptureGenerationSettings(
    const UCubusGeologyProfile* GeologyProfile,
    const FCubusGenerationSeeds& GenerationSeeds
)
{
    FCubusVegetationGenerationSettings Settings;

    Settings.bUseConfiguredBiomes = IsValid(GeologyProfile) && GeologyProfile->bGenerateBiomes;
    Settings.BiomeSettings = FCubusBiomeField::MakeSettings(GeologyProfile, GenerationSeeds.Biomes, GenerationSeeds.Rivers);

    static TAtomic<bool> bLoggedBiomeRuntime(false);
    bool bExpected = false;
    if (bLoggedBiomeRuntime.CompareExchange(bExpected, true))
    {
        UE_LOG(
            LogTemp,
            Display,
            TEXT("Cubus biome runtime: profile=%s enabled=%s definitions=%d generationVersion=%u"),
            *GetPathNameSafe(GeologyProfile),
            Settings.bUseConfiguredBiomes ? TEXT("YES") : TEXT("NO - VEGETATION FALLBACK ACTIVE"),
            Settings.BiomeSettings.Definitions.Num(),
            FCubusGenerationSeeds::CurrentGenerationVersion
        );
    }
    Settings.LandmarkSettings = FCubusLandmarkField::MakeSettings(GeologyProfile, GenerationSeeds.Terrain);

    if (!IsValid(GeologyProfile))
    {
        return Settings;
    }

    Settings.ForestGroveCoverage = GeologyProfile->ForestGroveCoverage;
    Settings.ForestTreeDensity = GeologyProfile->ForestTreeDensity;
    Settings.ForestBroadleafFraction = GeologyProfile->ForestBroadleafFraction;
    Settings.WetlandTreeDensity = GeologyProfile->WetlandTreeDensity;
    Settings.WetlandReedDensity = GeologyProfile->WetlandReedDensity;
    Settings.RockyAlpineDensity = GeologyProfile->RockyAlpineDensity;
    Settings.PlainsTreeDensity = GeologyProfile->PlainsTreeDensity;
    Settings.PlainsShrubFraction = GeologyProfile->PlainsShrubFraction;
    Settings.PlainsGroundCoverDensity = GeologyProfile->PlainsGroundCoverDensity;
    Settings.FallbackTreeDensity = GeologyProfile->FallbackTreeDensity;
    return Settings;
}

FCubusBlockVegetationGenerator::FColumnSelection
FCubusBlockVegetationGenerator::ResolveColumnSelection(
    const int32 WorldX,
    const int32 WorldY,
    const int32 VegetationSeed,
    const FCubusBiomeSample& BiomeSample,
    const FCubusVegetationGenerationSettings& Settings
)
{
    FColumnSelection Result;
    Result.ActivePlacementRoll = HashToUnitFloat(HashWorldColumn(WorldX, WorldY, VegetationSeed ^ 101));

    const CubusBlockVegetationGenerator::FEcologyWeights Ecology =
        CubusBlockVegetationGenerator::CalculateEcology(BiomeSample, Settings);
    Result.BiomeMask = Ecology.BiomeMask;

    if (Ecology.TreeDensity > 0.0f && IsSpacedTreeCandidate(WorldX, WorldY, VegetationSeed ^ 463, Ecology.TreeDensity))
    {
        const float TreeScore = Ecology.BroadleafScore + Ecology.ConiferScore;
        if (TreeScore > KINDA_SMALL_NUMBER)
        {
            const float TreeRoll = HashToUnitFloat(HashWorldColumn(WorldX, WorldY, VegetationSeed ^ 149));
            Result.TypeId = TreeRoll < Ecology.BroadleafScore / TreeScore
                ? CubusVegetationType::BroadleafTree
                : CubusVegetationType::ConiferTree;
            Result.Density = 1.0f;
            Result.ActivePlacementRoll = 0.0f;
            return Result;
        }
    }

    struct FGroundCandidate
    {
        int32 TypeId;
        float Density;
    };

    const FGroundCandidate GroundCandidates[] = {
        {CubusVegetationType::Grass, Ecology.GrassDensity},
        {CubusVegetationType::Shrub, Ecology.ShrubDensity},
        {CubusVegetationType::Reeds, Ecology.ReedDensity},
        {CubusVegetationType::Alpine, Ecology.AlpineDensity}
    };

    float TotalGroundDensity = 0.0f;
    for (const FGroundCandidate& Candidate : GroundCandidates)
    {
        TotalGroundDensity += FMath::Max(0.0f, Candidate.Density);
    }

    if (TotalGroundDensity <= KINDA_SMALL_NUMBER)
    {
        return Result;
    }

    float Remaining = HashToUnitFloat(HashWorldColumn(WorldX, WorldY, VegetationSeed ^ 149)) * TotalGroundDensity;
    for (const FGroundCandidate& Candidate : GroundCandidates)
    {
        Remaining -= FMath::Max(0.0f, Candidate.Density);
        if (Remaining <= 0.0f)
        {
            Result.TypeId = Candidate.TypeId;
            break;
        }
    }
    Result.Density = FMath::Clamp(TotalGroundDensity, 0.0f, 1.0f);
    return Result;
}

float FCubusBlockVegetationGenerator::ResolveMaximumSlopeDegrees(const int32 TypeId)
{
    switch (TypeId)
    {
        case CubusVegetationType::BroadleafTree: return 34.0f;
        case CubusVegetationType::ConiferTree: return 40.0f;
        case CubusVegetationType::Grass: return 42.0f;
        case CubusVegetationType::Shrub: return 48.0f;
        case CubusVegetationType::Reeds: return 18.0f;
        case CubusVegetationType::Alpine: return 58.0f;
        default: return 89.0f;
    }
}

bool FCubusBlockVegetationGenerator::IsTreeType(const int32 TypeId)
{
    return TypeId == CubusVegetationType::BroadleafTree || TypeId == CubusVegetationType::ConiferTree;
}

float FCubusBlockVegetationGenerator::SampleDensitySlopeDegrees(
    const int32 WorldX,
    const int32 WorldY,
    const FCubusTerrainDensityField& DensityField
)
{
    const FCubusBiomeSample BiomeSample = DensityField.SampleSurfaceBiome(static_cast<float>(WorldX), static_cast<float>(WorldY));
    return FMath::RadiansToDegrees(FMath::Atan(BiomeSample.Slope));
}

void FCubusBlockVegetationGenerator::Generate(
    FCubusBlockChunkData& Chunk,
    const UCubusGeologyProfile* GeologyProfile,
    const FCubusTerrainDensityField* DensityField,
    const bool bGenerateWater,
    const int32 WaterLevel
)
{
    TArray<FCubusVegetationInstance> Instances;

    const FCubusGenerationSeeds& GenerationSeeds = Chunk.GetGenerationSeeds();
    const FCubusVegetationGenerationSettings GenerationSettings = CaptureGenerationSettings(GeologyProfile, GenerationSeeds);
    const bool bUseConfiguredBiomes = GenerationSettings.bUseConfiguredBiomes;
    const FIntVector ChunkCoordinate = Chunk.GetChunkCoordinate();
    const int32 BaseX = ChunkCoordinate.X * Cubus::ChunkSize;
    const int32 BaseY = ChunkCoordinate.Y * Cubus::ChunkSize;
    const int32 BaseZ = ChunkCoordinate.Z * Cubus::ChunkSize;
    const int32 VegetationSeed = GenerationSeeds.Vegetation;

    const int32 TerrainOffsetX = CubusBlockVegetationGenerator::WholeChunkOffset(FCubusGenerationSeeds::DomainOffsetX(GenerationSeeds.Terrain));
    const int32 TerrainOffsetY = CubusBlockVegetationGenerator::WholeChunkOffset(FCubusGenerationSeeds::DomainOffsetY(GenerationSeeds.Terrain));

    int32 CountsByType[CubusVegetationType::Count] = {};

    for (int32 LocalY = 0; LocalY < Cubus::ChunkSize; ++LocalY)
    {
        for (int32 LocalX = 0; LocalX < Cubus::ChunkSize; ++LocalX)
        {
            const int32 WorldX = BaseX + LocalX;
            const int32 WorldY = BaseY + LocalY;
            int32 SurfaceWorldZ = INDEX_NONE;
            float ApproximateSlopeDegrees = 0.0f;
            FCubusBiomeSample BiomeSample;

            if (DensityField != nullptr)
            {
                BiomeSample = DensityField->SampleSurfaceBiome(static_cast<float>(WorldX), static_cast<float>(WorldY));
                SurfaceWorldZ = FMath::RoundToInt(BiomeSample.SurfaceWorldZ);
                ApproximateSlopeDegrees = FMath::RadiansToDegrees(FMath::Atan(BiomeSample.Slope));

                const int32 SurfaceLocalZ = SurfaceWorldZ - BaseZ;
                if (SurfaceLocalZ < 0 || SurfaceLocalZ >= Cubus::ChunkSize - 1) continue;
                if (bGenerateWater && SurfaceWorldZ < WaterLevel) continue;
            }
            else
            {
                int32 SurfaceLocalZ = INDEX_NONE;
                for (int32 LocalZ = Cubus::ChunkSize - 1; LocalZ >= 0; --LocalZ)
                {
                    const FCubusBlockVoxel* Voxel = Chunk.GetVoxel(LocalX, LocalY, LocalZ);
                    if (Voxel != nullptr && Voxel->MaterialId > 0 && !Voxel->IsWater())
                    {
                        SurfaceLocalZ = LocalZ;
                        break;
                    }
                }

                if (SurfaceLocalZ == INDEX_NONE || SurfaceLocalZ >= Cubus::ChunkSize - 1) continue;
                const FCubusBlockVoxel* AboveVoxel = Chunk.GetVoxel(LocalX, LocalY, SurfaceLocalZ + 1);
                if (AboveVoxel == nullptr || AboveVoxel->MaterialId > 0 || AboveVoxel->IsWater()) continue;

                auto FindSurfaceLocalZ = [&Chunk](const int32 X, const int32 Y) -> int32
                {
                    for (int32 Z = Cubus::ChunkSize - 1; Z >= 0; --Z)
                    {
                        const FCubusBlockVoxel* Voxel = Chunk.GetVoxel(X, Y, Z);
                        if (Voxel != nullptr && Voxel->MaterialId > 0 && !Voxel->IsWater()) return Z;
                    }
                    return INDEX_NONE;
                };

                const int32 West = FindSurfaceLocalZ(FMath::Max(0, LocalX - 1), LocalY);
                const int32 East = FindSurfaceLocalZ(FMath::Min(Cubus::ChunkSize - 1, LocalX + 1), LocalY);
                const int32 South = FindSurfaceLocalZ(LocalX, FMath::Max(0, LocalY - 1));
                const int32 North = FindSurfaceLocalZ(LocalX, FMath::Min(Cubus::ChunkSize - 1, LocalY + 1));
                const float GradientX = West != INDEX_NONE && East != INDEX_NONE ? static_cast<float>(East - West) * 0.5f : 0.0f;
                const float GradientY = South != INDEX_NONE && North != INDEX_NONE ? static_cast<float>(North - South) * 0.5f : 0.0f;
                const float LocalSlope = FMath::Sqrt(GradientX * GradientX + GradientY * GradientY);

                SurfaceWorldZ = BaseZ + SurfaceLocalZ;
                ApproximateSlopeDegrees = FMath::RadiansToDegrees(FMath::Atan(LocalSlope));
                BiomeSample = FCubusBiomeField::Sample(
                    static_cast<float>(WorldX), static_cast<float>(WorldY), static_cast<float>(SurfaceWorldZ), LocalSlope,
                    GenerationSettings.BiomeSettings
                );
            }

            if (CubusBlockVegetationGenerator::IsLandmarkColumn(
                WorldX, WorldY, TerrainOffsetX, TerrainOffsetY, GenerationSettings.LandmarkSettings)) continue;

            const FColumnSelection Selection = ResolveColumnSelection(WorldX, WorldY, VegetationSeed, BiomeSample, GenerationSettings);
            if (Selection.TypeId <= 0 ||
                ApproximateSlopeDegrees > ResolveMaximumSlopeDegrees(Selection.TypeId) ||
                Selection.ActivePlacementRoll > FMath::Clamp(Selection.Density, 0.0f, 1.0f)) continue;

            FCubusVegetationInstance Instance;
            Instance.WorldVoxel = FIntVector(WorldX, WorldY, SurfaceWorldZ + 1);
            Instance.RotationYaw = HashToUnitFloat(HashWorldColumn(WorldX, WorldY, VegetationSeed ^ 211)) * 360.0f;
            Instance.Scale = FMath::Lerp(0.85f, 1.15f, HashToUnitFloat(HashWorldColumn(WorldX, WorldY, VegetationSeed ^ 307)));
            if (IsTreeType(Selection.TypeId))
            {
                Instance.Scale *= FMath::Lerp(0.86f, 1.18f, BiomeSample.VisualTreeCover);
            }
            else if (Selection.TypeId == CubusVegetationType::Shrub)
            {
                Instance.Scale *= FMath::Lerp(0.84f, 1.16f, BiomeSample.VisualShrubCover);
            }
            Instance.TypeId = Selection.TypeId;
            Instance.BiomeMask = Selection.BiomeMask;
            Instance.Habitat = CubusBlockVegetationGenerator::MakeHabitatSample(BiomeSample);

            Instances.Add(Instance);
            if (Selection.TypeId > 0 && Selection.TypeId < CubusVegetationType::Count) ++CountsByType[Selection.TypeId];
        }
    }

    Chunk.SetVegetationInstances(MoveTemp(Instances));

    UE_LOG(
        LogTemp, Verbose,
        TEXT("Cubus vegetation chunk (%d, %d, %d), seed %d: grass %d, shrubs %d, broadleaf %d, conifers %d, reeds %d, alpine %d%s"),
        ChunkCoordinate.X, ChunkCoordinate.Y, ChunkCoordinate.Z, VegetationSeed,
        CountsByType[CubusVegetationType::Grass], CountsByType[CubusVegetationType::Shrub],
        CountsByType[CubusVegetationType::BroadleafTree], CountsByType[CubusVegetationType::ConiferTree],
        CountsByType[CubusVegetationType::Reeds], CountsByType[CubusVegetationType::Alpine],
        bUseConfiguredBiomes ? TEXT("") : TEXT(" (fallback)")
    );
}

void FCubusBlockVegetationGenerator::GenerateTreesForRegion(
    const FCubusVegetationRegion& Region,
    const FCubusGenerationSeeds& GenerationSeeds,
    const FCubusVegetationGenerationSettings& GenerationSettings,
    const FCubusTerrainDensityField& DensityField,
    TArray<FCubusVegetationInstance>& OutTrees
)
{
    OutTrees.Reset();
    if (Region.Maximum.X <= Region.Minimum.X || Region.Maximum.Y <= Region.Minimum.Y) return;

    const int32 TerrainOffsetX = CubusBlockVegetationGenerator::WholeChunkOffset(FCubusGenerationSeeds::DomainOffsetX(GenerationSeeds.Terrain));
    const int32 TerrainOffsetY = CubusBlockVegetationGenerator::WholeChunkOffset(FCubusGenerationSeeds::DomainOffsetY(GenerationSeeds.Terrain));
    const int32 VegetationSeed = GenerationSeeds.Vegetation;
    const int64 Width = static_cast<int64>(Region.Maximum.X - Region.Minimum.X);
    const int64 Height = static_cast<int64>(Region.Maximum.Y - Region.Minimum.Y);
    OutTrees.Reserve(static_cast<int32>(FMath::Min<int64>((Width * Height) / 16, MAX_int32)));

    for (int32 WorldY = Region.Minimum.Y; WorldY < Region.Maximum.Y; ++WorldY)
    {
        for (int32 WorldX = Region.Minimum.X; WorldX < Region.Maximum.X; ++WorldX)
        {
            if (CubusBlockVegetationGenerator::IsLandmarkColumn(
                WorldX, WorldY, TerrainOffsetX, TerrainOffsetY, GenerationSettings.LandmarkSettings)) continue;

            const FCubusBiomeSample BiomeSample = DensityField.SampleSurfaceBiome(static_cast<float>(WorldX), static_cast<float>(WorldY));
            const FColumnSelection Selection = ResolveColumnSelection(WorldX, WorldY, VegetationSeed, BiomeSample, GenerationSettings);

            if (!IsTreeType(Selection.TypeId) ||
                FMath::RadiansToDegrees(FMath::Atan(BiomeSample.Slope)) > ResolveMaximumSlopeDegrees(Selection.TypeId) ||
                Selection.ActivePlacementRoll > FMath::Clamp(Selection.Density, 0.0f, 1.0f)) continue;

            FCubusVegetationInstance Instance;
            Instance.WorldVoxel = FIntVector(WorldX, WorldY, FMath::RoundToInt(BiomeSample.SurfaceWorldZ) + 1);
            Instance.RotationYaw = HashToUnitFloat(HashWorldColumn(WorldX, WorldY, VegetationSeed ^ 211)) * 360.0f;
            Instance.Scale = FMath::Lerp(0.85f, 1.15f, HashToUnitFloat(HashWorldColumn(WorldX, WorldY, VegetationSeed ^ 307)));
            Instance.Scale *= FMath::Lerp(0.86f, 1.18f, BiomeSample.VisualTreeCover);
            Instance.TypeId = Selection.TypeId;
            Instance.BiomeMask = Selection.BiomeMask;
            Instance.Habitat = CubusBlockVegetationGenerator::MakeHabitatSample(BiomeSample);
            OutTrees.Add(Instance);
        }
    }
}

void FCubusBlockVegetationGenerator::GenerateFarTreesForRegion(
    const FCubusVegetationRegion& Region,
    const FCubusGenerationSeeds& GenerationSeeds,
    const FCubusVegetationGenerationSettings& GenerationSettings,
    const FCubusTerrainDensityField& DensityField,
    const int32 SampleStrideVoxels,
    const float DensityScale,
    TArray<FCubusVegetationInstance>& OutTrees
)
{
    OutTrees.Reset();
    if (Region.Maximum.X <= Region.Minimum.X || Region.Maximum.Y <= Region.Minimum.Y) return;

    const int32 SafeStride = FMath::Clamp(SampleStrideVoxels, 2, 64);
    const float SafeDensityScale = FMath::Clamp(DensityScale, 0.0f, 1.0f);
    if (SafeDensityScale <= 0.0f) return;

    const int32 VegetationSeed = GenerationSeeds.Vegetation;
    const int32 TerrainOffsetX = CubusBlockVegetationGenerator::WholeChunkOffset(FCubusGenerationSeeds::DomainOffsetX(GenerationSeeds.Terrain));
    const int32 TerrainOffsetY = CubusBlockVegetationGenerator::WholeChunkOffset(FCubusGenerationSeeds::DomainOffsetY(GenerationSeeds.Terrain));

    const int32 FirstCellX = FMath::FloorToInt(static_cast<double>(Region.Minimum.X) / static_cast<double>(SafeStride));
    const int32 FirstCellY = FMath::FloorToInt(static_cast<double>(Region.Minimum.Y) / static_cast<double>(SafeStride));
    const int32 LastCellX = FMath::FloorToInt(static_cast<double>(Region.Maximum.X - 1) / static_cast<double>(SafeStride));
    const int32 LastCellY = FMath::FloorToInt(static_cast<double>(Region.Maximum.Y - 1) / static_cast<double>(SafeStride));

    for (int32 CellY = FirstCellY; CellY <= LastCellY; ++CellY)
    {
        for (int32 CellX = FirstCellX; CellX <= LastCellX; ++CellX)
        {
            const int32 CandidateOffsetX = FMath::Min(
                SafeStride - 1,
                FMath::FloorToInt(HashToUnitFloat(HashWorldColumn(CellX, CellY, VegetationSeed ^ 1201)) * static_cast<float>(SafeStride))
            );
            const int32 CandidateOffsetY = FMath::Min(
                SafeStride - 1,
                FMath::FloorToInt(HashToUnitFloat(HashWorldColumn(CellX, CellY, VegetationSeed ^ 1217)) * static_cast<float>(SafeStride))
            );

            const int32 WorldX = CellX * SafeStride + CandidateOffsetX;
            const int32 WorldY = CellY * SafeStride + CandidateOffsetY;
            if (WorldX < Region.Minimum.X || WorldX >= Region.Maximum.X || WorldY < Region.Minimum.Y || WorldY >= Region.Maximum.Y) continue;
            if (CubusBlockVegetationGenerator::IsLandmarkColumn(
                WorldX, WorldY, TerrainOffsetX, TerrainOffsetY, GenerationSettings.LandmarkSettings)) continue;

            const FCubusBiomeSample BiomeSample = DensityField.SampleSurfaceBiome(static_cast<float>(WorldX), static_cast<float>(WorldY));
            const CubusBlockVegetationGenerator::FEcologyWeights Ecology =
                CubusBlockVegetationGenerator::CalculateEcology(BiomeSample, GenerationSettings);
            const float TreeScore = Ecology.BroadleafScore + Ecology.ConiferScore;
            if (Ecology.TreeDensity <= 0.0f || TreeScore <= KINDA_SMALL_NUMBER) continue;

            const float BroadleafFraction = Ecology.BroadleafScore / TreeScore;
            const float SelectedTreeSlopeLimit = BroadleafFraction >= 0.5f ? 34.0f : 40.0f;
            if (FMath::RadiansToDegrees(FMath::Atan(BiomeSample.Slope)) > SelectedTreeSlopeLimit) continue;

            const float RepresentedArea = static_cast<float>(SafeStride * SafeStride);
            const float RepresentativeProbability = FMath::Clamp(Ecology.TreeDensity * RepresentedArea * SafeDensityScale, 0.0f, 1.0f);
            if (HashToUnitFloat(HashWorldColumn(CellX, CellY, VegetationSeed ^ 1237)) > RepresentativeProbability) continue;

            FCubusVegetationInstance Instance;
            Instance.WorldVoxel = FIntVector(WorldX, WorldY, FMath::RoundToInt(BiomeSample.SurfaceWorldZ) + 1);
            Instance.RotationYaw = HashToUnitFloat(HashWorldColumn(CellX, CellY, VegetationSeed ^ 1277)) * 360.0f;
            Instance.Scale = FMath::Lerp(0.90f, 1.20f, HashToUnitFloat(HashWorldColumn(CellX, CellY, VegetationSeed ^ 1301)));
            Instance.TypeId = HashToUnitFloat(HashWorldColumn(CellX, CellY, VegetationSeed ^ 1259)) < BroadleafFraction
                ? CubusVegetationType::BroadleafTree
                : CubusVegetationType::ConiferTree;
            Instance.BiomeMask = Ecology.BiomeMask;
            Instance.Habitat = CubusBlockVegetationGenerator::MakeHabitatSample(BiomeSample);
            OutTrees.Add(Instance);
        }
    }
}

bool FCubusBlockVegetationGenerator::IsSpacedTreeCandidate(
    const int32 WorldX,
    const int32 WorldY,
    const int32 Seed,
    const float TargetDensity
)
{
    const float SafeDensity = FMath::Clamp(TargetDensity, 0.0f, 1.0f);
    if (SafeDensity <= 0.0f) return false;

    const int32 CellSize = FMath::Clamp(FMath::RoundToInt(FMath::Sqrt(1.0f / SafeDensity)), 2, 64);
    const int32 CellX = FMath::FloorToInt(static_cast<double>(WorldX) / static_cast<double>(CellSize));
    const int32 CellY = FMath::FloorToInt(static_cast<double>(WorldY) / static_cast<double>(CellSize));
    const int32 CandidateX = CellX * CellSize + FMath::Min(
        CellSize - 1,
        FMath::FloorToInt(HashToUnitFloat(HashWorldColumn(CellX, CellY, Seed ^ 811)) * static_cast<float>(CellSize))
    );
    const int32 CandidateY = CellY * CellSize + FMath::Min(
        CellSize - 1,
        FMath::FloorToInt(HashToUnitFloat(HashWorldColumn(CellX, CellY, Seed ^ 947)) * static_cast<float>(CellSize))
    );
    return WorldX == CandidateX && WorldY == CandidateY;
}

uint32 FCubusBlockVegetationGenerator::HashWorldColumn(const int32 WorldX, const int32 WorldY, const int32 Salt)
{
    uint32 Hash = static_cast<uint32>(WorldX) * 0x8da6b343u;
    Hash ^= static_cast<uint32>(WorldY) * 0xd8163841u;
    Hash ^= static_cast<uint32>(Salt) * 0xcb1ab31fu;
    Hash ^= Hash >> 13;
    Hash *= 0x85ebca6bu;
    Hash ^= Hash >> 16;
    return Hash;
}

float FCubusBlockVegetationGenerator::HashToUnitFloat(const uint32 Hash)
{
    return static_cast<float>(Hash & 0x00ffffffu) / static_cast<float>(0x01000000u);
}
