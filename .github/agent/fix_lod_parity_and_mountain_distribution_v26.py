from pathlib import Path

# Trigger v26 source patch.


def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected exactly one anchor in {path}, found {count}: {old!r}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")

replace_once(
    "Source/Orakai/CubusCore/Actors/CubusTerrainLodWorldActor.cpp",
    "\t/*\n\t * Coarse tiles must be a lower-resolution view of the exact same absolute\n\t * density field as LOD0. No centring offset belongs here: the component is\n\t * already placed at TileCoordinate * ChunkSize * stride in world space.\n\t * Offsetting source X/Y sampled a different landscape in every LOD tier;\n\t * offsetting Z lifted the outer rings by half a coarse tile (1008 canonical\n\t * voxels at stride 64), producing the false mountain/snow horizon ring.\n\t */\n\tconst FVector SourceOrigin = LogicalOrigin * static_cast<double>(SafeStride);\n",
    "\t/*\n\t * Preserve the LOD0 sample/render convention exactly. Density samples are\n\t * indexed from the chunk minimum, while the procedural mesh is centred on\n\t * its actor. LOD0 therefore evaluates density 16 canonical voxels ahead of\n\t * rendered world position. For coarse stride S, subtracting 16*(S-1) keeps\n\t * that same +16 offset instead of introducing a tier-dependent height shift.\n\t */\n\tconst double AlignmentOffset = static_cast<double>(Cubus::ChunkSize) * 0.5 * static_cast<double>(SafeStride - 1);\n\n\tconst FVector SourceOrigin =\n\t\tLogicalOrigin * static_cast<double>(SafeStride) - FVector(AlignmentOffset, AlignmentOffset, AlignmentOffset);\n",
)

terrain_path = "Source/Orakai/CubusCore/Generation/CubusTerrainForm.cpp"
insert_anchor = """    const float BasinProvince = FMath::Clamp(
        SmoothStep(0.58f, 0.82f, 1.0f - ProvinceElevation) *
        (1.0f - FoothillBelt * 0.62f),
        0.0f,
        1.0f
    );

"""
insert_new = insert_anchor + """    /*
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

"""
replace_once(terrain_path, insert_anchor, insert_new)
replace_once(terrain_path,
"""    Result.MountainCore = FMath::Clamp(
        FMath::Max(RangeCore, HighlandProvince * 0.22f),
        0.0f,
        1.0f
    );
""",
"""    Result.MountainCore = FMath::Clamp(
        FMath::Max(FMath::Max(RangeCore, DistributedRangeCore * 0.92f), HighlandProvince * 0.22f),
        0.0f,
        1.0f
    );
""")
replace_once(terrain_path,
"""    Result.FoothillWeight = FMath::Clamp(
        FMath::Max(FMath::Max(FoothillBelt, RangeCore), HighlandProvince * 0.56f),
        0.0f,
        1.0f
    );
""",
"""    Result.FoothillWeight = FMath::Clamp(
        FMath::Max(
            FMath::Max(FMath::Max(FoothillBelt, DistributedFoothillBelt * 0.90f), FMath::Max(RangeCore, DistributedRangeCore * 0.92f)),
            HighlandProvince * 0.56f
        ),
        0.0f,
        1.0f
    );
""")
replace_once(terrain_path,
"""    Result.MountainWeight = FMath::Clamp(
        FMath::Max(
            RangeCore + FoothillBelt * 0.48f,
            HighlandProvince * 0.58f
        ),
        0.0f,
        1.0f
    );
""",
"""    Result.MountainWeight = FMath::Clamp(
        FMath::Max(
            FMath::Max(RangeCore + FoothillBelt * 0.48f, DistributedRangeCore * 0.92f + DistributedFoothillBelt * 0.44f),
            HighlandProvince * 0.58f
        ),
        0.0f,
        1.0f
    );
""")
replace_once(terrain_path,
"""    Result.Ridge = FMath::Max(
        LocalRidge,
        MajorRidge * FMath::Max(RangeCore, HighlandProvince * 0.34f)
    );
""",
"""    Result.Ridge = FMath::Max(
        LocalRidge,
        MajorRidge * FMath::Max(FMath::Max(RangeCore, DistributedRangeCore * 0.88f), HighlandProvince * 0.34f)
    );
""")
replace_once(terrain_path,
"""    const float LegacyMassifCarrier = FMath::Clamp(
        FoothillBelt * 0.78f + RangeCore * 0.52f,
        0.0f,
        1.0f
    );
""",
"""    const float LegacyMassifCarrier = FMath::Clamp(
        FMath::Max(
            FoothillBelt * 0.78f + RangeCore * 0.52f,
            DistributedFoothillBelt * 0.74f + DistributedRangeCore * 0.50f
        ),
        0.0f,
        1.0f
    );
""")
replace_once(terrain_path,
"""    const float RangeUplift =
        Settings.RidgeAmplitude * Settings.MountainElevationScale *
        (
            FoothillBelt * 0.56f +
            RangeCore * (0.92f + MajorRidge * 0.72f)
        ) *
        (0.78f + PeakRhythm * 0.30f) *
        (0.72f + PassRhythm * 0.30f);
""",
"""    const float PrimaryRangeUplift =
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
""")
replace_once(
    "Source/Orakai/CubusCore/Generation/CubusGenerationSeeds.h",
    "    // Bumped to 25: coarse terrain LOD now samples the exact absolute canonical\n    // density coordinates used by LOD0; the old half-tile XYZ offset created a\n    // false elevated mountain/snow ring at the outer terrain tiers.\n    static constexpr uint32 CurrentGenerationVersion = 25;\n",
    "    // Bumped to 26: restore exact LOD0/coarse sample alignment and add\n    // independent regional orogenic ranges so tall/snow-capable mountains are\n    // spatially distributed instead of depending on the kilometre-scale spine.\n    static constexpr uint32 CurrentGenerationVersion = 26;\n",
)

lod = Path("Source/Orakai/CubusCore/Actors/CubusTerrainLodWorldActor.cpp").read_text(encoding="utf-8")
terrain = Path(terrain_path).read_text(encoding="utf-8")
seeds = Path("Source/Orakai/CubusCore/Generation/CubusGenerationSeeds.h").read_text(encoding="utf-8")
if "SafeStride - 1" not in lod or "AlignmentOffset" not in lod:
    raise RuntimeError("LOD parity alignment was not restored")
for token in ["DistributedRangeFrequency", "DistributedRangeCore", "DistributedFoothillBelt", "DistributedRangeUplift"]:
    if token not in terrain:
        raise RuntimeError(f"missing distributed terrain token: {token}")
if "FMath::Max(PrimaryRangeUplift, DistributedRangeUplift)" not in terrain:
    raise RuntimeError("bounded dual-range uplift missing")
if "CurrentGenerationVersion = 26" not in seeds:
    raise RuntimeError("generation version 26 missing")
