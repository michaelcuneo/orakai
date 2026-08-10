from pathlib import Path


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected 1 match, found {count}")
    return text.replace(old, new, 1)


def function_region(text: str, start_marker: str, end_marker: str) -> tuple[int, int, str]:
    start = text.index(start_marker)
    end = text.index(end_marker, start)
    return start, end, text[start:end]


header_path = Path("Source/Orakai/CubusCore/Actors/CubusTerrainLodWorldActor.h")
header = header_path.read_text(encoding="utf-8")

if "Lod3CanonicalVoxelStride" not in header:
    header = replace_once(
        header,
        "    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = \"Cubus|Terrain LOD|LOD2\", meta = (ClampMin = \"0\", ClampMax = \"4\"))\n    int32 Lod2VerticalRadiusTiles = 0;\n",
        "    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = \"Cubus|Terrain LOD|LOD2\", meta = (ClampMin = \"0\", ClampMax = \"4\"))\n    int32 Lod2VerticalRadiusTiles = 0;\n\n    /** LOD3 samples one point every sixty-four canonical voxels. */\n    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = \"Cubus|Terrain LOD|LOD3\", meta = (ClampMin = \"16\", ClampMax = \"256\"))\n    int32 Lod3CanonicalVoxelStride = 64;\n\n    /** Number of LOD3 tiles allowed to overlap the outer edge of LOD2. */\n    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = \"Cubus|Terrain LOD|LOD3\", meta = (ClampMin = \"0\", ClampMax = \"2\"))\n    int32 Lod3OverlapTiles = 1;\n\n    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = \"Cubus|Terrain LOD|LOD3\", meta = (ClampMin = \"1\", ClampMax = \"32\"))\n    int32 Lod3OuterRadiusTiles = 4;\n\n    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = \"Cubus|Terrain LOD|LOD3\", meta = (ClampMin = \"0\", ClampMax = \"4\"))\n    int32 Lod3VerticalRadiusTiles = 0;\n",
        "header LOD3 settings",
    )

if "FCubusTerrainLodTierRuntime Lod3Runtime;" not in header:
    header = replace_once(
        header,
        "    FCubusTerrainLodTierRuntime Lod2Runtime;\n",
        "    FCubusTerrainLodTierRuntime Lod2Runtime;\n    FCubusTerrainLodTierRuntime Lod3Runtime;\n",
        "header LOD3 runtime",
    )

header_path.write_text(header, encoding="utf-8")

cpp_path = Path("Source/Orakai/CubusCore/Actors/CubusTerrainLodWorldActor.cpp")
cpp = cpp_path.read_text(encoding="utf-8")

if "Lod3Runtime.LodLevel = 3;" not in cpp:
    cpp = replace_once(
        cpp,
        "    Lod2Runtime.LodLevel = 2;\n",
        "    Lod2Runtime.LodLevel = 2;\n    Lod3Runtime.LodLevel = 3;\n",
        "constructor LOD3 level",
    )

begin_start, begin_end, begin = function_region(
    cpp,
    "void ACubusTerrainLodWorldActor::BeginPlay()",
    "void ACubusTerrainLodWorldActor::Tick(",
)
if "lod3=(stride=%d" not in begin:
    begin = replace_once(
        begin,
        "lod2=(stride=%d overlap=%d outer=%d vertical=%d) concurrent=%d uploads=%d",
        "lod2=(stride=%d overlap=%d outer=%d vertical=%d) lod3=(stride=%d overlap=%d outer=%d vertical=%d) concurrent=%d uploads=%d",
        "startup log format",
    )
    begin = replace_once(
        begin,
        "        Lod2VerticalRadiusTiles,\n        MaxConcurrentLodBuilds,\n",
        "        Lod2VerticalRadiusTiles,\n        Lod3CanonicalVoxelStride,\n        Lod3OverlapTiles,\n        Lod3OuterRadiusTiles,\n        Lod3VerticalRadiusTiles,\n        MaxConcurrentLodBuilds,\n",
        "startup log args",
    )
    cpp = cpp[:begin_start] + begin + cpp[begin_end:]

