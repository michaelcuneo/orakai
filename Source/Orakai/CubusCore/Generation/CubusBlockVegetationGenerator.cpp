#include "CubusCore/Generation/CubusBlockVegetationGenerator.h"

#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Data/CubusBlockVoxel.h"
#include "CubusCore/Data/CubusGeologyProfile.h"
#include "CubusCore/Data/CubusVegetationInstance.h"

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

    int32 CountsByType[CubusVegetationType::Count] = {};

    for (int32 LocalY = 0; LocalY < Cubus::ChunkSize; ++LocalY)
    {
        for (int32 LocalX = 0; LocalX < Cubus::ChunkSize; ++LocalX)
        {
            int32 SurfaceLocalZ = INDEX_NONE;

            for (int32 LocalZ = Cubus::ChunkSize - 1; LocalZ >= 0; --LocalZ)
            {
                const FCubusBlockVoxel* Voxel =
                    Chunk.GetVoxel(LocalX, LocalY, LocalZ);

                if (
                    Voxel != nullptr &&
                    Voxel->MaterialId > 0 &&
                    !Voxel->IsWater()
                )
                {
                    SurfaceLocalZ = LocalZ;
                    break;
                }
            }

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

            const float PlacementRoll = HashToUnitFloat(
                HashWorldColumn(WorldX, WorldY, VegetationSeed ^ 101)
            );
            const float SpeciesRoll = HashToUnitFloat(
                HashWorldColumn(WorldX, WorldY, VegetationSeed ^ 149)
            );
            const float TreePlacementRoll = HashToUnitFloat(
                HashWorldColumn(WorldX, WorldY, VegetationSeed ^ 197)
            );

            int32 TypeId = 0;
            int32 BiomeMask = CubusVegetationBiome::All;
            float Density = 0.0f;
            float ActivePlacementRoll = PlacementRoll;

            if (bUseConfiguredBiomes)
            {
                if (SurfaceVoxel->MaterialId == GeologyProfile->ForestSurfaceMaterialId)
                {
                    BiomeMask = CubusVegetationBiome::Forest;

                    const int32 GroveCellSize = FMath::Max(
                        4,
                        GeologyProfile->ForestGroveCellSizeVoxels
                    );
                    const int32 GroveCellX = FMath::FloorToInt(
                        static_cast<double>(WorldX) /
                        static_cast<double>(GroveCellSize)
                    );
                    const int32 GroveCellY = FMath::FloorToInt(
                        static_cast<double>(WorldY) /
                        static_cast<double>(GroveCellSize)
                    );
                    const float GroveCoverage = FMath::Clamp(
                        GeologyProfile->ForestGroveCoverage,
                        0.05f,
                        1.0f
                    );
                    const float GroveRoll = HashToUnitFloat(
                        HashWorldColumn(
                            GroveCellX,
                            GroveCellY,
                            VegetationSeed ^ 463
                        )
                    );

                    if (GroveRoll <= GroveCoverage)
                    {
                        TypeId = SpeciesRoll < FMath::Clamp(
                            GeologyProfile->ForestBroadleafFraction,
                            0.0f,
                            1.0f
                        )
                            ? CubusVegetationType::BroadleafTree
                            : CubusVegetationType::ConiferTree;
                        Density = FMath::Clamp(
                            GeologyProfile->ForestTreeDensity / GroveCoverage,
                            0.0f,
                            1.0f
                        );
                    }
                }
                else if (SurfaceVoxel->MaterialId == GeologyProfile->WetlandSurfaceMaterialId)
                {
                    BiomeMask = CubusVegetationBiome::Wetland;

                    if (TreePlacementRoll <= GeologyProfile->WetlandTreeDensity)
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
                        Density = GeologyProfile->WetlandReedDensity;
                    }
                }
                else if (SurfaceVoxel->MaterialId == GeologyProfile->RockySurfaceMaterialId)
                {
                    BiomeMask = CubusVegetationBiome::Rocky;

                    if (TreePlacementRoll <= GeologyProfile->RockyTreeDensity)
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
                        Density = GeologyProfile->RockyAlpineDensity;
                    }
                }
                else if (SurfaceVoxel->MaterialId == GeologyProfile->PlainsSurfaceMaterialId)
                {
                    BiomeMask = CubusVegetationBiome::Plains;

                    if (TreePlacementRoll <= GeologyProfile->PlainsTreeDensity)
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
                        Density = GeologyProfile->PlainsGroundCoverDensity;
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
