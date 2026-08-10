#include "CubusCore/Generation/CubusBlockVegetationGenerator.h"
#include "CubusCore/Vegetation/CubusVegetationTypes.h"
#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Data/CubusBlockVoxel.h"
#include "CubusCore/Data/CubusGeologyProfile.h"
#include "CubusCore/Data/CubusVegetationInstance.h"
#include "CubusCore/Generation/CubusBiomeField.h"
#include "CubusCore/Generation/CubusGenerationSeeds.h"
#include "CubusCore/Generation/CubusLandmarkField.h"
#include "CubusCore/Generation/CubusTerrainDensityField.h"

FCubusVegetationGenerationSettings
FCubusBlockVegetationGenerator::CaptureGenerationSettings(
    const UCubusGeologyProfile* GeologyProfile,
    const FCubusGenerationSeeds& GenerationSeeds
)
{
    FCubusVegetationGenerationSettings Settings;

    Settings.bUseConfiguredBiomes =
        IsValid(GeologyProfile) &&
        GeologyProfile->bGenerateBiomes;

    Settings.BiomeSettings =
        FCubusBiomeField::MakeSettings(
            GeologyProfile,
            GenerationSeeds.Biomes,
            GenerationSeeds.Rivers
        );

    Settings.LandmarkSettings =
        FCubusLandmarkField::MakeSettings(
            GeologyProfile,
            GenerationSeeds.Terrain
        );

    if (!IsValid(GeologyProfile))
    {
        return Settings;
    }

    Settings.ForestGroveCoverage =
        GeologyProfile->ForestGroveCoverage;

    Settings.ForestTreeDensity =
        GeologyProfile->ForestTreeDensity;

    Settings.ForestBroadleafFraction =
        GeologyProfile->ForestBroadleafFraction;

    Settings.WetlandTreeDensity =
        GeologyProfile->WetlandTreeDensity;

    Settings.WetlandReedDensity =
        GeologyProfile->WetlandReedDensity;

    Settings.RockyAlpineDensity =
        GeologyProfile->RockyAlpineDensity;

    Settings.PlainsTreeDensity =
        GeologyProfile->PlainsTreeDensity;

    Settings.PlainsShrubFraction =
        GeologyProfile->PlainsShrubFraction;

    Settings.PlainsGroundCoverDensity =
        GeologyProfile->PlainsGroundCoverDensity;

    Settings.FallbackTreeDensity =
        GeologyProfile->FallbackTreeDensity;

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

    const float PlacementRoll =
        HashToUnitFloat(
            HashWorldColumn(
                WorldX,
                WorldY,
                VegetationSeed ^ 101
            )
        );

    const float SpeciesRoll =
        HashToUnitFloat(
            HashWorldColumn(
                WorldX,
                WorldY,
                VegetationSeed ^ 149
            )
        );

    Result.ActivePlacementRoll =
        PlacementRoll;

    Result.BiomeMask =
        CubusVegetationBiome::All;

    if (Settings.bUseConfiguredBiomes)
    {
        switch (BiomeSample.DominantBiome)
        {
            case ECubusBiomeKind::Forest:
            {
                Result.BiomeMask =
                    CubusVegetationBiome::Forest;

                const float GroveCoverage =
                    FMath::Clamp(
                        Settings.ForestGroveCoverage,
                        0.05f,
                        1.0f
                    );

                const float TreeDensity =
                    FMath::Clamp(
                        Settings.ForestTreeDensity *
                        FMath::Lerp(
                            0.45f,
                            1.65f,
                            BiomeSample.ForestWeight
                        ) *
                        FMath::Lerp(
                            0.7f,
                            1.25f,
                            GroveCoverage
                        ),
                        0.0f,
                        1.0f
                    );

                if (
                    IsSpacedTreeCandidate(
                        WorldX,
                        WorldY,
                        VegetationSeed ^ 463,
                        TreeDensity
                    )
                )
                {
                    Result.TypeId =
                        SpeciesRoll <
                        FMath::Clamp(
                            Settings.ForestBroadleafFraction,
                            0.0f,
                            1.0f
                        )
                            ? CubusVegetationType::
                                BroadleafTree
                            : CubusVegetationType::
                                ConiferTree;

                    Result.Density = 1.0f;

                    Result.ActivePlacementRoll =
                        0.0f;
                }

                break;
            }

            case ECubusBiomeKind::Wetland:
            {
                Result.BiomeMask =
                    CubusVegetationBiome::Wetland;

                if (
                    IsSpacedTreeCandidate(
                        WorldX,
                        WorldY,
                        VegetationSeed ^ 571,
                        Settings.WetlandTreeDensity *
                        FMath::Lerp(
                            0.5f,
                            1.35f,
                            BiomeSample.WetlandWeight
                        )
                    )
                )
                {
                    Result.TypeId =
                        SpeciesRoll < 0.85f
                            ? CubusVegetationType::
                                BroadleafTree
                            : CubusVegetationType::
                                ConiferTree;

                    Result.Density = 1.0f;

                    Result.ActivePlacementRoll =
                        0.0f;
                }
                else
                {
                    Result.TypeId =
                        CubusVegetationType::Reeds;

                    Result.Density =
                        Settings.WetlandReedDensity *
                        FMath::Lerp(
                            0.4f,
                            1.2f,
                            BiomeSample.WetlandWeight
                        );
                }

                break;
            }

            case ECubusBiomeKind::Rocky:
            {
                Result.BiomeMask =
                    CubusVegetationBiome::Rocky;

                Result.TypeId =
                    CubusVegetationType::Alpine;

                Result.Density =
                    Settings.RockyAlpineDensity *
                    FMath::Lerp(
                        0.35f,
                        1.0f,
                        BiomeSample.Moisture
                    );

                break;
            }

            case ECubusBiomeKind::Plains:
            {
                Result.BiomeMask =
                    CubusVegetationBiome::Plains;

                if (
                    IsSpacedTreeCandidate(
                        WorldX,
                        WorldY,
                        VegetationSeed ^ 677,
                        Settings.PlainsTreeDensity *
                        FMath::Lerp(
                            0.3f,
                            1.2f,
                            BiomeSample.Moisture
                        )
                    )
                )
                {
                    Result.TypeId =
                        SpeciesRoll < 0.90f
                            ? CubusVegetationType::
                                BroadleafTree
                            : CubusVegetationType::
                                ConiferTree;

                    Result.Density = 1.0f;

                    Result.ActivePlacementRoll =
                        0.0f;
                }
                else
                {
                    Result.TypeId =
                        SpeciesRoll <
                        FMath::Clamp(
                            Settings.PlainsShrubFraction,
                            0.0f,
                            1.0f
                        )
                            ? CubusVegetationType::Shrub
                            : CubusVegetationType::Grass;
                    Result.Density =
                        Settings.PlainsGroundCoverDensity *
                        FMath::Lerp(
                            0.45f,
                            1.15f,
                            BiomeSample.Moisture
                        );
                }

                break;
            }

            default:
            {
                break;
            }
        }

        return Result;
    }

    Result.BiomeMask =
        CubusVegetationBiome::Forest;

    Result.TypeId =
        SpeciesRoll < 0.72f
            ? CubusVegetationType::BroadleafTree
            : CubusVegetationType::ConiferTree;

    Result.Density =
        Settings.FallbackTreeDensity;

    return Result;
}

