#!/usr/bin/env python3
"""Build Orakai's physical-scale real DEM pyramid from LINZ's national 8 m DEM.

The important rule is that source *physical extent* is preserved. A 1 km LiDAR
patch is local detail; it is never enlarged into a mountain range. This tool
builds four larger source tiers directly from contiguous real terrain:

    Macro128km   ~131 km extent, 128 m/sample   mountain belts / major basins
    Macro64km     ~66 km extent,  64 m/sample   ranges / plateaus / long valleys
    Macro32km     ~33 km extent,  32 m/sample   mountains / regional drainage
    Regional8km    ~8 km extent,   8 m/sample   valleys / hills / coastal systems

Each prepared CDEM is roughly 1024 x 1024 samples regardless of physical tier.
Large windows are assembled from every LINZ COG intersecting the requested NZTM
bounds and are resampled directly into the target resolution, so a 131 km source
window never needs a 16,384 x 16,384 in-memory intermediate array.

Default source:
    New Zealand Contour-Interpolated 8m DEM, LINZ, CC BY 4.0

LINZ describes that dataset as cartographic source material rather than an
analysis-grade DEM. Orakai uses these files only as source geometry and runs its
own hydrology / landscape evolution after composition.

Run from the Orakai repository root:

    python Tools/DemLibrary/prepare_multiscale_dem_library.py

Useful variants:

    python Tools/DemLibrary/prepare_multiscale_dem_library.py --tier macro128 --tier macro64
    python Tools/DemLibrary/prepare_multiscale_dem_library.py --count-multiplier 2
    python Tools/DemLibrary/prepare_multiscale_dem_library.py --download-source-tiles
"""

from __future__ import annotations

import argparse
import contextlib
import json
import math
import pathlib
import random
import urllib.parse
from dataclasses import dataclass

import numpy as np
import rasterio
from rasterio.enums import Resampling
from rasterio.fill import fillnodata
from rasterio.merge import merge
from rasterio.warp import transform_bounds

import analyze_dem_library as analyzer
import prepare_linz_coastal as prep

COLLECTION_URL = (
    "https://nz-elevation.s3-ap-southeast-2.amazonaws.com/"
    "new-zealand/new-zealand-contour/dem_8m/2193/collection.json"
)
ATTRIBUTION = "Sourced from LINZ. CC BY 4.0"
TARGET_EPSG = 2193


@dataclass(frozen=True)
class TierSpec:
    key: str
    folder: str
    extent_m: float
    cell_m: float
    default_count: int
    minimum_valid_fraction: float


TIERS: dict[str, TierSpec] = {
    "macro128": TierSpec("macro128", "Macro128km", 131072.0, 128.0, 12, 0.72),
    "macro64": TierSpec("macro64", "Macro64km", 65536.0, 64.0, 24, 0.78),
    "macro32": TierSpec("macro32", "Macro32km", 32768.0, 32.0, 48, 0.84),
    "regional8": TierSpec("regional8", "Regional8km", 8192.0, 8.0, 96, 0.92),
}


@dataclass(frozen=True)
class CatalogTile:
    stem: str
    item_url: str
    tiff_url: str
    size_bytes: int
    bounds_nztm: tuple[float, float, float, float]
    relief_hint_m: float

    @property
    def center(self) -> tuple[float, float]:
        left, bottom, right, top = self.bounds_nztm
        return ((left + right) * 0.5, (bottom + top) * 0.5)


@dataclass(frozen=True)
class WindowSeed:
    x: float
    y: float
    relief_hint_m: float
    stem: str


def _asset_url(item_url: str, visual: dict) -> str:
    href = str(visual.get("href", "") or "")
    if not href:
        return item_url[:-5] + ".tiff"
    return urllib.parse.urljoin(item_url, href)


