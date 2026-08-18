#!/usr/bin/env python3
"""Prepare a broad LINZ 1 m DEM library for Orakai.

The source set deliberately spans coastal, alpine, volcanic, river-valley,
rolling-lowland and rugged hill-country terrain. Each collection is optional:
a retired/unavailable STAC collection is reported and skipped so one source can
never prevent the rest of the terrain library from being built.

Run from the Orakai repository root with the active Python environment:

    python Tools/DemLibrary/prepare_linz_library.py

The default build now targets three usable source tiles and three ranked 1 km
patches per tile for every configured collection. Use smaller values for a quick
smoke test or larger ones to grow the shipping source library.
"""

from __future__ import annotations

import argparse
import json
import pathlib
from dataclasses import dataclass

import analyze_dem_library as analyzer
import prepare_linz_coastal as prep


@dataclass(frozen=True)
class SourceSpec:
    key: str
    folder: str
    title: str
    collection_url: str
    domain: str


COASTAL_ROOT = "https://nz-coastal.s3-ap-southeast-2.amazonaws.com"
ELEVATION_ROOT = "https://nz-elevation.s3-ap-southeast-2.amazonaws.com"


def land_source(key: str, folder: str, title: str, region: str, collection: str, domain: str = "land") -> SourceSpec:
    return SourceSpec(
        key=key,
        folder=folder,
        title=title,
        collection_url=f"{ELEVATION_ROOT}/{region}/{collection}/dem_1m/2193/collection.json",
        domain=domain,
    )


SOURCES: dict[str, SourceSpec] = {
    # Coastal LiDAR with bathymetric/coastal context where available.
    "waiheke": SourceSpec(
        "waiheke", "Waiheke", "Auckland - Waiheke Island Coastal LiDAR 1m DEM (2025-2026)",
        f"{COASTAL_ROOT}/auckland/waiheke-island_2025-2026/dem_1m/2193/collection.json", "coastal"),
    "whangarei": SourceSpec(
        "whangarei", "WhangareiBreamBay", "Northland - Whangarei to Bream Bay Coastal LiDAR 1m DEM (2025)",
        f"{COASTAL_ROOT}/northland/whangarei-to-bream-bay_2025/dem_1m/2193/collection.json", "coastal"),
    "bluff": SourceSpec(
        "bluff", "Bluff", "Southland - Bluff Coastal LiDAR 1m DEM (2025)",
        f"{COASTAL_ROOT}/southland/bluff_2025/dem_1m/2193/collection.json", "coastal"),
    "dunedin_coast": SourceSpec(
        "dunedin_coast", "DunedinCoast", "Otago - Dunedin Coastal LiDAR 1m DEM (2025)",
        f"{COASTAL_ROOT}/otago/dunedin_2025/dem_1m/2193/collection.json", "coastal"),
    "okuru": SourceSpec(
        "okuru", "Okuru", "West Coast - Okuru Coastal LiDAR 1m DEM (2026)",
        f"{COASTAL_ROOT}/west-coast/okuru_2026/dem_1m/2193/collection.json", "coastal"),

    # North Island hill-country, volcanic and coastal terrain.
    "northland_2024": land_source("northland_2024", "Northland2024", "Northland LiDAR 1m DEM (2024)", "northland", "northland_2024"),
    "whangarei_heads": land_source("whangarei_heads", "WhangareiHeads", "Whangarei Heads LiDAR 1m DEM (2016)", "northland", "whangarei-heads_2016", "coastal"),
    "taranaki": land_source("taranaki", "Taranaki", "Taranaki LiDAR 1m DEM (2021)", "taranaki", "taranaki_2021", "volcanic"),
    "bay_of_plenty": land_source("bay_of_plenty", "BayOfPlenty", "Bay of Plenty LiDAR 1m DEM (2024)", "bay-of-plenty", "bay-of-plenty_2024", "volcanic_coastal"),
    "tauranga": land_source("tauranga", "Tauranga", "Tauranga LiDAR 1m DEM (2022)", "bay-of-plenty", "tauranga_2022", "coastal"),

    # South Island mountains, sounds, peninsulas, valleys and plains.
    "banks_peninsula": land_source("banks_peninsula", "BanksPeninsula", "Banks Peninsula LiDAR 1m DEM (2023)", "canterbury", "banks-peninsula_2023", "coastal_volcanic"),
    "canterbury": land_source("canterbury", "Canterbury", "Canterbury LiDAR 1m DEM (2020-2023)", "canterbury", "canterbury_2020-2023", "mixed"),
    "kaikoura": land_source("kaikoura", "KaikouraWaimakariri", "Kaikoura and Waimakariri LiDAR 1m DEM (2022)", "canterbury", "kaikoura-and-waimakariri_2022", "mountain_coastal"),
    "marlborough": land_source("marlborough", "Marlborough", "Marlborough LiDAR 1m DEM (2020-2022)", "marlborough", "marlborough_2020-2022", "mountain_coastal"),
    "west_coast": land_source("west_coast", "WestCoast", "West Coast LiDAR 1m DEM (2020-2022)", "west-coast", "west-coast_2020-2022", "mountain_coastal"),
    "abel_tasman": land_source("abel_tasman", "AbelTasmanGoldenBay", "Abel Tasman and Golden Bay LiDAR 1m DEM (2023)", "tasman", "abel-tasman-and-golden-bay_2023", "coastal"),
    "tasman": land_source("tasman", "Tasman", "Tasman LiDAR 1m DEM (2020-2022)", "tasman", "tasman_2020-2022", "mixed"),
    "central_otago": land_source("central_otago", "CentralOtago", "Central Otago LiDAR 1m DEM (2022-2023)", "otago", "central-otago_2022-2023", "dry_mountain"),
    "queenstown": land_source("queenstown", "Queenstown", "Queenstown LiDAR 1m DEM (2021)", "otago", "queenstown_2021", "alpine"),
    "wanaka": land_source("wanaka", "Wanaka", "Wanaka LiDAR 1m DEM (2022-2023)", "otago", "wanaka_2022-2023", "alpine"),
    "otago_coastal_catchments": land_source("otago_coastal_catchments", "OtagoCoastalCatchments", "Otago Coastal Catchments LiDAR 1m DEM (2021)", "otago", "coastal-catchments_2021", "coastal_hills"),
}

