#include "CubusCore/Vegetation/CubusVegetationCatalog.h"
#include "CubusCore/Data/CubusVegetationInstance.h"

#include "Engine/StaticMesh.h"
#include "UObject/SoftObjectPath.h"

namespace
{
    constexpr int32 ShrubType = 2;
    constexpr int32 BroadleafType = 3;
    constexpr int32 ConiferType = 6;

    float HashToUnitFloat(const uint32 Hash)
    {
        return static_cast<float>(Hash & 0x00ffffffu) /
            static_cast<float>(0x01000000u);
    }

    float SmoothRangeSuitability(
        const float Value,
        const float InMinimum,
        const float InMaximum,
        const float Softness
    )
    {
        const float Minimum = FMath::Min(InMinimum, InMaximum);
        const float Maximum = FMath::Max(InMinimum, InMaximum);
        const float SafeSoftness = FMath::Clamp(Softness, 0.01f, 0.5f);

        if (Minimum <= 0.0f && Maximum >= 1.0f)
        {
            return 1.0f;
        }

        const float EnterStart = FMath::Max(0.0f, Minimum - SafeSoftness);
        const float EnterEnd = FMath::Min(1.0f, Minimum + SafeSoftness);
        const float ExitStart = FMath::Max(0.0f, Maximum - SafeSoftness);
        const float ExitEnd = FMath::Min(1.0f, Maximum + SafeSoftness);

        const float Enter = Minimum <= 0.0f
            ? 1.0f
            : FMath::SmoothStep(EnterStart, FMath::Max(EnterStart + KINDA_SMALL_NUMBER, EnterEnd), Value);
        const float Exit = Maximum >= 1.0f
            ? 1.0f
            : 1.0f - FMath::SmoothStep(ExitStart, FMath::Max(ExitStart + KINDA_SMALL_NUMBER, ExitEnd), Value);
        return FMath::Clamp(Enter * Exit, 0.0f, 1.0f);
    }

    float CalculateHabitatSuitability(
        const FCubusVegetationInstance& Instance,
        const FCubusVegetationSpeciesCatalogEntry& Entry
    )
    {
        const FCubusVegetationHabitatEnvelope& Habitat = Entry.Habitat;
        const float Softness = Habitat.TransitionSoftness;
        const FCubusVegetationHabitatSample& Sample = Instance.Habitat;

        const float Moisture = SmoothRangeSuitability(Sample.GetMoisture(), Habitat.MinimumMoisture, Habitat.MaximumMoisture, Softness);
        const float Temperature = SmoothRangeSuitability(Sample.GetTemperature(), Habitat.MinimumTemperature, Habitat.MaximumTemperature, Softness);
        const float Soil = SmoothRangeSuitability(Sample.GetSoilDepth(), Habitat.MinimumSoilDepth, Habitat.MaximumSoilDepth, Softness);
        const float Drainage = SmoothRangeSuitability(Sample.GetDrainage(), Habitat.MinimumDrainage, Habitat.MaximumDrainage, Softness);
        const float River = SmoothRangeSuitability(Sample.GetRiverInfluence(), Habitat.MinimumRiverInfluence, Habitat.MaximumRiverInfluence, Softness);
        const float Rock = SmoothRangeSuitability(Sample.GetRockExposure(), Habitat.MinimumRockExposure, Habitat.MaximumRockExposure, Softness);
        const float Exposure = SmoothRangeSuitability(Sample.GetExposure(), Habitat.MinimumExposure, Habitat.MaximumExposure, Softness);
        const float Fertility = SmoothRangeSuitability(Sample.GetFertility(), Habitat.MinimumFertility, Habitat.MaximumFertility, Softness);
        const float Elevation = SmoothRangeSuitability(Sample.GetElevationNormalized(), Habitat.MinimumElevationNormalized, Habitat.MaximumElevationNormalized, Softness);
        const float TreeLine = SmoothRangeSuitability(Sample.GetTreeLineWeight(), Habitat.MinimumTreeLineWeight, Habitat.MaximumTreeLineWeight, Softness);
        const float Saturation = SmoothRangeSuitability(Sample.GetSoilSaturation(), Habitat.MinimumSoilSaturation, Habitat.MaximumSoilSaturation, Softness);
        const float Groundwater = SmoothRangeSuitability(Sample.GetGroundwaterPotential(), Habitat.MinimumGroundwaterPotential, Habitat.MaximumGroundwaterPotential, Softness);
        const float Solar = SmoothRangeSuitability(Sample.GetSolarExposure(), Habitat.MinimumSolarExposure, Habitat.MaximumSolarExposure, Softness);
        const float Erosion = SmoothRangeSuitability(Sample.GetErosion(), Habitat.MinimumErosion, Habitat.MaximumErosion, Softness);
        const float ColdPool = SmoothRangeSuitability(Sample.GetColdAirPooling(), Habitat.MinimumColdAirPooling, Habitat.MaximumColdAirPooling, Softness);
        const float Disturbance = SmoothRangeSuitability(Sample.GetDisturbance(), Habitat.MinimumDisturbance, Habitat.MaximumDisturbance, Softness);
        const float Canopy = SmoothRangeSuitability(Sample.GetCanopyPotential(), Habitat.MinimumCanopyPotential, Habitat.MaximumCanopyPotential, Softness);
        const float Coarseness = SmoothRangeSuitability(Sample.GetSoilCoarseness(), Habitat.MinimumSoilCoarseness, Habitat.MaximumSoilCoarseness, Softness);
        const float WaterHolding = SmoothRangeSuitability(Sample.GetWaterHoldingCapacity(), Habitat.MinimumWaterHoldingCapacity, Habitat.MaximumWaterHoldingCapacity, Softness);

        const float MaximumSlope = FMath::Clamp(Habitat.MaximumSlopeDegrees, 0.1f, 90.0f);
        const float SlopeSuitability = 1.0f - FMath::SmoothStep(
            MaximumSlope * 0.82f,
            MaximumSlope,
            Sample.GetSlopeDegrees()
        );

        // Geometric mean keeps every ecological constraint meaningful without
        // collapsing moderate multi-dimensional matches too aggressively.
        const float Product = FMath::Max(
            0.0f,
            Moisture * Temperature * Soil * Drainage * River * Rock * Exposure * Fertility *
            Elevation * TreeLine * Saturation * Groundwater * Solar * Erosion *
            ColdPool * Disturbance * Canopy * Coarseness * WaterHolding * SlopeSuitability
        );
        return FMath::Pow(Product, 1.0f / 20.0f);
    }
}

