from pathlib import Path


def replace_once(path, old, new):
    p = Path(path)
    text = p.read_text(encoding='utf-8')
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f'expected one anchor in {path}, found {count}: {old[:160]!r}')
    p.write_text(text.replace(old, new, 1), encoding='utf-8')


def require(path, needle, count=None):
    text = Path(path).read_text(encoding='utf-8')
    actual = text.count(needle)
    if actual == 0:
        raise RuntimeError(f'missing required text in {path}: {needle!r}')
    if count is not None and actual != count:
        raise RuntimeError(f'expected {count} copies in {path}, found {actual}: {needle!r}')


# -----------------------------------------------------------------------------
# Terrain form diagnostics for actual mesh-generating landforms.
# -----------------------------------------------------------------------------
replace_once(
    'Source/Orakai/CubusCore/Generation/CubusTerrainForm.h',
    '''    float Ridge = 0.0f;
    float SurfaceRoughness = 0.0f;
    float ErosionRills = 0.0f;
};
''',
    '''    float Ridge = 0.0f;
    float MassifWeight = 0.0f;
    float Escarpment = 0.0f;
    float ValleyCarve = 0.0f;
    float Cirque = 0.0f;
    float SurfaceRoughness = 0.0f;
    float ErosionRills = 0.0f;
};
'''
)

# -----------------------------------------------------------------------------
# Add meso-scale landforms. The old tectonic range remains the kilometre-scale
# carrier, but these frequencies are deliberately high enough to be visible in
# a normal streamed play area.
# -----------------------------------------------------------------------------
terrain_cpp = 'Source/Orakai/CubusCore/Generation/CubusTerrainForm.cpp'
replace_once(
    terrain_cpp,
    '''    Result.Ridge = FMath::Max(LocalRidge, MajorRidge * RangeCore);

    // The broad channel establishes a continuous valley. A finer channel is
''',
    '''    Result.Ridge = FMath::Max(LocalRidge, MajorRidge * RangeCore);

    /*
     * Meso-scale massifs bridge the enormous tectonic range carrier and the
     * metre-scale ridge detail. The previous hierarchy could span kilometres
     * before changing appreciably, so an ordinary streamed neighbourhood often
     * looked like one generic patch of a much larger range.
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
    Result.Ridge = FMath::Max(
        Result.Ridge,
        MassifFine * Result.MassifWeight
    );

    /*
     * A smooth signed regional front creates genuine mountain-front relief.
     * The broad step changes elevation on either side; Escarpment marks only
     * the transition band so the volumetric density stage can grow cliffs,
     * buttresses and undercuts there rather than everywhere.
     */
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

    // The broad channel establishes a continuous valley. A finer channel is
'''
)

replace_once(
    terrain_cpp,
    '''    Result.Drainage = FMath::Max(
        MainDrainage,
        Tributary * Catchment * 0.78f
    );

    const float RillA = SampleChannelMask(
''',
    '''    const float TributaryDrainage = Tributary * Catchment * 0.78f;
    Result.Drainage = FMath::Max(
        MainDrainage,
        TributaryDrainage
    );

    /*
     * Give valleys a wide geomorphic shoulder and a narrower incised floor.
     * This produces readable U/V valley systems instead of a single shallow
     * channel mask. Tributaries are intentionally shallower so they can hang
     * above the trunk valley in mountain terrain.
     */
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

    /* Mountain bowls/cirques are broad depressions, not high-frequency dents. */
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
'''
)

replace_once(
    terrain_cpp,
    '''    const float RangeUplift =
        Settings.RidgeAmplitude * Settings.MountainElevationScale *
        (
            FoothillBelt * 0.78f +
            RangeCore * (1.08f + MajorRidge * 0.92f)
        ) *
        (0.82f + PeakRhythm * 0.38f);
    const float LocalCrestRelief =
''',
    '''    const float RangeUplift =
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
'''
)

replace_once(
    terrain_cpp,
    '''        FineSurfaceRelief +
        RangeUplift * Erosion +
        LocalCrestRelief -
        ValleyFloor * Settings.ValleyDepth * ValleyStrength;
''',
    '''        FineSurfaceRelief +
        RangeUplift * Erosion +
        MassifUplift * Erosion +
        EscarpmentLift +
        LocalCrestRelief -
        MainValleyCut -
        TributaryValleyCut -
        CirqueCut;
'''
)