ATTRIBUTION = "Licensed by Toitu Te Whenua Land Information New Zealand for re-use under CC-BY-4.0."


def expand_source_tokens(tokens: list[str] | None) -> list[SourceSpec]:
    if not tokens:
        tokens = ["all"]

    keys: list[str] = []
    for raw in tokens:
        token = raw.strip().lower()
        if token == "all":
            requested = list(SOURCES.keys())
        elif token == "coastal":
            requested = [key for key, spec in SOURCES.items() if "coast" in spec.domain]
        elif token == "mountain":
            requested = [key for key, spec in SOURCES.items() if any(word in spec.domain for word in ("mountain", "alpine", "volcanic"))]
        elif token in SOURCES:
            requested = [token]
        else:
            valid = ", ".join(["all", "coastal", "mountain", *SOURCES.keys()])
            raise SystemExit(f"Unknown source '{raw}'. Valid values: {valid}")
        for key in requested:
            if key not in keys:
                keys.append(key)
    return [SOURCES[key] for key in keys]


def rename_prepared_files(output_dir: pathlib.Path, source_key: str) -> None:
    if source_key == "waiheke":
        return
    for source_path in output_dir.glob("waiheke_*.cdem"):
        suffix = source_path.name[len("waiheke_") :]
        source_path.replace(output_dir / f"{source_key}_{suffix}")


