from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one anchor in {path}, found {count}: {old[:120]!r}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")


terrain = "Source/Orakai/CubusCore/Generation/CubusTerrainForm.cpp"
density = "Source/Orakai/CubusCore/Generation/CubusTerrainDensityField.cpp"
seeds = "Source/Orakai/CubusCore/Generation/CubusGenerationSeeds.h"

# Terrain surface detail must be band-limited in canonical voxel space. At the
# current 80 cm lattice, geometry with a period of only a handful of samples
# exposes the Marching Cubes tessellation instead of reading as terrain.
replace_once(
    terrain,
    """    const float Detail = SampleFbm(
        TerrainX - 431.0f,
        TerrainY + 2671.0f,
        Settings.DetailFrequency,
        2,
        2.0f,
        0.42f
    );

    const float SurfacePatch = FMath::Clamp(
""",
    """    // Geometry detail is deliberately band-limited. A 0.045 cycle/voxel
    // ceiling gives a minimum wavelength of ~22 canonical voxels (~17.6 m at
    // 80 cm), safely above the range where Marching Cubes facets become the
    // visible feature. Sub-metre/metre texture belongs in materials, not mesh.
    const float GeometryDetailFrequency = FMath::Min(Settings.DetailFrequency, 0.045f);
    const float Detail = SampleFbm(
        TerrainX - 431.0f,
        TerrainY + 2671.0f,
        GeometryDetailFrequency * 0.72f,
        2,
        2.0f,
        0.42f
    );

    const float SurfacePatch = FMath::Clamp(
""",
)

for old, new in [
    ("Settings.DetailFrequency * 0.10f,", "GeometryDetailFrequency * 0.10f,"),
    ("Settings.DetailFrequency * 0.22f,", "GeometryDetailFrequency * 0.18f,"),
    ("Settings.DetailFrequency * 0.48f,", "GeometryDetailFrequency * 0.22f,"),
    ("Settings.DetailFrequency * 1.75f,", "GeometryDetailFrequency * 0.58f,"),
    ("Settings.DetailFrequency * 0.72f,", "GeometryDetailFrequency * 0.42f,"),
    ("Settings.DetailFrequency * 0.46f,", "GeometryDetailFrequency * 0.36f,"),
    ("Settings.DetailFrequency * 0.39f,", "GeometryDetailFrequency * 0.32f,"),
]:
    replace_once(terrain, old, new)

replace_once(
    terrain,
    """    const float BroadSurfaceRelief =
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
""",
    """    const float BroadSurfaceRelief =
        SoilUndulation * Settings.DetailAmplitude * 0.48f * UndulationStrength;
    const float RawFineSurfaceRelief =
        Detail * Settings.DetailAmplitude * 0.20f * DetailStrength +
        MicroRelief * Settings.DetailAmplitude * 0.16f * MicroStrength +
        BrokenGround * Settings.DetailAmplitude * 0.06f * BrokenGroundStrength -
        LocalRills * Settings.DetailAmplitude * 0.28f * RillStrength;
    const float FineSurfaceReliefLimit =
        Settings.DetailAmplitude *
        FMath::Lerp(0.26f, 0.42f, SurfacePatch) *
        FMath::Lerp(0.82f, 1.0f, Erosion);
""",
)

# Re-tighten rock exposure: structural landforms may suggest where cliffs can
# occur, but actual 3D density displacement must still require a steep surface.
replace_once(
    density,
    """\tconst float CurvatureExposure =
\t\tFMath::Lerp(0.78f, 1.18f, Column.Convexity) *
\t\tFMath::Lerp(1.0f, 0.78f, Column.Concavity);
\tColumn.RockExposure = FMath::Clamp(
\t\tFMath::Max(CliffExposure, StructuralExposure * 0.58f) *
\t\tLandformExposure * DrainageProtection * CurvatureExposure,
\t\t0.0f,
\t\t1.0f
\t);
""",
    """\tconst float CurvatureExposure =
\t\tFMath::Lerp(0.84f, 1.10f, Column.Convexity) *
\t\tFMath::Lerp(1.0f, 0.86f, Column.Concavity);
\tconst float StructuralSlopeGate = SmoothStep(
\t\tSettings.GeologyCliffSlopeStart * 0.72f,
\t\tFMath::Max(Settings.GeologyCliffSlopeStart + 0.35f, Settings.GeologyCliffSlopeFull * 0.82f),
\t\tColumn.Slope
\t);
\tColumn.RockExposure = FMath::Clamp(
\t\tFMath::Max(CliffExposure, StructuralExposure * 0.42f * StructuralSlopeGate) *
\t\tLandformExposure * DrainageProtection * CurvatureExposure,
\t\t0.0f,
\t\t1.0f
\t);
""",
)

