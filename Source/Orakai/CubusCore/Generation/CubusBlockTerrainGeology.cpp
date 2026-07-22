#include "CubusCore/Generation/CubusBlockTerrainGenerator.h"

#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Data/CubusBlockVoxel.h"
#include "CubusCore/Data/CubusGeologyProfile.h"

void FCubusBlockTerrainGenerator::GenerateHeightTerrain(
    FCubusBlockChunkData& Chunk,
    const int32 BaseHeight,
    const float ContinentAmplitude,
    const float ContinentFrequency,
    const float HillAmplitude,
    const float HillFrequency,
    const float DetailAmplitude,
    const float DetailFrequency,
    const float RidgeAmplitude,
    const float RidgeFrequency,
    const float ValleyDepth,
    const float ValleyFrequency,
    const float ValleyWidth,
    const float ValleyFalloff,
    const float ValleyWarpAmplitude,
    const float ValleyWarpFrequency,
    const float RegionFrequency,
    const float PlainsThreshold,
    const float PlainsBlend,
    const float MountainThreshold,
    const float MountainBlend,
    const int32 SurfaceMaterialId,
    const int32 SubsurfaceMaterialId,
    const int32 RockMaterialId,
    const int32 SnowMaterialId,
    const float RockSlopeThreshold,
    const int32 SnowMinimumHeight,
    const bool bGenerateWater,
    const int32 WaterLevel,
    const int32 WaterMaterialId,
    const UCubusGeologyProfile* GeologyProfile
)
{
    GenerateHeightTerrain(
        Chunk,
        BaseHeight,
        ContinentAmplitude,
        ContinentFrequency,
        HillAmplitude,
        HillFrequency,
        DetailAmplitude,
        DetailFrequency,
        RidgeAmplitude,
        RidgeFrequency,
        ValleyDepth,
        ValleyFrequency,
        ValleyWidth,
        ValleyFalloff,
        ValleyWarpAmplitude,
        ValleyWarpFrequency,
        RegionFrequency,
        PlainsThreshold,
        PlainsBlend,
        MountainThreshold,
        MountainBlend,
        SurfaceMaterialId,
        SubsurfaceMaterialId,
        RockMaterialId,
        SnowMaterialId,
        RockSlopeThreshold,
        SnowMinimumHeight,
        bGenerateWater,
        WaterLevel,
        WaterMaterialId
    );

    if (!IsValid(GeologyProfile))
    {
        return;
    }

    const FIntVector ChunkCoordinate =
        Chunk.GetChunkCoordinate();

    const int32 ChunkWorldBaseX =
        ChunkCoordinate.X *
        Cubus::ChunkSize;

    const int32 ChunkWorldBaseY =
        ChunkCoordinate.Y *
        Cubus::ChunkSize;

    const int32 ChunkWorldBaseZ =
        ChunkCoordinate.Z *
        Cubus::ChunkSize;

    for (
        int32 LocalY = 0;
        LocalY < Cubus::ChunkSize;
        ++LocalY
    )
    {
        const int32 WorldY =
            ChunkWorldBaseY +
            LocalY;

        for (
            int32 LocalX = 0;
            LocalX < Cubus::ChunkSize;
            ++LocalX
        )
        {
            const int32 WorldX =
                ChunkWorldBaseX +
                LocalX;

            const int32 SurfaceWorldZ =
                SampleTerrainHeight(
                    WorldX,
                    WorldY,
                    BaseHeight,
                    ContinentAmplitude,
                    ContinentFrequency,
                    HillAmplitude,
                    HillFrequency,
                    DetailAmplitude,
                    DetailFrequency,
                    RidgeAmplitude,
                    RidgeFrequency,
                    ValleyDepth,
                    ValleyFrequency,
                    ValleyWidth,
                    ValleyFalloff,
                    ValleyWarpAmplitude,
                    ValleyWarpFrequency,
                    RegionFrequency,
                    PlainsThreshold,
                    PlainsBlend,
                    MountainThreshold,
                    MountainBlend
                );

            for (
                int32 LocalZ = 0;
                LocalZ < Cubus::ChunkSize;
                ++LocalZ
            )
            {
                const int32 WorldZ =
                    ChunkWorldBaseZ +
                    LocalZ;

                if (WorldZ >= SurfaceWorldZ)
                {
                    continue;
                }

                FCubusBlockVoxel* Voxel =
                    Chunk.GetVoxel(
                        LocalX,
                        LocalY,
                        LocalZ
                    );

                if (
                    Voxel == nullptr ||
                    Voxel->IsWater() ||
                    Voxel->MaterialId != SubsurfaceMaterialId
                )
                {
                    continue;
                }

                Voxel->MaterialId =
                    SelectSubsurfaceMaterial(
                        SurfaceWorldZ,
                        WorldZ,
                        SubsurfaceMaterialId,
                        GeologyProfile
                    );
            }
        }
    }
}

int32 FCubusBlockTerrainGenerator::SelectSubsurfaceMaterial(
    const int32 SurfaceWorldZ,
    const int32 WorldZ,
    const int32 FallbackMaterialId,
    const UCubusGeologyProfile* GeologyProfile
)
{
    const int32 SafeFallbackMaterialId =
        FMath::Max(
            1,
            FallbackMaterialId
        );

    if (!IsValid(GeologyProfile))
    {
        return SafeFallbackMaterialId;
    }

    const int32 Depth =
        SurfaceWorldZ -
        WorldZ;

    for (
        const FCubusStrataLayer& Layer :
        GeologyProfile->StrataLayers
    )
    {
        const int32 MinimumDepth =
            FMath::Max(
                1,
                Layer.MinimumDepth
            );

        const int32 MaximumDepth =
            FMath::Max(
                MinimumDepth,
                Layer.MaximumDepth
            );

        if (
            Depth >= MinimumDepth &&
            Depth <= MaximumDepth
        )
        {
            return FMath::Max(
                1,
                Layer.MaterialId
            );
        }
    }

    return SafeFallbackMaterialId;
}