def prepare_source(
    spec: SourceSpec,
    *,
    tiles_per_source: int,
    patch_size: int,
    patches_per_tile: int,
    min_valid_fraction: float,
    prepared_root: pathlib.Path,
    cache_root: pathlib.Path,
) -> dict:
    print()
    print("=" * 78)
    print(f"SOURCE: {spec.key} - {spec.title}")
    print(f"COLLECTION: {spec.collection_url}")
    print("=" * 78)

    output_dir = prepared_root / spec.folder
    cache_dir = cache_root / spec.folder
    output_dir.mkdir(parents=True, exist_ok=True)
    cache_dir.mkdir(parents=True, exist_ok=True)

    record = {
        "key": spec.key,
        "folder": spec.folder,
        "title": spec.title,
        "domain": spec.domain,
        "collection": spec.collection_url,
        "license": "CC-BY-4.0",
        "attribution": ATTRIBUTION,
        "prepared_format": "Orakai CDEM v1 float32 metres",
        "patch_size_m": patch_size,
        "minimum_valid_fraction": min_valid_fraction,
        "requested_usable_tiles": tiles_per_source,
        "attempted_tiles": 0,
        "usable_tiles": 0,
        "prepared_patches": 0,
        "tiles": [],
        "status": "pending",
    }

    try:
        prep.COLLECTION_URL = spec.collection_url
        items = prep.discover_items()
        print(f"Found {len(items)} STAC DEM items; resolving asset sizes...")
        candidates = prep.resolve_candidates(items)

        for candidate in candidates:
            if record["usable_tiles"] >= tiles_per_source:
                break
            record["attempted_tiles"] += 1
            tiff_path = cache_dir / f"{candidate.item.stem}.tiff"
            try:
                prep.download(candidate.tiff_url, tiff_path)
                count = prep.prepare_tile(
                    tiff_path,
                    output_dir,
                    patch_size,
                    patches_per_tile,
                    min_valid_fraction,
                )
            except Exception as exc:
                print(f"[tile skip] {candidate.item.stem}: {type(exc).__name__}: {exc}")
                count = 0

            rename_prepared_files(output_dir, spec.key)
            record["prepared_patches"] += count
            if count > 0:
                record["usable_tiles"] += 1
            record["tiles"].append({
                "item": candidate.item.json_url,
                "tiff": candidate.tiff_url,
                "asset_size_bytes": candidate.size_bytes,
                "prepared_patches": count,
            })

        record["status"] = "ok" if record["prepared_patches"] > 0 else "no_usable_patches"
    except Exception as exc:
        record["status"] = "unavailable"
        record["error"] = f"{type(exc).__name__}: {exc}"
        print(f"[source skip] {spec.key}: {record['error']}")

    (output_dir / "source.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
    print(
        f"[{spec.key}] status={record['status']} patches={record['prepared_patches']} "
        f"usable tiles={record['usable_tiles']}/{record['attempted_tiles']}"
    )
    return record


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--source",
        action="append",
        help=("Source key/group. Repeatable. Values: all, coastal, mountain, " + ", ".join(SOURCES.keys())),
    )
    parser.add_argument("--tiles-per-source", type=int, default=3,
                        help="Usable COG tiles to keep per source (default: 3)")
    parser.add_argument("--patch-size", type=int, default=1024,
                        help="Square CDEM patch size in 1 m pixels (default: 1024)")
    parser.add_argument("--patches-per-tile", type=int, default=3)
    parser.add_argument("--min-valid-fraction", type=float, default=0.98)
    parser.add_argument("--prepared-root", type=pathlib.Path,
                        default=pathlib.Path("Content/Cubus/TerrainSources/DEM/Prepared"))
    parser.add_argument("--cache-root", type=pathlib.Path,
                        default=pathlib.Path("Saved/DemSourceCache"))
    parser.add_argument("--skip-analysis", action="store_true")
    args = parser.parse_args()

    selected = expand_source_tokens(args.source)
    tiles_per_source = max(1, args.tiles_per_source)
    patch_size = max(128, args.patch_size)
    patches_per_tile = max(1, args.patches_per_tile)
    min_valid_fraction = min(1.0, max(0.80, args.min_valid_fraction))

    args.prepared_root.mkdir(parents=True, exist_ok=True)
    args.cache_root.mkdir(parents=True, exist_ok=True)

    records = [
        prepare_source(
            spec,
            tiles_per_source=tiles_per_source,
            patch_size=patch_size,
            patches_per_tile=patches_per_tile,
            min_valid_fraction=min_valid_fraction,
            prepared_root=args.prepared_root,
            cache_root=args.cache_root,
        )
        for spec in selected
    ]

    successful = [record for record in records if record["prepared_patches"] > 0]
    library_manifest = {
        "license": "CC-BY-4.0",
        "attribution": ATTRIBUTION,
        "prepared_format": "Orakai CDEM v1 float32 metres",
        "requested_source_count": len(records),
        "successful_source_count": len(successful),
        "sources": records,
        "prepared_patch_count": sum(record["prepared_patches"] for record in records),
    }
    manifest_path = args.prepared_root / "library_manifest.json"
    manifest_path.write_text(json.dumps(library_manifest, indent=2) + "\n", encoding="utf-8")

    print()
    print("=" * 78)
    print(
        f"DEM LIBRARY COMPLETE: {library_manifest['prepared_patch_count']} patches across "
        f"{len(successful)}/{len(records)} usable source collections."
    )
    print(f"Manifest: {manifest_path}")
    print(f"Attribution: {ATTRIBUTION}")

    if not args.skip_analysis and library_manifest["prepared_patch_count"] > 0:
        print()
        print("Building searchable terrain index...")
        analyzer.analyse_library(args.prepared_root)

    return 0 if library_manifest["prepared_patch_count"] > 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
