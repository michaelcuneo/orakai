from pathlib import Path
import re

ROOT = Path('.')
H = ROOT / 'Source/Orakai/CubusCore/Actors/CubusTerrainLodWorldActor.h'
CPP = ROOT / 'Source/Orakai/CubusCore/Actors/CubusTerrainLodWorldActor.cpp'


def replace_once(path, old, new):
    text = path.read_text(encoding='utf-8')
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f'{path}: expected one exact anchor, got {count}: {old[:120]!r}')
    path.write_text(text.replace(old, new, 1), encoding='utf-8')


def regex_once(path, pattern, replacement):
    text = path.read_text(encoding='utf-8')
    out, count = re.subn(pattern, replacement, text, count=1, flags=re.S)
    if count != 1:
        raise RuntimeError(f'{path}: expected one regex anchor, got {count}: {pattern[:120]!r}')
    path.write_text(out, encoding='utf-8')


# -------------------------------------------------------------------------
# Header: add three more 2:1 visual tiers. Keeping radius 4 at each scale
# restores the old outer physical reach without exploding one tier to ~36
# tiles of radius.
# -------------------------------------------------------------------------
replace_once(
    H,
    '''\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD3", meta = (ClampMin = "0", ClampMax = "4"))\n\tint32 Lod3VerticalRadiusTiles = 0;\n\n\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|Streaming", meta = (ClampMin = "1", ClampMax = "16"))\n''',
    '''\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD3", meta = (ClampMin = "0", ClampMax = "4"))\n\tint32 Lod3VerticalRadiusTiles = 0;\n\n\t/** LOD4 continues the fixed 2:1 visual hierarchy at stride 16. */\n\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD4", meta = (ClampMin = "1", ClampMax = "16"))\n\tint32 Lod4OuterRadiusTiles = 4;\n\n\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD4", meta = (ClampMin = "0", ClampMax = "4"))\n\tint32 Lod4VerticalRadiusTiles = 0;\n\n\t/** LOD5 continues the fixed 2:1 visual hierarchy at stride 32. */\n\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD5", meta = (ClampMin = "1", ClampMax = "16"))\n\tint32 Lod5OuterRadiusTiles = 4;\n\n\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD5", meta = (ClampMin = "0", ClampMax = "4"))\n\tint32 Lod5VerticalRadiusTiles = 0;\n\n\t/** LOD6 restores the former stride-64 outer reach without a 4:1 jump. */\n\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD6", meta = (ClampMin = "1", ClampMax = "16"))\n\tint32 Lod6OuterRadiusTiles = 4;\n\n\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|LOD6", meta = (ClampMin = "0", ClampMax = "4"))\n\tint32 Lod6VerticalRadiusTiles = 0;\n\n\tUPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Terrain LOD|Streaming", meta = (ClampMin = "1", ClampMax = "16"))\n'''
)

replace_once(
    H,
    '''\tFCubusTerrainLodTierRuntime Lod1Runtime;\n\tFCubusTerrainLodTierRuntime Lod2Runtime;\n\tFCubusTerrainLodTierRuntime Lod3Runtime;\n\n\tfloat TimeUntilStreamingUpdate = 0.0f;\n''',
    '''\tFCubusTerrainLodTierRuntime Lod1Runtime;\n\tFCubusTerrainLodTierRuntime Lod2Runtime;\n\tFCubusTerrainLodTierRuntime Lod3Runtime;\n\tFCubusTerrainLodTierRuntime Lod4Runtime;\n\tFCubusTerrainLodTierRuntime Lod5Runtime;\n\tFCubusTerrainLodTierRuntime Lod6Runtime;\n\n\tfloat TimeUntilStreamingUpdate = 0.0f;\n'''
)

# -------------------------------------------------------------------------
# Constructor + diagnostics.
# -------------------------------------------------------------------------
replace_once(
    CPP,
    '''\tLod1Runtime.LodLevel = 1;\n\tLod2Runtime.LodLevel = 2;\n\tLod3Runtime.LodLevel = 3;\n''',
    '''\tLod1Runtime.LodLevel = 1;\n\tLod2Runtime.LodLevel = 2;\n\tLod3Runtime.LodLevel = 3;\n\tLod4Runtime.LodLevel = 4;\n\tLod5Runtime.LodLevel = 5;\n\tLod6Runtime.LodLevel = 6;\n'''
)

