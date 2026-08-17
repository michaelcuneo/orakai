#!/usr/bin/env python3
"""Prepare large-scale real DEM source patches for Orakai's 500 km world.

Source: LINZ New Zealand Contour-Interpolated 8m DEM (CC BY 4.0).
The source is intended by LINZ for cartographic visualisation rather than terrain
analysis. Orakai therefore uses it only as macro geometric source material; the
composed world is subsequently reconditioned by Orakai hydrology/erosion.

Each accepted source COG contributes one or more ~16.4 km patches. They are
block-averaged from 8 m to 128 m and written as ordinary CDEM files beneath:

    Content/Cubus/TerrainSources/DEM/Prepared/Macro

That physical scale is preserved by the runtime composer. These files are not
stretched from 1 km to 100 km.
"""

from __future__ import annotations

import argparse
import json
import pathlib

import numpy as np

import prepare_linz_coastal as prep

COLLECTION_URL = (
    "https://nz-elevation.s3-ap-southeast-2.amazonaws.com/"
    "new-zealand/new-zealand-contour/dem_8m/2193/collection.json"
)
ATTRIBUTION = "Sourced from LINZ. CC BY 4.0"


def block_mean(data: np.ndarray, factor: int) -> np.ndarray:
    height = (data.shape[0] // factor) * factor
    width = (data.shape[1] // factor) * factor
    trimmed = data[:height, :width]
    return trimmed.reshape(height // factor, factor, width // factor, factor).mean(axis=(1, 3))


def prepare_macro_tile(
    tiff_path: pathlib.Path,
    output_dir: pathlib.Path,
    source_window_pixels: int,
    target_cell_m: float,
    patches_per_tile: int,
    min_valid_fraction: float,
) -> int:
    import rasterio

    with rasterio.open(tiff_path) as src:
        if src.count != 1:
            return 0
        cell_x = abs(float(src.transform.a))
        cell_y = abs(float(src.transform.e))
        if abs(cell_x - cell_y) > 1e-3:
            return 0

        masked = src.read(1, masked=True)
        valid = ~np.ma.getmaskarray(masked)
        data = np.asarray(masked.filled(np.nan), dtype=np.float32)
        valid &= np.isfinite(data)

        choices = prep.rank_windows(
            data,
            valid,
            source_window_pixels,
            min_valid_fraction,
            patches_per_tile,
        )
        if not choices:
            print(f"[skip] no macro crop in {tiff_path.name}")
            return 0

        factor = max(1, int(round(target_cell_m / cell_x)))
        written = 0
        for choice in choices:
            x, y = choice.x, choice.y
            patch = data[y : y + source_window_pixels, x : x + source_window_pixels].copy()
            patch_valid = valid[y : y + source_window_pixels, x : x + source_window_pixels]
            if not bool(patch_valid.all()):
                patch[~patch_valid] = float(np.median(patch[patch_valid]))

            reduced = block_mean(patch, factor).astype(np.float32)
            out_cell_m = cell_x * factor
            out_name = f"macro_{tiff_path.stem}_{x}_{y}_{int(round(out_cell_m))}m.cdem"
            out_path = output_dir / out_name
            prep.write_cdem(out_path, reduced, out_cell_m)
            extent_km = (reduced.shape[1] - 1) * out_cell_m / 1000.0
            print(
                f"[macro] {out_path.name} {reduced.shape[1]}x{reduced.shape[0]} "
                f"cell={out_cell_m:.1f}m extent={extent_km:.1f}km "
                f"relief={float(np.percentile(reduced,95)-np.percentile(reduced,5)):.1f}m"
            )
            written += 1
        return written


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--patch-count", type=int, default=24)
    parser.add_argument("--source-window-pixels", type=int, default=2048)
    parser.add_argument("--target-cell-m", type=float, default=128.0)
    parser.add_argument("--patches-per-tile", type=int, default=2)
    parser.add_argument("--min-valid-fraction", type=float, default=0.98)
    parser.add_argument(
        "--output",
        type=pathlib.Path,
        default=pathlib.Path("Content/Cubus/TerrainSources/DEM/Prepared/Macro"),
    )
    parser.add_argument(
        "--cache",
        type=pathlib.Path,
        default=pathlib.Path("Saved/DemSourceCache/Macro8m"),
    )
    args = parser.parse_args()

    args.output.mkdir(parents=True, exist_ok=True)
    args.cache.mkdir(parents=True, exist_ok=True)
    prep.COLLECTION_URL = COLLECTION_URL

    items = prep.discover_items()
    print(f"Found {len(items)} national 8 m DEM items; resolving source assets...")
    candidates = prep.resolve_candidates(items)

    total = 0
    attempted = 0
    records: list[dict] = []
    for candidate in candidates:
        if total >= max(1, args.patch_count):
            break
        attempted += 1
        tiff_path = args.cache / f"{candidate.item.stem}.tiff"
        prep.download(candidate.tiff_url, tiff_path)
        count = prepare_macro_tile(
            tiff_path,
            args.output,
            max(512, args.source_window_pixels),
            max(32.0, args.target_cell_m),
            max(1, args.patches_per_tile),
            min(1.0, max(0.80, args.min_valid_fraction)),
        )
        total += count
        records.append(
            {
                "item": candidate.item.json_url,
                "tiff": candidate.tiff_url,
                "prepared_patches": count,
            }
        )

    source_record = {
        "key": "macro_nz_8m",
        "title": "New Zealand Contour-Interpolated 8m DEM",
        "domain": "macro",
        "collection": COLLECTION_URL,
        "license": "CC-BY-4.0",
        "attribution": ATTRIBUTION,
        "target_cell_m": args.target_cell_m,
        "source_window_pixels": args.source_window_pixels,
        "prepared_patches": total,
        "attempted_tiles": attempted,
        "tiles": records,
        "usage_note": "Macro source geometry only; Orakai recomputes hydrology and erosion after composition.",
    }
    (args.output / "source.json").write_text(json.dumps(source_record, indent=2) + "\n", encoding="utf-8")

    if total:
        print(f"Prepared {total} macro DEM patches.")
        print("Rebuilding terrain index...")
        import analyze_dem_library
        analyze_dem_library.analyse_library(pathlib.Path("Content/Cubus/TerrainSources/DEM/Prepared"))
        return 0
    print("Prepared 0 macro DEM patches.")
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