# Band-limit and amplitude-limit the volumetric geology itself. We keep real
# density-native ledges/undercuts, but each feature is broad and the total
# displacement is clamped to roughly one-to-two 80 cm voxels.
replace_once(
    density,
    """\tconst float SurfaceBandMask = 1.0f - SmoothStep(0.0f, Settings.GeologySurfaceBand, DistanceFromSurface);
\tconst float WorldX = static_cast<float>(GlobalSampleCoordinate.X) + static_cast<float>(Settings.TerrainOffsetX);
\tconst float WorldY = static_cast<float>(GlobalSampleCoordinate.Y) + static_cast<float>(Settings.TerrainOffsetY);
\tconst float WorldZ = static_cast<float>(GlobalSampleCoordinate.Z);
""",
    """\tconst float SurfaceBandMask = 1.0f - SmoothStep(0.0f, Settings.GeologySurfaceBand, DistanceFromSurface);
\tconst float WorldX = static_cast<float>(GlobalSampleCoordinate.X) + static_cast<float>(Settings.TerrainOffsetX);
\tconst float WorldY = static_cast<float>(GlobalSampleCoordinate.Y) + static_cast<float>(Settings.TerrainOffsetY);
\tconst float WorldZ = static_cast<float>(GlobalSampleCoordinate.Z);
\tconst float GeometryRockFrequency = FMath::Min(Settings.GeologyRockWarpFrequency, 0.022f);
\tconst float GeometryMassFrequency = FMath::Min(Settings.GeologyMassFrequency, 0.016f);
\tconst float GeometryStrataFrequency = FMath::Min(Settings.GeologyStrataFrequency, 0.032f);
\tconst float GeometryFractureFrequency = FMath::Min(Settings.GeologyFractureFrequency, 0.024f);
""",
)

for old, new in [
    ("\t\tSettings.GeologyRockWarpFrequency\n", "\t\tGeometryRockFrequency\n"),
    ("\t\tWorldZ * Settings.GeologyStrataFrequency +\n", "\t\tWorldZ * GeometryStrataFrequency +\n"),
    ("\t\t\tSettings.GeologyStrataFrequency * 0.18f\n", "\t\t\tGeometryStrataFrequency * 0.18f\n"),
    ("\t\tSettings.GeologyMassFrequency\n", "\t\tGeometryMassFrequency\n"),
    ("\t\tSettings.GeologyMassFrequency * 0.53f\n", "\t\tGeometryMassFrequency * 0.53f\n"),
    ("\t\tSettings.GeologyMassFrequency * 0.70f\n", "\t\tGeometryMassFrequency * 0.70f\n"),
    ("\t\t\tSettings.GeologyMassFrequency * 0.62f\n", "\t\t\tGeometryMassFrequency * 0.62f\n"),
    ("\t\tSettings.GeologyFractureFrequency\n", "\t\tGeometryFractureFrequency\n"),
    ("\t\t\tSettings.GeologyMassFrequency * 1.35f\n", "\t\t\tGeometryMassFrequency * 1.10f\n"),
]:
    replace_once(density, old, new)

