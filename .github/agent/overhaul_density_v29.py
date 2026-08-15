from pathlib import Path
import re


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text()
    if text.count(old) != 1:
        raise RuntimeError(f"Expected one exact match in {path}, found {text.count(old)}")
    p.write_text(text.replace(old, new, 1))


def replace_function(path: str, signature: str, next_signature: str, replacement: str) -> None:
    p = Path(path)
    text = p.read_text()
    start = text.find(signature)
    if start < 0:
        raise RuntimeError(f"Missing function signature in {path}: {signature}")
    end = text.find(next_signature, start)
    if end < 0:
        raise RuntimeError(f"Missing next signature in {path}: {next_signature}")
    p.write_text(text[:start] + replacement.rstrip() + "\n\n" + text[end:])


# 1) Continuous density gets its own bounded fine relief. This is deliberately
# separate from TerrainForm, so 4x adaptive sampling resolves geometry that is
# genuinely absent from the broad macro surface.
density_path = "Source/Orakai/CubusCore/Generation/CubusTerrainDensityField.cpp"
old_sample = '''FCubusDensitySample FCubusTerrainDensityField::SampleContinuous(const FVector& GlobalSampleCoordinate) const
{
\tconst FColumnData& Column = GetColumnData(static_cast<float>(GlobalSampleCoordinate.X), static_cast<float>(GlobalSampleCoordinate.Y));
\tconst float BaseTerrainDensity = Column.SurfaceSampleZ - static_cast<float>(GlobalSampleCoordinate.Z);
\tconst float TerrainDensity = Settings.bGenerateVolumetricGeology
\t\t? SampleGeologicalDensity(GlobalSampleCoordinate, Column, BaseTerrainDensity)
\t\t: BaseTerrainDensity;
'''
new_sample = '''FCubusDensitySample FCubusTerrainDensityField::SampleContinuous(const FVector& GlobalSampleCoordinate) const
{
\tconst FColumnData& Column = GetColumnData(static_cast<float>(GlobalSampleCoordinate.X), static_cast<float>(GlobalSampleCoordinate.Y));
\tconst float MacroTerrainDensity = Column.SurfaceSampleZ - static_cast<float>(GlobalSampleCoordinate.Z);

\t/*
\t * Fine terrain belongs to the continuous density field, not TerrainForm.
\t *
\t * TerrainForm is the stable kilometre/hillside support surface used by
\t * hydrology, biome classification and coarse streaming. This band adds
\t * bounded metre-to-decimetre relief directly to the scalar field. At the
\t * default 80 cm canonical voxel, 4x adaptive sampling evaluates it every
\t * 20 cm and therefore resolves genuine geometry that an 80 cm mesh cannot.
\t *
\t * The amplitude is deliberately less than half a canonical voxel. Fine
\t * relief can wrinkle soil and weather rock, but it cannot manufacture a
\t * detached spike or move a cliff by metres.
\t */
\tfloat FineSurfaceDisplacement = 0.0f;
\tif (FMath::Abs(MacroTerrainDensity) < 2.5f)
\t{
\t\tconst float WorldX = static_cast<float>(GlobalSampleCoordinate.X) + static_cast<float>(Settings.TerrainOffsetX);
\t\tconst float WorldY = static_cast<float>(GlobalSampleCoordinate.Y) + static_cast<float>(Settings.TerrainOffsetY);
\t\tconst float Roughness = FMath::Clamp(Column.FormSample.SurfaceRoughness, 0.0f, 1.0f);
\t\tconst float ValleyCalm = 1.0f - FMath::Clamp(Column.FormSample.ValleyCarve * 0.82f, 0.0f, 0.88f);
\t\tconst float MountainTexture = FMath::Lerp(0.72f, 1.0f, Column.FormSample.MountainWeight);

\t\tconst float MesoscopicNoise = SampleNoise2D(
\t\t\tWorldX + 42017.0f,
\t\t\tWorldY - 18311.0f,
\t\t\tFMath::Max(0.30f, Settings.DetailFrequency * 3.8f)
\t\t);
\t\tconst float FineNoise = SampleNoise2D(
\t\t\tWorldX * 0.79f - WorldY * 0.61f - 11719.0f,
\t\t\tWorldX * 0.61f + WorldY * 0.79f + 23801.0f,
\t\t\tFMath::Max(0.68f, Settings.DetailFrequency * 8.5f)
\t\t);
\t\tconst float FineWarp = SampleNoise2D(
\t\t\tWorldX - 7193.0f,
\t\t\tWorldY + 3119.0f,
\t\t\tFMath::Max(0.16f, Settings.DetailFrequency * 2.0f)
\t\t);
\t\tconst float WarpedFineNoise = SampleNoise2D(
\t\t\tWorldX + FineWarp * 0.42f + 13007.0f,
\t\t\tWorldY - FineWarp * 0.42f - 9113.0f,
\t\t\tFMath::Max(1.05f, Settings.DetailFrequency * 13.0f)
\t\t);

\t\tconst float FineAmplitude =
\t\t\tFMath::Lerp(0.08f, 0.34f, Roughness) *
\t\t\tValleyCalm *
\t\t\tMountainTexture;
\t\tFineSurfaceDisplacement = FMath::Clamp(
\t\t\t(MesoscopicNoise * 0.54f + FineNoise * 0.31f + WarpedFineNoise * 0.15f) * FineAmplitude,
\t\t\t-0.34f,
\t\t\t0.34f
\t\t);
\t}

\tconst float BaseTerrainDensity = MacroTerrainDensity + FineSurfaceDisplacement;
\tconst float TerrainDensity = Settings.bGenerateVolumetricGeology
\t\t? SampleGeologicalDensity(GlobalSampleCoordinate, Column, BaseTerrainDensity)
\t\t: BaseTerrainDensity;
'''
replace_once(density_path, old_sample, new_sample)