regex_once(
    CPP,
    r'''\tUE_LOG\(LogTemp, Display,\n\t\t   TEXT\("Cubus terrain LOD started:.*?MaxLodUploadsPerTick\);''',
    '''\tUE_LOG(LogTemp, Display,\n\t\tTEXT("Cubus terrain LOD started: enabled=%s fixed-strides=(2,4,8,16,32,64) outer=(%d,%d,%d,%d,%d,%d) concurrent=%d uploads=%d"),\n\t\tbEnableTerrainLod ? TEXT("true") : TEXT("false"),\n\t\tLod1OuterRadiusTiles, Lod2OuterRadiusTiles, Lod3OuterRadiusTiles,\n\t\tLod4OuterRadiusTiles, Lod5OuterRadiusTiles, Lod6OuterRadiusTiles,\n\t\tMaxConcurrentLodBuilds, MaxLodUploadsPerTick);'''
)

# -------------------------------------------------------------------------
# Tick accounting: one tier array prevents future count drift.
# -------------------------------------------------------------------------
regex_once(
    CPP,
    r'''\tLoadedLodTileCount = Lod1Runtime\.TileComponents\.Num\(\) \+ Lod2Runtime\.TileComponents\.Num\(\) \+ Lod3Runtime\.TileComponents\.Num\(\);\n\n\tBuildingLodTileCount = Lod1Runtime\.ActiveBuilds\.Num\(\) \+ Lod2Runtime\.ActiveBuilds\.Num\(\) \+ Lod3Runtime\.ActiveBuilds\.Num\(\);\n\n\tPendingLodTileCount = Lod1Runtime\.PendingTiles\.Num\(\) \+ Lod2Runtime\.PendingTiles\.Num\(\) \+ Lod3Runtime\.PendingTiles\.Num\(\);''',
    '''\tFCubusTerrainLodTierRuntime* Tiers[] =\n\t{\n\t\t&Lod1Runtime, &Lod2Runtime, &Lod3Runtime,\n\t\t&Lod4Runtime, &Lod5Runtime, &Lod6Runtime\n\t};\n\n\tLoadedLodTileCount = 0;\n\tBuildingLodTileCount = 0;\n\tPendingLodTileCount = 0;\n\tfor (const FCubusTerrainLodTierRuntime* Tier : Tiers)\n\t{\n\t\tLoadedLodTileCount += Tier->TileComponents.Num();\n\t\tBuildingLodTileCount += Tier->ActiveBuilds.Num();\n\t\tPendingLodTileCount += Tier->PendingTiles.Num();\n\t}'''
)