void FCubusVegetationCatalog::Rebuild(
    const TArray<FCubusVegetationSpeciesCatalogEntry>& InSpeciesCatalog
)
{
    SpeciesIndicesByType.Reset();
    TotalWeightByType.Reset();

    for (int32 SpeciesIndex = 0; SpeciesIndex < InSpeciesCatalog.Num(); ++SpeciesIndex)
    {
        const FCubusVegetationSpeciesCatalogEntry& Entry = InSpeciesCatalog[SpeciesIndex];
        if (Entry.TypeId <= 0 || Entry.GrowthStageMeshes.IsEmpty())
        {
            continue;
        }

        const float SafeWeight = FMath::Max(0.001f, Entry.Weight);
        SpeciesIndicesByType.FindOrAdd(Entry.TypeId).Add(SpeciesIndex);
        TotalWeightByType.FindOrAdd(Entry.TypeId) += SafeWeight;
    }
}

void FCubusVegetationCatalog::BuildDefaultsIfNeeded(
    TArray<FCubusVegetationSpeciesCatalogEntry>& SpeciesCatalog,
    bool bAutoSeedCatalogDefaults
)
{
    if (!bAutoSeedCatalogDefaults)
    {
        return;
    }

    auto UpgradeSpeciesToSkeletonStages = [](
        FCubusVegetationSpeciesCatalogEntry& Entry,
        const TCHAR* OldStageA,
        const TCHAR* OldStageB,
        const TCHAR* OldStageC,
        const TCHAR* OldStageD,
        const TCHAR* NewStageA,
        const TCHAR* NewStageB,
        const TCHAR* NewStageC,
        const TCHAR* NewStageD
    ) -> bool
    {
        if (Entry.GrowthStageMeshes.Num() < 4)
        {
            return false;
        }

        const FString StageA = Entry.GrowthStageMeshes[0].ToSoftObjectPath().ToString();
        const FString StageB = Entry.GrowthStageMeshes[1].ToSoftObjectPath().ToString();
        const FString StageC = Entry.GrowthStageMeshes[2].ToSoftObjectPath().ToString();
        const FString StageD = Entry.GrowthStageMeshes[3].ToSoftObjectPath().ToString();

        if (!StageA.Equals(OldStageA, ESearchCase::CaseSensitive) ||
            !StageB.Equals(OldStageB, ESearchCase::CaseSensitive) ||
            !StageC.Equals(OldStageC, ESearchCase::CaseSensitive) ||
            !StageD.Equals(OldStageD, ESearchCase::CaseSensitive))
        {
            return false;
        }

        Entry.GrowthStageMeshes[0] = TSoftObjectPtr<UObject>(FSoftObjectPath(NewStageA));
        Entry.GrowthStageMeshes[1] = TSoftObjectPtr<UObject>(FSoftObjectPath(NewStageB));
        Entry.GrowthStageMeshes[2] = TSoftObjectPtr<UObject>(FSoftObjectPath(NewStageC));
        Entry.GrowthStageMeshes[3] = TSoftObjectPtr<UObject>(FSoftObjectPath(NewStageD));
        return true;
    };

    if (!SpeciesCatalog.IsEmpty())
    {
        int32 UpgradedSpeciesCount = 0;
        for (FCubusVegetationSpeciesCatalogEntry& Entry : SpeciesCatalog)
        {
            bool bUpgraded = false;
            if (Entry.SpeciesId == TEXT("Elder"))
            {
                bUpgraded = UpgradeSpeciesToSkeletonStages(
                    Entry,
                    TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_A_Skeleton.Tree_Elder_01_A_Skeleton"),
                    TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_B_Skeleton.Tree_Elder_01_B_Skeleton"),
                    TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_C_Skeleton.Tree_Elder_01_C_Skeleton"),
                    TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_D_Skeleton.Tree_Elder_01_D_Skeleton"),
                    TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_A.Tree_Elder_01_A"),
                    TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_B.Tree_Elder_01_B"),
                    TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_C.Tree_Elder_01_C"),
                    TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_D.Tree_Elder_01_D")
                );
            }
            else if (Entry.SpeciesId == TEXT("NorwaySpruce"))
            {
                bUpgraded = UpgradeSpeciesToSkeletonStages(
                    Entry,
                    TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_A_Skeleton.Tree_Norway_Spruce_01_A_Skeleton"),
                    TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_B_Skeleton.Tree_Norway_Spruce_01_B_Skeleton"),
                    TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_C_Skeleton.Tree_Norway_Spruce_01_C_Skeleton"),
                    TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_D_Skeleton.Tree_Norway_Spruce_01_D_Skeleton"),
                    TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_A.Tree_Norway_Spruce_01_A"),
                    TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_B.Tree_Norway_Spruce_01_B"),
                    TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_C.Tree_Norway_Spruce_01_C"),
                    TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_D.Tree_Norway_Spruce_01_D")
                );
            }
            else if (Entry.SpeciesId == TEXT("Greasewood"))
            {
                bUpgraded = UpgradeSpeciesToSkeletonStages(
                    Entry,
                    TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01/Shrub_Greasewood_01_A_Skeleton.Shrub_Greasewood_01_A_Skeleton"),
                    TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01/Shrub_Greasewood_01_B_Skeleton.Shrub_Greasewood_01_B_Skeleton"),
                    TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01/Shrub_Greasewood_01_C_Skeleton.Shrub_Greasewood_01_C_Skeleton"),
                    TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01/Shrub_Greasewood_01_D_Skeleton.Shrub_Greasewood_01_D_Skeleton"),
                    TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01/Shrub_Greasewood_01_A.Shrub_Greasewood_01_A"),
                    TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01/Shrub_Greasewood_01_B.Shrub_Greasewood_01_B"),
                    TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01/Shrub_Greasewood_01_C.Shrub_Greasewood_01_C"),
                    TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01/Shrub_Greasewood_01_D.Shrub_Greasewood_01_D")
                );
            }

            if (bUpgraded)
            {
                ++UpgradedSpeciesCount;
            }
        }

        if (UpgradedSpeciesCount > 0)
        {
            UE_LOG(LogTemp, Display, TEXT("Cubus vegetation catalog: corrected %d species stage asset object paths"), UpgradedSpeciesCount);
        }
        return;
    }

    auto AddDefaultSpecies = [&SpeciesCatalog](
        const FName SpeciesId,
        const int32 TypeId,
        const TCHAR* StageA,
        const TCHAR* StageB,
        const TCHAR* StageC,
        const TCHAR* StageD
    )
    {
        FCubusVegetationSpeciesCatalogEntry Entry;
        Entry.SpeciesId = SpeciesId;
        Entry.TypeId = TypeId;
        Entry.Weight = 1.0f;

        if (TypeId == BroadleafType)
        {
            Entry.BiomeMask = static_cast<int32>(ECubusVegetationBiome::Forest) |
                static_cast<int32>(ECubusVegetationBiome::Plains) |
                static_cast<int32>(ECubusVegetationBiome::Wetland);
            Entry.Habitat.MinimumMoisture = 0.38f;
            Entry.Habitat.MinimumSoilDepth = 0.34f;
            Entry.Habitat.MaximumRockExposure = 0.55f;
            Entry.Habitat.MaximumExposure = 0.72f;
            Entry.Habitat.MinimumFertility = 0.30f;
            Entry.Habitat.MaximumElevationNormalized = 0.78f;
            Entry.Habitat.MinimumTreeLineWeight = 0.20f;
            Entry.Habitat.MaximumSoilSaturation = 0.86f;
            Entry.Habitat.MaximumErosion = 0.76f;
            Entry.Habitat.MaximumSlopeDegrees = 34.0f;
        }
        else if (TypeId == ConiferType)
        {
            Entry.BiomeMask = static_cast<int32>(ECubusVegetationBiome::Forest) |
                static_cast<int32>(ECubusVegetationBiome::Rocky);
            Entry.Habitat.MinimumMoisture = 0.28f;
            Entry.Habitat.MaximumTemperature = 0.72f;
            Entry.Habitat.MinimumSoilDepth = 0.18f;
            Entry.Habitat.MaximumRockExposure = 0.82f;
            Entry.Habitat.MaximumExposure = 0.90f;
            Entry.Habitat.MinimumElevationNormalized = 0.10f;
            Entry.Habitat.MaximumElevationNormalized = 0.94f;
            Entry.Habitat.MinimumTreeLineWeight = 0.04f;
            Entry.Habitat.MaximumSoilSaturation = 0.90f;
            Entry.Habitat.MaximumErosion = 0.86f;
            Entry.Habitat.MaximumSlopeDegrees = 40.0f;
        }
        else if (TypeId == ShrubType)
        {
            Entry.BiomeMask = static_cast<int32>(ECubusVegetationBiome::Plains) |
                static_cast<int32>(ECubusVegetationBiome::Forest) |
                static_cast<int32>(ECubusVegetationBiome::Wetland);
            Entry.Habitat.MaximumSoilDepth = 0.82f;
            Entry.Habitat.MaximumRiverInfluence = 0.75f;
            Entry.Habitat.MaximumSlopeDegrees = 48.0f;
        }

        Entry.GrowthStageMeshes.Reserve(4);
        Entry.GrowthStageMeshes.Add(TSoftObjectPtr<UObject>(FSoftObjectPath(StageA)));
        Entry.GrowthStageMeshes.Add(TSoftObjectPtr<UObject>(FSoftObjectPath(StageB)));
        Entry.GrowthStageMeshes.Add(TSoftObjectPtr<UObject>(FSoftObjectPath(StageC)));
        Entry.GrowthStageMeshes.Add(TSoftObjectPtr<UObject>(FSoftObjectPath(StageD)));
        SpeciesCatalog.Add(MoveTemp(Entry));
    };

    AddDefaultSpecies(
        TEXT("Elder"), BroadleafType,
        TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_A.Tree_Elder_01_A"),
        TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_B.Tree_Elder_01_B"),
        TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_C.Tree_Elder_01_C"),
        TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_D.Tree_Elder_01_D")
    );

    AddDefaultSpecies(
        TEXT("NorwaySpruce"), ConiferType,
        TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_A.Tree_Norway_Spruce_01_A"),
        TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_B.Tree_Norway_Spruce_01_B"),
        TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_C.Tree_Norway_Spruce_01_C"),
        TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_D.Tree_Norway_Spruce_01_D")
    );

    AddDefaultSpecies(
        TEXT("Greasewood"), ShrubType,
        TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01/Shrub_Greasewood_01_A.Shrub_Greasewood_01_A"),
        TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01/Shrub_Greasewood_01_B.Shrub_Greasewood_01_B"),
        TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01/Shrub_Greasewood_01_C.Shrub_Greasewood_01_C"),
        TEXT("/Game/Megaplant_Library/Shrub_Greasewood/Shrub_Greasewood_01/Shrub_Greasewood_01_D.Shrub_Greasewood_01_D")
    );
}

