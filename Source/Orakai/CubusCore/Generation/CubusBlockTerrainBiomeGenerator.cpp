#include "CubusCore/Generation/CubusBlockTerrainBiomeGenerator.h"

#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Data/CubusBlockVoxel.h"
#include "CubusCore/Data/CubusGeologyProfile.h"
#include "CubusCore/Generation/CubusBiomeField.h"
#include "CubusCore/Generation/CubusBlockVegetationGenerator.h"

void FCubusBlockTerrainBiomeGenerator::Apply(
    FCubusBlockChunkData& Chunk,
    const UCubusGeologyProfile* GeologyProfile
)
{
    if (!IsValid(GeologyProfile))
    {
        Chunk.ClearVegetationInstances();
        return;
    }

    if (!GeologyProfile->bGenerateBiomes)
    {
        FCubusBlockVegetationGenerator::Generate(Chunk, GeologyProfile);
        return;
    }

    const FIntVector ChunkCoordinate = Chunk.GetChunkCoordinate();
    const int32 BaseX = ChunkCoordinate.X * Cubus::ChunkSize;
    const int32 BaseY = ChunkCoordinate.Y * Cubus::ChunkSize;
    const int32 BaseZ = ChunkCoordinate.Z * Cubus::ChunkSize;
    const int32 BiomeSeed = Chunk.GetGenerationSeeds().Biomes;
    const int32 RiverSeed = Chunk.GetGenerationSeeds().Rivers;
    const FCubusBiomeFieldSettings BiomeSettings =
        FCubusBiomeField::MakeSettings(
            GeologyProfile,
            BiomeSeed,
            RiverSeed
        );

    int32 PlainsCount = 0;
    int32 ForestCount = 0;
    int32 RockyCount = 0;
    int32 WetlandCount = 0;
    int32 BuriedColumnCount = 0;

    for (int32 LocalY = 0; LocalY < Cubus::ChunkSize; ++LocalY)
    {
        const int32 WorldY = BaseY + LocalY;

        for (int32 LocalX = 0; LocalX < Cubus::ChunkSize; ++LocalX)
        {
            const int32 SurfaceLocalZ = FindSurfaceLocalZ(Chunk, LocalX, LocalY);

            if (SurfaceLocalZ == INDEX_NONE)
            {
                continue;
            }

            if (SurfaceLocalZ == Cubus::ChunkSize - 1)
            {
                ++BuriedColumnCount;
                continue;
            }

            const FCubusBlockVoxel* AboveVoxel = Chunk.GetVoxel(
                LocalX,
                LocalY,
                SurfaceLocalZ + 1
            );

            if (
                AboveVoxel != nullptr &&
                (AboveVoxel->MaterialId > 0 || AboveVoxel->IsWater())
            )
            {
                ++BuriedColumnCount;
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

            int32 MinimumNeighbourSurface = SurfaceLocalZ;
            int32 MaximumNeighbourSurface = SurfaceLocalZ;
            const int32 NeighbourSurfaces[] =
            {
                WestSurface,
                EastSurface,
                SouthSurface,
                NorthSurface
            };

            for (const int32 NeighbourSurface : NeighbourSurfaces)
            {
                if (NeighbourSurface == INDEX_NONE)
                {
                    continue;
                }

                MinimumNeighbourSurface = FMath::Min(
                    MinimumNeighbourSurface,
                    NeighbourSurface
                );
                MaximumNeighbourSurface = FMath::Max(
                    MaximumNeighbourSurface,
                    NeighbourSurface
                );
            }

            const float LocalSlope = static_cast<float>(
                MaximumNeighbourSurface - MinimumNeighbourSurface
            );
            const int32 WorldX = BaseX + LocalX;
            const int32 SurfaceWorldZ = BaseZ + SurfaceLocalZ;
            const FCubusBiomeSample BiomeSample = FCubusBiomeField::Sample(
                static_cast<float>(WorldX),
                static_cast<float>(WorldY),
                static_cast<float>(SurfaceWorldZ),
                LocalSlope,
                BiomeSettings
            );
            const int32 SelectedMaterialId = BiomeSample.SurfaceMaterialId;

            switch (BiomeSample.DominantBiome)
            {
                case ECubusBiomeKind::Forest:
                    ++ForestCount;
                    break;
                case ECubusBiomeKind::Rocky:
                    ++RockyCount;
                    break;
                case ECubusBiomeKind::Wetland:
                    ++WetlandCount;
                    break;
                default:
                    ++PlainsCount;
                    break;
            }

            FCubusBlockVoxel* SurfaceVoxel = Chunk.GetVoxel(
                LocalX,
                LocalY,
                SurfaceLocalZ
            );

            if (SurfaceVoxel == nullptr || SurfaceVoxel->IsWater())
            {
                continue;
            }

            SurfaceVoxel->MaterialId = SelectedMaterialId;
        }
    }

    FCubusBlockVegetationGenerator::Generate(Chunk, GeologyProfile);

    UE_LOG(
        LogTemp,
        Verbose,
        TEXT("Cubus biomes chunk (%d, %d, %d), seed %d: plains %d, forest %d, rocky %d, wetland %d, buried skipped %d"),
        ChunkCoordinate.X,
        ChunkCoordinate.Y,
        ChunkCoordinate.Z,
        BiomeSeed,
        PlainsCount,
        ForestCount,
        RockyCount,
        WetlandCount,
        BuriedColumnCount
    );
}

int32 FCubusBlockTerrainBiomeGenerator::FindSurfaceLocalZ(
    const FCubusBlockChunkData& Chunk,
    const int32 LocalX,
    const int32 LocalY
)
{
    for (int32 LocalZ = Cubus::ChunkSize - 1; LocalZ >= 0; --LocalZ)
    {
        const FCubusBlockVoxel* Voxel = Chunk.GetVoxel(LocalX, LocalY, LocalZ);

        if (
            Voxel != nullptr &&
            Voxel->MaterialId > 0 &&
            !Voxel->IsWater()
        )
        {
            return LocalZ;
        }
    }

    return INDEX_NONE;
}
