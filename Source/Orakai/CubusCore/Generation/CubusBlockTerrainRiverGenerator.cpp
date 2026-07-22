#include "CubusCore/Generation/CubusBlockTerrainRiverGenerator.h"

#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Data/CubusBlockVoxel.h"
#include "CubusCore/Data/CubusGeologyProfile.h"

void FCubusBlockTerrainRiverGenerator::Apply(
    FCubusBlockChunkData& Chunk,
    const UCubusGeologyProfile* GeologyProfile
)
{
    if (
        !IsValid(GeologyProfile) ||
        !GeologyProfile->bGenerateRivers
    )
    {
        return;
    }

    const FIntVector ChunkCoordinate =
        Chunk.GetChunkCoordinate();

    const int32 BaseX =
        ChunkCoordinate.X *
        Cubus::ChunkSize;

    const int32 BaseY =
        ChunkCoordinate.Y *
        Cubus::ChunkSize;

    const int32 BaseZ =
        ChunkCoordinate.Z *
        Cubus::ChunkSize;

    const float ChannelWidth =
        FMath::Clamp(
            GeologyProfile->RiverChannelWidth,
            0.0f,
            1.0f
        );

    const float ValleyWidth =
        FMath::Max(
            ChannelWidth + 0.0001f,
            GeologyProfile->RiverValleyWidth
        );

    const int32 ChannelDepth =
        FMath::Max(
            1,
            GeologyProfile->RiverChannelDepth
        );

    const int32 WaterDepth =
        FMath::Max(
            1,
            GeologyProfile->RiverWaterDepth
        );

    const int32 RiverbedMaterialId =
        FMath::Max(
            1,
            GeologyProfile->RiverbedMaterialId
        );

    const int32 RiverWaterMaterialId =
        FMath::Max(
            1,
            GeologyProfile->RiverWaterMaterialId
        );

    int32 RiverColumnCount = 0;
    int32 RiverWaterVoxelCount = 0;

    for (
        int32 LocalY = 0;
        LocalY < Cubus::ChunkSize;
        ++LocalY
    )
    {
        const int32 WorldY =
            BaseY +
            LocalY;

        for (
            int32 LocalX = 0;
            LocalX < Cubus::ChunkSize;
            ++LocalX
        )
        {
            const int32 WorldX =
                BaseX +
                LocalX;

            const float RiverDistance =
                SampleRiverDistance(
                    WorldX,
                    WorldY,
                    GeologyProfile
                );

            if (RiverDistance >= ValleyWidth)
            {
                continue;
            }

            int32 HighestSolidLocalZ = INDEX_NONE;

            for (
                int32 LocalZ = Cubus::ChunkSize - 1;
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
                    Voxel != nullptr &&
                    Voxel->MaterialId > 0 &&
                    !Voxel->IsWater()
                )
                {
                    HighestSolidLocalZ = LocalZ;
                    break;
                }
            }

            if (HighestSolidLocalZ == INDEX_NONE)
            {
                continue;
            }

            const float ValleyInfluence =
                1.0f -
                SmoothStep(
                    ChannelWidth,
                    ValleyWidth,
                    RiverDistance
                );

            const bool bInsideChannel =
                RiverDistance <=
                ChannelWidth;

            const int32 ValleyLowering =
                FMath::RoundToInt(
                    FMath::Max(
                        0.0f,
                        GeologyProfile->RiverValleyDepth
                    ) *
                    ValleyInfluence
                );

            const int32 TotalLowering =
                ValleyLowering +
                (
                    bInsideChannel
                        ? ChannelDepth
                        : 0
                );

            const int32 TargetSurfaceLocalZ =
                FMath::Clamp(
                    HighestSolidLocalZ -
                    TotalLowering,
                    0,
                    Cubus::ChunkSize - 1
                );

            if (TargetSurfaceLocalZ >= HighestSolidLocalZ)
            {
                continue;
            }

            for (
                int32 LocalZ = TargetSurfaceLocalZ + 1;
                LocalZ <= HighestSolidLocalZ;
                ++LocalZ
            )
            {
                FCubusBlockVoxel* Voxel =
                    Chunk.GetVoxel(
                        LocalX,
                        LocalY,
                        LocalZ
                    );

                if (Voxel == nullptr)
                {
                    continue;
                }

                Voxel->MaterialId = 0;
                Voxel->SetWater(false);
            }

            FCubusBlockVoxel* RiverbedVoxel =
                Chunk.GetVoxel(
                    LocalX,
                    LocalY,
                    TargetSurfaceLocalZ
                );

            if (RiverbedVoxel != nullptr)
            {
                RiverbedVoxel->MaterialId =
                    RiverbedMaterialId;

                RiverbedVoxel->SetWater(false);
            }

            if (!bInsideChannel)
            {
                continue;
            }

            ++RiverColumnCount;

            const int32 WaterTopLocalZ =
                FMath::Min(
                    HighestSolidLocalZ,
                    TargetSurfaceLocalZ +
                    WaterDepth
                );

            for (
                int32 LocalZ = TargetSurfaceLocalZ + 1;
                LocalZ <= WaterTopLocalZ;
                ++LocalZ
            )
            {
                FCubusBlockVoxel* WaterVoxel =
                    Chunk.GetVoxel(
                        LocalX,
                        LocalY,
                        LocalZ
                    );

                if (WaterVoxel == nullptr)
                {
                    continue;
                }

                WaterVoxel->MaterialId =
                    RiverWaterMaterialId;

                WaterVoxel->SetWater(true);
                ++RiverWaterVoxelCount;
            }
        }
    }

    UE_LOG(
        LogTemp,
        Display,
        TEXT(
            "Cubus rivers chunk (%d, %d, %d): %d channel columns, %d water voxels"
        ),
        ChunkCoordinate.X,
        ChunkCoordinate.Y,
        ChunkCoordinate.Z,
        RiverColumnCount,
        RiverWaterVoxelCount
    );
}

