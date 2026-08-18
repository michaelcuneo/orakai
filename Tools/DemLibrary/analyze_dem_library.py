#!/usr/bin/env python3
"""Analyse prepared Orakai CDEM patches and build a searchable terrain index.

The analyser intentionally derives geometry from the prepared float DEM itself;
it does not infer terrain quality from the source name. Source metadata is only
used for provenance/domain tags.

Run from the Orakai repository root:

    python Tools/DemLibrary/analyze_dem_library.py

Output:
    Content/Cubus/TerrainSources/DEM/Prepared/library_index.json

The index is designed to feed the 500 km terrain composer. It records relief,
slope distributions, roughness/curvature, directional structure, a coarse D8
drainage estimate, sea-level occupancy, scale persistence, terrain classes and
macro/regional/local suitability scores for every .cdem patch.
"""

from __future__ import annotations

import argparse
import collections
import datetime as dt
import json
import math
import pathlib
import struct
from dataclasses import dataclass

import numpy as np

MAGIC = 0x4D454443  # CDEM little endian
VERSION = 1
HEADER = struct.Struct("<IIiiffff")


@dataclass(frozen=True)
class Cdem:
    path: pathlib.Path
    width: int
    height: int
    cell_size_m: float
    minimum_m: float
    maximum_m: float
    mean_m: float
    elevation_m: np.ndarray


def read_cdem(path: pathlib.Path) -> Cdem:
    with path.open("rb") as stream:
        raw_header = stream.read(HEADER.size)
        if len(raw_header) != HEADER.size:
            raise ValueError(f"truncated CDEM header: {path}")
        magic, version, width, height, cell_size_m, minimum_m, maximum_m, mean_m = HEADER.unpack(raw_header)
        if magic != MAGIC or version != VERSION:
            raise ValueError(f"unsupported CDEM header: {path}")
        if width < 2 or height < 2 or cell_size_m <= 0:
            raise ValueError(f"invalid CDEM dimensions: {path}")
        expected_samples = width * height
        samples = np.fromfile(stream, dtype="<f4", count=expected_samples)
        if samples.size != expected_samples:
            raise ValueError(f"truncated CDEM samples: {path}")
        if stream.read(1):
            raise ValueError(f"unexpected bytes after CDEM payload: {path}")
    elevation = samples.reshape((height, width)).astype(np.float64, copy=False)
    if not np.isfinite(elevation).all():
        raise ValueError(f"CDEM contains non-finite samples: {path}")
    return Cdem(path, width, height, cell_size_m, minimum_m, maximum_m, mean_m, elevation)


def percentile(values: np.ndarray, q: float) -> float:
    return float(np.percentile(values, q))


