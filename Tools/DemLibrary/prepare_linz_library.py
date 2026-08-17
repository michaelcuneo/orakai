#!/usr/bin/env python3
"""Prepare a varied LINZ 1 m DEM library for Orakai.

This builds on prepare_linz_coastal.py's COG inspection/cropping code but pulls
from several independent LINZ coastal and land elevation collections. Prepared
CDEM patches are written beneath Content/Cubus/TerrainSources/DEM/Prepared and
are discovered recursively by CubusDemIslandGenerator.

Run from the Orakai repository root with the active Python environment:

    python Tools/DemLibrary/prepare_linz_library.py

Useful variants:

    python Tools/DemLibrary/prepare_linz_library.py --source coastal
    python Tools/DemLibrary/prepare_linz_library.py --source land
    python Tools/DemLibrary/prepare_linz_library.py --source taranaki --source okuru
    python Tools/DemLibrary/prepare_linz_library.py --tiles-per-source 3
"""

from __future__ import annotations

import argparse
import json
import pathlib
from dataclasses import dataclass

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

SOURCES: dict[str, SourceSpec] = {
    "waiheke": SourceSpec(
        key="waiheke",
        folder="Waiheke",
        title="Auckland - Waiheke Island Coastal LiDAR 1m DEM (2025-2026)",
        collection_url=(
            f"{COASTAL_ROOT}/auckland/waiheke-island_2025-2026/"
            "dem_1m/2193/collection.json"
        ),
        domain="coastal",
    ),
    "whangarei": SourceSpec(
        key="whangarei",
        folder="WhangareiBreamBay",
        title="Northland - Whangarei to Bream Bay Coastal LiDAR 1m DEM (2025)",
        collection_url=(
            f"{COASTAL_ROOT}/northland/whangarei-to-bream-bay_2025/"
            "dem_1m/2193/collection.json"
        ),
        domain="coastal",
    ),
    "bluff": SourceSpec(
        key="bluff",
        folder="Bluff",
        title="Southland - Bluff Coastal LiDAR 1m DEM (2025)",
        collection_url=(
            f"{COASTAL_ROOT}/southland/bluff_2025/dem_1m/2193/collection.json"
        ),
        domain="coastal",
    ),
    "dunedin": SourceSpec(
        key="dunedin",
        folder="Dunedin",
        title="Otago - Dunedin Coastal LiDAR 1m DEM (2025)",
        collection_url=(
            f"{COASTAL_ROOT}/otago/dunedin_2025/dem_1m/2193/collection.json"
        ),
        domain="coastal",
    ),
    "okuru": SourceSpec(
        key="okuru",
        folder="Okuru",
        title="West Coast - Okuru Coastal LiDAR 1m DEM (2026)",
        collection_url=(
            f"{COASTAL_ROOT}/west-coast/okuru_2026/dem_1m/2193/collection.json"
        ),
        domain="coastal",
    ),
    "banks_peninsula": SourceSpec(
        key="banks_peninsula",
        folder="BanksPeninsula",
        title="Canterbury - Banks Peninsula LiDAR 1m DEM (2023)",
        collection_url=(
            f"{ELEVATION_ROOT}/canterbury/banks-peninsula_2023/"
            "dem_1m/2193/collection.json"
        ),
        domain="land",
    ),
    "taranaki": SourceSpec(
        key="taranaki",
        folder="Taranaki",
        title="Taranaki LiDAR 1m DEM (2021)",
        collection_url=(
            f"{ELEVATION_ROOT}/taranaki/taranaki_2021/dem_1m/2193/collection.json"
        ),
        domain="land",
    ),
}

ATTRIBUTION = (
    "Licensed by Toitu Te Whenua Land Information New Zealand for re-use under CC-BY-4.0."
)


def expand_source_tokens(tokens: list[str] | None) -> list[SourceSpec]:
    if not tokens:
        tokens = ["all"]

    keys: list[str] = []
    for raw in tokens:
        token = raw.strip().lower()
        if token == "all":
            requested = list(SOURCES.keys())
        elif token in {"coastal", "land"}:
            requested = [key for key, spec in SOURCES.items() if spec.domain == token]
        elif token in SOURCES:
            requested = [token]
        else:
            valid = ", ".join(["all", "coastal", "land", *SOURCES.keys()])
            raise SystemExit(f"Unknown source '{raw}'. Valid values: {valid}")

        for key in requested:
            if key not in keys:
                keys.append(key)

    return [SOURCES[key] for key in keys]