# -------------------------------------------------------------------------
# Physical hierarchy. Six constant-radius 2:1 tiers reach the same stride-64
# outer scale as the old 4/16/64 hierarchy. At 80 cm voxels and radius 4,
# LOD6 reaches 288 canonical chunks ~= 7.37 km from the grid origin.
# -------------------------------------------------------------------------
regex_once(
    CPP,
    r'''\t/\*\n\t \* Keep every visual tier exactly one 2:1 sampling step from the previous.*?\n\tUpdateTierStreaming\(Lod3Runtime, StreamingGridLocation, DensityField, CanonicalChunkWorldSize, Lod2HalfExtentCanonicalChunks,\n\t\tSafeLod3Stride, 0, Lod3OuterRadiusTiles, Lod3VerticalRadiusTiles\);''',
    '''\t/*\n\t * Keep every visual tier exactly one 2:1 sampling step from the previous\n\t * tier, without sacrificing the original world draw distance. The previous\n\t * three-tier 4/16/64 layout reached roughly 288 canonical chunks with the\n\t * default radius. The six-tier 2/4/8/16/32/64 chain reaches the same outer\n\t * scale while every boundary remains a Transvoxel-compatible 2:1 step.\n\t */\n\tstruct FTierUpdate\n\t{\n\t\tFCubusTerrainLodTierRuntime* Runtime;\n\t\tint32 Stride;\n\t\tint32 OuterRadius;\n\t\tint32 VerticalRadius;\n\t};\n\n\tFTierUpdate TierUpdates[] =\n\t{\n\t\t{ &Lod1Runtime, 2,  Lod1OuterRadiusTiles, Lod1VerticalRadiusTiles },\n\t\t{ &Lod2Runtime, 4,  Lod2OuterRadiusTiles, Lod2VerticalRadiusTiles },\n\t\t{ &Lod3Runtime, 8,  Lod3OuterRadiusTiles, Lod3VerticalRadiusTiles },\n\t\t{ &Lod4Runtime, 16, Lod4OuterRadiusTiles, Lod4VerticalRadiusTiles },\n\t\t{ &Lod5Runtime, 32, Lod5OuterRadiusTiles, Lod5VerticalRadiusTiles },\n\t\t{ &Lod6Runtime, 64, Lod6OuterRadiusTiles, Lod6VerticalRadiusTiles }\n\t};\n\n\tdouble PreviousHalfExtentCanonicalChunks = Lod0HalfExtentCanonicalChunks;\n\tfor (const FTierUpdate& TierUpdate : TierUpdates)\n\t{\n\t\tUpdateTierStreaming(\n\t\t\t*TierUpdate.Runtime,\n\t\t\tStreamingGridLocation,\n\t\t\tDensityField,\n\t\t\tCanonicalChunkWorldSize,\n\t\t\tPreviousHalfExtentCanonicalChunks,\n\t\t\tTierUpdate.Stride,\n\t\t\t0,\n\t\t\tTierUpdate.OuterRadius,\n\t\t\tTierUpdate.VerticalRadius\n\t\t);\n\n\t\tconst int32 ResolvedOuterRadius = FMath::Max(\n\t\t\tTierUpdate.Runtime->InnerRadiusTiles + 1,\n\t\t\tTierUpdate.Runtime->OuterRadiusTiles\n\t\t);\n\t\tPreviousHalfExtentCanonicalChunks =\n\t\t\t(static_cast<double>(ResolvedOuterRadius) + 0.5) *\n\t\t\tstatic_cast<double>(TierUpdate.Stride);\n\t}'''
)

# -------------------------------------------------------------------------
# Collect completion across all six tiers.
# -------------------------------------------------------------------------
replace_once(
    CPP,
    '''void ACubusTerrainLodWorldActor::CollectCompletedBuilds()\n{\n\tCollectCompletedBuildsForTier(Lod1Runtime);\n\tCollectCompletedBuildsForTier(Lod2Runtime);\n\tCollectCompletedBuildsForTier(Lod3Runtime);\n}\n''',
    '''void ACubusTerrainLodWorldActor::CollectCompletedBuilds()\n{\n\tFCubusTerrainLodTierRuntime* Tiers[] =\n\t{\n\t\t&Lod1Runtime, &Lod2Runtime, &Lod3Runtime,\n\t\t&Lod4Runtime, &Lod5Runtime, &Lod6Runtime\n\t};\n\tfor (FCubusTerrainLodTierRuntime* Tier : Tiers)\n\t{\n\t\tCollectCompletedBuildsForTier(*Tier);\n\t}\n}\n'''
)

# -------------------------------------------------------------------------
# StartPendingBuilds: replace hard-coded three-tier checks/selection with one
# ordered array. Inner/finer tiers remain priority.
# -------------------------------------------------------------------------
replace_once(
    CPP,
    '''\tif (Lod1Runtime.PendingTiles.IsEmpty() && Lod2Runtime.PendingTiles.IsEmpty() && Lod3Runtime.PendingTiles.IsEmpty())\n\t{\n\t\treturn;\n\t}\n''',
    '''\tFCubusTerrainLodTierRuntime* Tiers[] =\n\t{\n\t\t&Lod1Runtime, &Lod2Runtime, &Lod3Runtime,\n\t\t&Lod4Runtime, &Lod5Runtime, &Lod6Runtime\n\t};\n\n\tbool bHasPendingTiles = false;\n\tfor (const FCubusTerrainLodTierRuntime* Tier : Tiers)\n\t{\n\t\tbHasPendingTiles |= !Tier->PendingTiles.IsEmpty();\n\t}\n\tif (!bHasPendingTiles)\n\t{\n\t\treturn;\n\t}\n'''
)