replace_once(
    density,
    """\tconst float RockMass =
\t\t(MassRidge - 0.48f) * Settings.GeologyMassStrength * 0.72f +
\t\tMassNoise * Settings.GeologyMassStrength * 0.24f;
""",
    """\tconst float RockMass =
\t\t(MassRidge - 0.48f) * Settings.GeologyMassStrength * 0.18f +
\t\tMassNoise * Settings.GeologyMassStrength * 0.08f;
""",
)
replace_once(
    density,
    """\tconst float CliffRibDisplacement =
\t\tSettings.GeologyMassStrength * 0.82f * CliffRib * Hardness;
""",
    """\tconst float CliffRibDisplacement =
\t\tSettings.GeologyMassStrength * 0.20f * CliffRib * Hardness;
""",
)
replace_once(
    density,
    """\tconst float AlcoveCut = Settings.GeologyUndercutStrength * 0.78f * AlcoveMask;
""",
    """\tconst float AlcoveCut = Settings.GeologyUndercutStrength * 0.16f * AlcoveMask;
""",
)
replace_once(
    density,
    """\tconst float FractureCut =
\t\tSmoothStep(0.72f, 0.96f, FractureVolume) *
\t\tColumn.Fracture *
\t\tSettings.GeologyFractureStrength;

\tconst float ShelfDisplacement = Settings.GeologyShelfStrength * ShelfBand * Hardness;
\tconst float UndercutDisplacement =
\t\tSettings.GeologyUndercutStrength * UndercutBand *
\t\tFMath::Lerp(1.15f, 0.65f, Column.RockHardness);
""",
    """\tconst float FractureCut =
\t\tSmoothStep(0.72f, 0.96f, FractureVolume) *
\t\tColumn.Fracture *
\t\tSettings.GeologyFractureStrength * 0.20f;

\tconst float ShelfDisplacement =
\t\tSettings.GeologyShelfStrength * 0.18f * ShelfBand * Hardness;
\tconst float UndercutDisplacement =
\t\tSettings.GeologyUndercutStrength * 0.14f * UndercutBand *
\t\tFMath::Lerp(1.08f, 0.72f, Column.RockHardness);
""",
)
replace_once(
    density,
    """\tconst float OverhangDisplacement =
\t\tSettings.GeologyOverhangStrength *
\t\tOverhangCarrier *
\t\tColumn.RockHardness *
\t\tSmoothStep(0.18f, 0.72f, FMath::Max(Column.RockExposure, StructuralCliff));
""",
    """\tconst float OverhangDisplacement =
\t\tSettings.GeologyOverhangStrength * 0.16f *
\t\tOverhangCarrier *
\t\tColumn.RockHardness *
\t\tSmoothStep(0.42f, 0.82f, Column.RockExposure);
""",
)
replace_once(
    density,
    """\tconst float TalusDisplacement =
\t\tSettings.GeologyMassStrength * 0.62f *
\t\tColumn.Talus *
\t\tFMath::Lerp(0.42f, 1.0f, TalusPattern);
""",
    """\tconst float TalusDisplacement =
\t\tSettings.GeologyMassStrength * 0.12f *
\t\tColumn.Talus *
\t\tFMath::Lerp(0.48f, 1.0f, TalusPattern);
""",
)
replace_once(
    density,
    """\t\tFractureCut +
\t\tRockWarp * FMath::Lerp(0.28f, 0.72f, StructuralCliff);

\tconst float GeologicalDisplacement =
\t\tBedrockDisplacement * BedrockCarrier +
\t\tTalusDisplacement;

\treturn BaseTerrainDensity + GeologicalDisplacement * SurfaceBandMask;
""",
    """\t\tFractureCut +
\t\tRockWarp * 0.12f * FMath::Lerp(0.30f, 0.70f, StructuralCliff);

\tconst float RawGeologicalDisplacement =
\t\tBedrockDisplacement * BedrockCarrier +
\t\tTalusDisplacement;
\tconst float DisplacementLimit = FMath::Lerp(0.70f, 1.55f, Column.RockExposure);
\tconst float GeologicalDisplacement = FMath::Clamp(
\t\tRawGeologicalDisplacement,
\t\t-DisplacementLimit,
\t\tDisplacementLimit
\t);

\treturn BaseTerrainDensity + GeologicalDisplacement * SurfaceBandMask;
""",
)

replace_once(
    seeds,
    """    // Bumped to 27: add resolvable branching ridges/headwater erosion plus
    // curvature-aware talus, cliff ribs, bedding alcoves and shallow 3D undercuts
    // to make the 80 cm density surface read as naturally eroded terrain.
    static constexpr uint32 CurrentGenerationVersion = 27;
""",
    """    // Bumped to 28: band-limit terrain/geology for the 80 cm density lattice.
    // Fine mesh perturbations are suppressed; resolvable broad geomorphology and
    // tightly clamped cliff-only 3D density structure remain.
    static constexpr uint32 CurrentGenerationVersion = 28;
""",
)

# Architecture guards.
t = Path(terrain).read_text(encoding="utf-8")
d = Path(density).read_text(encoding="utf-8")
s = Path(seeds).read_text(encoding="utf-8")
if "GeometryDetailFrequency = FMath::Min(Settings.DetailFrequency, 0.045f)" not in t:
    raise RuntimeError("terrain band-limit missing")
if "Settings.DetailFrequency * 1.75f" in t:
    raise RuntimeError("under-resolved micro relief frequency remains")
for token in ["GeometryRockFrequency", "GeometryMassFrequency", "GeometryStrataFrequency", "DisplacementLimit"]:
    if token not in d:
        raise RuntimeError(f"missing geology band-limit token {token}")
if "Settings.GeologyOverhangStrength *\n" in d:
    raise RuntimeError("unscaled overhang strength remains")
if "CurrentGenerationVersion = 28" not in s:
    raise RuntimeError("generation v28 missing")