# Aggregate diagnostic counters include all coarse tiers.
tick_start, tick_end, tick = function_region(
    cpp,
    "void ACubusTerrainLodWorldActor::Tick(const float DeltaSeconds)",
    "void ACubusTerrainLodWorldActor::EndPlay(",
)
if "Lod3Runtime.TileComponents.Num()" not in tick:
    tick = replace_once(
        tick,
        "    LoadedLodTileCount =\n        Lod1Runtime.TileComponents.Num() +\n        Lod2Runtime.TileComponents.Num();\n",
        "    LoadedLodTileCount =\n        Lod1Runtime.TileComponents.Num() +\n        Lod2Runtime.TileComponents.Num() +\n        Lod3Runtime.TileComponents.Num();\n",
        "loaded diagnostics",
    )
    tick = replace_once(
        tick,
        "    BuildingLodTileCount =\n        Lod1Runtime.ActiveBuilds.Num() +\n        Lod2Runtime.ActiveBuilds.Num();\n",
        "    BuildingLodTileCount =\n        Lod1Runtime.ActiveBuilds.Num() +\n        Lod2Runtime.ActiveBuilds.Num() +\n        Lod3Runtime.ActiveBuilds.Num();\n",
        "building diagnostics",
    )
    tick = replace_once(
        tick,
        "    PendingLodTileCount =\n        Lod1Runtime.PendingTiles.Num() +\n        Lod2Runtime.PendingTiles.Num();\n",
        "    PendingLodTileCount =\n        Lod1Runtime.PendingTiles.Num() +\n        Lod2Runtime.PendingTiles.Num() +\n        Lod3Runtime.PendingTiles.Num();\n",
        "pending diagnostics",
    )
    cpp = cpp[:tick_start] + tick + cpp[tick_end:]

# LOD3 starts outside LOD2's physical outer extent.
update_start, update_end, update = function_region(
    cpp,
    "void ACubusTerrainLodWorldActor::UpdateStreaming()",
    "void ACubusTerrainLodWorldActor::UpdateTierStreaming(",
)
if "Lod3Runtime," not in update:
    update = replace_once(
        update,
        "            SafeLod1Stride + 1,\n            256\n",
        "            SafeLod1Stride + 1,\n            255\n",
        "reserve stride 256 for LOD3",
    )

    lod2_start = update.index("    UpdateTierStreaming(\n        Lod2Runtime,")
    lod2_end = update.index("\n    );", lod2_start) + len("\n    );")
    lod3_block = """

    const int32 SafeLod2OuterRadius =
        FMath::Max(
            Lod2Runtime.InnerRadiusTiles + 1,
            Lod2Runtime.OuterRadiusTiles
        );

    const double Lod2HalfExtentCanonicalChunks =
        (
            static_cast<double>(SafeLod2OuterRadius) +
            0.5
        ) *
        static_cast<double>(SafeLod2Stride);

    const int32 SafeLod3Stride =
        FMath::Clamp(
            Lod3CanonicalVoxelStride,
            SafeLod2Stride + 1,
            256
        );

    UpdateTierStreaming(
        Lod3Runtime,
        StreamingGridLocation,
        CanonicalChunkWorldSize,
        Lod2HalfExtentCanonicalChunks,
        SafeLod3Stride,
        Lod3OverlapTiles,
        Lod3OuterRadiusTiles,
        Lod3VerticalRadiusTiles
    );"""
    update = update[:lod2_end] + lod3_block + update[lod2_end:]
    cpp = cpp[:update_start] + update + cpp[update_end:]

collect_start, collect_end, collect = function_region(
    cpp,
    "void ACubusTerrainLodWorldActor::CollectCompletedBuilds()",
    "void ACubusTerrainLodWorldActor::CollectCompletedBuildsForTier(",
)
if "CollectCompletedBuildsForTier(Lod3Runtime);" not in collect:
    collect = replace_once(
        collect,
        "    CollectCompletedBuildsForTier(Lod2Runtime);\n",
        "    CollectCompletedBuildsForTier(Lod2Runtime);\n    CollectCompletedBuildsForTier(Lod3Runtime);\n",
        "collect LOD3",
    )
    cpp = cpp[:collect_start] + collect + cpp[collect_end:]

start_start, start_end, start = function_region(
    cpp,
    "void ACubusTerrainLodWorldActor::StartPendingBuilds()",
    "void ACubusTerrainLodWorldActor::UploadCompletedBuilds()",
)
if "Lod3Runtime.PendingTiles.IsEmpty()" not in start:
    start = replace_once(
        start,
        "        Lod1Runtime.PendingTiles.IsEmpty() &&\n        Lod2Runtime.PendingTiles.IsEmpty()\n",
        "        Lod1Runtime.PendingTiles.IsEmpty() &&\n        Lod2Runtime.PendingTiles.IsEmpty() &&\n        Lod3Runtime.PendingTiles.IsEmpty()\n",
        "pending LOD3 condition",
    )
    start = replace_once(
        start,
        "            Lod1Runtime.ActiveBuilds.Num() +\n            Lod2Runtime.ActiveBuilds.Num();\n",
        "            Lod1Runtime.ActiveBuilds.Num() +\n            Lod2Runtime.ActiveBuilds.Num() +\n            Lod3Runtime.ActiveBuilds.Num();\n",
        "shared active build budget",
    )
    start = replace_once(
        start,
        "        else if (!Lod2Runtime.PendingTiles.IsEmpty())\n        {\n            Tier = &Lod2Runtime;\n        }\n        else\n",
        "        else if (!Lod2Runtime.PendingTiles.IsEmpty())\n        {\n            Tier = &Lod2Runtime;\n        }\n        else if (!Lod3Runtime.PendingTiles.IsEmpty())\n        {\n            Tier = &Lod3Runtime;\n        }\n        else\n",
        "LOD3 build priority",
    )
    cpp = cpp[:start_start] + start + cpp[start_end:]

