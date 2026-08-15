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

    // Kilometre-scale tectonic carrier. This provides the long connected spine,
    // but it no longer acts as the global on/off switch for mountain terrain.
    const float TectonicFrequency = FMath::Max(
        0.000001f,
        Settings.RegionFrequency * 0.10f
    );

    const float FormWarpFrequency = FMath::Max(
        0.000001f,
        TectonicFrequency * 0.65f
    );
    const float FormWarpAmplitude = FMath::Max(
        192.0f,
        Settings.ValleyWarpAmplitude * 9.0f
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

    /*
     * Regional terrain province.
     *
     * The v22 pass used a ridged province as a direct vertical displacement,
     * which made obvious synthetic shelves. A realistic temperate mountain
     * valley instead starts with a very broad elevation province. Tectonic
     * spines and massifs add relief inside the high country; broad low provinces
     * become basins rather than giant stamped depressions.
     */
    const float ProvinceElevation = FMath::Clamp(
        0.5f + 0.5f * SampleFbm(
            TerrainX * 0.77f - TerrainY * 0.64f + 41891.0f,
            TerrainX * 0.64f + TerrainY * 0.77f - 27617.0f,
            Settings.RegionFrequency * 0.34f,
            4,
            2.01f,
            0.52f
        ),
        0.0f,
        1.0f
    );
    const float ProvinceRuggedness = SampleRidgedFbm(
        TerrainX * 0.58f + TerrainY * 0.81f - 33461.0f,
        TerrainY * 0.58f - TerrainX * 0.81f + 19603.0f,
        Settings.RegionFrequency * 0.78f,
        3
    );
    const float HighlandProvince = FMath::Clamp(
        SmoothStep(0.48f, 0.72f, ProvinceElevation) *
        FMath::Lerp(0.72f, 1.0f, ProvinceRuggedness),
        0.0f,
        1.0f
    );
    const float BasinProvince = FMath::Clamp(
        SmoothStep(0.58f, 0.82f, 1.0f - ProvinceElevation) *
        (1.0f - FoothillBelt * 0.62f),
        0.0f,
        1.0f
    );

    /*
     * Distributed regional ranges.
     *
     * The tectonic carrier above has a ~4000-voxel wavelength at defaults. It
     * gives continent-scale structure, but it cannot be the only source of tall
     * relief or ordinary play windows will often see alpine peaks only far away.
     * This independent warped zero-crossing hierarchy is regional/playable scale
     * and remains entirely world-coordinate based, never player/radius based.
     */
    const float DistributedRangeFrequency = FMath::Max(
        Settings.RegionFrequency * 0.72f,
        TectonicFrequency * 5.5f
    );
    const float DistributedRangeSignal = SampleFbm(
        TerrainX * 0.67f - TerrainY * 0.74f + 53117.0f,
        TerrainX * 0.74f + TerrainY * 0.67f - 41729.0f,
        DistributedRangeFrequency,
        3,
        1.97f,
        0.52f
    );
    const float DistributedRangeDistance = FMath::Abs(DistributedRangeSignal);
    const float DistributedRangeContinuitySignal = SampleFbm(
        TerrainX * 0.89f + TerrainY * 0.46f - 36251.0f,
        TerrainY * 0.89f - TerrainX * 0.46f + 28793.0f,
        DistributedRangeFrequency * 0.42f,
        3,
        2.03f,
        0.50f
    );
    const float DistributedRangeContinuity = SmoothStep(-0.24f, 0.34f, DistributedRangeContinuitySignal);
    const float DistributedRangeCore =
        (1.0f - SmoothStep(0.075f, 0.19f, DistributedRangeDistance)) * DistributedRangeContinuity;
    const float DistributedFoothillBelt =
        (1.0f - SmoothStep(0.12f, 0.43f, DistributedRangeDistance)) * DistributedRangeContinuity;

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
    Result.MountainCore = FMath::Clamp(
        FMath::Max(FMath::Max(RangeCore, DistributedRangeCore * 0.92f), HighlandProvince * 0.22f),
        0.0f,
        1.0f
    );
    Result.FoothillWeight = FMath::Clamp(
        FMath::Max(
            FMath::Max(FMath::Max(FoothillBelt, DistributedFoothillBelt * 0.90f), FMath::Max(RangeCore, DistributedRangeCore * 0.92f)),
            HighlandProvince * 0.56f
        ),
        0.0f,
        1.0f
    );
    Result.PlainsWeight =
        (1.0f - PlainsExit) *
        (1.0f - Result.FoothillWeight) *
        FMath::Lerp(1.0f, 0.76f, BasinProvince);
    Result.MountainWeight = FMath::Clamp(
        FMath::Max(
            FMath::Max(RangeCore + FoothillBelt * 0.48f, DistributedRangeCore * 0.92f + DistributedFoothillBelt * 0.44f),
            HighlandProvince * 0.58f
        ),
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

    // Secondary ridge branches are broad enough to survive an 80 cm density
    // lattice. Two rotated fields make dendritic spurs rather than parallel ribs.
    const float BranchFrequency = FMath::Max(
        Settings.RegionFrequency * 2.15f,
        TectonicFrequency * 9.0f
    );
    const float BranchRidgeA = SampleRidgedFbm(
        TerrainX * 0.79f - TerrainY * 0.61f + 34781.0f,
        TerrainX * 0.61f + TerrainY * 0.79f - 20117.0f,
        BranchFrequency,
        3
    );
    const float BranchRidgeB = SampleRidgedFbm(
        TerrainX * 0.47f + TerrainY * 0.88f - 26339.0f,
        TerrainY * 0.47f - TerrainX * 0.88f + 38177.0f,
        BranchFrequency * 1.22f,
        3
    );
    const float BranchCarrier = FMath::Clamp(
        FMath::Max(
            FoothillBelt * 0.62f + RangeCore * 0.48f,
            DistributedFoothillBelt * 0.78f + DistributedRangeCore * 0.58f
        ),
        0.0f,
        1.0f
    );
    const float BranchNetwork =
        SmoothStep(0.38f, 0.76f, FMath::Max(BranchRidgeA, BranchRidgeB * 0.92f)) *
        BranchCarrier;

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
    const float PassRhythm = FMath::Clamp(
        0.5f + 0.5f * SampleFbm(
            TerrainX * 0.38f + TerrainY * 0.92f + 15877.0f,
            TerrainY * 0.38f - TerrainX * 0.92f - 24601.0f,
            Settings.RegionFrequency * 0.92f,
            3,
            1.93f,
            0.54f
        ),
        0.0f,
        1.0f
    );
    Result.Ridge = FMath::Max(
        FMath::Max(LocalRidge, BranchNetwork * 0.90f),
        MajorRidge * FMath::Max(FMath::Max(RangeCore, DistributedRangeCore * 0.88f), HighlandProvince * 0.34f)
    );

    /*
     * Meso-scale massifs are hundreds of voxels across. Their envelope is
     * broad and low-amplitude; sharper ridges are reserved for the tectonic
     * spine. This avoids the repeated "mountain bumps" look.
     */
    const float MesoscaleFrequency = FMath::Max(
        Settings.RegionFrequency * 0.95f,
        TectonicFrequency * 6.0f
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
        MesoscaleFrequency * 1.65f,
        3
    );
    const float LegacyMassifCarrier = FMath::Clamp(
        FMath::Max(
            FoothillBelt * 0.78f + RangeCore * 0.52f,
            DistributedFoothillBelt * 0.74f + DistributedRangeCore * 0.50f
        ),
        0.0f,
        1.0f
    );
    const float MassifCarrier = FMath::Clamp(
        FMath::Max(LegacyMassifCarrier, HighlandProvince * 0.70f),
        0.0f,
        1.0f
    );
    Result.MassifWeight = FMath::Clamp(
        (MassifBroad * 0.72f + MassifFine * 0.28f) * MassifCarrier,
        0.0f,
        1.0f
    );
    Result.Ridge = FMath::Max(Result.Ridge, MassifFine * Result.MassifWeight * 0.82f);

    // Mountain fronts are primarily a structural/exposure signal. The height
    // contribution later is deliberately small; volumetric geology can make a
    // true cliff where the resulting slope and rock structure support it.
    const float EscarpmentSignal = SampleFbm(
        TerrainX * 0.91f + TerrainY * 0.41f + 31991.0f,
        TerrainY * 0.91f - TerrainX * 0.41f - 11813.0f,
        Settings.RegionFrequency * 0.95f,
        3,
        2.03f,
        0.50f
    );
    const float EscarpmentPlateau = SmoothStep(-0.20f, 0.20f, EscarpmentSignal);
    const float EscarpmentEdge = 1.0f - SmoothStep(
        0.055f,
        0.31f,
        FMath::Abs(EscarpmentSignal)
    );
    const float ProvinceFront = 1.0f - SmoothStep(
        0.12f,
        0.38f,
        FMath::Abs(HighlandProvince - 0.46f)
    );
    const float EscarpmentCarrier = FMath::Clamp(
        FMath::Max(
            LegacyMassifCarrier * (0.32f + MassifBroad * 0.46f),
            ProvinceFront * 0.32f
        ),
        0.0f,
        1.0f
    );
    Result.Escarpment = FMath::Clamp(
        EscarpmentEdge * EscarpmentCarrier,
        0.0f,
        1.0f
    );

    /*
     * Valley hierarchy.
     *
     * Trunk valleys are intentionally much lower-frequency and wider than the
     * old procedural channel. Tributaries are smaller and only appear in a
     * catchment. This creates a readable main valley with side valleys instead
     * of a uniform web of equally-sized cuts.
     */
    const float MainDrainage = SampleChannelMask(
        TerrainX - 1379.0f,
        TerrainY + 733.0f,
        Settings.ValleyFrequency * 0.42f,
        Settings.ValleyWidth * 1.35f,
        Settings.ValleyFalloff * 1.25f
    );
    const float Tributary = SampleChannelMask(
        TerrainX * 0.82f - TerrainY * 0.57f + 6197.0f,
        TerrainX * 0.57f + TerrainY * 0.82f - 2467.0f,
        Settings.ValleyFrequency * 1.45f,
        Settings.ValleyWidth * 0.58f,
        Settings.ValleyFalloff * 0.52f
    );
    const float Catchment = SmoothStep(
        -0.20f,
        0.36f,
        SampleFbm(
            TerrainX - 7213.0f,
            TerrainY + 3907.0f,
            Settings.RegionFrequency * 1.25f,
            2,
            2.0f,
            0.5f
        )
    );
    const float TributaryDrainage = Tributary * Catchment * 0.72f;
    Result.Drainage = FMath::Max(MainDrainage, TributaryDrainage);

    // A broad shoulder plus narrow floor gives a glacial/fluvial valley cross
    // section. The lower exponent widens the shoulder; the higher exponent
    // confines the strong incision to the trunk floor.
    const float MainValleyShoulder = FMath::Pow(
        FMath::Clamp(MainDrainage, 0.0f, 1.0f),
        0.72f
    );
    const float MainValleyFloor = FMath::Pow(
        FMath::Clamp(MainDrainage, 0.0f, 1.0f),
        1.85f
    );
    const float TributaryValley = FMath::Pow(
        FMath::Clamp(TributaryDrainage, 0.0f, 1.0f),
        1.40f
    );
    Result.ValleyCarve = FMath::Clamp(
        FMath::Max(MainValleyShoulder, TributaryValley * 0.72f),
        0.0f,
        1.0f
    );

    // Cirques are sparse headwall bowls in mountain country, not random holes.
    const float CirqueField = FMath::Clamp(
        0.5f + 0.5f * SampleFbm(
            TerrainX * 0.72f - TerrainY * 0.69f + 27103.0f,
            TerrainX * 0.69f + TerrainY * 0.72f - 33791.0f,
            FMath::Max(Settings.RegionFrequency * 1.80f, Settings.ValleyFrequency * 0.55f),
            3,
            2.01f,
            0.48f
        ),
        0.0f,
        1.0f
    );
    Result.Cirque = FMath::Clamp(
        SmoothStep(0.64f, 0.86f, CirqueField) *
        FMath::Max(Result.MountainWeight, Result.MassifWeight) *
        (1.0f - MainValleyFloor * 0.78f),
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
    const float HeadwaterA = SampleChannelMask(
        TerrainX * 0.86f - TerrainY * 0.51f + 4297.0f,
        TerrainX * 0.51f + TerrainY * 0.86f - 16831.0f,
        Settings.ValleyFrequency * 2.55f,
        FMath::Max(0.012f, Settings.ValleyWidth * 0.30f),
        FMath::Max(0.04f, Settings.ValleyFalloff * 0.28f)
    );
    const float HeadwaterB = SampleChannelMask(
        TerrainX * 0.55f + TerrainY * 0.84f - 13327.0f,
        TerrainY * 0.55f - TerrainX * 0.84f + 21493.0f,
        Settings.ValleyFrequency * 2.95f,
        FMath::Max(0.010f, Settings.ValleyWidth * 0.24f),
        FMath::Max(0.035f, Settings.ValleyFalloff * 0.24f)
    );
    const float HeadwaterNotches = FMath::Clamp(
        FMath::Max(HeadwaterA, HeadwaterB * 0.86f) *
        Catchment *
        FMath::Max(Result.MountainWeight, Result.MassifWeight) *
        (1.0f - MainValleyFloor * 0.82f),
        0.0f,
        1.0f
    );

    const float ContinentStrength =
        0.30f * Result.PlainsWeight +
        0.72f * Result.RollingWeight +
        1.0f * Result.MountainWeight;
    const float HillStrength =
        0.08f * Result.PlainsWeight +
        1.0f * Result.RollingWeight +
        0.48f * Result.MountainWeight;
    const float RidgeStrength =
        0.0f * Result.PlainsWeight +
        0.16f * Result.RollingWeight +
        1.0f * Result.MountainWeight;
    const float ValleyStrength =
        0.38f * Result.PlainsWeight +
        0.76f * Result.RollingWeight +
        1.0f * Result.MountainWeight;

    const float ValleyFloor = Result.Drainage * Result.Drainage;
    const float Erosion = FMath::Clamp(1.0f - ValleyFloor * 0.84f, 0.14f, 1.0f);
    const float DetailStrength =
        (0.18f * Result.PlainsWeight +
         0.48f * Result.RollingWeight +
         0.82f * Result.MountainWeight) *
        Erosion;
    const float PatchStrength = FMath::Lerp(0.30f, 1.0f, SurfacePatch);
    const float UndulationStrength =
        (0.72f * Result.PlainsWeight +
         0.84f * Result.RollingWeight +
         0.50f * Result.MountainWeight) *
        FMath::Clamp(1.0f - ValleyFloor * 0.68f, 0.24f, 1.0f);
    const float MicroStrength =
        (0.30f * Result.PlainsWeight +
         0.70f * Result.RollingWeight +
         0.92f * Result.MountainWeight) *
        PatchStrength * Erosion;
    const float BrokenGroundStrength =
        (0.06f * Result.PlainsWeight +
         0.42f * Result.RollingWeight +
         0.90f * Result.MountainWeight) *
        SmoothStep(0.30f, 0.78f, SurfacePatch) * Erosion;
    const float RillStrength =
        (0.03f * Result.PlainsWeight +
         0.32f * Result.RollingWeight +
         0.90f * Result.MountainWeight) *
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
        SoilUndulation * Settings.DetailAmplitude * 0.62f * UndulationStrength;
    const float RawFineSurfaceRelief =
        Detail * Settings.DetailAmplitude * 0.42f * DetailStrength +
        MicroRelief * Settings.DetailAmplitude * 0.46f * MicroStrength +
        BrokenGround * Settings.DetailAmplitude * 0.18f * BrokenGroundStrength -
        LocalRills * Settings.DetailAmplitude * 0.54f * RillStrength;
    const float FineSurfaceReliefLimit =
        Settings.DetailAmplitude *
        FMath::Lerp(0.42f, 0.74f, SurfacePatch) *
        FMath::Lerp(0.80f, 1.0f, Erosion);
    const float FineSurfaceRelief = FMath::Clamp(
        RawFineSurfaceRelief,
        -FineSurfaceReliefLimit,
        FineSurfaceReliefLimit
    );

    /*
     * Relief budget.
     *
     * These terms are intentionally hierarchical rather than additive copies
     * of the same noise. Province uplift is broad and modest; ranges provide
     * most alpine elevation; massifs articulate the range; passes reduce ridge
     * height locally; valleys and cirques remove material where expected.
     */
    const float PrimaryRangeUplift =
        Settings.RidgeAmplitude * Settings.MountainElevationScale *
        (
            FoothillBelt * 0.56f +
            RangeCore * (0.92f + MajorRidge * 0.72f)
        ) *
        (0.78f + PeakRhythm * 0.30f) *
        (0.72f + PassRhythm * 0.30f);
    const float DistributedRangeUplift =
        Settings.RidgeAmplitude * Settings.MountainElevationScale *
        (
            DistributedFoothillBelt * 0.50f +
            DistributedRangeCore * (0.86f + MajorRidge * 0.62f)
        ) *
        (0.78f + PeakRhythm * 0.28f) *
        (0.74f + PassRhythm * 0.28f);
    const float RangeUplift = FMath::Max(PrimaryRangeUplift, DistributedRangeUplift);
    const float HighlandUplift =
        Settings.RidgeAmplitude * Settings.MountainElevationScale * 0.42f *
        FMath::Pow(HighlandProvince, 1.35f) *
        (0.78f + MassifBroad * 0.22f);
    const float MassifUplift =
        Settings.RidgeAmplitude * Settings.MountainElevationScale * 0.48f *
        Result.MassifWeight *
        (0.76f + PeakRhythm * 0.24f);
    const float EscarpmentLift =
        (EscarpmentPlateau * 2.0f - 1.0f) *
        Settings.RidgeAmplitude * 0.24f *
        Result.Escarpment;
    const float BasinCut =
        Settings.ValleyDepth * 0.44f * BasinProvince;
    const float MainValleyCut =
        Settings.ValleyDepth * ValleyStrength *
        (MainValleyFloor * 0.84f +
         FMath::Max(0.0f, MainValleyShoulder - MainValleyFloor) * 0.34f);
    const float TributaryValleyCut =
        Settings.ValleyDepth * ValleyStrength * TributaryValley * 0.38f;
    const float CirqueCut =
        Settings.ValleyDepth *
        FMath::Lerp(0.52f, 0.82f, Result.MountainWeight) *
        Result.Cirque;
    const float LocalCrestRelief =
        LocalRidge * Settings.RidgeAmplitude * RidgeStrength * Erosion * 0.34f;
    const float BranchCrestRelief =
        BranchNetwork * Settings.RidgeAmplitude * RidgeStrength * Erosion * 0.24f;
    const float HeadwaterCut =
        Settings.ValleyDepth * 0.22f * HeadwaterNotches *
        FMath::Lerp(0.72f, 1.0f, BranchNetwork);
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
        HighlandUplift * Erosion +
        MassifUplift * Erosion +
        EscarpmentLift +
        LocalCrestRelief +
        BranchCrestRelief -
        BasinCut -
        MainValleyCut -
        TributaryValleyCut -
        HeadwaterCut -
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
