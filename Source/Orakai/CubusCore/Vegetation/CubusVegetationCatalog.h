#pragma once

#include "CoreMinimal.h"

#include "CubusCore/Vegetation/CubusVegetationTypes.h"

struct FCubusVegetationInstance;

class ORAKAI_API FCubusVegetationCatalog
{
public:
    void Rebuild(
        const TArray<FCubusVegetationSpeciesCatalogEntry>& InSpeciesCatalog
    );

    void BuildDefaultsIfNeeded(
      TArray<FCubusVegetationSpeciesCatalogEntry>& SpeciesCatalog,
        bool bAutoSeedCatalogDefaults
    );

    int32 SelectSpeciesIndex(
        const FCubusVegetationInstance& Instance,
        const TArray<FCubusVegetationSpeciesCatalogEntry>& InSpeciesCatalog,
        bool bClusterTreeFamilies,
        int32 TreeFamilyCellSizeVoxels,
        int32 RuntimeRandomizationSeed
    ) const;

    int32 ResolveGrowthStageIndex(
        const FCubusVegetationInstance& Instance,
        int32 StageCount,
        bool bClusterTreeFamilies,
        int32 TreeFamilyCellSizeVoxels,
        float TreeFamilyCenterJitterFraction,
        float MatureTreeCoreRadius,
        float YoungTreeRingRadius,
        float SaplingTreeRingRadius,
        float TreeFamilyGrowthNoise,
        int32 RuntimeRandomizationSeed
    ) const;

private:
    TMap<int32, TArray<int32>> SpeciesIndicesByType;
    TMap<int32, float> TotalWeightByType;
};