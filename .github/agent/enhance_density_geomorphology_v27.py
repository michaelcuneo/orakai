from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one anchor in {path}, found {count}: {old[:120]!r}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")


header = "Source/Orakai/CubusCore/Generation/CubusTerrainDensityField.h"
density = "Source/Orakai/CubusCore/Generation/CubusTerrainDensityField.cpp"
terrain = "Source/Orakai/CubusCore/Generation/CubusTerrainForm.cpp"
seeds = "Source/Orakai/CubusCore/Generation/CubusGenerationSeeds.h"

# Preserve local curvature/deposition information with each sampled terrain column.
replace_once(
    header,
    """        float Slope = 0.0f;
        FVector2D Gradient = FVector2D::ZeroVector;
        float RockHardness = 0.5f;
""",
    """        float Slope = 0.0f;
        FVector2D Gradient = FVector2D::ZeroVector;
        float Curvature = 0.0f;
        float Convexity = 0.0f;
        float Concavity = 0.0f;
        float Talus = 0.0f;
        float RockHardness = 0.5f;
""",
)

# Derive curvature from the same neighbouring surface samples already used for slope.
replace_once(
    density,
    """\tconst float GradientX = (HeightPositiveX - HeightNegativeX) * 0.5f;
\tconst float GradientY = (HeightPositiveY - HeightNegativeY) * 0.5f;
\tColumn.Gradient = FVector2D(GradientX, GradientY);
\tColumn.Slope = Column.Gradient.Size();

\tconst float WorldX = WorldSampleX - 0.5f;
""",
    """\tconst float GradientX = (HeightPositiveX - HeightNegativeX) * 0.5f;
\tconst float GradientY = (HeightPositiveY - HeightNegativeY) * 0.5f;
\tColumn.Gradient = FVector2D(GradientX, GradientY);
\tColumn.Slope = Column.Gradient.Size();

\t// Discrete surface Laplacian: negative values are convex shoulders/ridges,
\t// positive values are concave hollows where colluvium/talus accumulates.
\tColumn.Curvature =
\t\tHeightPositiveX + HeightNegativeX + HeightPositiveY + HeightNegativeY -
\t\tColumn.SurfaceVoxelHeight * 4.0f;
\tColumn.Convexity = SmoothStep(0.08f, 1.25f, -Column.Curvature);
\tColumn.Concavity = SmoothStep(0.08f, 1.25f, Column.Curvature);
\tconst float TalusSlopeBand =
\t\tSmoothStep(0.28f, 0.72f, Column.Slope) *
\t\t(1.0f - SmoothStep(1.05f, 1.80f, Column.Slope));
\tconst float TalusLandform = FMath::Clamp(
\t\tColumn.FormSample.FoothillWeight * 0.55f +
\t\tColumn.FormSample.MassifWeight * 0.55f +
\t\tColumn.FormSample.Cirque * 0.40f,
\t\t0.0f,
\t\t1.0f
\t);
\tColumn.Talus = FMath::Clamp(
\t\tTalusSlopeBand * FMath::Lerp(0.28f, 1.0f, Column.Concavity) * TalusLandform,
\t\t0.0f,
\t\t1.0f
\t);

\tconst float WorldX = WorldSampleX - 0.5f;
""",
)

# Rock exposure now follows actual geomorphic structure instead of generic slope alone.
replace_once(
    density,
    """\tconst float CliffExposure = SmoothStep(
\t\tSettings.GeologyCliffSlopeStart,
\t\tSettings.GeologyCliffSlopeFull,
\t\tColumn.Slope
\t);
\tconst float LandformExposure = FMath::Clamp(
\t\tColumn.FormSample.MountainCore * 0.82f +
\t\tColumn.FormSample.FoothillWeight * 0.46f +
\t\tColumn.FormSample.Ridge * 0.32f,
\t\t0.0f,
\t\t1.0f
\t);
\tconst float DrainageProtection = 1.0f - FMath::Clamp(
\t\tColumn.FormSample.Drainage * Column.FormSample.Drainage,
\t\t0.0f,
\t\t1.0f
\t);
\tColumn.RockExposure = CliffExposure * LandformExposure * DrainageProtection;
""",
    """\tconst float CliffExposure = SmoothStep(
\t\tSettings.GeologyCliffSlopeStart,
\t\tSettings.GeologyCliffSlopeFull,
\t\tColumn.Slope
\t);
\tconst float StructuralExposure = FMath::Clamp(
\t\tFMath::Max(
\t\t\tColumn.FormSample.Escarpment * 0.96f,
\t\t\tFMath::Max(
\t\t\t\tColumn.FormSample.Cirque * 0.74f + Column.FormSample.MassifWeight * 0.26f,
\t\t\t\tColumn.FormSample.Ridge * 0.52f
\t\t\t)
\t\t),
\t\t0.0f,
\t\t1.0f
\t);
\tconst float LandformExposure = FMath::Clamp(
\t\tColumn.FormSample.MountainCore * 0.58f +
\t\tColumn.FormSample.FoothillWeight * 0.30f +
\t\tColumn.FormSample.MassifWeight * 0.44f +
\t\tStructuralExposure * 0.62f,
\t\t0.0f,
\t\t1.0f
\t);
\tconst float DrainageProtection = 1.0f - FMath::Clamp(
\t\tColumn.FormSample.Drainage * Column.FormSample.Drainage,
\t\t0.0f,
\t\t1.0f
\t);
\tconst float CurvatureExposure =
\t\tFMath::Lerp(0.78f, 1.18f, Column.Convexity) *
\t\tFMath::Lerp(1.0f, 0.78f, Column.Concavity);
\tColumn.RockExposure = FMath::Clamp(
\t\tFMath::Max(CliffExposure, StructuralExposure * 0.58f) *
\t\tLandformExposure * DrainageProtection * CurvatureExposure,
\t\t0.0f,
\t\t1.0f
\t);
""",
)

