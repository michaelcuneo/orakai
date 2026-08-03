#include "CubusCore/Generation/CubusTerrainForm.h"

FCubusTerrainFormSample FCubusTerrainForm::Sample(
    const float WorldX,
    const float WorldY,
    const FCubusTerrainFormSettings& InSettings
)
{
    FCubusTerrainFormSettings Settings = InSettings;
    Settings.ContinentAmplitude = FMath::Max(0.0f, Settings.ContinentAmplitude);
    Settings.ContinentFrequency = FMath::Max(0.000001f, Settings.ContinentFrequency);
    Settings.HillAmplitude = FMath::Max(0.0f, Settings.HillAmplitude);
    Settings.HillFrequency = FMath::Max(0.000001f, Settings.HillFrequency);
    Settings.DetailAmplitude = FMath::Max(0.0f, Settings.DetailAmplitude);
    Settings.DetailFrequency = FMath::Max(0.000001f, Settings.DetailFrequency);
    Settings.RidgeAmplitude = FMath::Max(0.0f, Settings.RidgeAmplitude);
    Settings.RidgeFrequency = FMath::Max(0.000001f, Settings.RidgeFrequency);
    Settings.ValleyDepth = FMath::Max(0.0f, Settings.ValleyDepth);
    Settings.ValleyFrequency = FMath::Max(0.000001f, Settings.ValleyFrequency);
    Settings.ValleyWidth = FMath::Clamp(Settings.ValleyWidth, 0.0f, 1.0f);
    Settings.ValleyFalloff = FMath::Clamp(Settings.ValleyFalloff, 0.001f, 1.0f);
    Settings.ValleyWarpAmplitude = FMath::Max(0.0f, Settings.ValleyWarpAmplitude);
    Settings.ValleyWarpFrequency = FMath::Max(0.000001f, Settings.ValleyWarpFrequency);
    Settings.RegionFrequency = FMath::Max(0.000001f, Settings.RegionFrequency);
    Settings.PlainsThreshold = FMath::Clamp(Settings.PlainsThreshold, -1.0f, 1.0f);
    Settings.PlainsBlend = FMath::Clamp(Settings.PlainsBlend, 0.001f, 1.0f);
    Settings.MountainThreshold = FMath::Clamp(
        Settings.MountainThreshold,
        Settings.PlainsThreshold,
        1.0f
    );
    Settings.MountainBlend = FMath::Clamp(Settings.MountainBlend, 0.001f, 1.0f);

    // One broad warp is shared by every landform. This keeps detail aligned
    // with the macro terrain instead of stacking unrelated noise layers.
    const float FormWarpFrequency = FMath::Max(
        0.000001f,
        FMath::Min(Settings.RegionFrequency, Settings.ValleyWarpFrequency) * 0.65f
    );
    const float FormWarpAmplitude = FMath::Max(
        12.0f,
        Settings.ValleyWarpAmplitude * 1.5f
    );
    const float WarpX = SampleFbm(
        WorldX + 4871.0f,
        WorldY - 3253.0f,
        FormWarpFrequency,
        3,
        2.03f,
        0.5f
    ) * FormWarpAmplitude;
    const float WarpY = SampleFbm(
        WorldX - 761.0f,
        WorldY + 5987.0f,
        FormWarpFrequency,
        3,
        1.97f,
        0.5f
    ) * FormWarpAmplitude;
    const float TerrainX = WorldX + WarpX;
    const float TerrainY = WorldY + WarpY;

    const float RegionSignal = SampleFbm(
        TerrainX + 10427.0f,
        TerrainY - 8633.0f,
        Settings.RegionFrequency,
        3,
        2.0f,
        0.52f
    );
    const float PlainsExit = SmoothStep(
        Settings.PlainsThreshold - Settings.PlainsBlend,
        Settings.PlainsThreshold + Settings.PlainsBlend,
        RegionSignal
    );
    const float MountainEntry = SmoothStep(
        Settings.MountainThreshold - Settings.MountainBlend,
        Settings.MountainThreshold + Settings.MountainBlend,
        RegionSignal
    );

    FCubusTerrainFormSample Result;
    Result.PlainsWeight = 1.0f - PlainsExit;
    Result.MountainWeight = MountainEntry;
    Result.RollingWeight = FMath::Max(
        0.0f,
        1.0f - Result.PlainsWeight - Result.MountainWeight
    );
    const float TotalRegionWeight =
        Result.PlainsWeight + Result.RollingWeight + Result.MountainWeight;
    if (TotalRegionWeight > KINDA_SMALL_NUMBER)
    {
        Result.PlainsWeight /= TotalRegionWeight;
        Result.RollingWeight /= TotalRegionWeight;
        Result.MountainWeight /= TotalRegionWeight;
    }

    const float Continent = SampleFbm(
        TerrainX,
        TerrainY,
        Settings.ContinentFrequency,
        4,
        2.01f,
        0.5f
    );
    const float Hills = SampleFbm(
        TerrainX + 1823.0f,
        TerrainY - 917.0f,
        Settings.HillFrequency,
        4,
        2.07f,
        0.48f
    );
    const float Detail = SampleFbm(
        TerrainX - 431.0f,
        TerrainY + 2671.0f,
        Settings.DetailFrequency,
        2,
        2.0f,
        0.42f
    );
    Result.Ridge = SampleRidgedFbm(
        TerrainX + 911.0f,
        TerrainY + 1511.0f,
        Settings.RidgeFrequency,
        4
    );

    // The broad channel establishes a continuous valley. A finer channel is
    // allowed only in selected catchments, producing tributaries instead of
    // equally strong contour bands everywhere.
    const float MainDrainage = SampleChannelMask(
        TerrainX - 1379.0f,
        TerrainY + 733.0f,
        Settings.ValleyFrequency,
        Settings.ValleyWidth,
        Settings.ValleyFalloff
    );
    const float Tributary = SampleChannelMask(
        TerrainX * 0.82f - TerrainY * 0.57f + 6197.0f,
        TerrainX * 0.57f + TerrainY * 0.82f - 2467.0f,
        Settings.ValleyFrequency * 1.85f,
        Settings.ValleyWidth * 0.62f,
        Settings.ValleyFalloff * 0.55f
    );
    const float Catchment = SmoothStep(
        -0.18f,
        0.34f,
        SampleFbm(
            TerrainX - 7213.0f,
            TerrainY + 3907.0f,
            Settings.RegionFrequency * 1.6f,
            2,
            2.0f,
            0.5f
        )
    );
    Result.Drainage = FMath::Max(
        MainDrainage,
        Tributary * Catchment * 0.78f
    );

    const float ContinentStrength =
        0.28f * Result.PlainsWeight +
        0.72f * Result.RollingWeight +
        1.0f * Result.MountainWeight;
    const float HillStrength =
        0.08f * Result.PlainsWeight +
        1.0f * Result.RollingWeight +
        0.52f * Result.MountainWeight;
    const float RidgeStrength =
        0.0f * Result.PlainsWeight +
        0.18f * Result.RollingWeight +
        1.0f * Result.MountainWeight;
    const float ValleyStrength =
        0.32f * Result.PlainsWeight +
        0.78f * Result.RollingWeight +
        1.0f * Result.MountainWeight;

    // Erosion removes small-scale chatter from floodplains and drainage
    // floors. Plains also receive much less high-frequency relief.
    const float ValleyFloor = Result.Drainage * Result.Drainage;
    const float Erosion = FMath::Clamp(1.0f - ValleyFloor * 0.82f, 0.12f, 1.0f);
    const float DetailStrength =
        (0.08f * Result.PlainsWeight +
         0.48f * Result.RollingWeight +
         0.9f * Result.MountainWeight) *
        Erosion;

    Result.Height =
        Settings.BaseHeight +
        Continent * Settings.ContinentAmplitude * ContinentStrength +
        Hills * Settings.HillAmplitude * HillStrength * Erosion +
        Detail * Settings.DetailAmplitude * DetailStrength +
        Result.Ridge * Settings.RidgeAmplitude * RidgeStrength -
        ValleyFloor * Settings.ValleyDepth * ValleyStrength;

    return Result;
}