def _statistics_relief_hint(visual: dict) -> float:
    bands = visual.get("raster:bands") or visual.get("bands") or []
    if not bands or not isinstance(bands[0], dict):
        return 0.0
    stats = bands[0].get("statistics", {}) or {}
    try:
        minimum = float(stats.get("minimum"))
        maximum = float(stats.get("maximum"))
        if math.isfinite(minimum) and math.isfinite(maximum) and maximum > minimum:
            return maximum - minimum
    except (TypeError, ValueError):
        pass
    try:
        stddev = float(stats.get("stddev"))
        if math.isfinite(stddev) and stddev > 0:
            return stddev * 5.0
    except (TypeError, ValueError):
        pass
    return 0.0


def _projected_bounds(metadata: dict, visual: dict) -> tuple[float, float, float, float] | None:
    properties = metadata.get("properties", {}) or {}
    projected = properties.get("proj:bbox") or visual.get("proj:bbox")
    if isinstance(projected, list) and len(projected) >= 4:
        return tuple(float(value) for value in projected[:4])

    bbox = metadata.get("bbox")
    if not isinstance(bbox, list) or len(bbox) < 4:
        return None
    try:
        return tuple(
            float(value)
            for value in transform_bounds(
                "EPSG:4326",
                f"EPSG:{TARGET_EPSG}",
                float(bbox[0]),
                float(bbox[1]),
                float(bbox[2]),
                float(bbox[3]),
                densify_pts=21,
            )
        )
    except Exception:
        return None


def load_catalog() -> list[CatalogTile]:
    collection = prep.fetch_json(COLLECTION_URL)
    base_url = COLLECTION_URL.rsplit("/", 1)[0] + "/"
    item_urls = [
        urllib.parse.urljoin(base_url, str(link.get("href", "")))
        for link in collection.get("links", [])
        if link.get("rel") == "item" and str(link.get("href", "")).endswith(".json")
    ]
    if not item_urls:
        raise RuntimeError("No STAC items found in national 8 m DEM collection")

    catalog: list[CatalogTile] = []
    for index, item_url in enumerate(item_urls, start=1):
        try:
            metadata = prep.fetch_json(item_url)
            visual = metadata.get("assets", {}).get("visual", {}) or {}
            bounds = _projected_bounds(metadata, visual)
            if bounds is None:
                print(f"[catalog skip] no usable projected bounds: {item_url}")
                continue
            stem = pathlib.PurePosixPath(item_url).stem
            tile = CatalogTile(
                stem=stem,
                item_url=item_url,
                tiff_url=_asset_url(item_url, visual),
                size_bytes=int(visual.get("file:size", 0) or 0),
                bounds_nztm=bounds,
                relief_hint_m=_statistics_relief_hint(visual),
            )
            catalog.append(tile)
            print(
                f"[catalog] {index:3d}/{len(item_urls)} {stem}: "
                f"relief hint {tile.relief_hint_m:.0f} m"
            )
        except Exception as exc:
            print(f"[catalog skip] {item_url}: {type(exc).__name__}: {exc}")

    if not catalog:
        raise RuntimeError("National DEM catalog contained no usable projected tiles")
    return catalog


def intersects(bounds: tuple[float, float, float, float], window: tuple[float, float, float, float]) -> bool:
    left, bottom, right, top = bounds
    w_left, w_bottom, w_right, w_top = window
    return not (right <= w_left or left >= w_right or top <= w_bottom or bottom >= w_top)


def window_bounds(seed: WindowSeed, extent_m: float) -> tuple[float, float, float, float]:
    half = extent_m * 0.5
    return (seed.x - half, seed.y - half, seed.x + half, seed.y + half)