replace_once(
    CPP,
    '''\t\tconst int32 ActiveBuildCount = Lod1Runtime.ActiveBuilds.Num() + Lod2Runtime.ActiveBuilds.Num() + Lod3Runtime.ActiveBuilds.Num();\n''',
    '''\t\tint32 ActiveBuildCount = 0;\n\t\tfor (const FCubusTerrainLodTierRuntime* CandidateTier : Tiers)\n\t\t{\n\t\t\tActiveBuildCount += CandidateTier->ActiveBuilds.Num();\n\t\t}\n'''
)

regex_once(
    CPP,
    r'''\t\tFCubusTerrainLodTierRuntime\* Tier = nullptr;\n\n\t\tif \(!Lod1Runtime\.PendingTiles\.IsEmpty\(\)\).*?\n\t\telse\n\t\t\{\n\t\t\tbreak;\n\t\t\}\n''',
    '''\t\tFCubusTerrainLodTierRuntime* Tier = nullptr;\n\t\tfor (FCubusTerrainLodTierRuntime* CandidateTier : Tiers)\n\t\t{\n\t\t\tif (!CandidateTier->PendingTiles.IsEmpty())\n\t\t\t{\n\t\t\t\tTier = CandidateTier;\n\t\t\t\tbreak;\n\t\t\t}\n\t\t}\n\t\tif (Tier == nullptr)\n\t\t{\n\t\t\tbreak;\n\t\t}\n'''
)

# -------------------------------------------------------------------------
# Upload completion: all tiers share the same ordered dispatch and retirement.
# -------------------------------------------------------------------------
replace_once(
    CPP,
    '''\tif (Lod1Runtime.CompletedBuilds.IsEmpty() && Lod2Runtime.CompletedBuilds.IsEmpty() && Lod3Runtime.CompletedBuilds.IsEmpty())\n\t{\n\t\treturn;\n\t}\n''',
    '''\tFCubusTerrainLodTierRuntime* Tiers[] =\n\t{\n\t\t&Lod1Runtime, &Lod2Runtime, &Lod3Runtime,\n\t\t&Lod4Runtime, &Lod5Runtime, &Lod6Runtime\n\t};\n\n\tbool bHasCompletedBuilds = false;\n\tfor (const FCubusTerrainLodTierRuntime* Tier : Tiers)\n\t{\n\t\tbHasCompletedBuilds |= !Tier->CompletedBuilds.IsEmpty();\n\t}\n\tif (!bHasCompletedBuilds)\n\t{\n\t\treturn;\n\t}\n'''
)

regex_once(
    CPP,
    r'''\t\tbool bUploaded = false;\n\n\t\tif \(!Lod1Runtime\.CompletedBuilds\.IsEmpty\(\)\).*?\n\t\telse\n\t\t\{\n\t\t\tbreak;\n\t\t\}\n''',
    '''\t\tbool bUploaded = false;\n\t\tbool bFoundCompletedTier = false;\n\t\tfor (FCubusTerrainLodTierRuntime* Tier : Tiers)\n\t\t{\n\t\t\tif (Tier->CompletedBuilds.IsEmpty())\n\t\t\t{\n\t\t\t\tcontinue;\n\t\t\t}\n\t\t\tbFoundCompletedTier = true;\n\t\t\tbUploaded = UploadOneCompletedBuild(\n\t\t\t\t*Tier, CanonicalVoxelSize, TerrainMaterial, WorkerMillisecondsThisTick\n\t\t\t);\n\t\t\tbreak;\n\t\t}\n\t\tif (!bFoundCompletedTier)\n\t\t{\n\t\t\tbreak;\n\t\t}\n'''
)

replace_once(
    CPP,
    '''\tRetireStaleTilesIfResident(Lod1Runtime);\n\tRetireStaleTilesIfResident(Lod2Runtime);\n\tRetireStaleTilesIfResident(Lod3Runtime);\n''',
    '''\tfor (FCubusTerrainLodTierRuntime* Tier : Tiers)\n\t{\n\t\tRetireStaleTilesIfResident(*Tier);\n\t}\n'''
)