# Replace the near-surface geology operator with a landform-aware compositing pass.
start = "float FCubusTerrainDensityField::SampleGeologicalDensity(\n"
end = "\nfloat FCubusTerrainDensityField::SampleCaveDensity(\n"
p = Path(density)
text = p.read_text(encoding="utf-8")
a = text.find(start)
b = text.find(end, a)
if a < 0 or b < 0:
    raise RuntimeError("SampleGeologicalDensity function bounds not found")
new_function = r'''float FCubusTerrainDensityField::SampleGeologicalDensity(
	const FVector& GlobalSampleCoordinate,
	const FColumnData& Column,
	const float BaseTerrainDensity
) const
{
	const float StructuralCliff = FMath::Clamp(
		FMath::Max(
			Column.FormSample.Escarpment * 0.96f,
			FMath::Max(
				Column.FormSample.Cirque * 0.78f,
				Column.FormSample.MassifWeight * 0.38f + Column.FormSample.Ridge * 0.28f
			)
		),
		0.0f,
		1.0f
	);
	const float GeomorphologyCarrier = FMath::Clamp(
		FMath::Max(Column.RockExposure, FMath::Max(StructuralCliff * 0.72f, Column.Talus * 0.62f)),
		0.0f,
		1.0f
	);
	const float DistanceFromSurface = FMath::Abs(BaseTerrainDensity);
	if (DistanceFromSurface >= Settings.GeologySurfaceBand || GeomorphologyCarrier <= KINDA_SMALL_NUMBER)
	{
		return BaseTerrainDensity;
	}

	const float SurfaceBandMask = 1.0f - SmoothStep(0.0f, Settings.GeologySurfaceBand, DistanceFromSurface);
	const float WorldX = static_cast<float>(GlobalSampleCoordinate.X) + static_cast<float>(Settings.TerrainOffsetX);
	const float WorldY = static_cast<float>(GlobalSampleCoordinate.Y) + static_cast<float>(Settings.TerrainOffsetY);
	const float WorldZ = static_cast<float>(GlobalSampleCoordinate.Z);

	FVector2D Downhill = -Column.Gradient;
	if (!Downhill.Normalize())
	{
		return BaseTerrainDensity;
	}

	const FVector2D AlongCliff(-Downhill.Y, Downhill.X);
	const float Across = WorldX * Downhill.X + WorldY * Downhill.Y;
	const float Along = WorldX * AlongCliff.X + WorldY * AlongCliff.Y;
	const float Hardness = FMath::Lerp(0.58f, 1.30f, Column.RockHardness);

	// Low-amplitude wall warp breaks planar Marching-Cubes faces but is restrained
	// outside real exposed structures so soil hills do not become stone noise.
	const float RockWarp = SampleNoise3D(
		WorldX + 1709.0f,
		WorldY - 3187.0f,
		WorldZ + 733.0f,
		Settings.GeologyRockWarpFrequency
	) * Settings.GeologyRockWarpStrength;

	// Bedding follows cliff aspect and slowly folded strata. Positive bands form
	// resistant ledges; negative bands become recessive seams and shallow alcoves.
	const float FoldedDip = Settings.GeologyStrataDip + Column.StrataTilt * 0.020f;
	const float StrataPhase =
		WorldZ * Settings.GeologyStrataFrequency +
		Across * FoldedDip +
		SampleNoise2D(
			Along + 9113.0f,
			Across - 4327.0f,
			Settings.GeologyStrataFrequency * 0.18f
		) * 0.35f;
	const float StrataWave = FMath::Sin(StrataPhase * 2.0f * PI);
	const float ShelfBand = FMath::Square(FMath::Max(0.0f, StrataWave));
	const float UndercutBand = FMath::Square(FMath::Max(0.0f, -StrataWave));

	const float MassRidge = SampleRidgedNoise3D(
		WorldX - 6211.0f,
		WorldY + 4177.0f,
		WorldZ - 1987.0f,
		Settings.GeologyMassFrequency
	);
	const float MassNoise = SampleNoise3D(
		WorldX + 2381.0f,
		WorldY + 7151.0f,
		WorldZ - 3319.0f,
		Settings.GeologyMassFrequency * 0.53f
	);
	const float RockMass =
		(MassRidge - 0.48f) * Settings.GeologyMassStrength * 0.72f +
		MassNoise * Settings.GeologyMassStrength * 0.24f;

	// Vertical/oblique ribs run along exposed faces. They are elongated in Z and
	// cliff-tangent space rather than isotropic 3D noise, so cliffs read as eroded
	// rock masses instead of porous blobs.
	const float CliffRibSignal = SampleRidgedNoise(
		Along + Across * 0.13f + 18457.0f,
		WorldZ * 0.34f - Across * 0.07f - 9271.0f,
		Settings.GeologyMassFrequency * 0.70f
	);
	const float CliffRib = SmoothStep(0.42f, 0.84f, CliffRibSignal) * StructuralCliff;
	const float CliffRibDisplacement =
		Settings.GeologyMassStrength * 0.82f * CliffRib * Hardness;

	// Sparse alcoves remove material behind the face and combine with recessive
	// bedding to create real shallow overhang topology in the scalar field.
	const float AlcoveNoise = FMath::Clamp(
		0.5f + 0.5f * SampleNoise3D(
			WorldX - 14321.0f,
			WorldY + 7817.0f,
			WorldZ + 4261.0f,
			Settings.GeologyMassFrequency * 0.62f
		),
		0.0f,
		1.0f
	);
	const float AlcoveMask =
		SmoothStep(0.64f, 0.88f, AlcoveNoise) *
		StructuralCliff *
		FMath::Lerp(1.0f, 0.58f, Column.Convexity);
	const float AlcoveCut = Settings.GeologyUndercutStrength * 0.78f * AlcoveMask;

	const float FractureVolume = SampleRidgedNoise3D(
		WorldX + 12011.0f,
		WorldY - 4919.0f,
		WorldZ + 2791.0f,
		Settings.GeologyFractureFrequency
	);
	const float FractureCut =
		SmoothStep(0.72f, 0.96f, FractureVolume) *
		Column.Fracture *
		Settings.GeologyFractureStrength;

	const float ShelfDisplacement = Settings.GeologyShelfStrength * ShelfBand * Hardness;
	const float UndercutDisplacement =
		Settings.GeologyUndercutStrength * UndercutBand *
		FMath::Lerp(1.15f, 0.65f, Column.RockHardness);
	const float OverhangCarrier = FMath::Clamp(
		ShelfBand * 0.52f + MassRidge * 0.28f + CliffRib * 0.38f,
		0.0f,
		1.0f
	);
	const float OverhangDisplacement =
		Settings.GeologyOverhangStrength *
		OverhangCarrier *
		Column.RockHardness *
		SmoothStep(0.18f, 0.72f, FMath::Max(Column.RockExposure, StructuralCliff));

	// Colluvium/talus is a separate depositional signal: medium slopes + local
	// concavity receive irregular positive density, building aprons beneath cliffs.
	const float TalusPattern = FMath::Clamp(
		0.5f + 0.5f * SampleNoise2D(
			Across + 3271.0f,
			Along - 11887.0f,
			Settings.GeologyMassFrequency * 1.35f
		),
		0.0f,
		1.0f
	);
	const float TalusDisplacement =
		Settings.GeologyMassStrength * 0.62f *
		Column.Talus *
		FMath::Lerp(0.42f, 1.0f, TalusPattern);

	const float BedrockCarrier = FMath::Clamp(
		FMath::Max(Column.RockExposure, StructuralCliff * 0.62f),
		0.0f,
		1.0f
	);
	const float BedrockDisplacement =
		ShelfDisplacement +
		RockMass +
		CliffRibDisplacement +
		OverhangDisplacement -
		UndercutDisplacement -
		AlcoveCut -
		FractureCut +
		RockWarp * FMath::Lerp(0.28f, 0.72f, StructuralCliff);

	const float GeologicalDisplacement =
		BedrockDisplacement * BedrockCarrier +
		TalusDisplacement;

	return BaseTerrainDensity + GeologicalDisplacement * SurfaceBandMask;
}
'''
text = text[:a] + new_function + text[b:]
p.write_text(text, encoding="utf-8")

