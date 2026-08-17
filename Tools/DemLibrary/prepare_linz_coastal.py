#!/usr/bin/env python3
"""Download LINZ coastal 1 m DEM tiles and prepare compact Orakai CDEM patches.

Default source: Auckland - Waiheke Island Coastal LiDAR 1m DEM (2025-2026)
Licence: CC-BY-4.0, Toitu Te Whenua Land Information New Zealand.

Requires Python 3.12+ and Rasterio 1.5+:
    python -m pip install --upgrade numpy rasterio

Run from the Orakai repository root:
    python Tools/DemLibrary/prepare_linz_coastal.py

The script scans the full STAC collection, prioritises substantial COG assets,
skips sparse/edge tiles, extracts seedable 1024 x 1024 metre terrain patches,
and writes .cdem files to:
    Content/Cubus/TerrainSources/DEM/Prepared/Waiheke

Orakai never parses GeoTIFF at runtime. .cdem is a tiny binary header followed by
row-major little-endian float32 elevation samples in metres.
"""

from __future__ import annotations

import argparse
import json
import math
import pathlib
import struct
import sys
import urllib.request
from dataclasses import dataclass

try:
    import numpy as np
    import rasterio
except Exception as exc:
    print("Failed to import the DEM preparation dependencies.", file=sys.stderr)
    print(f"Python executable: {sys.executable}", file=sys.stderr)
    print(f"Python version: {sys.version}", file=sys.stderr)
    print(f"Import error: {type(exc).__name__}: {exc}", file=sys.stderr)
    print("Verify with:", file=sys.stderr)
    print(
        '  python -c "import sys; print(sys.executable); import numpy; print(\'numpy\', numpy.__version__); '
        'import rasterio; print(\'rasterio\', rasterio.__version__)"',
        file=sys.stderr,
    )
    raise SystemExit(2) from exc

COLLECTION_URL = (
    "https://nz-coastal.s3-ap-southeast-2.amazonaws.com/"
    "auckland/waiheke-island_2025-2026/dem_1m/2193/collection.json"
)
ATTRIBUTION = (
    "Licensed by Toitu Te Whenua Land Information New Zealand for re-use under CC-BY-4.0."
)
MAGIC = 0x4D454443  # CDEM, little endian
VERSION = 1
HEADER = struct.Struct("<IIiiffff")


@dataclass(frozen=True)
class Item:
    stem: str
    json_url: str
    default_tiff_url: str


@dataclass(frozen=True)
class Candidate:
    item: Item
    tiff_url: str
    size_bytes: int


@dataclass(frozen=True)
class WindowChoice:
    x: int
    y: int
    valid_fraction: float
    relief_m: float
    score: float


def fetch_json(url: str) -> dict:
    req = urllib.request.Request(url, headers={"User-Agent": "Orakai-DEM-Prep/1.0"})
    with urllib.request.urlopen(req, timeout=60) as response:
        return json.load(response)


def download(url: str, destination: pathlib.Path) -> None:
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists() and destination.stat().st_size > 0:
        print(f"[cached] {destination}")
        return

    print(f"[download] {url}")
    tmp = destination.with_suffix(destination.suffix + ".part")
    req = urllib.request.Request(url, headers={"User-Agent": "Orakai-DEM-Prep/1.0"})
    with urllib.request.urlopen(req, timeout=180) as response, tmp.open("wb") as out:
        total = int(response.headers.get("Content-Length", "0") or "0")
        done = 0
        while True:
            chunk = response.read(1024 * 1024)
            if not chunk:
                break
            out.write(chunk)
            done += len(chunk)
            if total:
                print(f"  {done / (1024 * 1024):7.1f}/{total / (1024 * 1024):.1f} MiB", end="\r")
    if total:
        print()
    tmp.replace(destination)


def discover_items() -> list[Item]:
    collection = fetch_json(COLLECTION_URL)
    base = COLLECTION_URL.rsplit("/", 1)[0] + "/"
    items: list[Item] = []
    for link in collection.get("links", []):
        if link.get("rel") != "item":
            continue
        href = link.get("href", "")
        if not href.endswith(".json"):
            continue
        stem = pathlib.PurePosixPath(href).stem
        json_url = base + pathlib.PurePosixPath(href).name
        items.append(Item(stem, json_url, json_url[:-5] + ".tiff"))
    if not items:
        raise RuntimeError("No DEM items found in LINZ STAC collection")
    return items