int32 FCubusVegetationCatalog::SelectSpeciesIndex(
    const FCubusVegetationInstance& Instance,
    const TArray<FCubusVegetationSpeciesCatalogEntry>& InSpeciesCatalog,
    const bool bClusterTreeFamilies,
    const int32 TreeFamilyCellSizeVoxels,
    const int32 RuntimeRandomizationSeed
) const
{
    const TArray<int32>* SpeciesIndices = SpeciesIndicesByType.Find(Instance.TypeId);
    if (SpeciesIndices == nullptr || SpeciesIndices->IsEmpty())
    {
        return INDEX_NONE;
    }

    struct FWeightedSpecies
    {
        int32 Index = INDEX_NONE;
        float Weight = 0.0f;
    };

    TArray<FWeightedSpecies, TInlineAllocator<32>> EligibleSpecies;
    float EligibleTotalWeight = 0.0f;

    for (const int32 SpeciesIndex : *SpeciesIndices)
    {
        if (!InSpeciesCatalog.IsValidIndex(SpeciesIndex))
        {
            continue;
        }

        const FCubusVegetationSpeciesCatalogEntry& Entry = InSpeciesCatalog[SpeciesIndex];
        if ((Entry.BiomeMask & Instance.BiomeMask) == 0)
        {
            continue;
        }

        const float HabitatSuitability = CalculateHabitatSuitability(Instance, Entry);
        if (HabitatSuitability <= 0.001f)
        {
            continue;
        }

        const float EffectiveWeight = FMath::Max(0.001f, Entry.Weight) * HabitatSuitability;
        EligibleSpecies.Add({SpeciesIndex, EffectiveWeight});
        EligibleTotalWeight += EffectiveWeight;
    }

    // Compatibility fallback for catalogs whose old biome masks do not include
    // a newly resolved community. Habitat still applies; we never fall back to
    // a species that is ecologically impossible at this location.
    if (EligibleSpecies.IsEmpty())
    {
        for (const int32 SpeciesIndex : *SpeciesIndices)
        {
            if (!InSpeciesCatalog.IsValidIndex(SpeciesIndex))
            {
                continue;
            }

            const FCubusVegetationSpeciesCatalogEntry& Entry = InSpeciesCatalog[SpeciesIndex];
            const float HabitatSuitability = CalculateHabitatSuitability(Instance, Entry);
            if (HabitatSuitability <= 0.001f)
            {
                continue;
            }

            const float EffectiveWeight = FMath::Max(0.001f, Entry.Weight) * HabitatSuitability;
            EligibleSpecies.Add({SpeciesIndex, EffectiveWeight});
            EligibleTotalWeight += EffectiveWeight;
        }
    }

    if (EligibleSpecies.IsEmpty() || EligibleTotalWeight <= 0.0f)
    {
        return INDEX_NONE;
    }

    const bool bTreeType = Instance.TypeId == BroadleafType || Instance.TypeId == ConiferType;
    uint32 SelectionHash = 0;

    if (bClusterTreeFamilies && bTreeType)
    {
        const int32 CellSize = FMath::Max(4, TreeFamilyCellSizeVoxels);
        const FIntVector FamilyCell(
            FMath::FloorToInt(static_cast<double>(Instance.WorldVoxel.X) / static_cast<double>(CellSize)),
            FMath::FloorToInt(static_cast<double>(Instance.WorldVoxel.Y) / static_cast<double>(CellSize)),
            0
        );
        SelectionHash = GetTypeHash(FamilyCell);
        SelectionHash = HashCombineFast(SelectionHash, GetTypeHash(Instance.TypeId));
        SelectionHash = HashCombineFast(SelectionHash, GetTypeHash(RuntimeRandomizationSeed));
    }
    else
    {
        SelectionHash = GetTypeHash(Instance.WorldVoxel);
        SelectionHash = HashCombineFast(SelectionHash, GetTypeHash(Instance.RotationYaw));
    }

    float Remaining = HashToUnitFloat(SelectionHash) * EligibleTotalWeight;
    for (const FWeightedSpecies& Species : EligibleSpecies)
    {
        Remaining -= Species.Weight;
        if (Remaining <= 0.0f)
        {
            return Species.Index;
        }
    }

    return EligibleSpecies.Last().Index;
}

