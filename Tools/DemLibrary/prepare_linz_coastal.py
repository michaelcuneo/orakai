#!/usr/bin/env python3
"""Download LINZ coastal 1 m DEM tiles and prepare compact Orakai CDEM patches.

Default source: Auckland - Waiheke Island Coastal LiDAR 1m DEM (2025-2026)
Licence: CC-BY-4.0, Toitu Te Whenua Land Information New Zealand.

Requires Python 3.10+ and rasterio:
    py -m pip install rasterio numpy

Run from the Orakai repository root:
    py Tools/DemLibrary/prepare_linz_coastal.py

The script downloads a small subset of public Cloud Optimised GeoTIFFs, extracts
seedable 1024 x 1024 metre terrain patches, and writes .cdem files to:
    Content/Cubus/TerrainSources/DEM/Prepared/Waiheke

Orakai never parses GeoTIFF at runtime. .cdem is a tiny binary header followed by
row-major little-endian float32 elevation samples in metres.
"""

from __future__ import annotations

import argparse
import json
import math
import os
import pathlib
import struct
import sys
import urllib.request
from dataclasses import dataclass

try:
    import numpy as np
    import rasterio
except ImportError as exc:
    print("Missing dependency. Run: py -m pip install rasterio numpy", file=sys.stderr)
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
    tiff_url: str


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
                print(f"  {done / (1024*1024):7.1f}/{total / (1024*1024):.1f} MiB", end="\r")
    if total:
        print()
    tmp.replace(destination)


def discover_items(limit: int) -> list[Item]:
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
        # LINZ elevation/coastal convention: item JSON and COG share basename.
        tiff_url = json_url[:-5] + ".tiff"
        items.append(Item(stem, json_url, tiff_url))
        if len(items) >= limit:
            break
    if not items:
        raise RuntimeError("No DEM items found in LINZ STAC collection")
    return items


def valid_window(data: np.ndarray, nodata: float | None, size: int) -> tuple[int, int] | None:
    """Choose a high-relief valid crop without assuming where coast lies in a tile."""
    h, w = data.shape
    if h < size or w < size:
        return None
    step = max(size // 4, 128)
    best: tuple[float, int, int] | None = None
    for y in range(0, h - size + 1, step):
        for x in range(0, w - size + 1, step):
            patch = data[y : y + size, x : x + size]
            finite = np.isfinite(patch)
            if nodata is not None:
                finite &= patch != nodata
            valid_fraction = float(finite.mean())
            if valid_fraction < 0.995:
                continue
            values = patch[finite]
            relief = float(np.percentile(values, 95) - np.percentile(values, 5))
            # Prefer meaningful relief, but do not blindly select the most extreme cliff.
            score = relief - max(0.0, relief - 260.0) * 0.45
            if best is None or score > best[0]:
                best = (score, x, y)
    return None if best is None else (best[1], best[2])


def write_cdem(path: pathlib.Path, data: np.ndarray, cell_size_m: float) -> None:
    data = np.asarray(data, dtype="<f4", order="C")
    minimum = float(np.min(data))
    maximum = float(np.max(data))
    mean = float(np.mean(data))
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as out:
        out.write(HEADER.pack(MAGIC, VERSION, data.shape[1], data.shape[0], cell_size_m, minimum, maximum, mean))
        out.write(data.tobytes(order="C"))


def prepare_tile(tiff_path: pathlib.Path, output_dir: pathlib.Path, patch_size: int, patches_per_tile: int) -> int:
    written = 0
    with rasterio.open(tiff_path) as src:
        if src.count != 1:
            raise RuntimeError(f"Expected one elevation band: {tiff_path}")
        cell_x = abs(float(src.transform.a))
        cell_y = abs(float(src.transform.e))
        if not math.isclose(cell_x, cell_y, rel_tol=1e-4, abs_tol=1e-4):
            raise RuntimeError(f"Non-square DEM pixels are unsupported: {tiff_path}")
        data = src.read(1)
        nodata = src.nodata

        # First patch is selected by terrain relief. Additional patches are offset
        # deterministically so one source tile can contribute several landforms.
        chosen = valid_window(data, nodata, patch_size)
        if chosen is None:
            print(f"[skip] no {patch_size}x{patch_size} valid crop in {tiff_path.name}")
            return 0
        base_x, base_y = chosen
        candidates = [
            (base_x, base_y),
            (max(0, min(data.shape[1] - patch_size, base_x + patch_size // 2)), base_y),
            (base_x, max(0, min(data.shape[0] - patch_size, base_y + patch_size // 2))),
        ]
        seen: set[tuple[int, int]] = set()
        for x, y in candidates:
            if len(seen) >= patches_per_tile:
                break
            if (x, y) in seen:
                continue
            seen.add((x, y))
            patch = data[y : y + patch_size, x : x + patch_size]
            finite = np.isfinite(patch)
            if nodata is not None:
                finite &= patch != nodata
            if float(finite.mean()) < 0.995:
                continue
            # Fill the tiny residual invalid fraction conservatively with the valid median.
            if not bool(finite.all()):
                patch = patch.copy()
                patch[~finite] = float(np.median(patch[finite]))
            out_name = f"waiheke_{tiff_path.stem}_{x}_{y}_{patch_size}m.cdem"
            out_path = output_dir / out_name
            write_cdem(out_path, patch, cell_x)
            print(
                f"[prepared] {out_path}  min={float(patch.min()):.1f}m "
                f"max={float(patch.max()):.1f}m relief={float(patch.max()-patch.min()):.1f}m"
            )
            written += 1
    return written


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tiles", type=int, default=4, help="Number of LINZ COG tiles to download")
    parser.add_argument("--patch-size", type=int, default=1024, help="Square patch size in 1 m pixels")
    parser.add_argument("--patches-per-tile", type=int, default=2)
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

    items = discover_items(max(1, args.tiles))
    args.output.mkdir(parents=True, exist_ok=True)
    args.cache.mkdir(parents=True, exist_ok=True)

    total = 0
    source_manifest: dict = {
        "source": "Auckland - Waiheke Island Coastal LiDAR 1m DEM (2025-2026)",
        "collection": COLLECTION_URL,
        "license": "CC-BY-4.0",
        "attribution": ATTRIBUTION,
        "prepared_format": "Orakai CDEM v1 float32 metres",
        "patch_size_m": args.patch_size,
        "tiles": [],
    }

    for item in items:
        # Fetching the item validates that the STAC record exists. The public LINZ
        # convention then gives us the identically named .tiff asset.
        metadata = fetch_json(item.json_url)
        visual = metadata.get("assets", {}).get("visual", {})
        href = visual.get("href")
        tiff_url = item.tiff_url if not href else item.json_url.rsplit("/", 1)[0] + "/" + pathlib.PurePosixPath(href).name
        tiff_path = args.cache / f"{item.stem}.tiff"
        download(tiff_url, tiff_path)
        count = prepare_tile(tiff_path, args.output, max(128, args.patch_size), max(1, args.patches_per_tile))
        total += count
        source_manifest["tiles"].append({"item": item.json_url, "tiff": tiff_url, "prepared_patches": count})

    manifest_path = args.output.parent / "waiheke_source.json"
    manifest_path.write_text(json.dumps(source_manifest, indent=2) + "\n", encoding="utf-8")
    print(f"\nPrepared {total} real DEM patches.")
    print(f"Attribution: {ATTRIBUTION}")
    print(f"Manifest: {manifest_path}")
    return 0 if total else 1


if __name__ == "__main__":
    raise SystemExit(main())