def seed_order(catalog: list[CatalogTile], count: int, seed: int) -> list[WindowSeed]:
    """Interleave relief quantiles so a tier contains plains, hills and mountains."""
    ordered = sorted(catalog, key=lambda tile: (tile.relief_hint_m, tile.stem))
    rng = random.Random(seed)
    if len(ordered) == 1:
        x, y = ordered[0].center
        return [WindowSeed(x, y, ordered[0].relief_hint_m, ordered[0].stem)]

    attempts = max(count * 12, len(ordered) * 2)
    result: list[WindowSeed] = []
    golden = 0.6180339887498949
    phase = rng.random()
    for i in range(attempts):
        q = (phase + i * golden) % 1.0
        index = min(len(ordered) - 1, max(0, int(round(q * (len(ordered) - 1)))))
        tile = ordered[index]
        left, bottom, right, top = tile.bounds_nztm
        # Jitter remains inside the tile and prevents all source windows from being
        # centred on the source sheet grid.
        jitter_x = (rng.random() - 0.5) * (right - left) * 0.45
        jitter_y = (rng.random() - 0.5) * (top - bottom) * 0.45
        x, y = tile.center
        result.append(WindowSeed(x + jitter_x, y + jitter_y, tile.relief_hint_m, tile.stem))
    return result


def too_close(seed: WindowSeed, accepted: list[WindowSeed], extent_m: float, separation_fraction: float) -> bool:
    minimum_distance_sq = (extent_m * separation_fraction) ** 2
    return any((seed.x - other.x) ** 2 + (seed.y - other.y) ** 2 < minimum_distance_sq for other in accepted)


def local_tile_path(cache_root: pathlib.Path, tile: CatalogTile) -> pathlib.Path:
    return cache_root / f"{tile.stem}.tiff"


def open_sources(
    tiles: list[CatalogTile],
    cache_root: pathlib.Path,
    force_local: bool,
    stack: contextlib.ExitStack,
) -> list[rasterio.io.DatasetReader]:
    datasets: list[rasterio.io.DatasetReader] = []
    for tile in tiles:
        path = local_tile_path(cache_root, tile)
        if force_local:
            prep.download(tile.tiff_url, path)
            dataset = stack.enter_context(rasterio.open(path))
        else:
            try:
                dataset = stack.enter_context(rasterio.open(tile.tiff_url))
            except Exception as exc:
                print(f"[remote fallback] {tile.stem}: {type(exc).__name__}: {exc}")
                prep.download(tile.tiff_url, path)
                dataset = stack.enter_context(rasterio.open(path))
        if dataset.crs is None:
            raise RuntimeError(f"DEM tile has no CRS: {tile.stem}")
        datasets.append(dataset)
    return datasets


def merge_window(
    source_tiles: list[CatalogTile],
    bounds: tuple[float, float, float, float],
    target_cell_m: float,
    cache_root: pathlib.Path,
    force_local: bool,
) -> np.ma.MaskedArray:
    def do_merge(local_only: bool) -> np.ma.MaskedArray:
        with contextlib.ExitStack() as stack:
            datasets = open_sources(source_tiles, cache_root, local_only, stack)
            mosaic, _ = merge(
                datasets,
                bounds=bounds,
                res=(target_cell_m, target_cell_m),
                dtype="float32",
                nodata=np.nan,
                masked=True,
                resampling=Resampling.average,
                target_aligned_pixels=False,
            )
            return np.ma.asarray(mosaic[0])

    try:
        return do_merge(force_local)
    except Exception as exc:
        if force_local:
            raise
        print(f"[merge remote fallback] {type(exc).__name__}: {exc}")
        return do_merge(True)


def finish_missing(data: np.ndarray, valid: np.ndarray) -> np.ndarray:
    if bool(valid.all()):
        return data.astype(np.float32, copy=False)
    if not bool(valid.any()):
        raise ValueError("source window contains no valid terrain")

    # GDAL's fillnodata extends nearby real terrain into small source-sheet holes
    # without replacing an entire missing region with one arbitrary median height.
    working = np.asarray(data, dtype=np.float32).copy()
    working[~valid] = 0.0
    filled = fillnodata(
        working,
        mask=valid.astype(np.uint8),
        max_search_distance=max(32, int(max(data.shape) * 0.30)),
        smoothing_iterations=1,
    )
    still_bad = ~np.isfinite(filled)
    if bool(still_bad.any()):
        filled[still_bad] = float(np.median(data[valid]))
    return np.asarray(filled, dtype=np.float32)