# -----------------------------------------------------------------------------
# Density geology consumes the landform diagnostics so cliffs and overhangs
# are actual 3D scalar-field changes, not just height-map steepness.
# -----------------------------------------------------------------------------
density_cpp = 'Source/Orakai/CubusCore/Generation/CubusTerrainDensityField.cpp'
replace_once(
    density_cpp,
    '''    const float LandformExposure = FMath::Clamp(
        Column.FormSample.MountainCore * 0.82f +
        Column.FormSample.FoothillWeight * 0.46f +
        Column.FormSample.Ridge * 0.32f,
        0.0f,
        1.0f
    );
''',
    '''    const float LandformExposure = FMath::Clamp(
        Column.FormSample.MountainCore * 0.62f +
        Column.FormSample.FoothillWeight * 0.34f +
        Column.FormSample.Ridge * 0.26f +
        Column.FormSample.MassifWeight * 0.62f +
        Column.FormSample.Escarpment * 0.94f +
        Column.FormSample.Cirque * 0.22f,
        0.0f,
        1.0f
    );
'''
)

replace_once(
    density_cpp,
    '''    Column.RockExposure = CliffExposure * LandformExposure * DrainageProtection;
''',
    '''    Column.RockExposure = FMath::Clamp(
        FMath::Max(
            CliffExposure * LandformExposure * DrainageProtection,
            Column.FormSample.Escarpment *
            SmoothStep(0.18f, 0.92f, CliffExposure + Column.FormSample.MassifWeight * 0.42f) *
            0.88f
        ),
        0.0f,
        1.0f
    );
'''
)

replace_once(
    density_cpp,
    '''    const float DistanceFromSurface = FMath::Abs(BaseTerrainDensity);
    if (DistanceFromSurface >= Settings.GeologySurfaceBand || Column.RockExposure <= KINDA_SMALL_NUMBER)
    {
        return BaseTerrainDensity;
    }

    const float SurfaceBandMask = 1.0f - SmoothStep(0.0f, Settings.GeologySurfaceBand, DistanceFromSurface);
''',
    '''    const float DistanceFromSurface = FMath::Abs(BaseTerrainDensity);
    const float StructuralCliff = FMath::Clamp(
        FMath::Max(
            Column.RockExposure,
            Column.FormSample.Escarpment * 0.88f +
            Column.FormSample.MassifWeight * 0.16f +
            Column.FormSample.Cirque * 0.10f
        ),
        0.0f,
        1.0f
    );
    if (DistanceFromSurface >= Settings.GeologySurfaceBand || StructuralCliff <= KINDA_SMALL_NUMBER)
    {
        return BaseTerrainDensity;
    }

    const float SurfaceBandMask = 1.0f - SmoothStep(0.0f, Settings.GeologySurfaceBand, DistanceFromSurface);
'''
)

replace_once(
    density_cpp,
    '''    const float OverhangDisplacement =
        Settings.GeologyOverhangStrength *
        OverhangCarrier *
        Column.RockHardness *
        SmoothStep(0.18f, 0.72f, Column.RockExposure);

    const float GeologicalDisplacement =
        ShelfDisplacement +
        RockMass +
        OverhangDisplacement -
        UndercutDisplacement -
        FractureCut +
        RockWarp;

    return BaseTerrainDensity + GeologicalDisplacement * Column.RockExposure * SurfaceBandMask;
''',
    '''    const float OverhangDisplacement =
        Settings.GeologyOverhangStrength *
        OverhangCarrier *
        Column.RockHardness *
        SmoothStep(0.18f, 0.72f, StructuralCliff);

    /*
     * Escarpment-only 3D structure: resistant buttresses add connected rock
     * mass while alcove bands remove it. Because both are multiplied by the
     * near-surface mask and structural cliff carrier, they form ledges and
     * undercuts attached to mountain fronts instead of floating noise blobs.
     */
    const float ButtressRidge = SampleRidgedNoise3D(
        WorldX - 14731.0f,
        WorldY + 6113.0f,
        WorldZ + 3251.0f,
        Settings.GeologyMassFrequency * 0.68f
    );
    const float CliffButtress =
        (ButtressRidge - 0.42f) *
        Settings.GeologyMassStrength * 1.35f *
        Column.FormSample.Escarpment;
    const float AlcoveVolume = SampleRidgedNoise3D(
        WorldX + 18719.0f,
        WorldY - 10433.0f,
        WorldZ - 5119.0f,
        Settings.GeologyFractureFrequency * 0.58f
    );
    const float AlcoveCut =
        SmoothStep(0.68f, 0.94f, AlcoveVolume) *
        Settings.GeologyUndercutStrength * 1.25f *
        FMath::Clamp(
            Column.FormSample.Escarpment * 0.88f +
            Column.FormSample.Cirque * 0.42f,
            0.0f,
            1.0f
        );

    const float GeologicalDisplacement =
        ShelfDisplacement +
        RockMass +
        OverhangDisplacement +
        CliffButtress -
        UndercutDisplacement -
        FractureCut -
        AlcoveCut +
        RockWarp;

    return BaseTerrainDensity + GeologicalDisplacement * StructuralCliff * SurfaceBandMask;
'''
)