float
FCubusBlockVegetationGenerator::ResolveMaximumSlopeDegrees(
    const int32 TypeId
)
{
    switch (TypeId)
    {
        case CubusVegetationType::BroadleafTree:
        case CubusVegetationType::ConiferTree:
            return 32.0f;

        case CubusVegetationType::Grass:
            return 42.0f;

        case CubusVegetationType::Shrub:
            return 46.0f;

        case CubusVegetationType::Reeds:
            return 18.0f;

        case CubusVegetationType::Alpine:
            return 58.0f;

        default:
            return 89.0f;
    }
}

bool
FCubusBlockVegetationGenerator::IsTreeType(
    const int32 TypeId
)
{
    return
        TypeId ==
            CubusVegetationType::BroadleafTree ||
        TypeId ==
            CubusVegetationType::ConiferTree;
}

float
FCubusBlockVegetationGenerator::SampleDensitySlopeDegrees(
    const int32 WorldX,
    const int32 WorldY,
    const FCubusTerrainDensityField& DensityField
)
{
    const int32 WestSurface =
        FMath::RoundToInt(
            DensityField.SampleSurfaceVoxelHeight(
                static_cast<float>(WorldX - 1),
                static_cast<float>(WorldY)
            )
        );

    const int32 EastSurface =
        FMath::RoundToInt(
            DensityField.SampleSurfaceVoxelHeight(
                static_cast<float>(WorldX + 1),
                static_cast<float>(WorldY)
            )
        );

    const int32 SouthSurface =
        FMath::RoundToInt(
            DensityField.SampleSurfaceVoxelHeight(
                static_cast<float>(WorldX),
                static_cast<float>(WorldY - 1)
            )
        );

    const int32 NorthSurface =
        FMath::RoundToInt(
            DensityField.SampleSurfaceVoxelHeight(
                static_cast<float>(WorldX),
                static_cast<float>(WorldY + 1)
            )
        );

    const float GradientX =
        static_cast<float>(
            EastSurface -
            WestSurface
        ) *
        0.5f;

    const float GradientY =
        static_cast<float>(
            NorthSurface -
            SouthSurface
        ) *
        0.5f;

    const float LocalSlope =
        FMath::Sqrt(
            GradientX * GradientX +
            GradientY * GradientY
        );

    return
        FMath::RadiansToDegrees(
            FMath::Atan(
                LocalSlope
            )
        );
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

    const FCubusGenerationSeeds& GenerationSeeds =
        Chunk.GetGenerationSeeds();

    const FCubusVegetationGenerationSettings
        GenerationSettings =
            CaptureGenerationSettings(
                GeologyProfile,
                GenerationSeeds
            );

    const bool bUseConfiguredBiomes =
        GenerationSettings.bUseConfiguredBiomes;

    const FIntVector ChunkCoordinate = Chunk.GetChunkCoordinate();
    const int32 BaseX = ChunkCoordinate.X * Cubus::ChunkSize;
    const int32 BaseY = ChunkCoordinate.Y * Cubus::ChunkSize;
    const int32 BaseZ = ChunkCoordinate.Z * Cubus::ChunkSize;
    const int32 VegetationSeed =
        GenerationSeeds.Vegetation;
    
    const FCubusBiomeFieldSettings& BiomeSettings =
        GenerationSettings.BiomeSettings;
        
    const FCubusLandmarkFieldSettings& LandmarkSettings =
        GenerationSettings.LandmarkSettings;
        
    const int32 TerrainOffsetX =
        (FCubusGenerationSeeds::DomainOffsetX(
            Chunk.GetGenerationSeeds().Terrain
        ) / Cubus::ChunkSize) * Cubus::ChunkSize;
    const int32 TerrainOffsetY =
        (FCubusGenerationSeeds::DomainOffsetY(
            Chunk.GetGenerationSeeds().Terrain
        ) / Cubus::ChunkSize) * Cubus::ChunkSize;

    int32 CountsByType[CubusVegetationType::Count] = {};

    constexpr int32 SurfaceColumnCount =
    Cubus::ChunkSize *
    Cubus::ChunkSize;

    TArray<int32> SurfaceLocalZByColumn;
    SurfaceLocalZByColumn.SetNumUninitialized(
        SurfaceColumnCount
    );

    TArray<uint8> SurfaceSupportsVegetation;
    SurfaceSupportsVegetation.SetNumZeroed(
        SurfaceColumnCount
    );

    const auto SurfaceIndex =
        [](const int32 LocalX, const int32 LocalY)
        {
            return
                LocalY * Cubus::ChunkSize +
                LocalX;
        };

    for (
        int32 LocalY = 0;
        LocalY < Cubus::ChunkSize;
        ++LocalY
    )
    {
        for (
            int32 LocalX = 0;
            LocalX < Cubus::ChunkSize;
            ++LocalX
        )
        {
            const int32 ColumnIndex =
                SurfaceIndex(
                    LocalX,
                    LocalY
                );

            if (DensityField != nullptr)
            {
                const int32 WorldX =
                    BaseX + LocalX;

                const int32 WorldY =
                    BaseY + LocalY;

                const int32 SurfaceWorldZ =
                    FMath::RoundToInt(
                        DensityField
                            ->SampleSurfaceVoxelHeight(
                                static_cast<float>(
                                    WorldX
                                ),
                                static_cast<float>(
                                    WorldY
                                )
                            )
                    );

                const int32 SurfaceLocalZ =
                    SurfaceWorldZ - BaseZ;

                SurfaceLocalZByColumn[
                    ColumnIndex
                ] = SurfaceLocalZ;

                const bool bInsideChunk =
                    SurfaceLocalZ >= 0 &&
                    SurfaceLocalZ <
                        Cubus::ChunkSize - 1;

                SurfaceSupportsVegetation[
                    ColumnIndex
                ] =
                    bInsideChunk
                        ? 1
                        : 0;

                continue;
            }

            int32 SurfaceLocalZ =
                INDEX_NONE;

            for (
                int32 LocalZ =
                    Cubus::ChunkSize - 1;
                LocalZ >= 0;
                --LocalZ
            )
            {
                const FCubusBlockVoxel* Voxel =
                    Chunk.GetVoxel(
                        LocalX,
                        LocalY,
                        LocalZ
                    );

                if (
                    Voxel == nullptr ||
                    Voxel->MaterialId <= 0 ||
                    Voxel->IsWater()
                )
                {
                    continue;
                }

                SurfaceLocalZ = LocalZ;
                break;
            }

            SurfaceLocalZByColumn[
                ColumnIndex
            ] = SurfaceLocalZ;

            if (
                SurfaceLocalZ == INDEX_NONE ||
                SurfaceLocalZ >=
                    Cubus::ChunkSize - 1
            )
            {
                continue;
            }

            const FCubusBlockVoxel* SurfaceVoxel =
                Chunk.GetVoxel(
                    LocalX,
                    LocalY,
                    SurfaceLocalZ
                );

            const FCubusBlockVoxel* AboveVoxel =
                Chunk.GetVoxel(
                    LocalX,
                    LocalY,
                    SurfaceLocalZ + 1
                );

            SurfaceSupportsVegetation[
                ColumnIndex
            ] =
                SurfaceVoxel != nullptr &&
                !SurfaceVoxel->IsWater() &&
                AboveVoxel != nullptr &&
                AboveVoxel->MaterialId <= 0 &&
                !AboveVoxel->IsWater()
                    ? 1
                    : 0;
        }
    }

    for (int32 LocalY = 0; LocalY < Cubus::ChunkSize; ++LocalY)
    {
        for (int32 LocalX = 0; LocalX < Cubus::ChunkSize; ++LocalX)
        {
            const int32 SurfaceLocalZ = SurfaceLocalZByColumn[
                SurfaceIndex(LocalX, LocalY)
            ];

            if (
                SurfaceLocalZ == INDEX_NONE ||
                !SurfaceSupportsVegetation[
                    SurfaceIndex(
                        LocalX,
                        LocalY
                    )
                ]
            )
            {
                continue;
            }

            const int32 WorldX = BaseX + LocalX;
            const int32 WorldY = BaseY + LocalY;
            const int32 WorldZ = BaseZ + SurfaceLocalZ + 1;
            const FCubusLandmarkSample LandmarkSample =
                FCubusLandmarkField::Sample(
                    static_cast<float>(WorldX + TerrainOffsetX),
                    static_cast<float>(WorldY + TerrainOffsetY),
                    LandmarkSettings
                );

            if (LandmarkSample.IsInside())
            {
                continue;
            }

            float ApproximateSlopeDegrees = 0.0f;
            float LocalSlope = 0.0f;

            if (DensityField != nullptr)
            {
                ApproximateSlopeDegrees =
                    SampleDensitySlopeDegrees(
                        WorldX,
                        WorldY,
                        *DensityField
                    );

                LocalSlope =
                    FMath::Tan(
                        FMath::DegreesToRadians(
                            ApproximateSlopeDegrees
                        )
                    );
            }
            else
            {
                const int32 WestX =
                    FMath::Max(
                        0,
                        LocalX - 1
                    );

                const int32 EastX =
                    FMath::Min(
                        Cubus::ChunkSize - 1,
                        LocalX + 1
                    );

                const int32 SouthY =
                    FMath::Max(
                        0,
                        LocalY - 1
                    );

                const int32 NorthY =
                    FMath::Min(
                        Cubus::ChunkSize - 1,
                        LocalY + 1
                    );

                const int32 WestSurface =
                    SurfaceLocalZByColumn[
                        SurfaceIndex(
                            WestX,
                            LocalY
                        )
                    ];

                const int32 EastSurface =
                    SurfaceLocalZByColumn[
                        SurfaceIndex(
                            EastX,
                            LocalY
                        )
                    ];

                const int32 SouthSurface =
                    SurfaceLocalZByColumn[
                        SurfaceIndex(
                            LocalX,
                            SouthY
                        )
                    ];

                const int32 NorthSurface =
                    SurfaceLocalZByColumn[
                        SurfaceIndex(
                            LocalX,
                            NorthY
                        )
                    ];

                const float GradientX =
                    WestSurface != INDEX_NONE &&
                    EastSurface != INDEX_NONE
                        ? static_cast<float>(
                            EastSurface -
                            WestSurface
                        ) *
                            0.5f
                        : 0.0f;

                const float GradientY =
                    SouthSurface != INDEX_NONE &&
                    NorthSurface != INDEX_NONE
                        ? static_cast<float>(
                            NorthSurface -
                            SouthSurface
                        ) *
                            0.5f
                        : 0.0f;

                LocalSlope =
                    FMath::Sqrt(
                        GradientX * GradientX +
                        GradientY * GradientY
                    );

                ApproximateSlopeDegrees =
                    FMath::RadiansToDegrees(
                        FMath::Atan(
                            LocalSlope
                        )
                    );
            }

            const FCubusBiomeSample BiomeSample = FCubusBiomeField::Sample(
                static_cast<float>(WorldX),
                static_cast<float>(WorldY),
                static_cast<float>(BaseZ + SurfaceLocalZ),
                LocalSlope,
                BiomeSettings
            );

            const FColumnSelection Selection =
                ResolveColumnSelection(
                    WorldX,
                    WorldY,
                    VegetationSeed,
                    BiomeSample,
                    GenerationSettings
                );

            const int32 TypeId =
                Selection.TypeId;

            const int32 BiomeMask =
                Selection.BiomeMask;

            const float Density =
                Selection.Density;

            const float ActivePlacementRoll =
                Selection.ActivePlacementRoll;

            const float MaximumAllowedSlopeDegrees =
                ResolveMaximumSlopeDegrees(
                    TypeId
                );
                
            if (
                ApproximateSlopeDegrees >
                MaximumAllowedSlopeDegrees
            )
            {
                continue;
            }

            if (
                TypeId <= 0 ||
                ActivePlacementRoll > FMath::Clamp(Density, 0.0f, 1.0f)
            )
            {
                continue;
            }

            FCubusVegetationInstance Instance;
            Instance.WorldVoxel = FIntVector(WorldX, WorldY, WorldZ);
            Instance.RotationYaw = HashToUnitFloat(
                HashWorldColumn(WorldX, WorldY, VegetationSeed ^ 211)
            ) * 360.0f;
            Instance.Scale = FMath::Lerp(
                0.85f,
                1.15f,
                HashToUnitFloat(
                    HashWorldColumn(WorldX, WorldY, VegetationSeed ^ 307)
                )
            );
            Instance.TypeId = TypeId;
            Instance.BiomeMask = BiomeMask;

            Instances.Add(Instance);
            ++CountsByType[TypeId];
        }
    }

    Chunk.SetVegetationInstances(MoveTemp(Instances));

    UE_LOG(
        LogTemp,
        Verbose,
        TEXT("Cubus vegetation chunk (%d, %d, %d), seed %d: grass %d, shrubs %d, broadleaf %d, conifers %d, reeds %d, alpine %d%s"),
        ChunkCoordinate.X,
        ChunkCoordinate.Y,
        ChunkCoordinate.Z,
        VegetationSeed,
        CountsByType[CubusVegetationType::Grass],
        CountsByType[CubusVegetationType::Shrub],
        CountsByType[CubusVegetationType::BroadleafTree],
        CountsByType[CubusVegetationType::ConiferTree],
        CountsByType[CubusVegetationType::Reeds],
        CountsByType[CubusVegetationType::Alpine],
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

    if (
        Region.Maximum.X <= Region.Minimum.X ||
        Region.Maximum.Y <= Region.Minimum.Y
    )
    {
        return;
    }

    const int32 VegetationSeed =
        GenerationSeeds.Vegetation;

    const FCubusBiomeFieldSettings& BiomeSettings =
        GenerationSettings.BiomeSettings;

    const FCubusLandmarkFieldSettings& LandmarkSettings =
        GenerationSettings.LandmarkSettings;

    const int32 TerrainOffsetX =
        (
            FCubusGenerationSeeds::DomainOffsetX(
                GenerationSeeds.Terrain
            ) /
            Cubus::ChunkSize
        ) *
        Cubus::ChunkSize;

    const int32 TerrainOffsetY =
        (
            FCubusGenerationSeeds::DomainOffsetY(
                GenerationSeeds.Terrain
            ) /
            Cubus::ChunkSize
        ) *
        Cubus::ChunkSize;

    const int64 Width =
        static_cast<int64>(
            Region.Maximum.X -
            Region.Minimum.X
        );

    const int64 Height =
        static_cast<int64>(
            Region.Maximum.Y -
            Region.Minimum.Y
        );

    const int64 ColumnCount =
        Width * Height;

    /*
     * Trees are intentionally sparse relative to terrain columns.
     * Reserve conservatively rather than allocating one record per column.
     */
    OutTrees.Reserve(
        static_cast<int32>(
            FMath::Min<int64>(
                ColumnCount / 16,
                MAX_int32
            )
        )
    );

    for (
        int32 WorldY = Region.Minimum.Y;
        WorldY < Region.Maximum.Y;
        ++WorldY
    )
    {
        for (
            int32 WorldX = Region.Minimum.X;
            WorldX < Region.Maximum.X;
            ++WorldX
        )
        {
            const int32 SurfaceWorldZ =
                FMath::RoundToInt(
                    DensityField
                        .SampleSurfaceVoxelHeight(
                            static_cast<float>(
                                WorldX
                            ),
                            static_cast<float>(
                                WorldY
                            )
                        )
                );

            const float ApproximateSlopeDegrees =
                SampleDensitySlopeDegrees(
                    WorldX,
                    WorldY,
                    DensityField
                );

            const float LocalSlope =
                FMath::Tan(
                    FMath::DegreesToRadians(
                        ApproximateSlopeDegrees
                    )
                );

            const FCubusLandmarkSample LandmarkSample =
                FCubusLandmarkField::Sample(
                    static_cast<float>(
                        WorldX +
                        TerrainOffsetX
                    ),
                    static_cast<float>(
                        WorldY +
                        TerrainOffsetY
                    ),
                    LandmarkSettings
                );

            if (LandmarkSample.IsInside())
            {
                continue;
            }

            const FCubusBiomeSample BiomeSample =
                FCubusBiomeField::Sample(
                    static_cast<float>(WorldX),
                    static_cast<float>(WorldY),
                    static_cast<float>(
                        SurfaceWorldZ
                    ),
                    LocalSlope,
                    BiomeSettings
                );

            const FColumnSelection Selection =
                ResolveColumnSelection(
                    WorldX,
                    WorldY,
                    VegetationSeed,
                    BiomeSample,
                    GenerationSettings
                );

            if (
                !IsTreeType(
                    Selection.TypeId
                )
            )
            {
                continue;
            }

            if (
                ApproximateSlopeDegrees >
                ResolveMaximumSlopeDegrees(
                    Selection.TypeId
                )
            )
            {
                continue;
            }

            if (
                Selection.ActivePlacementRoll >
                    FMath::Clamp(
                        Selection.Density,
                        0.0f,
                        1.0f
                    )
            )
            {
                continue;
            }
            FCubusVegetationInstance Instance;

            Instance.WorldVoxel =
                FIntVector(
                    WorldX,
                    WorldY,
                    SurfaceWorldZ + 1
                );

            Instance.RotationYaw =
                HashToUnitFloat(
                    HashWorldColumn(
                        WorldX,
                        WorldY,
                        VegetationSeed ^ 211
                    )
                ) *
                360.0f;

            Instance.Scale =
                FMath::Lerp(
                    0.85f,
                    1.15f,
                    HashToUnitFloat(
                        HashWorldColumn(
                            WorldX,
                            WorldY,
                            VegetationSeed ^ 307
                        )
                    )
                );

            Instance.TypeId =
                Selection.TypeId;

            Instance.BiomeMask =
                Selection.BiomeMask;

            OutTrees.Add(
                Instance
            );
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

    if (
        Region.Maximum.X <= Region.Minimum.X ||
        Region.Maximum.Y <= Region.Minimum.Y
    )
    {
        return;
    }

    const int32 SafeStride =
        FMath::Clamp(
            SampleStrideVoxels,
            2,
            64
        );

    const float SafeDensityScale =
        FMath::Clamp(
            DensityScale,
            0.0f,
            1.0f
        );

    if (SafeDensityScale <= 0.0f)
    {
        return;
    }

    const int32 VegetationSeed =
        GenerationSeeds.Vegetation;

    const FCubusBiomeFieldSettings& BiomeSettings =
        GenerationSettings.BiomeSettings;

    const FCubusLandmarkFieldSettings& LandmarkSettings =
        GenerationSettings.LandmarkSettings;

    const int32 TerrainOffsetX =
        (
            FCubusGenerationSeeds::DomainOffsetX(
                GenerationSeeds.Terrain
            ) /
            Cubus::ChunkSize
        ) *
        Cubus::ChunkSize;

    const int32 TerrainOffsetY =
        (
            FCubusGenerationSeeds::DomainOffsetY(
                GenerationSeeds.Terrain
            ) /
            Cubus::ChunkSize
        ) *
        Cubus::ChunkSize;

    const int32 FirstCellX =
        FMath::FloorToInt(
            static_cast<double>(
                Region.Minimum.X
            ) /
            static_cast<double>(
                SafeStride
            )
        );

    const int32 FirstCellY =
        FMath::FloorToInt(
            static_cast<double>(
                Region.Minimum.Y
            ) /
            static_cast<double>(
                SafeStride
            )
        );

    const int32 LastCellX =
        FMath::FloorToInt(
            static_cast<double>(
                Region.Maximum.X - 1
            ) /
            static_cast<double>(
                SafeStride
            )
        );

    const int32 LastCellY =
        FMath::FloorToInt(
            static_cast<double>(
                Region.Maximum.Y - 1
            ) /
            static_cast<double>(
                SafeStride
            )
        );

    const int64 CellCount =
        static_cast<int64>(
            LastCellX -
            FirstCellX +
            1
        ) *
        static_cast<int64>(
            LastCellY -
            FirstCellY +
            1
        );

    OutTrees.Reserve(
        static_cast<int32>(
            FMath::Min<int64>(
                CellCount / 2,
                MAX_int32
            )
        )
    );

    for (
        int32 CellY = FirstCellY;
        CellY <= LastCellY;
        ++CellY
    )
    {
        for (
            int32 CellX = FirstCellX;
            CellX <= LastCellX;
            ++CellX
        )
        {
            /*
             * Each coarse cell chooses one stable candidate column.
             *
             * This prevents the far representation from snapping around when
             * streaming-cell boundaries move.
             */
            const uint32 CandidateXHash =
                HashWorldColumn(
                    CellX,
                    CellY,
                    VegetationSeed ^ 1201
                );

            const uint32 CandidateYHash =
                HashWorldColumn(
                    CellX,
                    CellY,
                    VegetationSeed ^ 1217
                );

            const int32 CandidateOffsetX =
                FMath::Min(
                    SafeStride - 1,
                    FMath::FloorToInt(
                        HashToUnitFloat(
                            CandidateXHash
                        ) *
                        static_cast<float>(
                            SafeStride
                        )
                    )
                );

            const int32 CandidateOffsetY =
                FMath::Min(
                    SafeStride - 1,
                    FMath::FloorToInt(
                        HashToUnitFloat(
                            CandidateYHash
                        ) *
                        static_cast<float>(
                            SafeStride
                        )
                    )
                );

            const int32 WorldX =
                CellX *
                    SafeStride +
                CandidateOffsetX;

            const int32 WorldY =
                CellY *
                    SafeStride +
                CandidateOffsetY;

            if (
                WorldX < Region.Minimum.X ||
                WorldX >= Region.Maximum.X ||
                WorldY < Region.Minimum.Y ||
                WorldY >= Region.Maximum.Y
            )
            {
                continue;
            }

            const int32 SurfaceWorldZ =
                FMath::RoundToInt(
                    DensityField
                        .SampleSurfaceVoxelHeight(
                            static_cast<float>(
                                WorldX
                            ),
                            static_cast<float>(
                                WorldY
                            )
                        )
                );

            const float ApproximateSlopeDegrees =
                SampleDensitySlopeDegrees(
                    WorldX,
                    WorldY,
                    DensityField
                );

            if (
                ApproximateSlopeDegrees >
                32.0f
            )
            {
                continue;
            }

            const float LocalSlope =
                FMath::Tan(
                    FMath::DegreesToRadians(
                        ApproximateSlopeDegrees
                    )
                );

            const FCubusLandmarkSample LandmarkSample =
                FCubusLandmarkField::Sample(
                    static_cast<float>(
                        WorldX +
                        TerrainOffsetX
                    ),
                    static_cast<float>(
                        WorldY +
                        TerrainOffsetY
                    ),
                    LandmarkSettings
                );

            if (LandmarkSample.IsInside())
            {
                continue;
            }

            const FCubusBiomeSample BiomeSample =
                FCubusBiomeField::Sample(
                    static_cast<float>(
                        WorldX
                    ),
                    static_cast<float>(
                        WorldY
                    ),
                    static_cast<float>(
                        SurfaceWorldZ
                    ),
                    LocalSlope,
                    BiomeSettings
                );

            float TreeDensity = 0.0f;
            float BroadleafFraction = 0.72f;
            int32 BiomeMask =
                CubusVegetationBiome::All;

            if (
                GenerationSettings
                    .bUseConfiguredBiomes
            )
            {
                switch (
                    BiomeSample.DominantBiome
                )
                {
                    case ECubusBiomeKind::Forest:
                    {
                        BiomeMask =
                            CubusVegetationBiome::Forest;

                        const float GroveCoverage =
                            FMath::Clamp(
                                GenerationSettings
                                    .ForestGroveCoverage,
                                0.05f,
                                1.0f
                            );

                        TreeDensity =
                            FMath::Clamp(
                                GenerationSettings
                                    .ForestTreeDensity *
                                FMath::Lerp(
                                    0.45f,
                                    1.65f,
                                    BiomeSample
                                        .ForestWeight
                                ) *
                                FMath::Lerp(
                                    0.7f,
                                    1.25f,
                                    GroveCoverage
                                ),
                                0.0f,
                                1.0f
                            );

                        BroadleafFraction =
                            FMath::Clamp(
                                GenerationSettings
                                    .ForestBroadleafFraction,
                                0.0f,
                                1.0f
                            );

                        break;
                    }

                    case ECubusBiomeKind::Wetland:
                    {
                        BiomeMask =
                            CubusVegetationBiome::Wetland;

                        TreeDensity =
                            FMath::Clamp(
                                GenerationSettings
                                    .WetlandTreeDensity *
                                FMath::Lerp(
                                    0.5f,
                                    1.35f,
                                    BiomeSample
                                        .WetlandWeight
                                ),
                                0.0f,
                                1.0f
                            );

                        BroadleafFraction =
                            0.85f;

                        break;
                    }

                    case ECubusBiomeKind::Plains:
                    {
                        BiomeMask =
                            CubusVegetationBiome::Plains;

                        TreeDensity =
                            FMath::Clamp(
                                GenerationSettings
                                    .PlainsTreeDensity *
                                FMath::Lerp(
                                    0.3f,
                                    1.2f,
                                    BiomeSample
                                        .Moisture
                                ),
                                0.0f,
                                1.0f
                            );

                        BroadleafFraction =
                            0.90f;

                        break;
                    }

                    default:
                    {
                        continue;
                    }
                }
            }
            else
            {
                BiomeMask =
                    CubusVegetationBiome::Forest;

                TreeDensity =
                    FMath::Clamp(
                        GenerationSettings
                            .FallbackTreeDensity,
                        0.0f,
                        1.0f
                    );

                BroadleafFraction =
                    0.72f;
            }

            if (TreeDensity <= 0.0f)
            {
                continue;
            }

            /*
             * Convert per-column source density into the probability that this
             * coarse cell should contain one representative tree.
             *
             * A SafeStride x SafeStride cell represents SafeStride^2 source
             * terrain columns.
             */
            const float RepresentedArea =
                static_cast<float>(
                    SafeStride *
                    SafeStride
                );

            const float RepresentativeProbability =
                FMath::Clamp(
                    TreeDensity *
                        RepresentedArea *
                        SafeDensityScale,
                    0.0f,
                    1.0f
                );

            const float PlacementRoll =
                HashToUnitFloat(
                    HashWorldColumn(
                        CellX,
                        CellY,
                        VegetationSeed ^ 1237
                    )
                );

            if (
                PlacementRoll >
                RepresentativeProbability
            )
            {
                continue;
            }

            const float SpeciesRoll =
                HashToUnitFloat(
                    HashWorldColumn(
                        CellX,
                        CellY,
                        VegetationSeed ^ 1259
                    )
                );

            FCubusVegetationInstance Instance;

            Instance.WorldVoxel =
                FIntVector(
                    WorldX,
                    WorldY,
                    SurfaceWorldZ + 1
                );

            Instance.RotationYaw =
                HashToUnitFloat(
                    HashWorldColumn(
                        CellX,
                        CellY,
                        VegetationSeed ^ 1277
                    )
                ) *
                360.0f;

            Instance.Scale =
                FMath::Lerp(
                    0.90f,
                    1.20f,
                    HashToUnitFloat(
                        HashWorldColumn(
                            CellX,
                            CellY,
                            VegetationSeed ^ 1301
                        )
                    )
                );

            Instance.TypeId =
                SpeciesRoll <
                    BroadleafFraction
                    ? CubusVegetationType::
                        BroadleafTree
                    : CubusVegetationType::
                        ConiferTree;

            Instance.BiomeMask =
                BiomeMask;

            OutTrees.Add(
                Instance
            );
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
    if (SafeDensity <= 0.0f)
    {
        return false;
    }

    const int32 CellSize = FMath::Clamp(
        FMath::RoundToInt(FMath::Sqrt(1.0f / SafeDensity)),
        2,
        64
    );
    const int32 CellX = FMath::FloorToInt(
        static_cast<double>(WorldX) / static_cast<double>(CellSize)
    );
    const int32 CellY = FMath::FloorToInt(
        static_cast<double>(WorldY) / static_cast<double>(CellSize)
    );
    const int32 CandidateX =
        CellX * CellSize +
        FMath::Min(
            CellSize - 1,
            FMath::FloorToInt(
                HashToUnitFloat(HashWorldColumn(CellX, CellY, Seed ^ 811)) *
                static_cast<float>(CellSize)
            )
        );
    const int32 CandidateY =
        CellY * CellSize +
        FMath::Min(
            CellSize - 1,
            FMath::FloorToInt(
                HashToUnitFloat(HashWorldColumn(CellX, CellY, Seed ^ 947)) *
                static_cast<float>(CellSize)
            )
        );
    return WorldX == CandidateX && WorldY == CandidateY;
}

uint32 FCubusBlockVegetationGenerator::HashWorldColumn(
    const int32 WorldX,
    const int32 WorldY,
    const int32 Salt
)
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
    return static_cast<float>(Hash & 0x00ffffffu) /
        static_cast<float>(0x01000000u);
}