int32 FCubusVegetationCatalog::ResolveGrowthStageIndex(
    const FCubusVegetationInstance& Instance,
    const int32 StageCount,
    const bool bClusterTreeFamilies,
    const int32 TreeFamilyCellSizeVoxels,
    const float TreeFamilyCenterJitterFraction,
    const float MatureTreeCoreRadius,
    const float YoungTreeRingRadius,
    const float SaplingTreeRingRadius,
    const float TreeFamilyGrowthNoise,
    const int32 RuntimeRandomizationSeed
) const
{
    if (StageCount <= 1)
    {
        return 0;
    }

    const bool bTreeType = Instance.TypeId == BroadleafType || Instance.TypeId == ConiferType;
    if (bClusterTreeFamilies && bTreeType)
    {
        const int32 CellSize = FMath::Max(4, TreeFamilyCellSizeVoxels);
        const int32 CellX = FMath::FloorToInt(static_cast<double>(Instance.WorldVoxel.X) / static_cast<double>(CellSize));
        const int32 CellY = FMath::FloorToInt(static_cast<double>(Instance.WorldVoxel.Y) / static_cast<double>(CellSize));
        const FIntVector FamilyCell(CellX, CellY, 0);

        uint32 FamilyHash = GetTypeHash(FamilyCell);
        FamilyHash = HashCombineFast(FamilyHash, GetTypeHash(Instance.TypeId));
        FamilyHash = HashCombineFast(FamilyHash, GetTypeHash(RuntimeRandomizationSeed));

        const float CenterJitter = FMath::Clamp(TreeFamilyCenterJitterFraction, 0.0f, 0.4f);
        const float CenterOffsetX = (HashToUnitFloat(HashCombineFast(FamilyHash, 0x68bc21ebu)) * 2.0f - 1.0f) * CenterJitter;
        const float CenterOffsetY = (HashToUnitFloat(HashCombineFast(FamilyHash, 0x02e5be93u)) * 2.0f - 1.0f) * CenterJitter;
        const float CenterX = (static_cast<float>(CellX) + 0.5f + CenterOffsetX) * static_cast<float>(CellSize);
        const float CenterY = (static_cast<float>(CellY) + 0.5f + CenterOffsetY) * static_cast<float>(CellSize);
        const float DeltaX = static_cast<float>(Instance.WorldVoxel.X) + 0.5f - CenterX;
        const float DeltaY = static_cast<float>(Instance.WorldVoxel.Y) + 0.5f - CenterY;
        const float MaxFamilyRadius = static_cast<float>(CellSize) * FMath::Sqrt(2.0f) * 0.5f;

        float NormalizedDistance = FVector2D(DeltaX, DeltaY).Size() / FMath::Max(1.0f, MaxFamilyRadius);
        uint32 GrowthNoiseHash = GetTypeHash(Instance.WorldVoxel);
        GrowthNoiseHash = HashCombineFast(GrowthNoiseHash, FamilyHash);
        const float GrowthNoise = (HashToUnitFloat(GrowthNoiseHash) * 2.0f - 1.0f) * FMath::Clamp(TreeFamilyGrowthNoise, 0.0f, 0.3f);
        NormalizedDistance = FMath::Clamp(NormalizedDistance + GrowthNoise, 0.0f, 1.0f);

        const float MatureRadius = FMath::Clamp(MatureTreeCoreRadius, 0.02f, 0.4f);
        const float YoungRadius = FMath::Clamp(FMath::Max(MatureRadius, YoungTreeRingRadius), MatureRadius, 0.8f);
        const float SaplingRadius = FMath::Clamp(FMath::Max(YoungRadius, SaplingTreeRingRadius), YoungRadius, 1.0f);

        if (StageCount >= 4)
        {
            if (NormalizedDistance <= MatureRadius) return StageCount - 1;
            if (NormalizedDistance <= YoungRadius) return StageCount - 2;
            if (NormalizedDistance <= SaplingRadius) return StageCount - 3;
            return 0;
        }

        return FMath::Clamp(
            FMath::RoundToInt((1.0f - NormalizedDistance) * static_cast<float>(StageCount - 1)),
            0,
            StageCount - 1
        );
    }

    uint32 GrowthHash = GetTypeHash(Instance.WorldVoxel);
    GrowthHash = HashCombineFast(GrowthHash, GetTypeHash(Instance.RotationYaw));
    GrowthHash = HashCombineFast(GrowthHash, GetTypeHash(Instance.Scale));
    return static_cast<int32>(GrowthHash % static_cast<uint32>(StageCount));
}
