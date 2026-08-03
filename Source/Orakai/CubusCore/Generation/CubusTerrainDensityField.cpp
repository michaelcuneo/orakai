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

    Settings.RiverFrequency =
        FMath::Max(0.000001f, Settings.RiverFrequency);
    Settings.RiverChannelWidth =
        FMath::Clamp(Settings.RiverChannelWidth, 0.0f, 1.0f);
    Settings.RiverValleyWidth =
        FMath::Clamp(
            FMath::Max(
                Settings.RiverChannelWidth + 0.0001f,
                Settings.RiverValleyWidth
            ),
            0.0001f,
            1.0f
        );
    Settings.RiverValleyDepth =
        FMath::Max(0.0f, Settings.RiverValleyDepth);
    Settings.RiverChannelDepth =
        FMath::Max(0.0f, Settings.RiverChannelDepth);
    Settings.RiverWarpAmplitude =
        FMath::Max(0.0f, Settings.RiverWarpAmplitude);
    Settings.RiverWarpFrequency =
        FMath::Max(0.000001f, Settings.RiverWarpFrequency);

    if (Settings.CaveMinimumWorldZ > Settings.CaveMaximumWorldZ)
    {
        Swap(
            Settings.CaveMinimumWorldZ,
            Settings.CaveMaximumWorldZ
        );
    }

    Settings.CaveSurfaceClearance =
        FMath::Max(1, Settings.CaveSurfaceClearance);
    Settings.CavePrimaryFrequency =
        FMath::Max(0.000001f, Settings.CavePrimaryFrequency);
    Settings.CaveSecondaryFrequency =
        FMath::Max(0.000001f, Settings.CaveSecondaryFrequency);
    Settings.CaveThreshold =
        FMath::Clamp(Settings.CaveThreshold, 0.0f, 1.0f);
    Settings.CaveSurfaceSharpness =
        FMath::Max(0.001f, Settings.CaveSurfaceSharpness);

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
    Settings.RockMaterialDepth = FMath::Max(
        Settings.SurfaceMaterialDepth,
        Settings.RockMaterialDepth
    );

    TerrainFormSettings.BaseHeight = Settings.BaseHeight;
    TerrainFormSettings.ContinentAmplitude = Settings.ContinentAmplitude;
    TerrainFormSettings.ContinentFrequency = Settings.ContinentFrequency;
    TerrainFormSettings.HillAmplitude = Settings.HillAmplitude;
    TerrainFormSettings.HillFrequency = Settings.HillFrequency;
    TerrainFormSettings.DetailAmplitude = Settings.DetailAmplitude;
    TerrainFormSettings.DetailFrequency = Settings.DetailFrequency;
    TerrainFormSettings.RidgeAmplitude = Settings.RidgeAmplitude;
    TerrainFormSettings.RidgeFrequency = Settings.RidgeFrequency;
    TerrainFormSettings.ValleyDepth = Settings.ValleyDepth;
    TerrainFormSettings.ValleyFrequency = Settings.ValleyFrequency;
    TerrainFormSettings.ValleyWidth = Settings.ValleyWidth;
    TerrainFormSettings.ValleyFalloff = Settings.ValleyFalloff;
    TerrainFormSettings.ValleyWarpAmplitude = Settings.ValleyWarpAmplitude;
    TerrainFormSettings.ValleyWarpFrequency = Settings.ValleyWarpFrequency;
    TerrainFormSettings.RegionFrequency = Settings.RegionFrequency;
    TerrainFormSettings.PlainsThreshold = Settings.PlainsThreshold;
    TerrainFormSettings.PlainsBlend = Settings.PlainsBlend;
    TerrainFormSettings.MountainThreshold = Settings.MountainThreshold;
    TerrainFormSettings.MountainBlend = Settings.MountainBlend;

    HeightCache.Reserve(1600);
    ColumnCache.Reserve(1600);
}

FCubusDensitySample FCubusTerrainDensityField::Sample(
    const FIntVector& GlobalSampleCoordinate
) const
{
    return SampleContinuous(
        FVector(
            static_cast<double>(GlobalSampleCoordinate.X),
            static_cast<double>(GlobalSampleCoordinate.Y),
            static_cast<double>(GlobalSampleCoordinate.Z)
        )
    );
}

