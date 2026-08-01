#include "CubusCore/Generation/CubusTerrainDensityField.h"

FCubusTerrainDensityField::FCubusTerrainDensityField(
    const FCubusTerrainDensitySettings& InSettings
)
    : Settings(InSettings)
{
    Settings.ContinentAmplitude =
        FMath::Max(0.0f, Settings.ContinentAmplitude);
    Settings.ContinentFrequency =
        FMath::Max(0.000001f, Settings.ContinentFrequency);

    Settings.HillAmplitude =
        FMath::Max(0.0f, Settings.HillAmplitude);
    Settings.HillFrequency =
        FMath::Max(0.000001f, Settings.HillFrequency);

    Settings.DetailAmplitude =
        FMath::Max(0.0f, Settings.DetailAmplitude);
    Settings.DetailFrequency =
        FMath::Max(0.000001f, Settings.DetailFrequency);

    Settings.RidgeAmplitude =
        FMath::Max(0.0f, Settings.RidgeAmplitude);
    Settings.RidgeFrequency =
        FMath::Max(0.000001f, Settings.RidgeFrequency);

    Settings.ValleyDepth =
        FMath::Max(0.0f, Settings.ValleyDepth);
    Settings.ValleyFrequency =
        FMath::Max(0.000001f, Settings.ValleyFrequency);
    Settings.ValleyWidth =
        FMath::Clamp(Settings.ValleyWidth, 0.0f, 1.0f);
    Settings.ValleyFalloff =
        FMath::Clamp(Settings.ValleyFalloff, 0.001f, 1.0f);
    Settings.ValleyWarpAmplitude =
        FMath::Max(0.0f, Settings.ValleyWarpAmplitude);
    Settings.ValleyWarpFrequency =
        FMath::Max(0.000001f, Settings.ValleyWarpFrequency);

    Settings.RegionFrequency =
        FMath::Max(0.000001f, Settings.RegionFrequency);
    Settings.PlainsThreshold =
        FMath::Clamp(Settings.PlainsThreshold, -1.0f, 1.0f);
    Settings.PlainsBlend =
        FMath::Clamp(Settings.PlainsBlend, 0.001f, 1.0f);
    Settings.MountainThreshold =
        FMath::Clamp(
            Settings.MountainThreshold,
            Settings.PlainsThreshold,
            1.0f
        );
    Settings.MountainBlend =
        FMath::Clamp(Settings.MountainBlend, 0.001f, 1.0f);

    Settings.SurfaceMaterialId =
        FMath::Max(1, Settings.SurfaceMaterialId);
    Settings.SubsurfaceMaterialId =
        FMath::Max(1, Settings.SubsurfaceMaterialId);
    Settings.RockMaterialId =
        FMath::Max(1, Settings.RockMaterialId);
    Settings.SnowMaterialId =
        FMath::Max(1, Settings.SnowMaterialId);

    Settings.RockSlopeThreshold =
        FMath::Max(0.0f, Settings.RockSlopeThreshold);
    Settings.SurfaceMaterialDepth =
        FMath::Max(0.01f, Settings.SurfaceMaterialDepth);

    HeightCache.Reserve(1400);
    ColumnCache.Reserve(1400);
}

FCubusDensitySample FCubusTerrainDensityField::Sample(
    const FIntVector& GlobalSampleCoordinate
) const
{
    const FColumnData& Column =
        GetColumnData(
            GlobalSampleCoordinate.X,
            GlobalSampleCoordinate.Y
        );

    FCubusDensitySample Result;
    Result.Density =
        Column.SurfaceSampleZ -
        static_cast<float>(GlobalSampleCoordinate.Z);

    if (Result.Density <= 0.0f)
    {
        Result.MaterialId = 0;
        return Result;
    }

    const float DepthBelowSurface =
        Column.SurfaceSampleZ -
        static_cast<float>(GlobalSampleCoordinate.Z);

    Result.MaterialId =
        DepthBelowSurface <= Settings.SurfaceMaterialDepth
            ? Column.SurfaceMaterialId
            : Settings.SubsurfaceMaterialId;

    return Result;
}