# 2) Replace the old additive shelf/overhang geology with a bounded deformation
# budget. The previous settings could stack >10 voxel units of displacement in
# one surface band, causing the reported forests of cliff spikes.
replace_function(
    density_path,
    "float FCubusTerrainDensityField::SampleGeologicalDensity(",
    "float FCubusTerrainDensityField::SampleCaveDensity(",
    r'''float FCubusTerrainDensityField::SampleGeologicalDensity(
\tconst FVector& GlobalSampleCoordinate,
\tconst FColumnData& Column,
\tconst float BaseTerrainDensity
) const
{
\tconst float DistanceFromSurface = FMath::Abs(BaseTerrainDensity);
\tconst float EffectiveSurfaceBand = FMath::Min(Settings.GeologySurfaceBand, 6.0f);
\tif (DistanceFromSurface >= EffectiveSurfaceBand || Column.RockExposure <= KINDA_SMALL_NUMBER)
\t{
\t\treturn BaseTerrainDensity;
\t}

\tconst float SurfaceBandMask = 1.0f - SmoothStep(0.0f, EffectiveSurfaceBand, DistanceFromSurface);
\tconst float WorldX = static_cast<float>(GlobalSampleCoordinate.X) + static_cast<float>(Settings.TerrainOffsetX);
\tconst float WorldY = static_cast<float>(GlobalSampleCoordinate.Y) + static_cast<float>(Settings.TerrainOffsetY);
\tconst float WorldZ = static_cast<float>(GlobalSampleCoordinate.Z);

\t/*
\t * Structural permission comes from the macro landform. A steep slope alone
\t * is not permission to turn every cliff into a noisy saw blade.
\t */
\tconst float StructuralCarrier = FMath::Clamp(
\t\tColumn.FormSample.Escarpment * 0.62f +
\t\tColumn.FormSample.MountainCore * 0.22f +
\t\tColumn.FormSample.MassifWeight * 0.18f +
\t\tColumn.FormSample.Ridge * 0.12f,
\t\t0.0f,
\t\t1.0f
\t);
\tconst float ValleyProtection = 1.0f - FMath::Clamp(
\t\tColumn.FormSample.ValleyCarve * 0.86f,
\t\t0.0f,
\t\t0.92f
\t);
\tconst float GeologyCarrier = FMath::Clamp(
\t\tColumn.RockExposure *
\t\tFMath::Lerp(0.34f, 1.0f, StructuralCarrier) *
\t\tValleyProtection,
\t\t0.0f,
\t\t1.0f
\t);
\tif (GeologyCarrier <= KINDA_SMALL_NUMBER)
\t{
\t\treturn BaseTerrainDensity;
\t}

\tFVector2D Downhill = -Column.Gradient;
\tif (!Downhill.Normalize())
\t{
\t\treturn BaseTerrainDensity;
\t}
\tconst FVector2D AlongCliff(-Downhill.Y, Downhill.X);
\tconst float Across = WorldX * Downhill.X + WorldY * Downhill.Y;
\tconst float Along = WorldX * AlongCliff.X + WorldY * AlongCliff.Y;

\t/* Broad strata. This is shape modulation, never a multi-voxel shelf stamp. */
\tconst float StrataFrequency = FMath::Clamp(
\t\tSettings.GeologyStrataFrequency,
\t\t0.008f,
\t\t0.16f
\t);
\tconst float FoldedDip = FMath::Clamp(
\t\tSettings.GeologyStrataDip + Column.StrataTilt * 0.012f,
\t\t-0.035f,
\t\t0.035f
\t);
\tconst float StrataPhase =
\t\tWorldZ * StrataFrequency +
\t\tAcross * FoldedDip +
\t\tSampleNoise2D(Along + 9113.0f, Across - 4327.0f, StrataFrequency * 0.16f) * 0.20f;
\tconst float StrataWave = FMath::Sin(StrataPhase * 2.0f * PI);

\tconst float RockMass = SampleNoise3D(
\t\tWorldX - 6211.0f,
\t\tWorldY + 4177.0f,
\t\tWorldZ - 1987.0f,
\t\tFMath::Clamp(Settings.GeologyMassFrequency, 0.006f, 0.18f)
\t);
\tconst float RockWarp = SampleNoise3D(
\t\tWorldX + 1709.0f,
\t\tWorldY - 3187.0f,
\t\tWorldZ + 733.0f,
\t\tFMath::Clamp(Settings.GeologyRockWarpFrequency, 0.008f, 0.22f)
\t);
\tconst float FractureVolume = SampleRidgedNoise3D(
\t\tWorldX + 12011.0f,
\t\tWorldY - 4919.0f,
\t\tWorldZ + 2791.0f,
\t\tFMath::Clamp(Settings.GeologyFractureFrequency, 0.006f, 0.18f)
\t);
\tconst float FractureCut =
\t\tSmoothStep(0.80f, 0.97f, FractureVolume) *
\t\tColumn.Fracture;

\t/*
\t * A vertical-varying lateral signal permits small real overhangs where the
\t * cliff structure supports them. Its budget is centimetres/decimetres in
\t * voxel-space, not the old several-metre displacement.
\t */
\tconst float OverhangSignal = SampleNoise3D(
\t\tAcross + 2381.0f,
\t\tAlong + 7151.0f,
\t\tWorldZ - 3319.0f,
\t\t0.11f
\t);
\tconst float OverhangPermission =
\t\tSmoothStep(0.42f, 0.88f, StructuralCarrier) *
\t\tColumn.RockHardness;

\tconst float RawDeformation =
\t\tStrataWave * 0.22f * FMath::Lerp(0.62f, 1.0f, Column.RockHardness) +
\t\tRockMass * 0.24f +
\t\tRockWarp * 0.12f +
\t\tOverhangSignal * 0.30f * OverhangPermission -
\t\tFractureCut * 0.20f;

\tconst float MaximumDisplacement = FMath::Lerp(
\t\t0.16f,
\t\t0.72f,
\t\tGeologyCarrier
\t) * SurfaceBandMask;
\tconst float GeologicalDisplacement = FMath::Clamp(
\t\tRawDeformation,
\t\t-MaximumDisplacement,
\t\tMaximumDisplacement
\t);

\treturn BaseTerrainDensity + GeologicalDisplacement;
}'''
)