FCubusDensitySample FCubusTerrainDensityField::SampleContinuous(
    const FVector& GlobalSampleCoordinate
) const
{
    const FColumnData& Column =
        GetColumnData(
            static_cast<float>(GlobalSampleCoordinate.X),
            static_cast<float>(GlobalSampleCoordinate.Y)
        );

    const float TerrainDensity =
        Column.SurfaceSampleZ -
        static_cast<float>(GlobalSampleCoordinate.Z);

    float CompositeDensity = TerrainDensity;

    if (Settings.bGenerateCaves)
    {
        CompositeDensity =
            FMath::Min(
                CompositeDensity,
                SampleCaveDensity(
                    GlobalSampleCoordinate,
                    Column.SurfaceVoxelHeight
                )
            );
    }

    FCubusDensitySample Result;
    Result.Density = CompositeDensity;

    if (CompositeDensity <= 0.0f)
    {
        Result.MaterialId = 0;
        return Result;
    }

    const float DepthBelowSurface =
        Column.SurfaceSampleZ -
        static_cast<float>(GlobalSampleCoordinate.Z);

    if (DepthBelowSurface <= Settings.SurfaceMaterialDepth)
    {
        Result.MaterialId = Column.SurfaceMaterialId;
    }
    else if (DepthBelowSurface >= Settings.RockMaterialDepth)
    {
        Result.MaterialId = Settings.RockMaterialId;
    }
    else
    {
        Result.MaterialId = Settings.SubsurfaceMaterialId;
    }

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

    const float TerrainX =
        WorldX +
        static_cast<float>(Settings.TerrainOffsetX);

    const float TerrainY =
        WorldY +
        static_cast<float>(Settings.TerrainOffsetY);

    const float BaseSurfaceHeight = FCubusTerrainForm::Sample(
        TerrainX,
        TerrainY,
        TerrainFormSettings
    ).Height;

    const FCubusLandmarkSample LandmarkSample = FCubusLandmarkField::Sample(
        TerrainX,
        TerrainY,
        Settings.LandmarkSettings
    );

    return ApplyRiverLowering(
        BaseSurfaceHeight + LandmarkSample.HeightOffset,
        WorldX,
        WorldY
    );
}

FIntPoint FCubusTerrainDensityField::MakeCoordinateCacheKey(
    const float WorldX,
    const float WorldY
)
{
    return FIntPoint(
        FMath::RoundToInt(WorldX * CoordinateCacheScale),
        FMath::RoundToInt(WorldY * CoordinateCacheScale)
    );
}

float FCubusTerrainDensityField::GetCachedSurfaceVoxelHeight(
    const float WorldSampleX,
    const float WorldSampleY
) const
{
    const FIntPoint Key = MakeCoordinateCacheKey(
        WorldSampleX,
        WorldSampleY
    );

    if (const float* ExistingHeight = HeightCache.Find(Key))
    {
        return *ExistingHeight;
    }

    // Integer density samples represent voxel corners. The existing block
    // generator evaluates columns at cell coordinates, so sample X/Y map to
    // that same domain at X/Y - 0.5.
    const float Height =
        SampleSurfaceVoxelHeight(
            WorldSampleX - 0.5f,
            WorldSampleY - 0.5f
        );

    HeightCache.Add(Key, Height);
    return Height;
}