def resolve_candidates(items: list[Item]) -> list[Candidate]:
    """Resolve tiny STAC JSON records first, then try the largest DEM assets first."""
    candidates: list[Candidate] = []
    for index, item in enumerate(items, start=1):
        metadata = fetch_json(item.json_url)
        visual = metadata.get("assets", {}).get("visual", {})
        href = visual.get("href")
        tiff_url = (
            item.default_tiff_url
            if not href
            else item.json_url.rsplit("/", 1)[0] + "/" + pathlib.PurePosixPath(href).name
        )
        size_bytes = int(visual.get("file:size", 0) or 0)
        candidates.append(Candidate(item, tiff_url, size_bytes))
        if size_bytes:
            print(f"[catalog] {index:2d}/{len(items)} {item.stem}: {size_bytes / (1024 * 1024):.1f} MiB")
        else:
            print(f"[catalog] {index:2d}/{len(items)} {item.stem}: size unavailable")

    # Large COGs are overwhelmingly more likely to contain substantial continuous
    # land coverage than tiny edge slivers. Sorting here avoids downloading obvious
    # sparse tiles before useful interior tiles.
    candidates.sort(key=lambda candidate: candidate.size_bytes, reverse=True)
    return candidates


def integral_valid_mask(valid: np.ndarray) -> np.ndarray:
    integral = np.pad(valid.astype(np.int64), ((1, 0), (1, 0)), mode="constant")
    return integral.cumsum(axis=0).cumsum(axis=1)


def window_valid_count(integral: np.ndarray, x: int, y: int, size: int) -> int:
    x1 = x + size
    y1 = y + size
    return int(integral[y1, x1] - integral[y, x1] - integral[y1, x] + integral[y, x])


