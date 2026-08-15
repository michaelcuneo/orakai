from pathlib import Path
import subprocess

# Trigger adaptive density v28 migration.
BASE = "09e5b4b203e60188d28c0d2b64bab54485c49491"

# Revert the failed v27 mesh-detail experiment while preserving the v26
# distributed-range terrain and all earlier crash/LOD parity fixes.
for path in [
    "Source/Orakai/CubusCore/Generation/CubusTerrainForm.cpp",
    "Source/Orakai/CubusCore/Generation/CubusTerrainDensityField.cpp",
    "Source/Orakai/CubusCore/Generation/CubusTerrainDensityField.h",
]:
    content = subprocess.check_output(
        ["git", "show", f"{BASE}:{path}"],
        text=True,
    )
    Path(path).write_text(content, encoding="utf-8")

# New generation namespace: v28 intentionally returns the base terrain to v26
# geometry while changing the runtime sampling architecture to 20/40/80 cm for
# an 80 cm canonical voxel. This prevents any v27 baseline from being reused.
seed_path = Path("Source/Orakai/CubusCore/Generation/CubusGenerationSeeds.h")
seed_text = seed_path.read_text(encoding="utf-8")
old_seed = """    // Bumped to 27: add resolvable branching ridges/headwater erosion plus
    // curvature-aware talus, cliff ribs, bedding alcoves and shallow 3D undercuts
    // to make the 80 cm density surface read as naturally eroded terrain.
    static constexpr uint32 CurrentGenerationVersion = 27;
"""
new_seed = """    // Bumped to 28: retire the under-resolved v27 mesh-detail experiment and
    // re-enable bounded adaptive density sampling. The procedural field remains
    // canonical; near/middle/far meshes only change sample spacing.
    static constexpr uint32 CurrentGenerationVersion = 28;
"""
if seed_text.count(old_seed) != 1:
    raise RuntimeError("v27 generation-version anchor not found exactly once")
seed_path.write_text(seed_text.replace(old_seed, new_seed, 1), encoding="utf-8")

# Update the LOD scale test for the runtime-safe 1x/2x/4x power-of-two tiers.
test_path = Path("Source/Orakai/CubusCore/Tests/CubusDensityLodTests.cpp")
test_text = test_path.read_text(encoding="utf-8")
old_test = """    TestEqual(
        TEXT(\"100 cm density uses one sample per canonical voxel\"),
        FCubusDensityLod::ResolveSubdivisionsForSpacing(100.0f, 100.0f),
        1
    );
    TestEqual(
        TEXT(\"50 cm density uses two samples per canonical voxel\"),
        FCubusDensityLod::ResolveSubdivisionsForSpacing(100.0f, 50.0f),
        2
    );
    TestEqual(
        TEXT(\"25 cm density uses four samples per canonical voxel\"),
        FCubusDensityLod::ResolveSubdivisionsForSpacing(100.0f, 25.0f),
        4
    );
    TestEqual(
        TEXT(\"10 cm density uses ten samples per canonical voxel\"),
        FCubusDensityLod::ResolveSubdivisionsForSpacing(100.0f, 10.0f),
        10
    );

    TestEqual(
        TEXT(\"Fine density does not change the canonical voxel size\"),
        FCubusDensityLod::GetSampleSpacing(100.0f, 10),
        10.0f
    );
"""
new_test = """    TestEqual(
        TEXT(\"100 cm density uses one sample per canonical voxel\"),
        FCubusDensityLod::ResolveSubdivisionsForSpacing(100.0f, 100.0f),
        1
    );
    TestEqual(
        TEXT(\"50 cm density uses two samples per canonical voxel\"),
        FCubusDensityLod::ResolveSubdivisionsForSpacing(100.0f, 50.0f),
        2
    );
    TestEqual(
        TEXT(\"25 cm density uses four samples per canonical voxel\"),
        FCubusDensityLod::ResolveSubdivisionsForSpacing(100.0f, 25.0f),
        4
    );
    TestEqual(
        TEXT(\"Runtime refinement is capped at four subdivisions\"),
        FCubusDensityLod::ResolveSubdivisionsForSpacing(100.0f, 10.0f),
        4
    );

    TestEqual(
        TEXT(\"80 cm canonical terrain resolves 20 cm near spacing\"),
        FCubusDensityLod::ResolveSubdivisionsForSpacing(80.0f, 20.0f),
        4
    );
    TestEqual(
        TEXT(\"80 cm canonical terrain resolves 40 cm middle spacing\"),
        FCubusDensityLod::ResolveSubdivisionsForSpacing(80.0f, 40.0f),
        2
    );
    TestEqual(
        TEXT(\"80 cm canonical terrain keeps 80 cm far spacing\"),
        FCubusDensityLod::ResolveSubdivisionsForSpacing(80.0f, 80.0f),
        1
    );

    TestEqual(
        TEXT(\"Fine density changes mesh spacing, not canonical voxel identity\"),
        FCubusDensityLod::GetSampleSpacing(80.0f, 4),
        20.0f
    );
"""
if test_text.count(old_test) != 1:
    raise RuntimeError("density LOD test anchor not found exactly once")
test_path.write_text(test_text.replace(old_test, new_test, 1), encoding="utf-8")

# Static architectural guards.
lod = Path("Source/Orakai/CubusCore/Meshing/CubusDensityLod.h").read_text(encoding="utf-8")
terrain = Path("Source/Orakai/CubusCore/Generation/CubusTerrainForm.cpp").read_text(encoding="utf-8")
density = Path("Source/Orakai/CubusCore/Generation/CubusTerrainDensityField.cpp").read_text(encoding="utf-8")
seeds = seed_path.read_text(encoding="utf-8")
tests = test_path.read_text(encoding="utf-8")

if "MaximumRuntimeSubdivisions = 4" not in lod:
    raise RuntimeError("adaptive density runtime cap is missing")
if "return 1;" not in lod or "return 2;" not in lod or "return 4;" not in lod:
    raise RuntimeError("power-of-two density tiers are missing")
for forbidden in ["BranchNetwork", "HeadwaterNotches"]:
    if forbidden in terrain:
        raise RuntimeError(f"v27 terrain artifact remains: {forbidden}")
for forbidden in ["CliffRibDisplacement", "AlcoveCut", "TalusDisplacement"]:
    if forbidden in density:
        raise RuntimeError(f"v27 density artifact remains: {forbidden}")
if "const FSurfaceData Surface = GetCachedSurfaceData" not in density:
    raise RuntimeError("SurfaceCache reference-lifetime crash fix was lost")
if "CurrentGenerationVersion = 28" not in seeds:
    raise RuntimeError("generation v28 missing")
if "80 cm canonical terrain resolves 20 cm near spacing" not in tests:
    raise RuntimeError("80 cm adaptive-density coverage missing")