float FCubusTerrainDensityField::SampleSurfaceVoxelHeight(
    const float WorldX,
    const float WorldY
) const
{
    if (!Settings.bUseHeightTerrain)
    {
        return Settings.FlatSurfaceWorldZ;
    }

    const FTerrainRegionWeights RegionWeights =
        SampleTerrainRegions(WorldX, WorldY);

    const float ContinentNoise =
        SampleNoise(
            WorldX,
            WorldY,
            Settings.ContinentFrequency
        );

    const float HillNoise =
        SampleNoise(
            WorldX + 1823.0f,
            WorldY - 917.0f,
            Settings.HillFrequency
        );

    const float DetailNoise =
        SampleNoise(
            WorldX - 431.0f,
            WorldY + 2671.0f,
            Settings.DetailFrequency
        );

    const float RidgeNoise =
        SampleRidgedNoise(
            WorldX + 911.0f,
            WorldY + 1511.0f,
            Settings.RidgeFrequency
        );

    const float ValleyMask =
        SampleValleyMask(
            WorldX - 1379.0f,
            WorldY + 733.0f
        );

    const float ContinentStrength =
        0.35f * RegionWeights.Plains +
        0.75f * RegionWeights.Rolling +
        1.00f * RegionWeights.Mountains;

    const float HillStrength =
        0.10f * RegionWeights.Plains +
        1.00f * RegionWeights.Rolling +
        0.65f * RegionWeights.Mountains;

    const float DetailStrength =
        0.15f * RegionWeights.Plains +
        0.55f * RegionWeights.Rolling +
        1.00f * RegionWeights.Mountains;

    const float RidgeStrength =
        0.00f * RegionWeights.Plains +
        0.20f * RegionWeights.Rolling +
        1.00f * RegionWeights.Mountains;

    const float ValleyStrength =
        0.35f * RegionWeights.Plains +
        0.75f * RegionWeights.Rolling +
        1.00f * RegionWeights.Mountains;

    return
        Settings.BaseHeight +
        ContinentNoise *
            Settings.ContinentAmplitude *
            ContinentStrength +
        HillNoise *
            Settings.HillAmplitude *
            HillStrength +
        DetailNoise *
            Settings.DetailAmplitude *
            DetailStrength +
        RidgeNoise *
            Settings.RidgeAmplitude *
            RidgeStrength -
        ValleyMask *
            Settings.ValleyDepth *
            ValleyStrength;
}

float FCubusTerrainDensityField::GetCachedSurfaceVoxelHeight(
    const int32 WorldSampleX,
    const int32 WorldSampleY
) const
{
    const FIntPoint Key(WorldSampleX, WorldSampleY);

    if (const float* ExistingHeight = HeightCache.Find(Key))
    {
        return *ExistingHeight;
    }

    // Existing block terrain evaluates its column function at the block-cell
    // coordinate. In density sample space that cell centre is X + 0.5, so a
    // corner sample at X evaluates the same function at X - 0.5.
    const float Height =
        SampleSurfaceVoxelHeight(
            static_cast<float>(WorldSampleX) - 0.5f,
            static_cast<float>(WorldSampleY) - 0.5f
        );

    HeightCache.Add(Key, Height);
    return Height;
}

const FCubusTerrainDensityField::FColumnData&
FCubusTerrainDensityField::GetColumnData(
    const int32 WorldSampleX,
    const int32 WorldSampleY
) const
{
    const FIntPoint Key(WorldSampleX, WorldSampleY);

    if (const FColumnData* ExistingColumn = ColumnCache.Find(Key))
    {
        return *ExistingColumn;
    }

    FColumnData Column;
    Column.SurfaceVoxelHeight =
        GetCachedSurfaceVoxelHeight(
            WorldSampleX,
            WorldSampleY
        );

    // A generated surface voxel at integer height H occupies the cell ending
    // at sample plane H + 1. Keeping this +1 preserves the current world-space
    // vertical alignment while allowing H itself to remain fractional.
    Column.SurfaceSampleZ =
        Column.SurfaceVoxelHeight +
        1.0f;

    const float HeightPositiveX =
        GetCachedSurfaceVoxelHeight(
            WorldSampleX + 1,
            WorldSampleY
        );

    const float HeightNegativeX =
        GetCachedSurfaceVoxelHeight(
            WorldSampleX - 1,
            WorldSampleY
        );

    const float HeightPositiveY =
        GetCachedSurfaceVoxelHeight(
            WorldSampleX,
            WorldSampleY + 1
        );

    const float HeightNegativeY =
        GetCachedSurfaceVoxelHeight(
            WorldSampleX,
            WorldSampleY - 1
        );

    const float GradientX =
        (HeightPositiveX - HeightNegativeX) *
        0.5f;

    const float GradientY =
        (HeightPositiveY - HeightNegativeY) *
        0.5f;

    Column.Slope =
        FMath::Sqrt(
            GradientX * GradientX +
            GradientY * GradientY
        );

    if (Column.Slope >= Settings.RockSlopeThreshold)
    {
        Column.SurfaceMaterialId =
            Settings.RockMaterialId;
    }
    else if (
        Column.SurfaceVoxelHeight >=
        Settings.SnowMinimumHeight
    )
    {
        Column.SurfaceMaterialId =
            Settings.SnowMaterialId;
    }
    else
    {
        Column.SurfaceMaterialId =
            Settings.SurfaceMaterialId;
    }

    ColumnCache.Add(Key, Column);
    return ColumnCache.FindChecked(Key);
}

