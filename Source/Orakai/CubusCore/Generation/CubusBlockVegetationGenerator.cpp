#include "CubusCore/Generation/CubusBlockVegetationGenerator.h"

#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Data/CubusBlockVoxel.h"
#include "CubusCore/Data/CubusGeologyProfile.h"
#include "CubusCore/Data/CubusVegetationInstance.h"
#include "CubusCore/Generation/CubusBiomeField.h"
#include "CubusCore/Generation/CubusGenerationSeeds.h"
#include "CubusCore/Generation/CubusLandmarkField.h"

namespace CubusVegetationType
{
    constexpr int32 Grass = 1;
    constexpr int32 Shrub = 2;
    constexpr int32 BroadleafTree = 3;
    constexpr int32 Reeds = 4;
    constexpr int32 Alpine = 5;
    constexpr int32 ConiferTree = 6;
    constexpr int32 Count = 7;
}

void FCubusBlockVegetationGenerator::Generate(
    FCubusBlockChunkData& Chunk,
    const UCubusGeologyProfile* GeologyProfile
)
{
    TArray<FCubusVegetationInstance> Instances;

    const bool bUseConfiguredBiomes =
        IsValid(GeologyProfile) &&
        GeologyProfile->bGenerateBiomes;

    const FIntVector ChunkCoordinate = Chunk.GetChunkCoordinate();
    const int32 BaseX = ChunkCoordinate.X * Cubus::ChunkSize;
    const int32 BaseY = ChunkCoordinate.Y * Cubus::ChunkSize;
    const int32 BaseZ = ChunkCoordinate.Z * Cubus::ChunkSize;
    const int32 VegetationSeed = Chunk.GetGenerationSeeds().Vegetation;
    const FCubusBiomeFieldSettings BiomeSettings =
        FCubusBiomeField::MakeSettings(
            GeologyProfile,
            Chunk.GetGenerationSeeds().Biomes,
            Chunk.GetGenerationSeeds().Rivers
        );
    const FCubusLandmarkFieldSettings LandmarkSettings =
        FCubusLandmarkField::MakeSettings(
            GeologyProfile,
            Chunk.GetGenerationSeeds().Terrain
        );
    const int32 TerrainOffsetX =
        (FCubusGenerationSeeds::DomainOffsetX(
            Chunk.GetGenerationSeeds().Terrain
        ) / Cubus::ChunkSize) * Cubus::ChunkSize;
    const int32 TerrainOffsetY =
        (FCubusGenerationSeeds::DomainOffsetY(
            Chunk.GetGenerationSeeds().Terrain
        ) / Cubus::ChunkSize) * Cubus::ChunkSize;

    int32 CountsByType[CubusVegetationType::Count] = {};

    for (int32 LocalY = 0; LocalY < Cubus::ChunkSize; ++LocalY)
    {
        for (int32 LocalX = 0; LocalX < Cubus::ChunkSize; ++LocalX)
        {
            const int32 SurfaceLocalZ = FindSurfaceLocalZ(
                Chunk,
                LocalX,
                LocalY
            );

            if (
                SurfaceLocalZ == INDEX_NONE ||
                SurfaceLocalZ >= Cubus::ChunkSize - 1
            )
            {
                continue;
            }

            const FCubusBlockVoxel* SurfaceVoxel = Chunk.GetVoxel(
                LocalX,
                LocalY,
                SurfaceLocalZ
            );
            const FCubusBlockVoxel* AboveVoxel = Chunk.GetVoxel(
                LocalX,
                LocalY,
                SurfaceLocalZ + 1
            );

            if (
                SurfaceVoxel == nullptr ||
                SurfaceVoxel->IsWater() ||
                AboveVoxel == nullptr ||
                AboveVoxel->MaterialId > 0 ||
                AboveVoxel->IsWater()
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

            const int32 WestSurface = FindSurfaceLocalZ(
                Chunk,
                FMath::Max(0, LocalX - 1),
                LocalY
            );
            const int32 EastSurface = FindSurfaceLocalZ(
                Chunk,
                FMath::Min(Cubus::ChunkSize - 1, LocalX + 1),
                LocalY
            );
            const int32 SouthSurface = FindSurfaceLocalZ(
                Chunk,
                LocalX,
                FMath::Max(0, LocalY - 1)
            );
            const int32 NorthSurface = FindSurfaceLocalZ(
                Chunk,
                LocalX,
                FMath::Min(Cubus::ChunkSize - 1, LocalY + 1)
            );
            const float GradientX =
                WestSurface != INDEX_NONE && EastSurface != INDEX_NONE
                    ? static_cast<float>(EastSurface - WestSurface) * 0.5f
                    : 0.0f;
            const float GradientY =
                SouthSurface != INDEX_NONE && NorthSurface != INDEX_NONE
                    ? static_cast<float>(NorthSurface - SouthSurface) * 0.5f
                    : 0.0f;
            const float LocalSlope = FMath::Sqrt(
                GradientX * GradientX + GradientY * GradientY
            );
            const FCubusBiomeSample BiomeSample = FCubusBiomeField::Sample(
                static_cast<float>(WorldX),
                static_cast<float>(WorldY),
                static_cast<float>(BaseZ + SurfaceLocalZ),
                LocalSlope,
                BiomeSettings
            );

            const float PlacementRoll = HashToUnitFloat(
                HashWorldColumn(WorldX, WorldY, VegetationSeed ^ 101)
            );
            const float SpeciesRoll = HashToUnitFloat(
                HashWorldColumn(WorldX, WorldY, VegetationSeed ^ 149)
            );
            int32 TypeId = 0;
            int32 BiomeMask = CubusVegetationBiome::All;
            float Density = 0.0f;
            float ActivePlacementRoll = PlacementRoll;

            if (bUseConfiguredBiomes)
            {
                if (BiomeSample.DominantBiome == ECubusBiomeKind::Forest)
                {
                    BiomeMask = CubusVegetationBiome::Forest;
                    const float GroveCoverage = FMath::Clamp(
                        GeologyProfile->ForestGroveCoverage,
                        0.05f,
                        1.0f
                    );
                    const float TreeDensity = FMath::Clamp(
                        GeologyProfile->ForestTreeDensity *
                        FMath::Lerp(0.45f, 1.65f, BiomeSample.ForestWeight) *
                        FMath::Lerp(0.7f, 1.25f, GroveCoverage),
                        0.0f,
                        1.0f
                    );

                    if (IsSpacedTreeCandidate(
                        WorldX,
                        WorldY,
                        VegetationSeed ^ 463,
                        TreeDensity
                    ))
                    {
                        TypeId = SpeciesRoll < FMath::Clamp(
                            GeologyProfile->ForestBroadleafFraction,
                            0.0f,
                            1.0f
                        )
                            ? CubusVegetationType::BroadleafTree
                            : CubusVegetationType::ConiferTree;
                        Density = 1.0f;
                        ActivePlacementRoll = 0.0f;
                    }
                }
                else if (BiomeSample.DominantBiome == ECubusBiomeKind::Wetland)
                {
                    BiomeMask = CubusVegetationBiome::Wetland;

                    if (IsSpacedTreeCandidate(
                        WorldX,
                        WorldY,
                        VegetationSeed ^ 571,
                        GeologyProfile->WetlandTreeDensity *
                            FMath::Lerp(0.5f, 1.35f, BiomeSample.WetlandWeight)
                    ))
                    {
                        TypeId = SpeciesRoll < 0.85f
                            ? CubusVegetationType::BroadleafTree
                            : CubusVegetationType::ConiferTree;
                        Density = 1.0f;
                        ActivePlacementRoll = 0.0f;
                    }
                    else
                    {
                        TypeId = CubusVegetationType::Reeds;
                        Density = GeologyProfile->WetlandReedDensity *
                            FMath::Lerp(0.4f, 1.2f, BiomeSample.WetlandWeight);
                    }
                }
                else if (BiomeSample.DominantBiome == ECubusBiomeKind::Rocky)
                {
                    BiomeMask = CubusVegetationBiome::Rocky;

                    if (IsSpacedTreeCandidate(
                        WorldX,
                        WorldY,
                        VegetationSeed ^ 619,
                        GeologyProfile->RockyTreeDensity *
                            FMath::Lerp(0.35f, 1.0f, BiomeSample.Moisture)
                    ))
                    {
                        TypeId = SpeciesRoll < 0.10f
                            ? CubusVegetationType::BroadleafTree
                            : CubusVegetationType::ConiferTree;
                        Density = 1.0f;
                        ActivePlacementRoll = 0.0f;
                    }
                    else
                    {
                        TypeId = CubusVegetationType::Alpine;
                        Density = GeologyProfile->RockyAlpineDensity *
                            FMath::Lerp(0.35f, 1.0f, BiomeSample.Moisture);
                    }
                }
                else if (BiomeSample.DominantBiome == ECubusBiomeKind::Plains)
                {
                    BiomeMask = CubusVegetationBiome::Plains;

                    if (IsSpacedTreeCandidate(
                        WorldX,
                        WorldY,
                        VegetationSeed ^ 677,
                        GeologyProfile->PlainsTreeDensity *
                            FMath::Lerp(0.3f, 1.2f, BiomeSample.Moisture)
                    ))
                    {
                        TypeId = SpeciesRoll < 0.90f
                            ? CubusVegetationType::BroadleafTree
                            : CubusVegetationType::ConiferTree;
                        Density = 1.0f;
                        ActivePlacementRoll = 0.0f;
                    }
                    else
                    {
                        TypeId = SpeciesRoll < FMath::Clamp(
                            GeologyProfile->PlainsShrubFraction,
                            0.0f,
                            1.0f
                        )
                            ? CubusVegetationType::Shrub
                            : CubusVegetationType::Grass;
                        Density = GeologyProfile->PlainsGroundCoverDensity *
                            FMath::Lerp(0.45f, 1.15f, BiomeSample.Moisture);
                    }
                }
            }
            else
            {
                BiomeMask = CubusVegetationBiome::Forest;
                TypeId = SpeciesRoll < 0.72f
                    ? CubusVegetationType::BroadleafTree
                    : CubusVegetationType::ConiferTree;
                Density = IsValid(GeologyProfile)
                    ? GeologyProfile->FallbackTreeDensity
                    : 0.012f;
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

int32 FCubusBlockVegetationGenerator::FindSurfaceLocalZ(
    const FCubusBlockChunkData& Chunk,
    const int32 LocalX,
    const int32 LocalY
)
{
    for (int32 LocalZ = Cubus::ChunkSize - 1; LocalZ >= 0; --LocalZ)
    {
        const FCubusBlockVoxel* Voxel = Chunk.GetVoxel(LocalX, LocalY, LocalZ);
        if (Voxel != nullptr && Voxel->MaterialId > 0 && !Voxel->IsWater())
        {
            return LocalZ;
        }
    }
    return INDEX_NONE;
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
