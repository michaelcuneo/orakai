#include "CubusCore/Generation/CubusBlockTerrainRiverGenerator.h"

#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Data/CubusBlockVoxel.h"
#include "CubusCore/Data/CubusGeologyProfile.h"
#include "CubusCore/Generation/CubusBlockTerrainBiomeGenerator.h"

void FCubusBlockTerrainRiverGenerator::Apply(
    FCubusBlockChunkData& Chunk,
    const UCubusGeologyProfile* GeologyProfile,
    const FCubusHydrologySettings& HydrologySettings
)
{
    if (!IsValid(GeologyProfile))
    {
        return;
    }

    if (GeologyProfile->bGenerateRivers && HydrologySettings.bEnabled)
    {
        const FIntVector ChunkCoordinate = Chunk.GetChunkCoordinate();
        const int32 BaseX = ChunkCoordinate.X * Cubus::ChunkSize;
        const int32 BaseY = ChunkCoordinate.Y * Cubus::ChunkSize;
        const int32 BaseZ = ChunkCoordinate.Z * Cubus::ChunkSize;

        const int32 WaterDepth = FMath::Max(1, GeologyProfile->RiverWaterDepth);
        const int32 RiverbedMaterialId = FMath::Max(1, GeologyProfile->RiverbedMaterialId);
        const int32 RiverWaterMaterialId = FMath::Max(1, GeologyProfile->RiverWaterMaterialId);

        int32 RiverColumnCount = 0;
        int32 RiverWaterVoxelCount = 0;
        int32 BuriedColumnCount = 0;

        for (int32 LocalY = 0; LocalY < Cubus::ChunkSize; ++LocalY)
        {
            const int32 WorldY = BaseY + LocalY;

            for (int32 LocalX = 0; LocalX < Cubus::ChunkSize; ++LocalX)
            {
                const int32 WorldX = BaseX + LocalX;
                const FCubusHydrologySample Hydrology = FCubusHydrologyField::Sample(
                    static_cast<float>(WorldX),
                    static_cast<float>(WorldY),
                    HydrologySettings
                );

                if (!Hydrology.IsChannel())
                {
                    continue;
                }

                const float Strength = 0.20f + Hydrology.ChannelStrength * 0.80f;
                const float ChannelHalfWidth = FMath::Lerp(
                    HydrologySettings.ChannelHalfWidth * 0.75f,
                    HydrologySettings.ChannelHalfWidth * 1.35f,
                    Hydrology.ChannelStrength
                );
                const float ValleyHalfWidth = FMath::Lerp(
                    ChannelHalfWidth * 2.5f,
                    HydrologySettings.ValleyHalfWidth,
                    Hydrology.ChannelStrength
                );

                if (Hydrology.DistanceToChannel >= ValleyHalfWidth)
                {
                    continue;
                }

                int32 HighestSolidLocalZ = INDEX_NONE;
                for (int32 LocalZ = Cubus::ChunkSize - 1; LocalZ >= 0; --LocalZ)
                {
                    const FCubusBlockVoxel* Voxel = Chunk.GetVoxel(LocalX, LocalY, LocalZ);
                    if (Voxel != nullptr && Voxel->MaterialId > 0 && !Voxel->IsWater())
                    {
                        HighestSolidLocalZ = LocalZ;
                        break;
                    }
                }

                if (HighestSolidLocalZ == INDEX_NONE)
                {
                    continue;
                }

                if (HighestSolidLocalZ == Cubus::ChunkSize - 1)
                {
                    ++BuriedColumnCount;
                    continue;
                }

                const FCubusBlockVoxel* AboveVoxel = Chunk.GetVoxel(LocalX, LocalY, HighestSolidLocalZ + 1);
                if (AboveVoxel != nullptr && (AboveVoxel->MaterialId > 0 || AboveVoxel->IsWater()))
                {
                    ++BuriedColumnCount;
                    continue;
                }

                const float SurfaceWorldZ = static_cast<float>(BaseZ + HighestSolidLocalZ);
                const float ValleyInfluence = 1.0f - SmoothStep(
                    ChannelHalfWidth,
                    ValleyHalfWidth,
                    Hydrology.DistanceToChannel
                );
                const float ChannelInfluence = 1.0f - SmoothStep(
                    0.0f,
                    ChannelHalfWidth * 1.8f,
                    Hydrology.DistanceToChannel
                );

                const float ValleyDepth = HydrologySettings.ValleyDepth * Strength;
                const float ChannelDepth = HydrologySettings.ChannelDepth * Strength;
                const float ValleyFloorWorldZ = Hydrology.HydraulicHeight - ValleyDepth;
                const float ValleyTargetWorldZ = FMath::Lerp(SurfaceWorldZ, ValleyFloorWorldZ, ValleyInfluence);
                const float ChannelFloorWorldZ = Hydrology.HydraulicHeight - ValleyDepth - ChannelDepth;
                const float TargetWorldZ = FMath::Min(
                    SurfaceWorldZ,
                    FMath::Lerp(ValleyTargetWorldZ, ChannelFloorWorldZ, ChannelInfluence)
                );

                const int32 TargetSurfaceLocalZ = FMath::Clamp(
                    FMath::FloorToInt(TargetWorldZ) - BaseZ,
                    0,
                    Cubus::ChunkSize - 1
                );

                if (TargetSurfaceLocalZ >= HighestSolidLocalZ)
                {
                    continue;
                }

                for (int32 LocalZ = TargetSurfaceLocalZ + 1; LocalZ <= HighestSolidLocalZ; ++LocalZ)
                {
                    FCubusBlockVoxel* Voxel = Chunk.GetVoxel(LocalX, LocalY, LocalZ);
                    if (Voxel == nullptr)
                    {
                        continue;
                    }
                    Voxel->MaterialId = 0;
                    Voxel->SetWater(false);
                }

                FCubusBlockVoxel* RiverbedVoxel = Chunk.GetVoxel(LocalX, LocalY, TargetSurfaceLocalZ);
                if (RiverbedVoxel != nullptr)
                {
                    RiverbedVoxel->MaterialId = RiverbedMaterialId;
                    RiverbedVoxel->SetWater(false);
                }

                const bool bInsideChannel = Hydrology.DistanceToChannel <= ChannelHalfWidth;
                if (!bInsideChannel)
                {
                    continue;
                }

                ++RiverColumnCount;

                const int32 HydraulicWaterTopWorldZ = FMath::FloorToInt(
                    Hydrology.HydraulicHeight - ValleyDepth
                );
                const int32 RequestedWaterTopLocalZ = FMath::Min(
                    TargetSurfaceLocalZ + WaterDepth,
                    HydraulicWaterTopWorldZ - BaseZ
                );
                const int32 WaterTopLocalZ = FMath::Clamp(
                    RequestedWaterTopLocalZ,
                    TargetSurfaceLocalZ,
                    HighestSolidLocalZ
                );

                for (int32 LocalZ = TargetSurfaceLocalZ + 1; LocalZ <= WaterTopLocalZ; ++LocalZ)
                {
                    FCubusBlockVoxel* WaterVoxel = Chunk.GetVoxel(LocalX, LocalY, LocalZ);
                    if (WaterVoxel == nullptr)
                    {
                        continue;
                    }

                    WaterVoxel->MaterialId = RiverWaterMaterialId;
                    WaterVoxel->SetWater(true);
                    ++RiverWaterVoxelCount;
                }
            }
        }

        UE_LOG(
            LogTemp,
            Display,
            TEXT("Cubus hydrology chunk (%d, %d, %d): %d channel columns, %d water voxels, buried skipped %d"),
            ChunkCoordinate.X,
            ChunkCoordinate.Y,
            ChunkCoordinate.Z,
            RiverColumnCount,
            RiverWaterVoxelCount,
            BuriedColumnCount
        );
    }

    FCubusBlockTerrainBiomeGenerator::Apply(
        Chunk,
        GeologyProfile,
        &HydrologySettings
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
        return Value >= EdgeMaximum ? 1.0f : 0.0f;
    }

    const float Alpha = FMath::Clamp(
        (Value - EdgeMinimum) /
        (EdgeMaximum - EdgeMinimum),
        0.0f,
        1.0f
    );

    return Alpha * Alpha * (3.0f - 2.0f * Alpha);
}