# 3) Adaptive refinement must not cull a cell merely because its 80 cm corners
# share a sign. With bounded fine/geology displacement, refine a conservative
# surface band even when coarse corners do not cross the iso-surface.
mesher_path = "Source/Orakai/CubusCore/Meshing/CubusDensityMesher.cpp"
replace_function(
    mesher_path,
    "    bool CellMayContainFineSurface(",
    "    FInterpolatedVertex InterpolateAdaptiveEdge(",
    r'''    bool CellMayContainFineSurface(
        FAdaptiveSampleCache& SampleCache,
        const FIntVector& CoarseCellOrigin,
        const int32 Subdivisions,
        const float IsoLevel
    )
    {
        const FIntVector FineCellOrigin =
            CoarseCellOrigin * Subdivisions;

        bool bAnySolid = false;
        bool bAnyEmpty = false;
        float MinimumDistanceFromIso = MAX_flt;

        for (int32 CornerIndex = 0; CornerIndex < 8; ++CornerIndex)
        {
            const FIntVector FineCorner =
                FineCellOrigin +
                CornerOffsets[CornerIndex] * Subdivisions;
            const FCubusDensitySample CornerSample =
                SampleCache.GetSample(FineCorner);
            bAnySolid |= CornerSample.IsSolid(IsoLevel);
            bAnyEmpty |= !CornerSample.IsSolid(IsoLevel);
            MinimumDistanceFromIso = FMath::Min(
                MinimumDistanceFromIso,
                FMath::Abs(CornerSample.Density - IsoLevel)
            );
        }

        if (bAnySolid && bAnyEmpty)
        {
            return true;
        }

        /*
         * Do not let the canonical 80 cm lattice decide whether 20 cm detail is
         * allowed to exist. Terrain fine relief is capped at 0.34 voxel and
         * geology at 0.72 voxel; a 2.0-voxel conservative band therefore
         * guarantees that a bounded interior zero-crossing is refined rather
         * than discarded before the fine lattice is sampled.
         */
        if (MinimumDistanceFromIso <= 2.0f)
        {
            return true;
        }

        const int32 Half = Subdivisions / 2;
        const FIntVector ProbeOffsets[] =
        {
            FIntVector(Half, Half, Half),
            FIntVector(0, Half, Half),
            FIntVector(Subdivisions, Half, Half),
            FIntVector(Half, 0, Half),
            FIntVector(Half, Subdivisions, Half),
            FIntVector(Half, Half, 0),
            FIntVector(Half, Half, Subdivisions)
        };

        for (const FIntVector& ProbeOffset : ProbeOffsets)
        {
            const FCubusDensitySample Probe = SampleCache.GetSample(
                FineCellOrigin + ProbeOffset
            );
            if (Probe.IsSolid(IsoLevel) != bAnySolid ||
                FMath::Abs(Probe.Density - IsoLevel) <= 1.0f)
            {
                return true;
            }
        }

        return false;
    }'''
)

