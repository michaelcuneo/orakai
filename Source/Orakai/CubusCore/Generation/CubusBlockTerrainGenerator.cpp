#include "CubusCore/Generation/CubusBlockTerrainGenerator.h"

#include "CubusCore/Data/CubusBlockVoxel.h"
#include "CubusCore/Chunks/CubusBlockChunkData.h"
#include "CubusCore/Chunks/CubusChunkConstants.h"

namespace CubusBlockTerrainGenerator
{
    void SetTerrainVoxel(
        FCubusBlockChunkData& Chunk,
        const int32 LocalX,
        const int32 LocalY,
        const int32 LocalZ,
        const int32 MaterialId
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
            return;
        }

        Voxel->MaterialId =
            MaterialId;

        Voxel->SetWater(false);
    }

    void SetWaterVoxel(
        FCubusBlockChunkData& Chunk,
        const int32 LocalX,
        const int32 LocalY,
        const int32 LocalZ,
        const int32 MaterialId
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
            return;
        }

        Voxel->MaterialId =
            MaterialId;

        Voxel->SetWater(true);
    }
}

void FCubusBlockTerrainGenerator::GenerateFlatTerrain(
    FCubusBlockChunkData& Chunk,
    const int32 SurfaceWorldZ,
    const int32 SurfaceMaterialId,
    const int32 SubsurfaceMaterialId
)
{
    Chunk.Clear();

    const FIntVector ChunkCoordinate =
        Chunk.GetChunkCoordinate();

    const int32 ChunkWorldBaseZ =
        ChunkCoordinate.Z *
        Cubus::ChunkSize;

    for (
        int32 LocalZ = 0;
        LocalZ < Cubus::ChunkSize;
        ++LocalZ
    )
    {
        const int32 WorldZ =
            ChunkWorldBaseZ +
            LocalZ;

        if (WorldZ > SurfaceWorldZ)
        {
            continue;
        }

        const int32 MaterialId =
            WorldZ == SurfaceWorldZ
                ? SurfaceMaterialId
                : SubsurfaceMaterialId;

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
                CubusBlockTerrainGenerator::SetTerrainVoxel(
                    Chunk,
                    LocalX,
                    LocalY,
                    LocalZ,
                    MaterialId
                );
            }
        }
    }
}

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
    const int32 WaterMaterialId
){
    Chunk.Clear();

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

            const int32 SelectedSurfaceMaterialId =
                SelectSurfaceMaterial(
                    WorldX,
                    WorldY,
                    SurfaceWorldZ,
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
                    RockMaterialId,
                    SnowMaterialId,
                    RockSlopeThreshold,
                    SnowMinimumHeight
                );

                const int32 SafeWaterMaterialId =
                    FMath::Max(
                        1,
                        WaterMaterialId
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

                    if (WorldZ <= SurfaceWorldZ)
                    {
                        const int32 MaterialId =
                            WorldZ == SurfaceWorldZ
                                ? SelectedSurfaceMaterialId
                                : SubsurfaceMaterialId;

                        CubusBlockTerrainGenerator::SetTerrainVoxel(
                            Chunk,
                            LocalX,
                            LocalY,
                            LocalZ,
                            MaterialId
                        );

                        continue;
                    }

                    if (
                        bGenerateWater &&
                        WorldZ <= WaterLevel
                    )
                    {
                        CubusBlockTerrainGenerator::SetWaterVoxel(
                            Chunk,
                            LocalX,
                            LocalY,
                            LocalZ,
                            SafeWaterMaterialId
                        );
                    }
                }
              }
    }
}