# Add medium-scale branching ridge networks that remain resolvable at 80 cm.
replace_once(
    terrain,
    """    const float LocalRidge = FMath::Clamp(
        LocalRidgeShoulder * 0.58f + LocalRidgeFine * 0.42f,
        0.0f,
        1.0f
    );

    const float MajorRidgeFine = SampleRidgedFbm(
""",
    """    const float LocalRidge = FMath::Clamp(
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
""",
)
replace_once(
    terrain,
    """    Result.Ridge = FMath::Max(
        LocalRidge,
        MajorRidge * FMath::Max(FMath::Max(RangeCore, DistributedRangeCore * 0.88f), HighlandProvince * 0.34f)
    );
""",
    """    Result.Ridge = FMath::Max(
        FMath::Max(LocalRidge, BranchNetwork * 0.90f),
        MajorRidge * FMath::Max(FMath::Max(RangeCore, DistributedRangeCore * 0.88f), HighlandProvince * 0.34f)
    );
""",
)

# Add headwater incision after local rills: narrow mountain gullies feed the
# existing tributary hierarchy without carving uniform high-frequency channels.
replace_once(
    terrain,
    """    const float LocalRills = FMath::Lerp(
        RillA,
        RillB,
        SmoothStep(0.36f, 0.64f, SurfacePatch)
    );

    const float ContinentStrength =
""",
    """    const float LocalRills = FMath::Lerp(
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
""",
)