float FCubusTerrainForm::SampleNoise(
    const float WorldX,
    const float WorldY,
    const float Frequency
)
{
    const float SafeFrequency = FMath::Max(0.000001f, Frequency);
    return FMath::PerlinNoise2D(
        FVector2D(WorldX * SafeFrequency, WorldY * SafeFrequency)
    );
}

float FCubusTerrainForm::SampleFbm(
    const float WorldX,
    const float WorldY,
    const float Frequency,
    const int32 Octaves,
    const float Lacunarity,
    const float Gain
)
{
    float Sum = 0.0f;
    float Weight = 1.0f;
    float TotalWeight = 0.0f;
    float CurrentFrequency = FMath::Max(0.000001f, Frequency);
    const int32 SafeOctaves = FMath::Clamp(Octaves, 1, 6);

    for (int32 Octave = 0; Octave < SafeOctaves; ++Octave)
    {
        Sum += SampleNoise(
            WorldX + static_cast<float>(Octave) * 1931.0f,
            WorldY - static_cast<float>(Octave) * 1877.0f,
            CurrentFrequency
        ) * Weight;
        TotalWeight += Weight;
        CurrentFrequency *= FMath::Max(1.01f, Lacunarity);
        Weight *= FMath::Clamp(Gain, 0.01f, 0.99f);
    }

    return TotalWeight > KINDA_SMALL_NUMBER ? Sum / TotalWeight : 0.0f;
}