int32 FCubusBlockTerrainGenerator::SampleTerrainHeight(
    const int32 WorldX,
    const int32 WorldY,
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
    const float MountainBlend
)
{
    const FTerrainRegionWeights RegionWeights =
        SampleTerrainRegions(
            WorldX,
            WorldY,
            RegionFrequency,
            PlainsThreshold,
            PlainsBlend,
            MountainThreshold,
            MountainBlend
        );

    const float ContinentNoise =
        SampleNoise(
            WorldX,
            WorldY,
            ContinentFrequency
        );

    const float HillNoise =
        SampleNoise(
            WorldX + 1823,
            WorldY - 917,
            HillFrequency
        );

    const float DetailNoise =
        SampleNoise(
            WorldX - 431,
            WorldY + 2671,
            DetailFrequency
        );

    const float RidgeNoise =
        SampleRidgedNoise(
            WorldX + 911,
            WorldY + 1511,
            RidgeFrequency
        );

    const float ValleyMask =
        SampleValleyMask(
            WorldX - 1379,
            WorldY + 733,
            ValleyFrequency,
            ValleyWidth,
            ValleyFalloff,
            ValleyWarpAmplitude,
            ValleyWarpFrequency
        );

    const float PlainsInfluence =
        RegionWeights.Plains;

    const float RollingInfluence =
        RegionWeights.Rolling;

    const float MountainInfluence =
        RegionWeights.Mountains;

    const float ContinentStrength =
        0.35f *
            PlainsInfluence +
        0.75f *
            RollingInfluence +
        1.0f *
            MountainInfluence;

    const float HillStrength =
        0.10f *
            PlainsInfluence +
        1.0f *
            RollingInfluence +
        0.65f *
            MountainInfluence;

    const float DetailStrength =
        0.15f *
            PlainsInfluence +
        0.55f *
            RollingInfluence +
        1.0f *
            MountainInfluence;

    const float RidgeStrength =
        0.0f *
            PlainsInfluence +
        0.20f *
            RollingInfluence +
        1.0f *
            MountainInfluence;

    const float ValleyStrength =
        0.35f *
            PlainsInfluence +
        0.75f *
            RollingInfluence +
        1.0f *
            MountainInfluence;

    const float HeightOffset =
        ContinentNoise *
            FMath::Max(
                0.0f,
                ContinentAmplitude
            ) *
            ContinentStrength +
        HillNoise *
            FMath::Max(
                0.0f,
                HillAmplitude
            ) *
            HillStrength +
        DetailNoise *
            FMath::Max(
                0.0f,
                DetailAmplitude
            ) *
            DetailStrength +
        RidgeNoise *
            FMath::Max(
                0.0f,
                RidgeAmplitude
            ) *
            RidgeStrength -
        ValleyMask *
            FMath::Max(
                0.0f,
                ValleyDepth
            ) *
            ValleyStrength;

    return BaseHeight +
        FMath::RoundToInt(
            HeightOffset
        );
}