const FCubusTerrainDensityField::FColumnData&
FCubusTerrainDensityField::GetColumnData(
    const float WorldSampleX,
    const float WorldSampleY
) const
{
    const FIntPoint Key = MakeCoordinateCacheKey(
        WorldSampleX,
        WorldSampleY
    );

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

    // A block surface at voxel height H ends at the sample plane H + 1.
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

    const float TerrainX =
        WorldSampleX - 0.5f +
        static_cast<float>(Settings.TerrainOffsetX);
    const float TerrainY =
        WorldSampleY - 0.5f +
        static_cast<float>(Settings.TerrainOffsetY);
    const FCubusLandmarkSample LandmarkSample = FCubusLandmarkField::Sample(
        TerrainX,
        TerrainY,
        Settings.LandmarkSettings
    );

    if (LandmarkSample.IsInside())
    {
        Column.SurfaceMaterialId = FMath::Max(
            1,
            Settings.LandmarkSettings.SurfaceMaterialId
        );
    }
    else if (Column.Slope >= Settings.RockSlopeThreshold)
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
    else if (Settings.BiomeSettings.bEnabled)
    {
        Column.SurfaceMaterialId = FCubusBiomeField::Sample(
            WorldSampleX - 0.5f,
            WorldSampleY - 0.5f,
            Column.SurfaceVoxelHeight,
            Column.Slope,
            Settings.BiomeSettings
        ).SurfaceMaterialId;
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
        SampleNoise2D(
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

float FCubusTerrainDensityField::SampleNoise2D(
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

float FCubusTerrainDensityField::SampleNoise3D(
    const float WorldX,
    const float WorldY,
    const float WorldZ,
    const float Frequency
) const
{
    const float SafeFrequency =
        FMath::Max(0.000001f, Frequency);

    return FMath::PerlinNoise3D(
        FVector(
            WorldX * SafeFrequency,
            WorldY * SafeFrequency,
            WorldZ * SafeFrequency
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
        SampleNoise2D(
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
        SampleNoise2D(
            WorldX + 4871.0f,
            WorldY - 3253.0f,
            Settings.ValleyWarpFrequency
        ) *
        Settings.ValleyWarpAmplitude;

    const float WarpY =
        SampleNoise2D(
            WorldX - 761.0f,
            WorldY + 5987.0f,
            Settings.ValleyWarpFrequency
        ) *
        Settings.ValleyWarpAmplitude;

    const float ValleyNoise =
        SampleNoise2D(
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

float FCubusTerrainDensityField::SampleRiverDistance(
    const float WorldX,
    const float WorldY
) const
{
    FCubusBiomeFieldSettings RiverSettings = Settings.BiomeSettings;
    RiverSettings.bGenerateRivers = Settings.bGenerateRivers;
    RiverSettings.RiverFrequency = Settings.RiverFrequency;
    RiverSettings.RiverWarpAmplitude = Settings.RiverWarpAmplitude;
    RiverSettings.RiverWarpFrequency = Settings.RiverWarpFrequency;
    RiverSettings.RiverOffsetX = Settings.RiverOffsetX;
    RiverSettings.RiverOffsetY = Settings.RiverOffsetY;
    return FCubusBiomeField::SampleRiverDistance(
        WorldX,
        WorldY,
        RiverSettings
    );
}

float FCubusTerrainDensityField::ApplyRiverLowering(
    const float SurfaceHeight,
    const float WorldX,
    const float WorldY
) const
{
    if (!Settings.bGenerateRivers)
    {
        return SurfaceHeight;
    }

    const float RiverDistance =
        SampleRiverDistance(WorldX, WorldY);

    if (RiverDistance >= Settings.RiverValleyWidth)
    {
        return SurfaceHeight;
    }

    const float ValleyInfluence =
        1.0f -
        SmoothStep(
            Settings.RiverChannelWidth,
            Settings.RiverValleyWidth,
            RiverDistance
        );

    const bool bInsideChannel =
        RiverDistance <= Settings.RiverChannelWidth;

    const float Lowering =
        Settings.RiverValleyDepth *
            ValleyInfluence +
        (bInsideChannel
            ? Settings.RiverChannelDepth
            : 0.0f);

    return SurfaceHeight - Lowering;
}

float FCubusTerrainDensityField::SampleCaveDensity(
    const FVector& GlobalSampleCoordinate,
    const float SurfaceVoxelHeight
) const
{
    const float WorldZ =
        static_cast<float>(GlobalSampleCoordinate.Z);

    if (
        WorldZ < Settings.CaveMinimumWorldZ ||
        WorldZ > Settings.CaveMaximumWorldZ ||
        WorldZ >
            SurfaceVoxelHeight -
            static_cast<float>(Settings.CaveSurfaceClearance)
    )
    {
        return MAX_flt;
    }

    const float WorldX =
        static_cast<float>(GlobalSampleCoordinate.X) +
        static_cast<float>(Settings.CaveOffsetX);

    const float WorldY =
        static_cast<float>(GlobalSampleCoordinate.Y) +
        static_cast<float>(Settings.CaveOffsetY);

    const float ShiftedWorldZ =
        static_cast<float>(GlobalSampleCoordinate.Z) +
        static_cast<float>(Settings.CaveOffsetZ);

    const float PrimaryNoise =
        FMath::Abs(
            SampleNoise3D(
                WorldX,
                WorldY,
                ShiftedWorldZ,
                Settings.CavePrimaryFrequency
            )
        );

    const float SecondaryNoise =
        FMath::Abs(
            SampleNoise3D(
                WorldX + 1871.0f,
                WorldY - 953.0f,
                ShiftedWorldZ + 421.0f,
                Settings.CaveSecondaryFrequency
            )
        );

    return
        (
            PrimaryNoise +
            SecondaryNoise -
            Settings.CaveThreshold
        ) *
        Settings.CaveSurfaceSharpness;
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