# -----------------------------------------------------------------------------
# Tests now assert that the local playable region contains actual macro/meso
# landforms rather than merely a kilometres-wide carrier outside the viewport.
# -----------------------------------------------------------------------------
test_cpp = 'Source/Orakai/CubusCore/Tests/CubusTerrainFormTests.cpp'
replace_once(
    test_cpp,
    '''    float MaximumErosionRill = 0.0f;
    float MaximumLocalDetailContribution = 0.0f;
''',
    '''    float MaximumErosionRill = 0.0f;
    float MaximumMassifWeight = 0.0f;
    float MaximumEscarpment = 0.0f;
    float MaximumValleyCarve = 0.0f;
    float MaximumCirque = 0.0f;
    float MinimumLocalHeight = MAX_flt;
    float MaximumLocalHeight = -MAX_flt;
    float MaximumLocalDetailContribution = 0.0f;
'''
)

replace_once(
    test_cpp,
    '''            MaximumErosionRill = FMath::Max(
                MaximumErosionRill,
                Sample.ErosionRills
            );
            MaximumLocalDetailContribution = FMath::Max(
''',
    '''            MaximumErosionRill = FMath::Max(
                MaximumErosionRill,
                Sample.ErosionRills
            );
            MaximumMassifWeight = FMath::Max(MaximumMassifWeight, Sample.MassifWeight);
            MaximumEscarpment = FMath::Max(MaximumEscarpment, Sample.Escarpment);
            MaximumValleyCarve = FMath::Max(MaximumValleyCarve, Sample.ValleyCarve);
            MaximumCirque = FMath::Max(MaximumCirque, Sample.Cirque);
            MinimumLocalHeight = FMath::Min(MinimumLocalHeight, Sample.Height);
            MaximumLocalHeight = FMath::Max(MaximumLocalHeight, Sample.Height);
            MaximumLocalDetailContribution = FMath::Max(
'''
)

replace_once(
    test_cpp,
    '''    TestTrue(
        TEXT("Intermittent erosion rills affect some local terrain"),
        MaximumErosionRill > 0.12f
    );
''',
    '''    TestTrue(
        TEXT("Intermittent erosion rills affect some local terrain"),
        MaximumErosionRill > 0.12f
    );
    TestTrue(
        TEXT("The playable-scale sample contains a visible massif"),
        MaximumMassifWeight > 0.16f
    );
    TestTrue(
        TEXT("The playable-scale sample contains a mountain-front escarpment"),
        MaximumEscarpment > 0.10f
    );
    TestTrue(
        TEXT("The playable-scale sample contains strongly carved valleys"),
        MaximumValleyCarve > 0.55f
    );
    TestTrue(
        TEXT("Mountain terrain contains broad bowl/cirque structure"),
        MaximumCirque > 0.04f
    );
    TestTrue(
        TEXT("Local terrain spans a visibly different vertical landscape"),
        MaximumLocalHeight - MinimumLocalHeight > 32.0f
    );
'''
)

# Density cache must invalidate because scalar samples and zero crossings change.
replace_once(
    'Source/Orakai/CubusCore/Generation/CubusGenerationSeeds.h',
    '''    // Bumped to 19: ecological communities now emit an explicit visible phenotype
    // that strongly drives tree composition and ground-cover structure.
    static constexpr uint32 CurrentGenerationVersion = 19;
''',
    '''    // Bumped to 20: terrain geometry now has playable-scale massifs, broad/incised
    // valley systems, mountain-front escarpments and cirques, with 3D density
    // buttresses/alcoves on structural cliffs. This changes scalar zero crossings.
    static constexpr uint32 CurrentGenerationVersion = 20;
'''
)

# Static guards against another invisible classification-only change.
require('Source/Orakai/CubusCore/Generation/CubusTerrainForm.h', 'float MassifWeight = 0.0f;', 1)
require(terrain_cpp, 'const float MassifUplift =', 1)
require(terrain_cpp, 'const float MainValleyCut =', 1)
require(terrain_cpp, 'const float EscarpmentLift =', 1)
require(terrain_cpp, 'const float CirqueCut =', 1)
require(density_cpp, 'const float StructuralCliff =', 1)
require(density_cpp, 'const float CliffButtress =', 1)
require(density_cpp, 'const float AlcoveCut =', 1)
require(density_cpp, 'GeologicalDisplacement * StructuralCliff * SurfaceBandMask', 1)
require('Source/Orakai/CubusCore/Generation/CubusGenerationSeeds.h', 'CurrentGenerationVersion = 20', 1)