int32 FCubusBlockTerrainGenerator::SelectSurfaceMaterial(
    const int32 WorldX,
    const int32 WorldY,
    const int32 SurfaceWorldZ,
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
    const int32 RockMaterialId,
    const int32 SnowMaterialId,
    const float RockSlopeThreshold,
    const int32 SnowMinimumHeight
)
{
    const int32 HeightPositiveX =
        SampleTerrainHeight(
            WorldX + 1,
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

    const int32 HeightNegativeX =
        SampleTerrainHeight(
            WorldX - 1,
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

    const int32 HeightPositiveY =
        SampleTerrainHeight(
            WorldX,
            WorldY + 1,
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

    const int32 HeightNegativeY =
        SampleTerrainHeight(
            WorldX,
            WorldY - 1,
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

    const float GradientX =
        static_cast<float>(
            HeightPositiveX -
            HeightNegativeX
        ) *
        0.5f;

    const float GradientY =
        static_cast<float>(
            HeightPositiveY -
            HeightNegativeY
        ) *
        0.5f;

    const float Slope =
        FMath::Sqrt(
            GradientX *
                GradientX +
            GradientY *
                GradientY
        );

    const float SafeRockSlopeThreshold =
        FMath::Max(
            0.0f,
            RockSlopeThreshold
        );

    if (Slope >= SafeRockSlopeThreshold)
    {
        return FMath::Max(
            1,
            RockMaterialId
        );
    }

    if (SurfaceWorldZ >= SnowMinimumHeight)
    {
        return FMath::Max(
            1,
            SnowMaterialId
        );
    }

    return FMath::Max(
        1,
        SurfaceMaterialId
    );
}

FCubusBlockTerrainGenerator::FTerrainRegionWeights
FCubusBlockTerrainGenerator::SampleTerrainRegions(
    const int32 WorldX,
    const int32 WorldY,
    const float Frequency,
    const float PlainsThreshold,
    const float PlainsBlend,
    const float MountainThreshold,
    const float MountainBlend
)
{
    const float SafeFrequency =
        FMath::Max(
            0.000001f,
            Frequency
        );

    const float SafePlainsBlend =
        FMath::Max(
            0.001f,
            PlainsBlend
        );

    const float SafeMountainBlend =
        FMath::Max(
            0.001f,
            MountainBlend
        );

    const float ClampedPlainsThreshold =
        FMath::Clamp(
            PlainsThreshold,
            -1.0f,
            1.0f
        );

    const float ClampedMountainThreshold =
        FMath::Clamp(
            MountainThreshold,
            ClampedPlainsThreshold,
            1.0f
        );

    const float RegionNoise =
        SampleNoise(
            WorldX + 10427,
            WorldY - 8633,
            SafeFrequency
        );

    const float PlainsExit =
        SmoothStep(
            ClampedPlainsThreshold -
                SafePlainsBlend,
            ClampedPlainsThreshold +
                SafePlainsBlend,
            RegionNoise
        );

    const float MountainEntry =
        SmoothStep(
            ClampedMountainThreshold -
                SafeMountainBlend,
            ClampedMountainThreshold +
                SafeMountainBlend,
            RegionNoise
        );

    FTerrainRegionWeights Result;

    Result.Plains =
        1.0f -
        PlainsExit;

    Result.Mountains =
        MountainEntry;

    Result.Rolling =
        FMath::Max(
            0.0f,
            1.0f -
                Result.Plains -
                Result.Mountains
        );

    const float TotalWeight =
        Result.Plains +
        Result.Rolling +
        Result.Mountains;

    if (TotalWeight > KINDA_SMALL_NUMBER)
    {
        Result.Plains /=
            TotalWeight;

        Result.Rolling /=
            TotalWeight;

        Result.Mountains /=
            TotalWeight;
    }
    else
    {
        Result.Plains = 0.0f;
        Result.Rolling = 1.0f;
        Result.Mountains = 0.0f;
    }

    return Result;
}

float FCubusBlockTerrainGenerator::SampleNoise(
    const int32 WorldX,
    const int32 WorldY,
    const float Frequency
)
{
    return SampleNoise(
        static_cast<float>(WorldX),
        static_cast<float>(WorldY),
        Frequency
    );
}

float FCubusBlockTerrainGenerator::SampleNoise(
    const float WorldX,
    const float WorldY,
    const float Frequency
)
{
    const float SafeFrequency =
        FMath::Max(
            0.000001f,
            Frequency
        );

    return FMath::PerlinNoise2D(
        FVector2D(
            WorldX *
                SafeFrequency,
            WorldY *
                SafeFrequency
        )
    );
}

float FCubusBlockTerrainGenerator::SampleRidgedNoise(
    const int32 WorldX,
    const int32 WorldY,
    const float Frequency
)
{
    const float NoiseValue =
        SampleNoise(
            WorldX,
            WorldY,
            Frequency
        );

    const float RidgeValue =
        1.0f -
        FMath::Abs(
            NoiseValue
        );

    return RidgeValue *
        RidgeValue;
}

float FCubusBlockTerrainGenerator::SampleValleyMask(
    const int32 WorldX,
    const int32 WorldY,
    const float Frequency,
    const float Width,
    const float Falloff,
    const float WarpAmplitude,
    const float WarpFrequency
)
{
    const float SafeWidth =
        FMath::Clamp(
            Width,
            0.0f,
            1.0f
        );

    const float SafeFalloff =
        FMath::Max(
            0.001f,
            Falloff
        );

    const float SafeWarpAmplitude =
        FMath::Max(
            0.0f,
            WarpAmplitude
        );

    const float SafeWarpFrequency =
        FMath::Max(
            0.000001f,
            WarpFrequency
        );

    const float WarpX =
        SampleNoise(
            WorldX + 4871,
            WorldY - 3253,
            SafeWarpFrequency
        ) *
        SafeWarpAmplitude;

    const float WarpY =
        SampleNoise(
            WorldX - 761,
            WorldY + 5987,
            SafeWarpFrequency
        ) *
        SafeWarpAmplitude;

    const float WarpedWorldX =
        static_cast<float>(WorldX) +
        WarpX;

    const float WarpedWorldY =
        static_cast<float>(WorldY) +
        WarpY;

    const float ValleyNoise =
        SampleNoise(
            WarpedWorldX,
            WarpedWorldY,
            Frequency
        );

    const float DistanceFromChannel =
        FMath::Abs(
            ValleyNoise
        );

    const float OuterEdge =
        FMath::Min(
            1.0f,
            SafeWidth +
            SafeFalloff
        );

    if (DistanceFromChannel >= OuterEdge)
    {
        return 0.0f;
    }

    if (DistanceFromChannel <= SafeWidth)
    {
        return 1.0f;
    }

    const float BlendRange =
        FMath::Max(
            0.001f,
            OuterEdge -
            SafeWidth
        );

    const float NormalizedDistance =
        (
            DistanceFromChannel -
            SafeWidth
        ) /
        BlendRange;

    const float SmoothDistance =
        NormalizedDistance *
        NormalizedDistance *
        (
            3.0f -
            2.0f *
                NormalizedDistance
        );

    return 1.0f -
        SmoothDistance;
}

float FCubusBlockTerrainGenerator::SmoothStep(
    const float EdgeMinimum,
    const float EdgeMaximum,
    const float Value
)
{
    if (
        FMath::IsNearlyEqual(
            EdgeMinimum,
            EdgeMaximum
        )
    )
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