# Add branch relief and headwater cuts to the relief budget.
replace_once(
    terrain,
    """    const float LocalCrestRelief =
        LocalRidge * Settings.RidgeAmplitude * RidgeStrength * Erosion * 0.38f;
    const float Continent =
""",
    """    const float LocalCrestRelief =
        LocalRidge * Settings.RidgeAmplitude * RidgeStrength * Erosion * 0.34f;
    const float BranchCrestRelief =
        BranchNetwork * Settings.RidgeAmplitude * RidgeStrength * Erosion * 0.24f;
    const float HeadwaterCut =
        Settings.ValleyDepth * 0.22f * HeadwaterNotches *
        FMath::Lerp(0.72f, 1.0f, BranchNetwork);
    const float Continent =
""",
)
replace_once(
    terrain,
    """        MassifUplift * Erosion +
        EscarpmentLift +
        LocalCrestRelief -
        BasinCut -
""",
    """        MassifUplift * Erosion +
        EscarpmentLift +
        LocalCrestRelief +
        BranchCrestRelief -
        BasinCut -
""",
)
replace_once(
    terrain,
    """        MainValleyCut -
        TributaryValleyCut -
        CirqueCut;
""",
    """        MainValleyCut -
        TributaryValleyCut -
        HeadwaterCut -
        CirqueCut;
""",
)

# New scalar-field output means a fresh persisted density namespace.
replace_once(
    seeds,
    """    // Bumped to 26: restore exact LOD0/coarse sample alignment and add
    // independent regional orogenic ranges so tall/snow-capable mountains are
    // spatially distributed instead of depending on the kilometre-scale spine.
    static constexpr uint32 CurrentGenerationVersion = 26;
""",
    """    // Bumped to 27: add resolvable branching ridges/headwater erosion plus
    // curvature-aware talus, cliff ribs, bedding alcoves and shallow 3D undercuts
    // to make the 80 cm density surface read as naturally eroded terrain.
    static constexpr uint32 CurrentGenerationVersion = 27;
""",
)

# Guard the intended architecture and prevent accidental LOD regressions.
h = Path(header).read_text(encoding="utf-8")
d = Path(density).read_text(encoding="utf-8")
t = Path(terrain).read_text(encoding="utf-8")
s = Path(seeds).read_text(encoding="utf-8")
for token in ["Curvature", "Convexity", "Concavity", "Talus"]:
    if token not in h:
        raise RuntimeError(f"missing column field {token}")
for token in ["StructuralCliff", "CliffRibDisplacement", "AlcoveCut", "TalusDisplacement"]:
    if token not in d:
        raise RuntimeError(f"missing geological detail {token}")
for token in ["BranchNetwork", "HeadwaterNotches", "BranchCrestRelief", "HeadwaterCut"]:
    if token not in t:
        raise RuntimeError(f"missing terrain detail {token}")
if "CurrentGenerationVersion = 27" not in s:
    raise RuntimeError("generation v27 missing")