FCubusTerrainDensityField::FTerrainRegionWeights
FCubusTerrainDensityField::SampleTerrainRegions(
    const float WorldX,
    const float WorldY
) const
{
    const float RegionNoise =
        SampleNoise(
            WorldX + 10427.0f,
            WorldY - 8633.0f,
            Settings.RegionFrequency
        );

    const float PlainsExit =
        SmoothStep(
            Settings.PlainsThreshold -
                Settings.PlainsBlend,
            Settings.PlainsThreshold +
                Settings.PlainsBlend,
            RegionNoise
        );

    const float MountainEntry =
        SmoothStep(
            Settings.MountainThreshold -
                Settings.MountainBlend,
            Settings.MountainThreshold +
                Settings.MountainBlend,
            RegionNoise
        );

    FTerrainRegionWeights Result;
    Result.Plains = 1.0f - PlainsExit;
    Result.Mountains = MountainEntry;
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
        Result.Plains /= TotalWeight;
        Result.Rolling /= TotalWeight;
        Result.Mountains /= TotalWeight;
    }
    else
    {
        Result.Plains = 0.0f;
        Result.Rolling = 1.0f;
        Result.Mountains = 0.0f;
    }

    return Result;
}

float FCubusTerrainDensityField::SampleNoise(
    const float WorldX,
    const float WorldY,
    const float Frequency
) const
{
    const float SafeFrequency =
        FMath::Max(0.000001f, Frequency);

    return FMath::PerlinNoise2D(
        FVector2D(
            WorldX * SafeFrequency,
            WorldY * SafeFrequency
        )
    );
}

float FCubusTerrainDensityField::SampleRidgedNoise(
    const float WorldX,
    const float WorldY,
    const float Frequency
) const
{
    const float NoiseValue =
        SampleNoise(
            WorldX,
            WorldY,
            Frequency
        );

    const float RidgeValue =
        1.0f -
        FMath::Abs(NoiseValue);

    return RidgeValue * RidgeValue;
}

float FCubusTerrainDensityField::SampleValleyMask(
    const float WorldX,
    const float WorldY
) const
{
    const float WarpX =
        SampleNoise(
            WorldX + 4871.0f,
            WorldY - 3253.0f,
            Settings.ValleyWarpFrequency
        ) *
        Settings.ValleyWarpAmplitude;

    const float WarpY =
        SampleNoise(
            WorldX - 761.0f,
            WorldY + 5987.0f,
            Settings.ValleyWarpFrequency
        ) *
        Settings.ValleyWarpAmplitude;

    const float ValleyNoise =
        SampleNoise(
            WorldX + WarpX,
            WorldY + WarpY,
            Settings.ValleyFrequency
        );

    const float DistanceFromChannel =
        FMath::Abs(ValleyNoise);

    const float OuterEdge =
        FMath::Min(
            1.0f,
            Settings.ValleyWidth +
                Settings.ValleyFalloff
        );

    if (DistanceFromChannel >= OuterEdge)
    {
        return 0.0f;
    }

    if (DistanceFromChannel <= Settings.ValleyWidth)
    {
        return 1.0f;
    }

    const float BlendRange =
        FMath::Max(
            0.001f,
            OuterEdge -
                Settings.ValleyWidth
        );

    const float NormalizedDistance =
        (
            DistanceFromChannel -
            Settings.ValleyWidth
        ) /
        BlendRange;

    const float SmoothDistance =
        NormalizedDistance *
        NormalizedDistance *
        (
            3.0f -
            2.0f * NormalizedDistance
        );

    return 1.0f - SmoothDistance;
}

float FCubusTerrainDensityField::SmoothStep(
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

    return
        Alpha *
        Alpha *
        (
            3.0f -
            2.0f * Alpha
        );
}
