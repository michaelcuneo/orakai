from pathlib import Path

# Trigger v25 source patch from the already-installed workflow definition.

def replace_once(path: str, old: str, new: str) -> None:
    p = Path(path)
    text = p.read_text(encoding="utf-8")
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected exactly one anchor in {path}, found {count}: {old!r}")
    p.write_text(text.replace(old, new, 1), encoding="utf-8")


# GetColumnData keeps the centre surface alive while probing neighbouring
# columns. Those probes can insert into SurfaceCache and rehash the TMap, so a
# reference into it is not stable. Copy the small surface record before any
# neighbour insertion.
replace_once(
    "Source/Orakai/CubusCore/Generation/CubusTerrainDensityField.cpp",
    "\tconst FSurfaceData& Surface = GetCachedSurfaceData(WorldSampleX, WorldSampleY);\n",
    "\t// Copy before neighbour probes: GetCachedSurfaceVoxelHeight() can insert\n"
    "\t// into SurfaceCache and rehash its TMap, invalidating references.\n"
    "\tconst FSurfaceData Surface = GetCachedSurfaceData(WorldSampleX, WorldSampleY);\n",
)

# Coarse LOD geometry is placed at TileCoordinate * ChunkSize * stride. The
# source density sample must therefore use the same absolute canonical voxel
# coordinate. The old half-tile alignment offset moved every tier into a
# different source region and, critically, shifted Z by half a coarse tile.
# At stride 64 this was 1008 canonical voxels, making the outer terrain appear
# hundreds of metres higher than the canonical LOD0 world.
replace_once(
    "Source/Orakai/CubusCore/Actors/CubusTerrainLodWorldActor.cpp",
    "\tconst double AlignmentOffset = static_cast<double>(Cubus::ChunkSize) * 0.5 * static_cast<double>(SafeStride - 1);\n\n"
    "\tconst FVector SourceOrigin =\n"
    "\t\tLogicalOrigin * static_cast<double>(SafeStride) - FVector(AlignmentOffset, AlignmentOffset, AlignmentOffset);\n",
    "\t/*\n"
    "\t * Coarse tiles must be a lower-resolution view of the exact same absolute\n"
    "\t * density field as LOD0. No centring offset belongs here: the component is\n"
    "\t * already placed at TileCoordinate * ChunkSize * stride in world space.\n"
    "\t * Offsetting source X/Y sampled a different landscape in every LOD tier;\n"
    "\t * offsetting Z lifted the outer rings by half a coarse tile (1008 canonical\n"
    "\t * voxels at stride 64), producing the false mountain/snow horizon ring.\n"
    "\t */\n"
    "\tconst FVector SourceOrigin = LogicalOrigin * static_cast<double>(SafeStride);\n",
)

# Explicit cache identity for the corrected terrain/LOD relationship.
replace_once(
    "Source/Orakai/CubusCore/Generation/CubusGenerationSeeds.h",
    "    static constexpr uint32 CurrentGenerationVersion = 24;\n",
    "    static constexpr uint32 CurrentGenerationVersion = 25;\n",
)

# Update the nearby explanatory comment independently of its previous wording.
seeds_path = Path("Source/Orakai/CubusCore/Generation/CubusGenerationSeeds.h")
seeds_text = seeds_path.read_text(encoding="utf-8")
old_comment = (
    "    // Bumped to 24: legacy cellular mesa landmarks no longer stamp arbitrary\n"
    "    // height offsets into otherwise coherent mountain-valley terrain. Large\n"
    "    // natural features now come from the shared geomorphology hierarchy.\n"
)
new_comment = (
    "    // Bumped to 25: coarse terrain LOD now samples the exact absolute canonical\n"
    "    // density coordinates used by LOD0; the old half-tile XYZ offset created a\n"
    "    // false elevated mountain/snow ring at the outer terrain tiers.\n"
)
if old_comment in seeds_text:
    seeds_text = seeds_text.replace(old_comment, new_comment, 1)
seeds_path.write_text(seeds_text, encoding="utf-8")

# Static sanity guards.
density = Path("Source/Orakai/CubusCore/Generation/CubusTerrainDensityField.cpp").read_text(encoding="utf-8")
lod = Path("Source/Orakai/CubusCore/Actors/CubusTerrainLodWorldActor.cpp").read_text(encoding="utf-8")
seeds = Path("Source/Orakai/CubusCore/Generation/CubusGenerationSeeds.h").read_text(encoding="utf-8")

if "const FSurfaceData& Surface = GetCachedSurfaceData(WorldSampleX, WorldSampleY);" in density:
    raise RuntimeError("unsafe SurfaceCache reference remains")
if "const FSurfaceData Surface = GetCachedSurfaceData(WorldSampleX, WorldSampleY);" not in density:
    raise RuntimeError("safe SurfaceCache copy missing")
if "AlignmentOffset" in lod[lod.find("FCubusTerrainLodTileBuildResult ACubusTerrainLodWorldActor::BuildTile"):]:
    raise RuntimeError("LOD BuildTile alignment offset remains")
if "const FVector SourceOrigin = LogicalOrigin * static_cast<double>(SafeStride);" not in lod:
    raise RuntimeError("absolute LOD source origin missing")
if "CurrentGenerationVersion = 25" not in seeds:
    raise RuntimeError("generation version 25 missing")