upload_start, upload_end, upload = function_region(
    cpp,
    "void ACubusTerrainLodWorldActor::UploadCompletedBuilds()",
    "bool ACubusTerrainLodWorldActor::UploadOneCompletedBuild(",
)
if "Lod3Runtime.CompletedBuilds.IsEmpty()" not in upload:
    upload = replace_once(
        upload,
        "        Lod1Runtime.CompletedBuilds.IsEmpty() &&\n        Lod2Runtime.CompletedBuilds.IsEmpty()\n",
        "        Lod1Runtime.CompletedBuilds.IsEmpty() &&\n        Lod2Runtime.CompletedBuilds.IsEmpty() &&\n        Lod3Runtime.CompletedBuilds.IsEmpty()\n",
        "completed LOD3 condition",
    )
    upload = replace_once(
        upload,
        "        else if (!Lod2Runtime.CompletedBuilds.IsEmpty())\n        {\n            bUploaded = UploadOneCompletedBuild(\n                Lod2Runtime,\n                CanonicalVoxelSize,\n                TerrainMaterial,\n                WorkerMillisecondsThisTick\n            );\n        }\n        else\n",
        "        else if (!Lod2Runtime.CompletedBuilds.IsEmpty())\n        {\n            bUploaded = UploadOneCompletedBuild(\n                Lod2Runtime,\n                CanonicalVoxelSize,\n                TerrainMaterial,\n                WorkerMillisecondsThisTick\n            );\n        }\n        else if (!Lod3Runtime.CompletedBuilds.IsEmpty())\n        {\n            bUploaded = UploadOneCompletedBuild(\n                Lod3Runtime,\n                CanonicalVoxelSize,\n                TerrainMaterial,\n                WorkerMillisecondsThisTick\n            );\n        }\n        else\n",
        "LOD3 upload priority",
    )
    upload = replace_once(
        upload,
        "    RetireStaleTilesIfResident(Lod2Runtime);\n",
        "    RetireStaleTilesIfResident(Lod2Runtime);\n    RetireStaleTilesIfResident(Lod3Runtime);\n",
        "LOD3 make-before-break retirement",
    )
    upload = replace_once(
        upload,
        "loaded=(lod1=%d lod2=%d) completed=(%d,%d) building=(%d,%d) pending=(%d,%d)",
        "loaded=(lod1=%d lod2=%d lod3=%d) completed=(%d,%d,%d) building=(%d,%d,%d) pending=(%d,%d,%d)",
        "upload log format",
    )
    upload = replace_once(
        upload,
        "            Lod1Runtime.TileComponents.Num(),\n            Lod2Runtime.TileComponents.Num(),\n            Lod1Runtime.CompletedBuilds.Num(),\n            Lod2Runtime.CompletedBuilds.Num(),\n            Lod1Runtime.ActiveBuilds.Num(),\n            Lod2Runtime.ActiveBuilds.Num(),\n            Lod1Runtime.PendingTiles.Num(),\n            Lod2Runtime.PendingTiles.Num()\n",
        "            Lod1Runtime.TileComponents.Num(),\n            Lod2Runtime.TileComponents.Num(),\n            Lod3Runtime.TileComponents.Num(),\n            Lod1Runtime.CompletedBuilds.Num(),\n            Lod2Runtime.CompletedBuilds.Num(),\n            Lod3Runtime.CompletedBuilds.Num(),\n            Lod1Runtime.ActiveBuilds.Num(),\n            Lod2Runtime.ActiveBuilds.Num(),\n            Lod3Runtime.ActiveBuilds.Num(),\n            Lod1Runtime.PendingTiles.Num(),\n            Lod2Runtime.PendingTiles.Num(),\n            Lod3Runtime.PendingTiles.Num()\n",
        "upload log args",
    )
    cpp = cpp[:upload_start] + upload + cpp[upload_end:]

clear_start, clear_end, clear = function_region(
    cpp,
    "void ACubusTerrainLodWorldActor::ClearAllTiles()",
    "void ACubusTerrainLodWorldActor::ClearTier(",
)
if "ClearTier(Lod3Runtime);" not in clear:
    clear = replace_once(
        clear,
        "    ClearTier(Lod2Runtime);\n",
        "    ClearTier(Lod2Runtime);\n    ClearTier(Lod3Runtime);\n",
        "clear LOD3",
    )
    cpp = cpp[:clear_start] + clear + cpp[clear_end:]

cpp_path.write_text(cpp, encoding="utf-8")