float FCubusTerrainForm::SampleRidgedFbm(
    const float WorldX,
    const float WorldY,
    const float Frequency,
    const int32 Octaves
)
{
    float Sum = 0.0f;
    float Weight = 1.0f;
    float TotalWeight = 0.0f;
    float CurrentFrequency = FMath::Max(0.000001f, Frequency);
    float PreviousRidge = 1.0f;

    for (int32 Octave = 0; Octave < FMath::Clamp(Octaves, 1, 6); ++Octave)
    {
        float Ridge = 1.0f - FMath::Abs(
            SampleNoise(
                WorldX + static_cast<float>(Octave) * 1297.0f,
                WorldY - static_cast<float>(Octave) * 1699.0f,
                CurrentFrequency
            )
        );
        Ridge *= Ridge;
        Ridge *= FMath::Lerp(0.35f, 1.0f, PreviousRidge);
        Sum += Ridge * Weight;
        TotalWeight += Weight;
        PreviousRidge = Ridge;
        CurrentFrequency *= 2.04f;
        Weight *= 0.5f;
    }

    return TotalWeight > KINDA_SMALL_NUMBER ? Sum / TotalWeight : 0.0f;
}

float FCubusTerrainForm::SampleChannelMask(
    const float WorldX,
    const float WorldY,
    const float Frequency,
    const float Width,
    const float Falloff
)
{
    const float Distance = FMath::Abs(
        SampleFbm(WorldX, WorldY, Frequency, 2, 2.0f, 0.35f)
    );
    const float SafeWidth = FMath::Clamp(Width, 0.0f, 1.0f);
    const float OuterEdge = FMath::Min(
        1.0f,
        SafeWidth + FMath::Clamp(Falloff, 0.001f, 1.0f)
    );
    return 1.0f - SmoothStep(SafeWidth, OuterEdge, Distance);
}

float FCubusTerrainForm::SmoothStep(
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
        (Value - EdgeMinimum) / (EdgeMaximum - EdgeMinimum),
        0.0f,
        1.0f
    );
    return Alpha * Alpha * (3.0f - 2.0f * Alpha);
}
