#include "CubusCore/Vegetation/CubusVegetationCatalog.h"
#include "CubusCore/Data/CubusVegetationInstance.h"

#include "Engine/StaticMesh.h"
#include "UObject/SoftObjectPath.h"

namespace
{
    // Temprary constants for vegetation type IDs. These are not guaranteed to be stable across Cubus versions.
    constexpr int32 ShrubType = 2;
    constexpr int32 BroadleafType = 3;
    constexpr int32 ConiferType = 6;

    float HashToUnitFloat(const uint32 Hash)
    {
        return static_cast<float>(Hash & 0x00ffffffu) /
            static_cast<float>(0x01000000u);
    }
}

void FCubusVegetationCatalog::Rebuild(
    const TArray<FCubusVegetationSpeciesCatalogEntry>& InSpeciesCatalog
)
{
    SpeciesIndicesByType.Reset();
    TotalWeightByType.Reset();

    for (
        int32 SpeciesIndex = 0;
        SpeciesIndex < InSpeciesCatalog.Num();
        ++SpeciesIndex
    )
    {
        const FCubusVegetationSpeciesCatalogEntry& Entry =
            InSpeciesCatalog[SpeciesIndex];

        if (
            Entry.TypeId <= 0 ||
            (
                Entry.GrowthStageMeshes.IsEmpty() &&
                Entry.StaticGrowthStageMeshes.IsEmpty()
            )
        )
        {
            continue;
        }

        const float SafeWeight =
            FMath::Max(0.001f, Entry.Weight);

        SpeciesIndicesByType
            .FindOrAdd(Entry.TypeId)
            .Add(SpeciesIndex);

        TotalWeightByType.FindOrAdd(Entry.TypeId) +=
            SafeWeight;
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

        const FString StageA =
            Entry.GrowthStageMeshes[0].ToSoftObjectPath().ToString();
        const FString StageB =
            Entry.GrowthStageMeshes[1].ToSoftObjectPath().ToString();
        const FString StageC =
            Entry.GrowthStageMeshes[2].ToSoftObjectPath().ToString();
        const FString StageD =
            Entry.GrowthStageMeshes[3].ToSoftObjectPath().ToString();

        if (
            !StageA.Equals(OldStageA, ESearchCase::CaseSensitive) ||
            !StageB.Equals(OldStageB, ESearchCase::CaseSensitive) ||
            !StageC.Equals(OldStageC, ESearchCase::CaseSensitive) ||
            !StageD.Equals(OldStageD, ESearchCase::CaseSensitive)
        )
        {
            return false;
        }

        Entry.GrowthStageMeshes[0] =
            TSoftObjectPtr<UObject>(FSoftObjectPath(NewStageA));
        Entry.GrowthStageMeshes[1] =
            TSoftObjectPtr<UObject>(FSoftObjectPath(NewStageB));
        Entry.GrowthStageMeshes[2] =
            TSoftObjectPtr<UObject>(FSoftObjectPath(NewStageC));
        Entry.GrowthStageMeshes[3] =
            TSoftObjectPtr<UObject>(FSoftObjectPath(NewStageD));
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
            UE_LOG(
                LogTemp,
                Display,
                TEXT("Cubus vegetation catalog: corrected %d species stage asset object paths"),
                UpgradedSpeciesCount
            );
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
            Entry.BiomeMask =
                static_cast<int32>(ECubusVegetationBiome::Forest) |
                static_cast<int32>(ECubusVegetationBiome::Plains) |
                static_cast<int32>(ECubusVegetationBiome::Wetland);
        }
        else if (TypeId == ConiferType)
        {
            Entry.BiomeMask =
                static_cast<int32>(ECubusVegetationBiome::Forest) |
                static_cast<int32>(ECubusVegetationBiome::Rocky);
        }
        else if (TypeId == ShrubType)
        {
            Entry.BiomeMask =
                static_cast<int32>(ECubusVegetationBiome::Plains) |
                static_cast<int32>(ECubusVegetationBiome::Forest) |
                static_cast<int32>(ECubusVegetationBiome::Wetland);
        }

        Entry.GrowthStageMeshes.Reserve(4);
        Entry.GrowthStageMeshes.Add(
            TSoftObjectPtr<UObject>(FSoftObjectPath(StageA))
        );
        Entry.GrowthStageMeshes.Add(
            TSoftObjectPtr<UObject>(FSoftObjectPath(StageB))
        );
        Entry.GrowthStageMeshes.Add(
            TSoftObjectPtr<UObject>(FSoftObjectPath(StageC))
        );
        Entry.GrowthStageMeshes.Add(
            TSoftObjectPtr<UObject>(FSoftObjectPath(StageD))
        );
        SpeciesCatalog.Add(MoveTemp(Entry));
    };

    AddDefaultSpecies(
        TEXT("Elder"),
        BroadleafType,
        TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_A.Tree_Elder_01_A"),
        TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_B.Tree_Elder_01_B"),
        TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_C.Tree_Elder_01_C"),
        TEXT("/Game/Megaplant_Library/Tree_Elder/Tree_Elder_01/Tree_Elder_01_D.Tree_Elder_01_D")
    );

    AddDefaultSpecies(
        TEXT("NorwaySpruce"),
        ConiferType,
        TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_A.Tree_Norway_Spruce_01_A"),
        TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_B.Tree_Norway_Spruce_01_B"),
        TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_C.Tree_Norway_Spruce_01_C"),
        TEXT("/Game/Megaplant_Library/Tree_Norway_Spruce/Tree_Norway_Spruce_01/Tree_Norway_Spruce_01_D.Tree_Norway_Spruce_01_D")
    );

    AddDefaultSpecies(
        TEXT("Greasewood"),
        ShrubType,
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
    const TArray<int32>* SpeciesIndices =
        SpeciesIndicesByType.Find(Instance.TypeId);

    if (
        SpeciesIndices == nullptr ||
        SpeciesIndices->IsEmpty()
    )
    {
        return INDEX_NONE;
    }

    TArray<int32, TInlineAllocator<32>>
        EligibleSpeciesIndices;

    float EligibleTotalWeight = 0.0f;

    for (const int32 SpeciesIndex : *SpeciesIndices)
    {
        if (!InSpeciesCatalog.IsValidIndex(SpeciesIndex))
        {
            continue;
        }

        const FCubusVegetationSpeciesCatalogEntry& Entry =
            InSpeciesCatalog[SpeciesIndex];

        if ((Entry.BiomeMask & Instance.BiomeMask) == 0)
        {
            continue;
        }

        EligibleSpeciesIndices.Add(SpeciesIndex);

        EligibleTotalWeight +=
            FMath::Max(0.001f, Entry.Weight);
    }

    if (EligibleSpeciesIndices.IsEmpty())
    {
        for (const int32 SpeciesIndex : *SpeciesIndices)
        {
            if (!InSpeciesCatalog.IsValidIndex(SpeciesIndex))
            {
                continue;
            }

            EligibleSpeciesIndices.Add(SpeciesIndex);

            EligibleTotalWeight += FMath::Max(
                0.001f,
                InSpeciesCatalog[SpeciesIndex].Weight
            );
        }
    }

    if (EligibleSpeciesIndices.IsEmpty())
    {
        return INDEX_NONE;
    }

    if (EligibleTotalWeight <= 0.0f)
    {
        return EligibleSpeciesIndices[0];
    }

    const bool bTreeType =
        Instance.TypeId == BroadleafType ||
        Instance.TypeId == ConiferType;

    uint32 SelectionHash = 0;

    if (bClusterTreeFamilies && bTreeType)
    {
        const int32 CellSize =
            FMath::Max(4, TreeFamilyCellSizeVoxels);

        const FIntVector FamilyCell(
            FMath::FloorToInt(
                static_cast<double>(Instance.WorldVoxel.X) /
                static_cast<double>(CellSize)
            ),
            FMath::FloorToInt(
                static_cast<double>(Instance.WorldVoxel.Y) /
                static_cast<double>(CellSize)
            ),
            0
        );

        SelectionHash = GetTypeHash(FamilyCell);

        SelectionHash = HashCombineFast(
            SelectionHash,
            GetTypeHash(Instance.TypeId)
        );

        SelectionHash = HashCombineFast(
            SelectionHash,
            GetTypeHash(RuntimeRandomizationSeed)
        );
    }
    else
    {
        SelectionHash =
            GetTypeHash(Instance.WorldVoxel);

        SelectionHash = HashCombineFast(
            SelectionHash,
            GetTypeHash(Instance.RotationYaw)
        );
    }

    const float Unit =
        HashToUnitFloat(SelectionHash);

    float Remaining =
        Unit * EligibleTotalWeight;

    for (const int32 SpeciesIndex : EligibleSpeciesIndices)
    {
        const float SafeWeight = FMath::Max(
            0.001f,
            InSpeciesCatalog[SpeciesIndex].Weight
        );

        Remaining -= SafeWeight;

        if (Remaining <= 0.0f)
        {
            return SpeciesIndex;
        }
    }

    return EligibleSpeciesIndices.Last();
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

    const bool bTreeType =
        Instance.TypeId == BroadleafType ||
        Instance.TypeId == ConiferType;

    if (bClusterTreeFamilies && bTreeType)
    {
        const int32 CellSize =
            FMath::Max(4, TreeFamilyCellSizeVoxels);

        const int32 CellX = FMath::FloorToInt(
            static_cast<double>(Instance.WorldVoxel.X) /
            static_cast<double>(CellSize)
        );

        const int32 CellY = FMath::FloorToInt(
            static_cast<double>(Instance.WorldVoxel.Y) /
            static_cast<double>(CellSize)
        );

        const FIntVector FamilyCell(
            CellX,
            CellY,
            0
        );

        uint32 FamilyHash =
            GetTypeHash(FamilyCell);

        FamilyHash = HashCombineFast(
            FamilyHash,
            GetTypeHash(Instance.TypeId)
        );

        FamilyHash = HashCombineFast(
            FamilyHash,
            GetTypeHash(RuntimeRandomizationSeed)
        );

        const float CenterJitter = FMath::Clamp(
            TreeFamilyCenterJitterFraction,
            0.0f,
            0.4f
        );

        const float CenterOffsetX =
            (
                HashToUnitFloat(
                    HashCombineFast(
                        FamilyHash,
                        0x68bc21ebu
                    )
                ) *
                2.0f -
                1.0f
            ) *
            CenterJitter;

        const float CenterOffsetY =
            (
                HashToUnitFloat(
                    HashCombineFast(
                        FamilyHash,
                        0x02e5be93u
                    )
                ) *
                2.0f -
                1.0f
            ) *
            CenterJitter;

        const float CenterX =
            (
                static_cast<float>(CellX) +
                0.5f +
                CenterOffsetX
            ) *
            static_cast<float>(CellSize);

        const float CenterY =
            (
                static_cast<float>(CellY) +
                0.5f +
                CenterOffsetY
            ) *
            static_cast<float>(CellSize);

        const float DeltaX =
            static_cast<float>(Instance.WorldVoxel.X) +
            0.5f -
            CenterX;

        const float DeltaY =
            static_cast<float>(Instance.WorldVoxel.Y) +
            0.5f -
            CenterY;

        const float MaxFamilyRadius =
            static_cast<float>(CellSize) *
            FMath::Sqrt(2.0f) *
            0.5f;

        float NormalizedDistance =
            FVector2D(DeltaX, DeltaY).Size() /
            FMath::Max(1.0f, MaxFamilyRadius);

        uint32 GrowthNoiseHash =
            GetTypeHash(Instance.WorldVoxel);

        GrowthNoiseHash = HashCombineFast(
            GrowthNoiseHash,
            FamilyHash
        );

        const float GrowthNoise =
            (
                HashToUnitFloat(GrowthNoiseHash) *
                2.0f -
                1.0f
            ) *
            FMath::Clamp(
                TreeFamilyGrowthNoise,
                0.0f,
                0.3f
            );

        NormalizedDistance = FMath::Clamp(
            NormalizedDistance + GrowthNoise,
            0.0f,
            1.0f
        );

        const float MatureRadius = FMath::Clamp(
            MatureTreeCoreRadius,
            0.02f,
            0.4f
        );

        const float YoungRadius = FMath::Clamp(
            FMath::Max(
                MatureRadius,
                YoungTreeRingRadius
            ),
            MatureRadius,
            0.8f
        );

        const float SaplingRadius = FMath::Clamp(
            FMath::Max(
                YoungRadius,
                SaplingTreeRingRadius
            ),
            YoungRadius,
            1.0f
        );

        if (StageCount >= 4)
        {
            if (NormalizedDistance <= MatureRadius)
            {
                return StageCount - 1;
            }

            if (NormalizedDistance <= YoungRadius)
            {
                return StageCount - 2;
            }

            if (NormalizedDistance <= SaplingRadius)
            {
                return StageCount - 3;
            }

            return 0;
        }

        return FMath::Clamp(
            FMath::RoundToInt(
                (1.0f - NormalizedDistance) *
                static_cast<float>(StageCount - 1)
            ),
            0,
            StageCount - 1
        );
    }

    uint32 GrowthHash =
        GetTypeHash(Instance.WorldVoxel);

    GrowthHash = HashCombineFast(
        GrowthHash,
        GetTypeHash(Instance.RotationYaw)
    );

    GrowthHash = HashCombineFast(
        GrowthHash,
        GetTypeHash(Instance.Scale)
    );

    return static_cast<int32>(
        GrowthHash %
        static_cast<uint32>(StageCount)
    );
}