def rename_prepared_files(output_dir: pathlib.Path, source_key: str) -> None:
    """prepare_linz_coastal currently emits a waiheke_ prefix; normalise it here."""
    if source_key == "waiheke":
        return

    for source_path in output_dir.glob("waiheke_*.cdem"):
        suffix = source_path.name[len("waiheke_") :]
        target_path = output_dir / f"{source_key}_{suffix}"
        source_path.replace(target_path)


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

    # Reuse the tested STAC/COG selection logic from prepare_linz_coastal.py.
    prep.COLLECTION_URL = spec.collection_url

    output_dir = prepared_root / spec.folder
    cache_dir = cache_root / spec.folder
    output_dir.mkdir(parents=True, exist_ok=True)
    cache_dir.mkdir(parents=True, exist_ok=True)

    items = prep.discover_items()
    print(f"Found {len(items)} STAC DEM items; resolving asset sizes...")
    candidates = prep.resolve_candidates(items)

    usable_tiles = 0
    attempted_tiles = 0
    total_patches = 0
    tile_records: list[dict] = []

    for candidate in candidates:
        if usable_tiles >= tiles_per_source:
            break

        attempted_tiles += 1
        tiff_path = cache_dir / f"{candidate.item.stem}.tiff"
        prep.download(candidate.tiff_url, tiff_path)
        count = prep.prepare_tile(
            tiff_path,
            output_dir,
            patch_size,
            patches_per_tile,
            min_valid_fraction,
        )
        rename_prepared_files(output_dir, spec.key)

        total_patches += count
        if count > 0:
            usable_tiles += 1

        tile_records.append(
            {
                "item": candidate.item.json_url,
                "tiff": candidate.tiff_url,
                "asset_size_bytes": candidate.size_bytes,
                "prepared_patches": count,
            }
        )

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
        "attempted_tiles": attempted_tiles,
        "usable_tiles": usable_tiles,
        "prepared_patches": total_patches,
        "tiles": tile_records,
    }
    (output_dir / "source.json").write_text(
        json.dumps(record, indent=2) + "\n", encoding="utf-8"
    )

    print(
        f"[{spec.key}] prepared {total_patches} patches from {usable_tiles} usable "
        f"tile(s) after inspecting {attempted_tiles}."
    )
    return record


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--source",
        action="append",
        help=(
            "Source key/group. Repeatable. Values: all, coastal, land, "
            + ", ".join(SOURCES.keys())
        ),
    )
    parser.add_argument(
        "--tiles-per-source",
        type=int,
        default=2,
        help="Usable COG tiles to keep per source (default: 2)",
    )
    parser.add_argument(
        "--patch-size",
        type=int,
        default=1024,
        help="Square CDEM patch size in 1 m pixels (default: 1024)",
    )
    parser.add_argument("--patches-per-tile", type=int, default=2)
    parser.add_argument("--min-valid-fraction", type=float, default=0.98)
    parser.add_argument(
        "--prepared-root",
        type=pathlib.Path,
        default=pathlib.Path("Content/Cubus/TerrainSources/DEM/Prepared"),
    )
    parser.add_argument(
        "--cache-root",
        type=pathlib.Path,
        default=pathlib.Path("Saved/DemSourceCache"),
    )
    args = parser.parse_args()

    selected = expand_source_tokens(args.source)
    tiles_per_source = max(1, args.tiles_per_source)
    patch_size = max(128, args.patch_size)
    patches_per_tile = max(1, args.patches_per_tile)
    min_valid_fraction = min(1.0, max(0.80, args.min_valid_fraction))

    args.prepared_root.mkdir(parents=True, exist_ok=True)
    args.cache_root.mkdir(parents=True, exist_ok=True)

    records: list[dict] = []
    for spec in selected:
        records.append(
            prepare_source(
                spec,
                tiles_per_source=tiles_per_source,
                patch_size=patch_size,
                patches_per_tile=patches_per_tile,
                min_valid_fraction=min_valid_fraction,
                prepared_root=args.prepared_root,
                cache_root=args.cache_root,
            )
        )

    library_manifest = {
        "license": "CC-BY-4.0",
        "attribution": ATTRIBUTION,
        "prepared_format": "Orakai CDEM v1 float32 metres",
        "sources": records,
        "prepared_patch_count": sum(record["prepared_patches"] for record in records),
    }
    manifest_path = args.prepared_root / "library_manifest.json"
    manifest_path.write_text(
        json.dumps(library_manifest, indent=2) + "\n", encoding="utf-8"
    )

    print()
    print("=" * 78)
    print(
        f"DEM LIBRARY COMPLETE: {library_manifest['prepared_patch_count']} patches "
        f"across {len(records)} source collections."
    )
    print(f"Manifest: {manifest_path}")
    print(f"Attribution: {ATTRIBUTION}")
    return 0 if library_manifest["prepared_patch_count"] > 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
