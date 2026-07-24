#include "CubusCore/Generation/CubusBlockTerrainBiomeGenerator.h"

#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Data/CubusBlockVoxel.h"
#include "CubusCore/Data/CubusGeologyProfile.h"
#include "CubusCore/Generation/CubusBlockVegetationGenerator.h"
#include "CubusCore/Generation/CubusGenerationSeeds.h"

void FCubusBlockTerrainBiomeGenerator::Apply(
    FCubusBlockChunkData& Chunk,
    const UCubusGeologyProfile* GeologyProfile
)
{
    if (!IsValid(GeologyProfile) || !GeologyProfile->bGenerateBiomes)
    {
        Chunk.ClearVegetationInstances();
        return;
    }

    const FIntVector ChunkCoordinate = Chunk.GetChunkCoordinate();
    const int32 BaseX = ChunkCoordinate.X * Cubus::ChunkSize;
    const int32 BaseY = ChunkCoordinate.Y * Cubus::ChunkSize;
    const int32 BaseZ = ChunkCoordinate.Z * Cubus::ChunkSize;
    const float Frequency = FMath::Max(0.000001f, GeologyProfile->BiomeFrequency);

    const int32 BiomeSeed = Chunk.GetGenerationSeeds().Biomes;
    const int32 BiomeOffsetX = FCubusGenerationSeeds::DomainOffsetX(BiomeSeed);
    const int32 BiomeOffsetY = FCubusGenerationSeeds::DomainOffsetY(BiomeSeed);
    const int32 RiverSeed = Chunk.GetGenerationSeeds().Rivers;
    const int32 RiverOffsetX = FCubusGenerationSeeds::DomainOffsetX(RiverSeed);
    const int32 RiverOffsetY = FCubusGenerationSeeds::DomainOffsetY(RiverSeed);

    const int32 PlainsMaterialId = FMath::Max(1, GeologyProfile->PlainsSurfaceMaterialId);
    const int32 ForestMaterialId = FMath::Max(1, GeologyProfile->ForestSurfaceMaterialId);
    const int32 RockyMaterialId = FMath::Max(1, GeologyProfile->RockySurfaceMaterialId);
    const int32 WetlandMaterialId = FMath::Max(1, GeologyProfile->WetlandSurfaceMaterialId);

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
            const float RiverDistance = SampleRiverDistance(
                WorldX + RiverOffsetX,
                WorldY + RiverOffsetY,
                GeologyProfile
            );
            const float BiomeNoise = SampleBiomeNoise(
                WorldX + BiomeOffsetX,
                WorldY + BiomeOffsetY,
                Frequency
            );

            int32 SelectedMaterialId = PlainsMaterialId;

            if (
                GeologyProfile->bGenerateRivers &&
                RiverDistance <= GeologyProfile->WetlandRiverDistance &&
                LocalSlope < GeologyProfile->RockySlopeThreshold
            )
            {
                SelectedMaterialId = WetlandMaterialId;
                ++WetlandCount;
            }
            else if (
                SurfaceWorldZ >= GeologyProfile->RockyMinimumWorldZ ||
                LocalSlope >= GeologyProfile->RockySlopeThreshold
            )
            {
                SelectedMaterialId = RockyMaterialId;
                ++RockyCount;
            }
            else if (BiomeNoise >= GeologyProfile->ForestThreshold)
            {
                SelectedMaterialId = ForestMaterialId;
                ++ForestCount;
            }
            else
            {
                ++PlainsCount;
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
        Display,
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

float FCubusBlockTerrainBiomeGenerator::SampleBiomeNoise(
    const int32 WorldX,
    const int32 WorldY,
    const float Frequency
)
{
    return FMath::PerlinNoise2D(
        FVector2D(
            static_cast<double>(WorldX + 4219) * Frequency,
            static_cast<double>(WorldY - 1877) * Frequency
        )
    );
}

float FCubusBlockTerrainBiomeGenerator::SampleRiverDistance(
    const int32 WorldX,
    const int32 WorldY,
    const UCubusGeologyProfile* GeologyProfile
)
{
    const float RiverFrequency = FMath::Max(
        0.000001f,
        GeologyProfile->RiverFrequency
    );
    const float WarpFrequency = FMath::Max(
        0.000001f,
        GeologyProfile->RiverWarpFrequency
    );
    const float WarpAmplitude = FMath::Max(
        0.0f,
        GeologyProfile->RiverWarpAmplitude
    );

    const float WarpX = FMath::PerlinNoise2D(
        FVector2D(
            static_cast<double>(WorldX) * WarpFrequency,
            static_cast<double>(WorldY) * WarpFrequency
        )
    ) * WarpAmplitude;

    const float WarpY = FMath::PerlinNoise2D(
        FVector2D(
            static_cast<double>(WorldX + 7919) * WarpFrequency,
            static_cast<double>(WorldY - 3571) * WarpFrequency
        )
    ) * WarpAmplitude;

    return FMath::Abs(
        FMath::PerlinNoise2D(
            FVector2D(
                (static_cast<double>(WorldX) + WarpX) * RiverFrequency,
                (static_cast<double>(WorldY) + WarpY) * RiverFrequency
            )
        )
    );
}