regex_once(
    CPP,
    r'''\tif \(UploadedThisTick > 0\)\n\t\{\n\t\tconst double UploadMilliseconds = \(FPlatformTime::Seconds\(\) - UploadTickStart\) \* 1000\.0;\n\n\t\tUE_LOG\(LogTemp, Display,.*?\n\t\}\n\}\n\nbool ACubusTerrainLodWorldActor::UploadOneCompletedBuild''',
    '''\tif (UploadedThisTick > 0)\n\t{\n\t\tconst double UploadMilliseconds = (FPlatformTime::Seconds() - UploadTickStart) * 1000.0;\n\t\tint32 LoadedCount = 0;\n\t\tint32 CompletedCount = 0;\n\t\tint32 ActiveCount = 0;\n\t\tint32 PendingCount = 0;\n\t\tfor (const FCubusTerrainLodTierRuntime* Tier : Tiers)\n\t\t{\n\t\t\tLoadedCount += Tier->TileComponents.Num();\n\t\t\tCompletedCount += Tier->CompletedBuilds.Num();\n\t\t\tActiveCount += Tier->ActiveBuilds.Num();\n\t\t\tPendingCount += Tier->PendingTiles.Num();\n\t\t}\n\n\t\tUE_LOG(LogTemp, Display,\n\t\t\tTEXT("Cubus terrain LOD upload: tiles=%d worker=%.2fms upload=%.2fms loaded=%d completed=%d building=%d pending=%d"),\n\t\t\tUploadedThisTick, WorkerMillisecondsThisTick, UploadMilliseconds,\n\t\t\tLoadedCount, CompletedCount, ActiveCount, PendingCount);\n\t}\n}\n\nbool ACubusTerrainLodWorldActor::UploadOneCompletedBuild'''
)

# -------------------------------------------------------------------------
# Clear all six tiers.
# -------------------------------------------------------------------------
replace_once(
    CPP,
    '''void ACubusTerrainLodWorldActor::ClearAllTiles()\n{\n\tClearTier(Lod1Runtime);\n\tClearTier(Lod2Runtime);\n\tClearTier(Lod3Runtime);\n\n\tLoadedLodTileCount\t = 0;\n''',
    '''void ACubusTerrainLodWorldActor::ClearAllTiles()\n{\n\tFCubusTerrainLodTierRuntime* Tiers[] =\n\t{\n\t\t&Lod1Runtime, &Lod2Runtime, &Lod3Runtime,\n\t\t&Lod4Runtime, &Lod5Runtime, &Lod6Runtime\n\t};\n\tfor (FCubusTerrainLodTierRuntime* Tier : Tiers)\n\t{\n\t\tClearTier(*Tier);\n\t}\n\n\tLoadedLodTileCount\t = 0;\n'''
)

# -------------------------------------------------------------------------
# Guards: distance chain, all six runtime owners, and no old 3-tier-only sums.
# -------------------------------------------------------------------------
h = H.read_text(encoding='utf-8')
cpp = CPP.read_text(encoding='utf-8')
for token in [
    'Lod4OuterRadiusTiles = 4;', 'Lod5OuterRadiusTiles = 4;', 'Lod6OuterRadiusTiles = 4;',
    'FCubusTerrainLodTierRuntime Lod4Runtime;', 'FCubusTerrainLodTierRuntime Lod5Runtime;', 'FCubusTerrainLodTierRuntime Lod6Runtime;'
]:
    if token not in h:
        raise RuntimeError(f'header guard missing {token}')
for token in [
    'Lod6Runtime.LodLevel = 6;',
    '{ &Lod4Runtime, 16, Lod4OuterRadiusTiles, Lod4VerticalRadiusTiles }',
    '{ &Lod5Runtime, 32, Lod5OuterRadiusTiles, Lod5VerticalRadiusTiles }',
    '{ &Lod6Runtime, 64, Lod6OuterRadiusTiles, Lod6VerticalRadiusTiles }',
    'PreviousHalfExtentCanonicalChunks',
    '&Lod4Runtime, &Lod5Runtime, &Lod6Runtime'
]:
    if token not in cpp:
        raise RuntimeError(f'cpp guard missing {token}')
if 'Lod1Runtime.ActiveBuilds.Num() + Lod2Runtime.ActiveBuilds.Num() + Lod3Runtime.ActiveBuilds.Num()' in cpp:
    raise RuntimeError('three-tier-only active build accounting survived')
