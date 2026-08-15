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
    Settings.MountainElevationScale = FMath::Clamp(Settings.MountainElevationScale, 1.0f, 4.0f);

    // The tectonic carrier remains deliberately slow so complete mountain
    // systems are connected over kilometre scales.
    const float TectonicFrequency = FMath::Max(
        0.000001f,
        Settings.RegionFrequency * 0.10f
    );

    const float FormWarpFrequency = FMath::Max(
        0.000001f,
        TectonicFrequency * 0.65f
    );
    const float FormWarpAmplitude = FMath::Max(
        256.0f,
        Settings.ValleyWarpAmplitude * 12.0f
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

    // Mountain ranges follow warped tectonic zero contours rather than isolated
    // threshold blobs, keeping the macro structure connected.
    const float TectonicSignal = SampleFbm(
        TerrainX + 10427.0f,
        TerrainY - 8633.0f,
        TectonicFrequency,
        3,
        1.93f,
        0.52f
    );
    const float TectonicDistance = FMath::Abs(TectonicSignal);
    const float RangeContinuitySignal = SampleFbm(
        TerrainX - 28391.0f,
        TerrainY + 17657.0f,
        TectonicFrequency * 0.43f,
        3,
        2.01f,
        0.5f
    );
    const float RangeContinuity = SmoothStep(
        -0.52f + Settings.MountainThreshold * 0.32f,
        0.08f + Settings.MountainThreshold * 0.22f,
        RangeContinuitySignal
    );
    const float CoreWidth = FMath::Lerp(
        0.08f,
        0.15f,
        1.0f - FMath::Clamp(Settings.MountainThreshold, 0.0f, 1.0f)
    );
    const float CoreFalloff = FMath::Lerp(
        0.045f,
        0.14f,
        Settings.MountainBlend
    );
    const float FoothillEdge = FMath::Min(
        0.82f,
        CoreWidth + CoreFalloff + 0.28f + Settings.MountainBlend * 0.35f
    );
    const float RangeCore =
        (1.0f - SmoothStep(
            CoreWidth,
            CoreWidth + CoreFalloff,
            TectonicDistance
        )) * RangeContinuity;
    const float FoothillBelt =
        (1.0f - SmoothStep(
            CoreWidth + CoreFalloff * 0.35f,
            FoothillEdge,
            TectonicDistance
        )) * RangeContinuity;

    const float RegionSignal = SampleFbm(
        TerrainX - 11717.0f,
        TerrainY + 23431.0f,
        TectonicFrequency * 0.72f,
        3,
        2.0f,
        0.52f
    );
    const float PlainsExit = SmoothStep(
        Settings.PlainsThreshold - Settings.PlainsBlend,
        Settings.PlainsThreshold + Settings.PlainsBlend,
        RegionSignal
    );

    FCubusTerrainFormSample Result;
    Result.MountainCore = RangeCore;
    Result.FoothillWeight = FMath::Max(FoothillBelt, RangeCore);
    Result.PlainsWeight =
        (1.0f - PlainsExit) * (1.0f - Result.FoothillWeight);
    Result.MountainWeight = FMath::Clamp(
        RangeCore + FoothillBelt * 0.48f,
        0.0f,
        1.0f
    );
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

    const float MacroRelief = SampleFbm(
        TerrainX + 317.0f,
        TerrainY - 941.0f,
        TectonicFrequency * 0.68f,
        4,
        2.01f,
        0.5f
    );
    const float RegionalRelief = SampleFbm(
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

    const float SurfacePatch = FMath::Clamp(
        0.5f + 0.5f * SampleFbm(
            TerrainX + 24793.0f,
            TerrainY - 19319.0f,
            Settings.DetailFrequency * 0.10f,
            3,
            2.11f,
            0.48f
        ),
        0.0f,
        1.0f
    );
    const float SoilUndulation = SampleFbm(
        TerrainX * 0.86f - TerrainY * 0.51f + 3761.0f,
        TerrainX * 0.51f + TerrainY * 0.86f - 8291.0f,
        Settings.DetailFrequency * 0.22f,
        3,
        2.03f,
        0.52f
    );
    const float GrainWarpDistance = FMath::Clamp(
        0.75f / Settings.DetailFrequency,
        4.0f,
        18.0f
    );
    const float GrainWarp = SampleFbm(
        TerrainX - 15413.0f,
        TerrainY + 1087.0f,
        Settings.DetailFrequency * 0.48f,
        2,
        1.97f,
        0.5f
    ) * GrainWarpDistance;
    const float MicroRelief = SampleFbm(
        TerrainX + GrainWarp + 613.0f,
        TerrainY - GrainWarp - 5441.0f,
        Settings.DetailFrequency * 1.75f,
        3,
        2.17f,
        0.43f
    );
    const float BrokenGround =
        SampleRidgedFbm(
            TerrainX * 0.73f + TerrainY * 0.68f - 9137.0f,
            TerrainY * 0.73f - TerrainX * 0.68f + 12491.0f,
            Settings.DetailFrequency * 0.72f,
            3
        ) * 2.0f - 1.0f;

    const float LocalRidgeFine = SampleRidgedFbm(
        TerrainX + 911.0f,
        TerrainY + 1511.0f,
        Settings.RidgeFrequency,
        4
    );
    const float LocalRidgeShoulder = SampleRidgedFbm(
        TerrainX + 911.0f,
        TerrainY + 1511.0f,
        Settings.RidgeFrequency * 0.52f,
        3
    );
    const float LocalRidge = FMath::Clamp(
        LocalRidgeShoulder * 0.58f + LocalRidgeFine * 0.42f,
        0.0f,
        1.0f
    );

    const float MajorRidgeFine = SampleRidgedFbm(
        TerrainX * 0.91f - TerrainY * 0.41f + 15401.0f,
        TerrainX * 0.41f + TerrainY * 0.91f - 12011.0f,
        TectonicFrequency * 4.5f,
        3
    );
    const float MajorRidgeShoulder = SampleRidgedFbm(
        TerrainX * 0.91f - TerrainY * 0.41f + 15401.0f,
        TerrainX * 0.41f + TerrainY * 0.91f - 12011.0f,
        TectonicFrequency * 2.45f,
        3
    );
    const float MajorRidge = FMath::Clamp(
        MajorRidgeShoulder * 0.62f + MajorRidgeFine * 0.38f,
        0.0f,
        1.0f
    );

    const float PeakRhythm = FMath::Clamp(
        0.5f + 0.5f * SampleFbm(
            TerrainX * 0.78f + TerrainY * 0.63f - 6191.0f,
            TerrainY * 0.78f - TerrainX * 0.63f + 9319.0f,
            TectonicFrequency * 2.75f,
            3,
            2.06f,
            0.48f
        ),
        0.0f,
        1.0f
    );
    Result.Ridge = FMath::Max(LocalRidge, MajorRidge * RangeCore);

    /*
     * Playable-scale massifs bridge the enormous tectonic carrier and the
     * metre-scale ridge detail. At the default RegionFrequency this changes on
     * hundreds-of-voxels scales, so an ordinary streamed area can contain a
     * whole mountain mass instead of only one tiny part of a 4,000-voxel wave.
     */
    const float MesoscaleFrequency = FMath::Max(
        Settings.RegionFrequency * 0.85f,
        TectonicFrequency * 5.0f
    );
    const float MassifBroad = SampleRidgedFbm(
        TerrainX * 0.84f - TerrainY * 0.54f + 22391.0f,
        TerrainX * 0.54f + TerrainY * 0.84f - 16417.0f,
        MesoscaleFrequency,
        3
    );
    const float MassifFine = SampleRidgedFbm(
        TerrainX * 0.63f + TerrainY * 0.78f - 17311.0f,
        TerrainY * 0.63f - TerrainX * 0.78f + 25793.0f,
        MesoscaleFrequency * 1.85f,
        3
    );
    const float MassifCarrier = FMath::Clamp(
        FoothillBelt * 0.82f + RangeCore * 0.55f,
        0.0f,
        1.0f
    );
    Result.MassifWeight = FMath::Clamp(
        (MassifBroad * 0.72f + MassifFine * 0.28f) * MassifCarrier,
        0.0f,
        1.0f
    );
    Result.Ridge = FMath::Max(Result.Ridge, MassifFine * Result.MassifWeight);

    // A smooth signed regional front creates a real mountain-front step. The
    // edge mask is also passed into the 3D density geology stage for cliffs.
    const float EscarpmentSignal = SampleFbm(
        TerrainX * 0.91f + TerrainY * 0.41f + 31991.0f,
        TerrainY * 0.91f - TerrainX * 0.41f - 11813.0f,
        Settings.RegionFrequency * 1.15f,
        3,
        2.03f,
        0.50f
    );
    const float EscarpmentPlateau = SmoothStep(-0.18f, 0.18f, EscarpmentSignal);
    const float EscarpmentEdge = 1.0f - SmoothStep(
        0.055f,
        0.28f,
        FMath::Abs(EscarpmentSignal)
    );
    const float EscarpmentCarrier = FMath::Clamp(
        MassifCarrier * (0.42f + MassifBroad * 0.58f),
        0.0f,
        1.0f
    );
    Result.Escarpment = FMath::Clamp(
        EscarpmentEdge * EscarpmentCarrier,
        0.0f,
        1.0f
    );

    // Main trunk and tributary valley hierarchy.
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
    const float TributaryDrainage = Tributary * Catchment * 0.78f;
    Result.Drainage = FMath::Max(MainDrainage, TributaryDrainage);

    const float MainValleyShoulder = FMath::Pow(
        FMath::Clamp(MainDrainage, 0.0f, 1.0f),
        0.68f
    );
    const float MainValleyFloor = FMath::Pow(
        FMath::Clamp(MainDrainage, 0.0f, 1.0f),
        1.70f
    );
    const float TributaryValley = FMath::Pow(
        FMath::Clamp(TributaryDrainage, 0.0f, 1.0f),
        1.35f
    );
    Result.ValleyCarve = FMath::Clamp(
        FMath::Max(MainValleyShoulder, TributaryValley * 0.74f),
        0.0f,
        1.0f
    );

    // Broad glacial-style bowls/cirques are gated to mountain mass and kept
    // away from the strongest trunk valley floor.
    const float CirqueField = FMath::Clamp(
        0.5f + 0.5f * SampleFbm(
            TerrainX * 0.72f - TerrainY * 0.69f + 27103.0f,
            TerrainX * 0.69f + TerrainY * 0.72f - 33791.0f,
            FMath::Max(Settings.RegionFrequency * 2.35f, Settings.ValleyFrequency * 0.72f),
            3,
            2.01f,
            0.48f
        ),
        0.0f,
        1.0f
    );
    Result.Cirque = FMath::Clamp(
        SmoothStep(0.58f, 0.82f, CirqueField) *
        FMath::Max(Result.MountainWeight, Result.MassifWeight) *
        (1.0f - MainValleyFloor * 0.70f),
        0.0f,
        1.0f
    );

    const float RillA = SampleChannelMask(
        TerrainX * 0.94f - TerrainY * 0.34f + 11939.0f,
        TerrainX * 0.34f + TerrainY * 0.94f - 4153.0f,
        Settings.DetailFrequency * 0.46f,
        0.035f,
        0.11f
    );
    const float RillB = SampleChannelMask(
        TerrainX * 0.57f + TerrainY * 0.82f - 6983.0f,
        TerrainY * 0.57f - TerrainX * 0.82f + 15731.0f,
        Settings.DetailFrequency * 0.39f,
        0.028f,
        0.13f
    );
    const float LocalRills = FMath::Lerp(
        RillA,
        RillB,
        SmoothStep(0.36f, 0.64f, SurfacePatch)
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

    const float ValleyFloor = Result.Drainage * Result.Drainage;
    const float Erosion = FMath::Clamp(1.0f - ValleyFloor * 0.82f, 0.12f, 1.0f);
    const float DetailStrength =
        (0.20f * Result.PlainsWeight +
         0.52f * Result.RollingWeight +
         0.88f * Result.MountainWeight) *
        Erosion;
    const float PatchStrength = FMath::Lerp(0.32f, 1.0f, SurfacePatch);
    const float UndulationStrength =
        (0.72f * Result.PlainsWeight +
         0.88f * Result.RollingWeight +
         0.54f * Result.MountainWeight) *
        FMath::Clamp(1.0f - ValleyFloor * 0.62f, 0.28f, 1.0f);
    const float MicroStrength =
        (0.34f * Result.PlainsWeight +
         0.76f * Result.RollingWeight +
         1.0f * Result.MountainWeight) *
        PatchStrength * Erosion;
    const float BrokenGroundStrength =
        (0.08f * Result.PlainsWeight +
         0.48f * Result.RollingWeight +
         1.0f * Result.MountainWeight) *
        SmoothStep(0.28f, 0.76f, SurfacePatch) * Erosion;
    const float RillStrength =
        (0.04f * Result.PlainsWeight +
         0.38f * Result.RollingWeight +
         1.0f * Result.MountainWeight) *
        FMath::Lerp(0.42f, 1.0f, Catchment) *
        (1.0f - ValleyFloor);
    Result.SurfaceRoughness = FMath::Clamp(
        MicroStrength * 0.68f + BrokenGroundStrength * 0.32f,
        0.0f,
        1.0f
    );
    Result.ErosionRills = FMath::Clamp(
        LocalRills * RillStrength,
        0.0f,
        1.0f
    );

    const float BroadSurfaceRelief =
        SoilUndulation * Settings.DetailAmplitude * 0.70f * UndulationStrength;
    const float RawFineSurfaceRelief =
        Detail * Settings.DetailAmplitude * 0.46f * DetailStrength +
        MicroRelief * Settings.DetailAmplitude * 0.54f * MicroStrength +
        BrokenGround * Settings.DetailAmplitude * 0.24f * BrokenGroundStrength -
        LocalRills * Settings.DetailAmplitude * 0.62f * RillStrength;
    const float FineSurfaceReliefLimit =
        Settings.DetailAmplitude *
        FMath::Lerp(0.48f, 0.82f, SurfacePatch) *
        FMath::Lerp(0.82f, 1.0f, Erosion);
    const float FineSurfaceRelief = FMath::Clamp(
        RawFineSurfaceRelief,
        -FineSurfaceReliefLimit,
        FineSurfaceReliefLimit
    );

    const float RangeUplift =
        Settings.RidgeAmplitude * Settings.MountainElevationScale *
        (
            FoothillBelt * 0.78f +
            RangeCore * (1.08f + MajorRidge * 0.92f)
        ) *
        (0.82f + PeakRhythm * 0.38f);
    const float MassifUplift =
        Settings.RidgeAmplitude * Settings.MountainElevationScale * 0.72f *
        Result.MassifWeight *
        (0.72f + PeakRhythm * 0.28f);
    const float EscarpmentLift =
        (EscarpmentPlateau * 2.0f - 1.0f) *
        Settings.RidgeAmplitude * 0.68f *
        EscarpmentCarrier;
    const float MainValleyCut =
        Settings.ValleyDepth * ValleyStrength *
        (MainValleyFloor * 0.92f +
         FMath::Max(0.0f, MainValleyShoulder - MainValleyFloor) * 0.48f);
    const float TributaryValleyCut =
        Settings.ValleyDepth * ValleyStrength * TributaryValley * 0.52f;
    const float CirqueCut =
        Settings.ValleyDepth *
        FMath::Lerp(0.52f, 0.90f, Result.MountainWeight) *
        Result.Cirque;
    const float LocalCrestRelief =
        LocalRidge * Settings.RidgeAmplitude * RidgeStrength * Erosion * 0.52f;
    const float Continent =
        MacroRelief * 0.68f +
        RegionalRelief * 0.32f;

    Result.Height =
        Settings.BaseHeight +
        Continent * Settings.ContinentAmplitude * ContinentStrength +
        Hills * Settings.HillAmplitude * HillStrength * Erosion +
        BroadSurfaceRelief +
        FineSurfaceRelief +
        RangeUplift * Erosion +
        MassifUplift * Erosion +
        EscarpmentLift +
        LocalCrestRelief -
        MainValleyCut -
        TributaryValleyCut -
        CirqueCut;

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

        Ridge = FMath::Pow(FMath::Clamp(Ridge, 0.0f, 1.0f), 1.35f);
        Ridge *= FMath::Lerp(0.45f, 1.0f, PreviousRidge);
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