def prepare_window(
    spec: TierSpec,
    seed: WindowSeed,
    catalog: list[CatalogTile],
    output_dir: pathlib.Path,
    cache_root: pathlib.Path,
    force_local: bool,
    ordinal: int,
) -> dict | None:
    bounds = window_bounds(seed, spec.extent_m)
    source_tiles = [tile for tile in catalog if intersects(tile.bounds_nztm, bounds)]
    if not source_tiles:
        return None

    try:
        mosaic = merge_window(source_tiles, bounds, spec.cell_m, cache_root, force_local)
    except Exception as exc:
        print(
            f"[window skip] {spec.key} {seed.stem}: merge failed: "
            f"{type(exc).__name__}: {exc}"
        )
        return None

    mask = np.ma.getmaskarray(mosaic)
    raw = np.asarray(mosaic.filled(np.nan), dtype=np.float32)
    valid = ~mask & np.isfinite(raw)
    valid_fraction = float(valid.mean()) if valid.size else 0.0
    if valid_fraction < spec.minimum_valid_fraction:
        print(
            f"[window skip] {spec.key} {seed.stem}: "
            f"{valid_fraction * 100.0:.1f}% valid < {spec.minimum_valid_fraction * 100.0:.1f}%"
        )
        return None

    data = finish_missing(raw, valid)
    relief_p90 = float(np.percentile(data, 95) - np.percentile(data, 5))
    output_name = (
        f"{spec.key}_{ordinal:03d}_{seed.stem}_"
        f"{int(round(seed.x))}_{int(round(seed.y))}.cdem"
    )
    output_path = output_dir / output_name
    prep.write_cdem(output_path, data, spec.cell_m)
    actual_extent_x = (data.shape[1] - 1) * spec.cell_m
    actual_extent_y = (data.shape[0] - 1) * spec.cell_m
    print(
        f"[{spec.key}] {output_path.name}: {data.shape[1]}x{data.shape[0]}, "
        f"{spec.cell_m:.0f} m/sample, {actual_extent_x / 1000.0:.1f} x "
        f"{actual_extent_y / 1000.0:.1f} km, P90 relief {relief_p90:.0f} m, "
        f"valid {valid_fraction * 100.0:.1f}%"
    )
    return {
        "path": output_path.as_posix(),
        "seed_sheet": seed.stem,
        "center_nztm": [seed.x, seed.y],
        "bounds_nztm": list(bounds),
        "source_tiles": [tile.stem for tile in source_tiles],
        "cell_size_m": spec.cell_m,
        "extent_x_m": actual_extent_x,
        "extent_y_m": actual_extent_y,
        "valid_fraction": valid_fraction,
        "relief_p90_m": relief_p90,
    }


def prepare_tier(
    spec: TierSpec,
    catalog: list[CatalogTile],
    prepared_root: pathlib.Path,
    cache_root: pathlib.Path,
    requested_count: int,
    seed: int,
    force_local: bool,
) -> dict:
    output_dir = prepared_root / spec.folder
    output_dir.mkdir(parents=True, exist_ok=True)
    tier_cache = cache_root / "National8m"
    tier_cache.mkdir(parents=True, exist_ok=True)

    print()
    print("=" * 80)
    print(
        f"TIER {spec.key}: target {requested_count} windows, "
        f"{spec.extent_m / 1000.0:.1f} km physical extent, {spec.cell_m:.0f} m/sample"
    )
    print("=" * 80)

    accepted_seeds: list[WindowSeed] = []
    records: list[dict] = []
    attempts = seed_order(catalog, requested_count, seed)
    # Start with strong separation; relax only if New Zealand's long narrow shape
    # prevents us from filling the requested library count.
    separation_fractions = (0.62, 0.46, 0.30, 0.16)
    used_seed_keys: set[tuple[int, int]] = set()

    for separation in separation_fractions:
        if len(records) >= requested_count:
            break
        for candidate in attempts:
            if len(records) >= requested_count:
                break
            key = (int(round(candidate.x / 1000.0)), int(round(candidate.y / 1000.0)))
            if key in used_seed_keys:
                continue
            if too_close(candidate, accepted_seeds, spec.extent_m, separation):
                continue
            used_seed_keys.add(key)
            record = prepare_window(
                spec,
                candidate,
                catalog,
                output_dir,
                tier_cache,
                force_local,
                len(records),
            )
            if record is None:
                continue
            accepted_seeds.append(candidate)
            records.append(record)

    source_record = {
        "key": spec.key,
        "title": "New Zealand Contour-Interpolated 8m DEM",
        "domain": spec.key,
        "tier": spec.key,
        "collection": COLLECTION_URL,
        "license": "CC-BY-4.0",
        "attribution": ATTRIBUTION,
        "physical_extent_m": spec.extent_m,
        "target_cell_m": spec.cell_m,
        "requested_patches": requested_count,
        "prepared_patches": len(records),
        "minimum_valid_fraction": spec.minimum_valid_fraction,
        "usage_note": (
            "Real physical-scale source geometry only; Orakai recomputes global hydrology and erosion after composition."
        ),
        "patches": records,
    }
    (output_dir / "source.json").write_text(
        json.dumps(source_record, indent=2) + "\n", encoding="utf-8"
    )
    print(f"[{spec.key}] prepared {len(records)}/{requested_count} windows")
    return source_record