def block_mean(data: np.ndarray, factor: int) -> np.ndarray:
    if factor <= 1:
        return data
    height = (data.shape[0] // factor) * factor
    width = (data.shape[1] // factor) * factor
    if height < factor or width < factor:
        return np.asarray([[float(np.mean(data))]], dtype=np.float64)
    trimmed = data[:height, :width]
    return trimmed.reshape(height // factor, factor, width // factor, factor).mean(axis=(1, 3))


def scale_relief(data: np.ndarray, cell_size_m: float) -> dict[str, float]:
    results: dict[str, float] = {}
    scales = (4.0, 16.0, 64.0, 256.0)
    base_relief = max(1e-6, percentile(data, 95) - percentile(data, 5))
    persistent_scale = cell_size_m

    for target_m in scales:
        factor = max(1, int(round(target_m / cell_size_m)))
        reduced = block_mean(data, factor)
        relief = percentile(reduced, 95) - percentile(reduced, 5) if reduced.size > 4 else float(np.ptp(reduced))
        actual_scale = factor * cell_size_m
        results[f"relief_p90_at_{int(round(target_m))}m"] = float(relief)
        results[f"relief_persistence_at_{int(round(target_m))}m"] = float(np.clip(relief / base_relief, 0.0, 2.0))
        if relief >= base_relief * 0.65:
            persistent_scale = max(persistent_scale, actual_scale)

    results["dominant_structure_scale_m"] = float(persistent_scale)
    return results


def structure_tensor_metrics(dx: np.ndarray, dy: np.ndarray) -> tuple[float, float]:
    jxx = float(np.mean(dx * dx))
    jyy = float(np.mean(dy * dy))
    jxy = float(np.mean(dx * dy))
    trace = jxx + jyy
    discriminant = math.sqrt(max(0.0, (jxx - jyy) ** 2 + 4.0 * jxy * jxy))
    lambda1 = 0.5 * (trace + discriminant)
    lambda2 = 0.5 * (trace - discriminant)
    anisotropy = (lambda1 - lambda2) / max(lambda1 + lambda2, 1e-12)

    # Principal tensor direction is the dominant gradient direction. Terrain
    # structure (ridges/contours) is perpendicular to that direction.
    gradient_angle = 0.5 * math.atan2(2.0 * jxy, jxx - jyy)
    structure_angle = (math.degrees(gradient_angle) + 90.0) % 180.0
    return float(np.clip(anisotropy, 0.0, 1.0)), float(structure_angle)


def drainage_metrics(data: np.ndarray, cell_size_m: float) -> dict[str, float]:
    """Estimate drainage on a coarse grid; enough for patch classification, not simulation."""
    target_cell_m = max(8.0, cell_size_m)
    factor = max(1, int(round(target_cell_m / cell_size_m)))
    z = block_mean(data, factor)
    cell_m = factor * cell_size_m
    height, width = z.shape
    count = height * width
    if count < 9:
        return {
            "drainage_density_km_per_km2": 0.0,
            "channel_fraction": 0.0,
            "maximum_coarse_drainage_km2": 0.0,
        }

    flat = z.ravel()
    receivers = np.full(count, -1, dtype=np.int64)
    diagonal = math.sqrt(2.0)
    offsets = (
        (-1, -1, diagonal), (0, -1, 1.0), (1, -1, diagonal),
        (-1, 0, 1.0),                       (1, 0, 1.0),
        (-1, 1, diagonal),  (0, 1, 1.0),  (1, 1, diagonal),
    )

    for y in range(height):
        for x in range(width):
            index = y * width + x
            here = z[y, x]
            best_slope = 0.0
            best = -1
            for ox, oy, distance in offsets:
                nx = x + ox
                ny = y + oy
                if nx < 0 or nx >= width or ny < 0 or ny >= height:
                    continue
                drop = here - z[ny, nx]
                slope = drop / (cell_m * distance)
                if slope > best_slope:
                    best_slope = slope
                    best = ny * width + nx
            receivers[index] = best

    accumulation = np.ones(count, dtype=np.float64)
    # Strictly lower receivers make this descending elevation order acyclic.
    for index in np.argsort(flat)[::-1]:
        receiver = receivers[index]
        if receiver >= 0:
            accumulation[receiver] += accumulation[index]

    cell_area_km2 = (cell_m * cell_m) / 1_000_000.0
    patch_area_km2 = max(cell_area_km2 * count, cell_area_km2)
    drainage_km2 = accumulation * cell_area_km2
    threshold_km2 = max(0.002, patch_area_km2 * 0.005)
    channel_mask = drainage_km2 >= threshold_km2
    channel_cells = int(np.count_nonzero(channel_mask))
    channel_length_km = channel_cells * cell_m / 1000.0

    return {
        "drainage_density_km_per_km2": float(channel_length_km / patch_area_km2),
        "channel_fraction": float(channel_cells / count),
        "maximum_coarse_drainage_km2": float(np.max(drainage_km2)),
    }


def source_metadata_for(path: pathlib.Path, root: pathlib.Path) -> dict:
    current = path.parent
    while current != root.parent:
        source_json = current / "source.json"
        if source_json.exists():
            try:
                return json.loads(source_json.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError):
                pass
        if current == root:
            break
        current = current.parent
    return {}


def classify(metrics: dict, source_meta: dict) -> tuple[str, list[str]]:
    relief = metrics["relief_p90_m"]
    slope = metrics["mean_slope_deg"]
    steep30 = metrics["slope_fraction_over_30deg"]
    below_zero = metrics["below_zero_fraction"]
    anisotropy = metrics["directional_anisotropy"]
    drainage = metrics["drainage_density_km_per_km2"]

    if relief < 25.0 and slope < 3.0:
        terrain_class = "flat_plain"
    elif relief < 90.0 and slope < 8.0:
        terrain_class = "rolling_lowland"
    elif relief > 650.0 or slope > 27.0:
        terrain_class = "high_mountain"
    elif relief > 300.0 and (slope > 15.0 or steep30 > 0.18):
        terrain_class = "steep_mountain"
    elif relief > 150.0 and drainage > 2.5:
        terrain_class = "dissected_hills"
    elif relief > 100.0:
        terrain_class = "rugged_hills"
    else:
        terrain_class = "gentle_hills"

    if below_zero > 0.02 and terrain_class in {"steep_mountain", "rugged_hills", "dissected_hills"}:
        terrain_class = "rugged_coast"
    elif below_zero > 0.02 and terrain_class in {"flat_plain", "rolling_lowland", "gentle_hills"}:
        terrain_class = "low_coast"

    tags = [terrain_class]
    domain = str(source_meta.get("domain", "unknown"))
    if domain != "unknown":
        tags.append(domain)
    if below_zero > 0.02:
        tags.append("contains_shoreline")
    if relief > 300.0:
        tags.append("high_relief")
    if steep30 > 0.20:
        tags.append("steep")
    if anisotropy > 0.45:
        tags.append("directional_ridges")
    if drainage > 3.0:
        tags.append("dense_drainage")
    if metrics["valley_strength"] > metrics["ridge_strength"] * 1.15:
        tags.append("valley_dominant")
    elif metrics["ridge_strength"] > metrics["valley_strength"] * 1.15:
        tags.append("ridge_dominant")
    else:
        tags.append("balanced_relief")
    return terrain_class, tags


def suitability(metrics: dict) -> dict[str, float]:
    relief = metrics["relief_p90_m"]
    slope = metrics["mean_slope_deg"]
    persistence64 = metrics["relief_persistence_at_64m"]
    persistence256 = metrics["relief_persistence_at_256m"]
    anisotropy = metrics["directional_anisotropy"]
    roughness = metrics["normalized_roughness"]

    macro = (
        0.35 * np.clip(persistence256, 0.0, 1.0)
        + 0.25 * np.clip(persistence64, 0.0, 1.0)
        + 0.20 * np.clip(relief / 500.0, 0.0, 1.0)
        + 0.20 * anisotropy
    )
    regional = (
        0.30 * np.clip(persistence64, 0.0, 1.0)
        + 0.25 * np.clip(relief / 350.0, 0.0, 1.0)
        + 0.25 * np.clip(slope / 22.0, 0.0, 1.0)
        + 0.20 * anisotropy
    )
    local = (
        0.30 * np.clip(relief / 250.0, 0.0, 1.0)
        + 0.30 * np.clip(slope / 20.0, 0.0, 1.0)
        + 0.25 * np.clip(roughness, 0.0, 1.0)
        + 0.15 * np.clip(metrics["drainage_density_km_per_km2"] / 4.0, 0.0, 1.0)
    )
    return {
        "macro_suitability": float(np.clip(macro, 0.0, 1.0)),
        "regional_suitability": float(np.clip(regional, 0.0, 1.0)),
        "local_suitability": float(np.clip(local, 0.0, 1.0)),
    }


def analyse_patch(cdem: Cdem, root: pathlib.Path) -> dict:
    z = cdem.elevation_m
    cell = cdem.cell_size_m

    dy, dx = np.gradient(z, cell, cell)
    slope_radians = np.arctan(np.hypot(dx, dy))
    slope_degrees = np.degrees(slope_radians)

    d2x = np.gradient(dx, cell, axis=1)
    d2y = np.gradient(dy, cell, axis=0)
    laplacian = d2x + d2y
    curvature_scale = max(1e-9, percentile(np.abs(laplacian), 95))
    ridge_strength = percentile(np.maximum(-laplacian, 0.0), 90) / curvature_scale
    valley_strength = percentile(np.maximum(laplacian, 0.0), 90) / curvature_scale

    neighbour_steps = np.concatenate(
        [np.abs(np.diff(z, axis=0)).ravel(), np.abs(np.diff(z, axis=1)).ravel()]
    )
    relief_p90 = percentile(z, 95) - percentile(z, 5)
    normalized_roughness = percentile(neighbour_steps, 90) / max(relief_p90, 1.0)
    normalized_roughness = float(np.clip(normalized_roughness * 30.0, 0.0, 1.0))

    anisotropy, structure_angle = structure_tensor_metrics(dx, dy)
    scale = scale_relief(z, cell)
    drainage = drainage_metrics(z, cell)

    metrics: dict[str, float] = {
        "minimum_elevation_m": float(np.min(z)),
        "maximum_elevation_m": float(np.max(z)),
        "mean_elevation_m": float(np.mean(z)),
        "median_elevation_m": percentile(z, 50),
        "elevation_p05_m": percentile(z, 5),
        "elevation_p95_m": percentile(z, 95),
        "relief_full_m": float(np.ptp(z)),
        "relief_p90_m": float(relief_p90),
        "mean_slope_deg": float(np.mean(slope_degrees)),
        "median_slope_deg": percentile(slope_degrees, 50),
        "slope_p90_deg": percentile(slope_degrees, 90),
        "slope_p95_deg": percentile(slope_degrees, 95),
        "slope_fraction_over_15deg": float(np.mean(slope_degrees > 15.0)),
        "slope_fraction_over_30deg": float(np.mean(slope_degrees > 30.0)),
        "slope_fraction_over_45deg": float(np.mean(slope_degrees > 45.0)),
        "maximum_neighbour_step_m": float(np.max(neighbour_steps)),
        "neighbour_step_p90_m": percentile(neighbour_steps, 90),
        "normalized_roughness": normalized_roughness,
        "ridge_strength": float(np.clip(ridge_strength, 0.0, 1.0)),
        "valley_strength": float(np.clip(valley_strength, 0.0, 1.0)),
        "directional_anisotropy": anisotropy,
        "dominant_structure_angle_deg": structure_angle,
        "below_zero_fraction": float(np.mean(z <= 0.0)),
        "near_sea_level_fraction": float(np.mean((z >= -2.0) & (z <= 5.0))),
    }
    metrics.update(scale)
    metrics.update(drainage)
    metrics.update(suitability(metrics))

    source_meta = source_metadata_for(cdem.path, root)
    terrain_class, tags = classify(metrics, source_meta)

    relative = cdem.path.relative_to(root).as_posix()
    return {
        "path": relative,
        "source_key": source_meta.get("key", cdem.path.parent.name.lower()),
        "source_title": source_meta.get("title", cdem.path.parent.name),
        "source_domain": source_meta.get("domain", "unknown"),
        "terrain_class": terrain_class,
        "tags": tags,
        "width": cdem.width,
        "height": cdem.height,
        "cell_size_m": cdem.cell_size_m,
        "extent_x_m": float((cdem.width - 1) * cdem.cell_size_m),
        "extent_y_m": float((cdem.height - 1) * cdem.cell_size_m),
        **metrics,
    }


def analyse_library(root: pathlib.Path, output_path: pathlib.Path | None = None) -> dict:
    root = root.resolve()
    paths = sorted(root.rglob("*.cdem"))
    if not paths:
        raise RuntimeError(f"No .cdem patches found below {root}")

    entries: list[dict] = []
    failures: list[dict] = []
    for index, path in enumerate(paths, start=1):
        try:
            entry = analyse_patch(read_cdem(path), root)
            entries.append(entry)
            print(
                f"[analyse] {index:3d}/{len(paths)} {entry['path']}  "
                f"class={entry['terrain_class']} relief={entry['relief_p90_m']:.1f}m "
                f"slope={entry['mean_slope_deg']:.1f}deg "
                f"macro={entry['macro_suitability']:.2f} "
                f"regional={entry['regional_suitability']:.2f} local={entry['local_suitability']:.2f}"
            )
        except Exception as exc:  # Keep one corrupt patch from destroying the whole index.
            print(f"[error] {path}: {type(exc).__name__}: {exc}")
            failures.append({"path": path.relative_to(root).as_posix(), "error": f"{type(exc).__name__}: {exc}"})

    classes = collections.Counter(entry["terrain_class"] for entry in entries)
    sources = collections.Counter(entry["source_key"] for entry in entries)
    index_data = {
        "schema_version": 1,
        "generated_utc": dt.datetime.now(dt.timezone.utc).isoformat(),
        "prepared_root": "Content/Cubus/TerrainSources/DEM/Prepared",
        "patch_count": len(entries),
        "failed_patch_count": len(failures),
        "terrain_classes": dict(sorted(classes.items())),
        "sources": dict(sorted(sources.items())),
        "patches": entries,
        "failures": failures,
    }

    destination = output_path.resolve() if output_path else root / "library_index.json"
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_text(json.dumps(index_data, indent=2) + "\n", encoding="utf-8")

    print()
    print(f"DEM INDEX: {len(entries)} patches, {len(failures)} failures")
    print("Terrain classes:")
    for name, count in sorted(classes.items()):
        print(f"  {name:24s} {count}")
    print(f"Index: {destination}")
    return index_data


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--prepared-root",
        type=pathlib.Path,
        default=pathlib.Path("Content/Cubus/TerrainSources/DEM/Prepared"),
    )
    parser.add_argument("--output", type=pathlib.Path)
    args = parser.parse_args()

    try:
        result = analyse_library(args.prepared_root, args.output)
    except Exception as exc:
        print(f"DEM analysis failed: {type(exc).__name__}: {exc}")
        return 1
    return 0 if result["patch_count"] > 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
