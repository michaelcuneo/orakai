#include "CubusCore/Generation/CubusTerrainForm.h"
#include "CubusCore/Generation/CubusWorldScale.h"

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
    Settings.MountainElevationScale = FMath::Clamp(Settings.MountainElevationScale, 1.0f, 4.0f);

    /*
     * v29 terrain-form reset
     * ----------------------
     *
     * TerrainForm owns only kilometre-to-hillside scale geomorphology. It does
     * not manufacture cliffs, overhangs or decimetre surface noise. Those are
     * density-domain concerns and are deliberately budgeted separately.
     *
     * The old form accumulated several independent ridged fields, escarpment
     * lifts, cirque cuts and micro-relief terms. Individually they looked
     * plausible, but together they produced repeated high-curvature silhouettes
     * with no common drainage or structural reason. This form instead follows a
     * small hierarchy:
     *
     *   province -> connected range -> massif -> trunk valley -> tributary
     *
     * Every height term is broad and smooth. Diagnostics still expose where a
     * density-domain cliff or rough rock surface is allowed to exist later.
     */

    const float VoxelSizeCm = FMath::Max(1.0f, Settings.VoxelSizeCm);
    const bool bPhysicalScale = Settings.bUsePhysicalWorldScale;

    // The physical model is authored in metres. Frequencies below are cycles
    // per canonical voxel, derived from the actual voxel size exactly once.
    const float RegionFrequency = bPhysicalScale
        ? CubusWorldScale::FrequencyForWavelengthKilometres(
            Settings.MountainSystemSpacingKm * 2.0f, VoxelSizeCm)
        : Settings.RegionFrequency;
    const float MountainSystemFrequency = bPhysicalScale
        ? CubusWorldScale::FrequencyForWavelengthKilometres(Settings.MountainSystemSpacingKm, VoxelSizeCm)
        : FMath::Max(0.00035f, RegionFrequency * 0.30f);
    const float MassifFrequency = bPhysicalScale
        ? CubusWorldScale::FrequencyForWavelengthKilometres(Settings.MassifScaleKm, VoxelSizeCm)
        : RegionFrequency * 0.62f;
    const float MajorValleyFrequency = bPhysicalScale
        ? CubusWorldScale::FrequencyForWavelengthKilometres(Settings.MajorValleySpacingKm, VoxelSizeCm)
        : Settings.ValleyFrequency * 0.30f;
    const float TributaryFrequency = bPhysicalScale
        ? CubusWorldScale::FrequencyForWavelengthKilometres(Settings.TributaryValleyScaleKm, VoxelSizeCm)
        : Settings.ValleyFrequency * 0.72f;
    const float HillFrequency = bPhysicalScale
        ? CubusWorldScale::FrequencyForWavelengthMeters(Settings.RollingHillScaleMeters, VoxelSizeCm)
        : Settings.HillFrequency * 0.34f;
    const float BroadReliefFrequency = bPhysicalScale
        ? CubusWorldScale::FrequencyForWavelengthMeters(Settings.BroadReliefScaleMeters, VoxelSizeCm)
        : Settings.DetailFrequency * 0.26f;
    const float LocalReliefFrequency = bPhysicalScale
        ? CubusWorldScale::FrequencyForWavelengthMeters(Settings.LocalReliefScaleMeters, VoxelSizeCm)
        : Settings.DetailFrequency * 0.82f;

    const float RegionalReliefVoxels = bPhysicalScale
        ? CubusWorldScale::MetersToVoxels(Settings.RegionalReliefMeters, VoxelSizeCm)
        : Settings.ContinentAmplitude;
    const float MountainReliefVoxels = bPhysicalScale
        ? CubusWorldScale::MetersToVoxels(Settings.MountainReliefMeters, VoxelSizeCm)
        : Settings.RidgeAmplitude * Settings.MountainElevationScale;
    const float ValleyReliefVoxels = bPhysicalScale
        ? CubusWorldScale::MetersToVoxels(Settings.ValleyReliefMeters, VoxelSizeCm)
        : Settings.ValleyDepth;
    const float HillReliefVoxels = bPhysicalScale
        ? CubusWorldScale::MetersToVoxels(Settings.RollingHillReliefMeters, VoxelSizeCm)
        : Settings.HillAmplitude;
    const float BroadReliefVoxels = bPhysicalScale
        ? CubusWorldScale::MetersToVoxels(Settings.BroadReliefMeters, VoxelSizeCm)
        : Settings.DetailAmplitude;
    const float LocalReliefVoxels = bPhysicalScale
        ? CubusWorldScale::MetersToVoxels(Settings.LocalReliefMeters, VoxelSizeCm)
        : Settings.DetailAmplitude;
    const float MacroWarpFrequency = FMath::Max(
        0.000001f,
        RegionFrequency * 0.22f
    );
    const float MacroWarpAmplitude = FMath::Max(
        72.0f,
        Settings.ValleyWarpAmplitude * 4.0f
    );

    const float WarpX = SampleFbm(
        WorldX + 4871.0f,
        WorldY - 3253.0f,
        MacroWarpFrequency,
        3,
        2.01f,
        0.50f
    ) * MacroWarpAmplitude;
    const float WarpY = SampleFbm(
        WorldX - 761.0f,
        WorldY + 5987.0f,
        MacroWarpFrequency,
        3,
        1.97f,
        0.50f
    ) * MacroWarpAmplitude;

    const float TerrainX = WorldX + WarpX;
    const float TerrainY = WorldY + WarpY;

    /* Broad elevation province. No thresholded plateau is added directly. */
    const float ProvinceElevation = FMath::Clamp(
        0.5f + 0.5f * SampleFbm(
            TerrainX * 0.79f - TerrainY * 0.61f + 41891.0f,
            TerrainX * 0.61f + TerrainY * 0.79f - 27617.0f,
            RegionFrequency * 0.24f,
            4,
            2.01f,
            0.52f
        ),
        0.0f,
        1.0f
    );
    const float HighlandProvince = SmoothStep(
        0.47f,
        0.73f,
        ProvinceElevation
    );
    const float LowlandProvince = SmoothStep(
        0.46f,
        0.76f,
        1.0f - ProvinceElevation
    );

    /*
     * One connected folded range family supplies the world-scale backbone.
     * A weaker branch family is allowed only inside its foothills, so secondary
     * ridges read as ribs of the same mountain system rather than unrelated
     * noise mountains.
     */
    const float RangeFrequency = MountainSystemFrequency;
    const float RangeSignal = SampleFbm(
        TerrainX * 0.86f - TerrainY * 0.51f + 10427.0f,
        TerrainX * 0.51f + TerrainY * 0.86f - 8633.0f,
        RangeFrequency,
        3,
        1.94f,
        0.52f
    );
    const float RangeDistance = FMath::Abs(RangeSignal);
    const float ContinuityNoise = SampleFbm(
        TerrainX * 0.47f + TerrainY * 0.88f - 28391.0f,
        TerrainY * 0.47f - TerrainX * 0.88f + 17657.0f,
        RangeFrequency * 0.31f,
        3,
        2.03f,
        0.50f
    );
    const float RangeContinuity = FMath::Lerp(
        0.58f,
        1.0f,
        SmoothStep(-0.42f, 0.30f, ContinuityNoise)
    );

    const float PrimaryRangeCore =
        (1.0f - SmoothStep(0.055f, 0.20f, RangeDistance)) *
        RangeContinuity;
    const float PrimaryFoothill =
        (1.0f - SmoothStep(0.12f, 0.48f, RangeDistance)) *
        RangeContinuity;

    const float BranchSignal = SampleFbm(
        TerrainX * 0.58f + TerrainY * 0.81f - 33461.0f,
        TerrainY * 0.58f - TerrainX * 0.81f + 19603.0f,
        RangeFrequency * 1.55f,
        2,
        2.02f,
        0.50f
    );
    const float BranchDistance = FMath::Abs(BranchSignal);
    const float BranchCarrier = FMath::Clamp(
        PrimaryFoothill *
        FMath::Lerp(0.45f, 1.0f, HighlandProvince),
        0.0f,
        1.0f
    );
    const float BranchCore =
        (1.0f - SmoothStep(0.045f, 0.17f, BranchDistance)) *
        BranchCarrier * 0.48f;
    const float BranchFoothill =
        (1.0f - SmoothStep(0.11f, 0.34f, BranchDistance)) *
        BranchCarrier * 0.38f;

    const float RangeCore = FMath::Clamp(
        FMath::Max(PrimaryRangeCore, BranchCore),
        0.0f,
        1.0f
    );
    const float FoothillBelt = FMath::Clamp(
        FMath::Max(PrimaryFoothill, BranchFoothill),
        0.0f,
        1.0f
    );

    const float AlongRangeRhythm = FMath::Clamp(
        0.5f + 0.5f * SampleFbm(
            TerrainX * 0.31f + TerrainY * 0.95f + 15877.0f,
            TerrainY * 0.31f - TerrainX * 0.95f - 24601.0f,
            RangeFrequency * 0.43f,
            3,
            2.03f,
            0.50f
        ),
        0.0f,
        1.0f
    );

    /* Massifs modulate an existing range; they never spawn mountain islands. */
    const float MassifSignal = FMath::Clamp(
        0.5f + 0.5f * SampleFbm(
            TerrainX * 0.72f - TerrainY * 0.69f + 22391.0f,
            TerrainX * 0.69f + TerrainY * 0.72f - 16417.0f,
            MassifFrequency,
            3,
            2.01f,
            0.52f
        ),
        0.0f,
        1.0f
    );
    const float MassifShape = SmoothStep(0.48f, 0.78f, MassifSignal);

    FCubusTerrainFormSample Result;
    Result.MountainCore = RangeCore;
    Result.FoothillWeight = FoothillBelt;
    Result.MassifWeight = FMath::Clamp(
        MassifShape * (RangeCore * 0.72f + FoothillBelt * 0.38f),
        0.0f,
        1.0f
    );
    Result.Ridge = FMath::Clamp(
        RangeCore * FMath::Lerp(0.72f, 1.0f, AlongRangeRhythm) +
        BranchCore * 0.32f,
        0.0f,
        1.0f
    );

    const float MountainRaw = FMath::Clamp(
        FMath::Max(
            RangeCore + FoothillBelt * 0.46f,
            HighlandProvince * 0.42f
        ),
        0.0f,
        1.0f
    );
    const float PlainsRaw = FMath::Clamp(
        (1.0f - MountainRaw) *
        SmoothStep(0.38f, 0.72f, LowlandProvince),
        0.0f,
        1.0f
    );
    const float RollingRaw = FMath::Max(
        0.0f,
        1.0f - MountainRaw - PlainsRaw
    );
    const float RegionTotal = FMath::Max(
        KINDA_SMALL_NUMBER,
        MountainRaw + PlainsRaw + RollingRaw
    );
    Result.MountainWeight = MountainRaw / RegionTotal;
    Result.PlainsWeight = PlainsRaw / RegionTotal;
    Result.RollingWeight = RollingRaw / RegionTotal;

    /*
     * Drainage hierarchy. The terrain-form valley is broad enough to be a
     * landscape feature; the terrain-derived hydrology system later decides
     * where the actual river channel sits inside it.
     */
    const float MainDrainage = SampleChannelMask(
        TerrainX - 1379.0f,
        TerrainY + 733.0f,
        MajorValleyFrequency,
        Settings.ValleyWidth * 1.55f,
        Settings.ValleyFalloff * 1.45f
    );
    const float Catchment = SmoothStep(
        -0.18f,
        0.42f,
        SampleFbm(
            TerrainX - 7213.0f,
            TerrainY + 3907.0f,
            RegionFrequency * 0.84f,
            2,
            2.0f,
            0.50f
        )
    );
    const float TributaryRaw = SampleChannelMask(
        TerrainX * 0.82f - TerrainY * 0.57f + 6197.0f,
        TerrainX * 0.57f + TerrainY * 0.82f - 2467.0f,
        TributaryFrequency,
        Settings.ValleyWidth * 0.70f,
        Settings.ValleyFalloff * 0.68f
    );
    const float Tributary = FMath::Clamp(
        TributaryRaw *
        Catchment *
        (FoothillBelt * 0.62f + RangeCore * 0.28f) *
        (1.0f - MainDrainage * 0.58f),
        0.0f,
        1.0f
    );

    Result.Drainage = FMath::Clamp(
        FMath::Max(MainDrainage, Tributary * 0.78f),
        0.0f,
        1.0f
    );

    const float MainValleyShoulder = FMath::Pow(
        FMath::Clamp(MainDrainage, 0.0f, 1.0f),
        0.66f
    );
    const float MainValleyFloor = FMath::Pow(
        FMath::Clamp(MainDrainage, 0.0f, 1.0f),
        1.90f
    );
    const float TributaryValley = FMath::Pow(
        FMath::Clamp(Tributary, 0.0f, 1.0f),
        1.32f
    );
    Result.ValleyCarve = FMath::Clamp(
        FMath::Max(MainValleyShoulder, TributaryValley * 0.72f),
        0.0f,
        1.0f
    );

    /*
     * Cirque and escarpment are permissions/diagnostics, not height stamps.
     * Density geology may use them later, but TerrainForm itself stays smooth.
     */
    const float HeadwaterField = FMath::Clamp(
        0.5f + 0.5f * SampleFbm(
            TerrainX * 0.67f - TerrainY * 0.74f + 27103.0f,
            TerrainX * 0.74f + TerrainY * 0.67f - 33791.0f,
            RegionFrequency * 1.08f,
            2,
            2.01f,
            0.50f
        ),
        0.0f,
        1.0f
    );
    Result.Cirque = FMath::Clamp(
        SmoothStep(0.68f, 0.86f, HeadwaterField) *
        Result.MassifWeight *
        Tributary *
        (1.0f - MainValleyFloor),
        0.0f,
        1.0f
    );
    Result.Escarpment = FMath::Clamp(
        FoothillBelt *
        (1.0f - RangeCore * 0.68f) *
        SmoothStep(0.34f, 0.78f, MassifSignal),
        0.0f,
        1.0f
    );

    /* Surface character is diagnostic. Decimetre detail lives in density. */
    const float RoughnessPatch = FMath::Clamp(
        0.5f + 0.5f * SampleFbm(
            TerrainX + 24793.0f,
            TerrainY - 19319.0f,
            Settings.DetailFrequency * 0.10f,
            3,
            2.09f,
            0.50f
        ),
        0.0f,
        1.0f
    );
    const float FloodplainCalm = 1.0f - FMath::Clamp(
        MainValleyFloor * 0.88f + MainValleyShoulder * 0.34f,
        0.0f,
        0.92f
    );
    Result.SurfaceRoughness = FMath::Clamp(
        (
            Result.PlainsWeight * 0.18f +
            Result.RollingWeight * 0.52f +
            Result.MountainWeight * 0.82f
        ) *
        FMath::Lerp(0.42f, 1.0f, RoughnessPatch) *
        FloodplainCalm,
        0.0f,
        1.0f
    );

    const float RillA = SampleChannelMask(
        TerrainX * 0.94f - TerrainY * 0.34f + 11939.0f,
        TerrainX * 0.34f + TerrainY * 0.94f - 4153.0f,
        Settings.DetailFrequency * 0.22f,
        0.035f,
        0.12f
    );
    const float RillB = SampleChannelMask(
        TerrainX * 0.57f + TerrainY * 0.82f - 6983.0f,
        TerrainY * 0.57f - TerrainX * 0.82f + 15731.0f,
        Settings.DetailFrequency * 0.18f,
        0.030f,
        0.13f
    );
    Result.ErosionRills = FMath::Clamp(
        FMath::Max(RillA, RillB * 0.82f) *
        Catchment *
        Result.SurfaceRoughness *
        (1.0f - MainValleyFloor),
        0.0f,
        1.0f
    );

    /*
     * Height composition. There are no direct escarpment, shelf, broken-ground
     * or sharp ridged-noise terms here. Large features have large wavelengths;
     * the result remains a well-behaved support surface for density geology.
     */
    const float MacroRelief = SampleFbm(
        TerrainX + 317.0f,
        TerrainY - 941.0f,
        Settings.ContinentFrequency * 0.52f,
        4,
        2.01f,
        0.50f
    );
    const float RollingRelief = SampleFbm(
        TerrainX + 1823.0f,
        TerrainY - 917.0f,
        HillFrequency,
        3,
        2.03f,
        0.50f
    );

    const float ProvinceUplift =
        (ProvinceElevation - 0.5f) * 2.0f *
        RegionalReliefVoxels * 0.54f;

    const float ContinentStrength =
        Result.PlainsWeight * 0.36f +
        Result.RollingWeight * 0.72f +
        Result.MountainWeight;
    const float HillStrength =
        Result.PlainsWeight * 0.12f +
        Result.RollingWeight * 0.78f +
        Result.MountainWeight * 0.34f;

    const float RangeShoulder = FMath::Pow(
        FMath::Clamp(FoothillBelt, 0.0f, 1.0f),
        1.18f
    );
    const float RangeCrest = FMath::Pow(
        FMath::Clamp(RangeCore, 0.0f, 1.0f),
        1.28f
    );
    const float RangeRhythm = FMath::Lerp(
        0.82f,
        1.10f,
        AlongRangeRhythm
    );
    const float RangeUplift =
        MountainReliefVoxels *
        FMath::Clamp(
            RangeShoulder * 0.24f +
            RangeCrest * 0.68f +
            Result.MassifWeight * 0.22f,
            0.0f,
            1.0f
        ) *
        RangeRhythm;

    const float ValleyStrength =
        Result.PlainsWeight * 0.45f +
        Result.RollingWeight * 0.72f +
        Result.MountainWeight;
    const float MainValleyCut =
        ValleyReliefVoxels * ValleyStrength *
        (
            MainValleyShoulder * 0.34f +
            MainValleyFloor * 0.58f
        );
    const float TributaryCut =
        ValleyReliefVoxels * TributaryValley *
        (0.18f + Result.MountainWeight * 0.16f);
    const float BasinCut =
        ValleyReliefVoxels * 0.16f * LowlandProvince;
    const float CirqueCut =
        ValleyReliefVoxels * 0.10f * Result.Cirque;

    const float BroadDetail = SampleFbm(
        TerrainX + 3761.0f,
        TerrainY - 8291.0f,
        BroadReliefFrequency,
        2,
        2.03f,
        0.48f
    );
    const float FineDetail = SampleNoise(
        TerrainX - 15413.0f,
        TerrainY + 1087.0f,
        LocalReliefFrequency
    );
    const float LocalDetail =
        Result.SurfaceRoughness *
        (
            BroadDetail * BroadReliefVoxels * 0.34f +
            FineDetail * LocalReliefVoxels * 0.14f
        );
    const float RillCut =
        Result.ErosionRills * LocalReliefVoxels * 0.12f;

    Result.Height =
        Settings.BaseHeight +
        MacroRelief * RegionalReliefVoxels * 0.34f * ContinentStrength +
        ProvinceUplift +
        RollingRelief * HillReliefVoxels * HillStrength * FloodplainCalm +
        RangeUplift -
        BasinCut -
        MainValleyCut -
        TributaryCut -
        CirqueCut +
        LocalDetail -
        RillCut;

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

    for (int32 Octave = 0; Octave < FMath::Clamp(Octaves, 1, 6); ++Octave)
    {
        const float Ridge = FMath::Square(
            1.0f - FMath::Abs(
                SampleNoise(
                    WorldX + static_cast<float>(Octave) * 1297.0f,
                    WorldY - static_cast<float>(Octave) * 1699.0f,
                    CurrentFrequency
                )
            )
        );
        Sum += Ridge * Weight;
        TotalWeight += Weight;
        CurrentFrequency *= 2.0f;
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