# 4) New generation namespace. Old v28 disk baselines must not be reused.
seed_path = "Source/Orakai/CubusCore/Generation/CubusGenerationSeeds.h"
replace_once(
    seed_path,
    '''    // Bumped to 28: retire the under-resolved v27 mesh-detail experiment and
    // re-enable bounded adaptive density sampling. The procedural field remains
    // canonical; near/middle/far meshes only change sample spacing.
    static constexpr uint32 CurrentGenerationVersion = 28;''',
    '''    // Bumped to 29: terrain generation is split into smooth macro form,
    // bounded continuous sub-voxel surface relief, and bounded volumetric rock
    // deformation. Adaptive refinement now preserves interior fine crossings.
    static constexpr uint32 CurrentGenerationVersion = 29;'''
)

# 5) Terrain-form test should test macro behavior, not demand >0.5 voxel local
# detail from the macro layer. Fine detail is now intentionally density-domain.
test_path = "Source/Orakai/CubusCore/Tests/CubusTerrainFormTests.cpp"
replace_once(
    test_path,
    '''    TestTrue(
        TEXT("Local detail produces visible terrain-scale height variation"),
        MaximumLocalDetailContribution > 0.5f
    );''',
    '''    TestTrue(
        TEXT("Macro terrain retains restrained local height variation"),
        MaximumLocalDetailContribution > 0.05f &&
            MaximumLocalDetailContribution < Settings.DetailAmplitude * 0.75f
    );'''
)

print("Applied v29 terrain/density overhaul")