float FCubusBlockTerrainRiverGenerator::SampleRiverDistance(
    const int32 WorldX,
    const int32 WorldY,
    const UCubusGeologyProfile* GeologyProfile
)
{
    const float RiverFrequency =
        FMath::Max(
            0.000001f,
            GeologyProfile->RiverFrequency
        );

    const float WarpFrequency =
        FMath::Max(
            0.000001f,
            GeologyProfile->RiverWarpFrequency
        );

    const float WarpAmplitude =
        FMath::Max(
            0.0f,
            GeologyProfile->RiverWarpAmplitude
        );

    const float WarpX =
        FMath::PerlinNoise2D(
            FVector2D(
                static_cast<double>(WorldX) *
                    WarpFrequency,
                static_cast<double>(WorldY) *
                    WarpFrequency
            )
        ) *
        WarpAmplitude;

    const float WarpY =
        FMath::PerlinNoise2D(
            FVector2D(
                static_cast<double>(WorldX + 7919) *
                    WarpFrequency,
                static_cast<double>(WorldY - 3571) *
                    WarpFrequency
            )
        ) *
        WarpAmplitude;

    return FMath::Abs(
        FMath::PerlinNoise2D(
            FVector2D(
                (
                    static_cast<double>(WorldX) +
                    WarpX
                ) *
                RiverFrequency,
                (
                    static_cast<double>(WorldY) +
                    WarpY
                ) *
                RiverFrequency
            )
        )
    );
}

float FCubusBlockTerrainRiverGenerator::SmoothStep(
    const float EdgeMinimum,
    const float EdgeMaximum,
    const float Value
)
{
    if (FMath::IsNearlyEqual(EdgeMinimum, EdgeMaximum))
    {
        return Value >= EdgeMaximum
            ? 1.0f
            : 0.0f;
    }

    const float Alpha =
        FMath::Clamp(
            (
                Value -
                EdgeMinimum
            ) /
            (
                EdgeMaximum -
                EdgeMinimum
            ),
            0.0f,
            1.0f
        );

    return Alpha *
        Alpha *
        (
            3.0f -
            2.0f *
            Alpha
        );
}