def rank_windows(
    data: np.ndarray,
    valid: np.ndarray,
    size: int,
    min_valid_fraction: float,
    wanted: int,
) -> list[WindowChoice]:
    """Rank distinct, mostly-valid crops by useful relief without assuming coastline position."""
    height, width = data.shape
    if height < size or width < size:
        return []

    step = max(size // 4, 128)
    pixel_count = size * size
    integral = integral_valid_mask(valid)
    candidates: list[WindowChoice] = []

    for y in range(0, height - size + 1, step):
        for x in range(0, width - size + 1, step):
            valid_fraction = window_valid_count(integral, x, y, size) / pixel_count
            if valid_fraction < min_valid_fraction:
                continue

            # Relief scoring does not need every million values in the crop. Sample
            # at 8 m spacing here; the final .cdem still preserves every 1 m sample.
            sample_data = data[y : y + size : 8, x : x + size : 8]
            sample_valid = valid[y : y + size : 8, x : x + size : 8]
            values = sample_data[sample_valid]
            if values.size < 64:
                continue

            relief = float(np.percentile(values, 95) - np.percentile(values, 5))
            # Prefer meaningful terrain but avoid blindly selecting only the most
            # pathological cliff. Slightly prefer completely populated windows.
            score = relief - max(0.0, relief - 260.0) * 0.45 + valid_fraction * 8.0
            candidates.append(WindowChoice(x, y, valid_fraction, relief, score))

    candidates.sort(key=lambda choice: choice.score, reverse=True)

    selected: list[WindowChoice] = []
    minimum_center_distance_sq = (size * 0.42) ** 2
    for choice in candidates:
        center_x = choice.x + size * 0.5
        center_y = choice.y + size * 0.5
        if any(
            (center_x - (other.x + size * 0.5)) ** 2
            + (center_y - (other.y + size * 0.5)) ** 2
            < minimum_center_distance_sq
            for other in selected
        ):
            continue
        selected.append(choice)
        if len(selected) >= wanted:
            break
    return selected


def write_cdem(path: pathlib.Path, data: np.ndarray, cell_size_m: float) -> None:
    data = np.asarray(data, dtype="<f4", order="C")
    minimum = float(np.min(data))
    maximum = float(np.max(data))
    mean = float(np.mean(data))
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as out:
        out.write(
            HEADER.pack(
                MAGIC,
                VERSION,
                data.shape[1],
                data.shape[0],
                cell_size_m,
                minimum,
                maximum,
                mean,
            )
        )
        out.write(data.tobytes(order="C"))


def prepare_tile(
    tiff_path: pathlib.Path,
    output_dir: pathlib.Path,
    patch_size: int,
    patches_per_tile: int,
    min_valid_fraction: float,
) -> int:
    with rasterio.open(tiff_path) as src:
        if src.count != 1:
            raise RuntimeError(f"Expected one elevation band: {tiff_path}")

        cell_x = abs(float(src.transform.a))
        cell_y = abs(float(src.transform.e))
        if not math.isclose(cell_x, cell_y, rel_tol=1e-4, abs_tol=1e-4):
            raise RuntimeError(f"Non-square DEM pixels are unsupported: {tiff_path}")

        masked = src.read(1, masked=True)
        valid = ~np.ma.getmaskarray(masked)
        data = np.asarray(masked.filled(np.nan), dtype=np.float32)
        valid &= np.isfinite(data)

        whole_valid_fraction = float(valid.mean()) if valid.size else 0.0
        print(
            f"[inspect] {tiff_path.name}: {src.width}x{src.height}, "
            f"{cell_x:.2f} m/pixel, {whole_valid_fraction * 100.0:.1f}% valid"
        )

        choices = rank_windows(
            data,
            valid,
            patch_size,
            min_valid_fraction,
            patches_per_tile,
        )
        if not choices:
            print(
                f"[skip] no {patch_size}x{patch_size} crop reaches "
                f"{min_valid_fraction * 100.0:.1f}% valid in {tiff_path.name}"
            )
            return 0

        written = 0
        for choice in choices:
            x = choice.x
            y = choice.y
            patch = data[y : y + patch_size, x : x + patch_size].copy()
            patch_valid = valid[y : y + patch_size, x : x + patch_size]

            if not bool(patch_valid.all()):
                # The source threshold keeps this residual small. Fill only those
                # holes from the local valid median so runtime never receives NaN.
                patch[~patch_valid] = float(np.median(patch[patch_valid]))

            out_name = f"waiheke_{tiff_path.stem}_{x}_{y}_{patch_size}m.cdem"
            out_path = output_dir / out_name
            write_cdem(out_path, patch, cell_x)
            print(
                f"[prepared] {out_path}  valid={choice.valid_fraction * 100.0:.1f}% "
                f"min={float(patch.min()):.1f}m max={float(patch.max()):.1f}m "
                f"relief={float(patch.max() - patch.min()):.1f}m"
            )
            written += 1

        return written


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--tiles",
        type=int,
        default=4,
        help="Number of usable LINZ source tiles to prepare (sparse tiles do not count)",
    )
    parser.add_argument("--patch-size", type=int, default=1024, help="Square patch size in 1 m pixels")
    parser.add_argument("--patches-per-tile", type=int, default=2)
    parser.add_argument(
        "--min-valid-fraction",
        type=float,
        default=0.98,
        help="Minimum real-data fraction required in a prepared patch (default: 0.98)",
    )
    parser.add_argument(
        "--output",
        type=pathlib.Path,
        default=pathlib.Path("Content/Cubus/TerrainSources/DEM/Prepared/Waiheke"),
    )
    parser.add_argument(
        "--cache",
        type=pathlib.Path,
        default=pathlib.Path("Saved/DemSourceCache/Waiheke"),
    )
    args = parser.parse_args()

    requested_usable_tiles = max(1, args.tiles)
    patch_size = max(128, args.patch_size)
    patches_per_tile = max(1, args.patches_per_tile)
    min_valid_fraction = min(1.0, max(0.80, args.min_valid_fraction))

    args.output.mkdir(parents=True, exist_ok=True)
    args.cache.mkdir(parents=True, exist_ok=True)

    items = discover_items()
    print(f"Found {len(items)} STAC DEM items; resolving asset sizes...")
    candidates = resolve_candidates(items)

    total_patches = 0
    usable_tiles = 0
    attempted_tiles = 0
    source_manifest: dict = {
        "source": "Auckland - Waiheke Island Coastal LiDAR 1m DEM (2025-2026)",
        "collection": COLLECTION_URL,
        "license": "CC-BY-4.0",
        "attribution": ATTRIBUTION,
        "prepared_format": "Orakai CDEM v1 float32 metres",
        "patch_size_m": patch_size,
        "minimum_valid_fraction": min_valid_fraction,
        "requested_usable_tiles": requested_usable_tiles,
        "tiles": [],
    }

    for candidate in candidates:
        if usable_tiles >= requested_usable_tiles:
            break

        attempted_tiles += 1
        tiff_path = args.cache / f"{candidate.item.stem}.tiff"
        download(candidate.tiff_url, tiff_path)
        count = prepare_tile(
            tiff_path,
            args.output,
            patch_size,
            patches_per_tile,
            min_valid_fraction,
        )
        total_patches += count
        if count > 0:
            usable_tiles += 1

        source_manifest["tiles"].append(
            {
                "item": candidate.item.json_url,
                "tiff": candidate.tiff_url,
                "asset_size_bytes": candidate.size_bytes,
                "prepared_patches": count,
            }
        )

    source_manifest["attempted_tiles"] = attempted_tiles
    source_manifest["usable_tiles"] = usable_tiles
    source_manifest["prepared_patches"] = total_patches

    manifest_path = args.output.parent / "waiheke_source.json"
    manifest_path.write_text(json.dumps(source_manifest, indent=2) + "\n", encoding="utf-8")

    print()
    print(
        f"Prepared {total_patches} real DEM patches from {usable_tiles} usable "
        f"tile(s) after inspecting {attempted_tiles}."
    )
    if usable_tiles < requested_usable_tiles:
        print(
            f"Warning: the collection only yielded {usable_tiles}/{requested_usable_tiles} usable "
            f"tiles at {patch_size} m and {min_valid_fraction * 100.0:.1f}% validity."
        )
    print(f"Attribution: {ATTRIBUTION}")
    print(f"Manifest: {manifest_path}")
    return 0 if total_patches else 1


if __name__ == "__main__":
    raise SystemExit(main())