def expand_tiers(tokens: list[str] | None) -> list[TierSpec]:
    if not tokens or "all" in {token.lower() for token in tokens}:
        return list(TIERS.values())
    result: list[TierSpec] = []
    for token in tokens:
        key = token.strip().lower()
        if key not in TIERS:
            raise SystemExit(f"Unknown tier '{token}'. Valid values: all, {', '.join(TIERS)}")
        if TIERS[key] not in result:
            result.append(TIERS[key])
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tier", action="append", help="Repeatable: macro128, macro64, macro32, regional8, all")
    parser.add_argument(
        "--count-multiplier",
        type=float,
        default=1.0,
        help="Multiply the default patch count for every selected tier",
    )
    parser.add_argument("--seed", type=int, default=1337, help="Deterministic source-window selection seed")
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
    parser.add_argument(
        "--download-source-tiles",
        action="store_true",
        help="Always cache intersecting source COGs locally instead of reading COG ranges over HTTPS",
    )
    parser.add_argument(
        "--skip-analysis",
        action="store_true",
        help="Do not rebuild library_index.json after preparing the selected tiers",
    )
    args = parser.parse_args()

    selected = expand_tiers(args.tier)
    multiplier = max(0.05, args.count_multiplier)
    args.prepared_root.mkdir(parents=True, exist_ok=True)
    args.cache_root.mkdir(parents=True, exist_ok=True)

    print("Loading national DEM STAC catalog...")
    catalog = load_catalog()
    print(f"Catalog ready: {len(catalog)} projected source sheets")

    records: list[dict] = []
    for index, spec in enumerate(selected):
        requested = max(1, int(round(spec.default_count * multiplier)))
        records.append(
            prepare_tier(
                spec,
                catalog,
                args.prepared_root,
                args.cache_root,
                requested,
                args.seed + index * 7919,
                args.download_source_tiles,
            )
        )

    manifest = {
        "schema_version": 1,
        "source": "New Zealand Contour-Interpolated 8m DEM",
        "collection": COLLECTION_URL,
        "license": "CC-BY-4.0",
        "attribution": ATTRIBUTION,
        "tiers": records,
        "prepared_patch_count": sum(record["prepared_patches"] for record in records),
    }
    manifest_path = args.prepared_root / "multiscale_manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")

    if not args.skip_analysis and manifest["prepared_patch_count"] > 0:
        print()
        print("Rebuilding searchable DEM index...")
        analyzer.analyse_library(args.prepared_root)

    print()
    print(
        f"MULTISCALE DEM COMPLETE: {manifest['prepared_patch_count']} patches across "
        f"{len(records)} physical-scale tiers"
    )
    print(f"Manifest: {manifest_path}")
    return 0 if manifest["prepared_patch_count"] > 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
