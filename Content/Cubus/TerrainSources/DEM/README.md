# Orakai Real DEM Terrain Sources

This folder contains prepared real-world elevation patches used as geological source material for seeded Orakai worlds.

## Source hierarchy

Orakai now uses two real-DEM source tiers rather than stretching one resolution across the whole world.

### Macro tier: 500 km world structure

Source: **New Zealand Contour-Interpolated 8 m DEM** (LINZ, CC BY 4.0).

LINZ describes this dataset as suitable for cartographic visualisation rather than terrain analysis. Orakai therefore uses it only as large-scale geometric source material. The remixed world is subsequently reconditioned by Orakai's own hydrology and erosion systems.

The macro preparer extracts roughly 32 km source windows, block-averages them to roughly 128 m per sample, and stores them under:

```text
Content/Cubus/TerrainSources/DEM/Prepared/Macro/
```

The runtime 500 km composer quilts these macro patches across the island at approximately their real physical scale, with overlap and seeded quarter-turn/mirror transforms. It does **not** stretch a 1 km LiDAR patch into a 100 km mountain range.

Prepare this tier with:

```powershell
python Tools/DemLibrary/prepare_macro_dem_library.py
```

The script rebuilds `library_index.json` after preparing the macro patches.

### Regional/local tier: high-resolution morphology

The high-resolution source library uses deliberately different LINZ 1 m LiDAR terrain families:

- Auckland - Waiheke Island Coastal LiDAR 1 m DEM (2025-2026)
- Northland - Whangarei to Bream Bay Coastal LiDAR 1 m DEM (2025)
- Southland - Bluff Coastal LiDAR 1 m DEM (2025)
- Otago - Dunedin Coastal LiDAR 1 m DEM (2025)
- West Coast - Okuru Coastal LiDAR 1 m DEM (2026)
- Canterbury - Banks Peninsula LiDAR 1 m DEM (2023)
- Taranaki LiDAR 1 m DEM (2021)

Prepare the local/regional library with:

```powershell
python -m pip install --upgrade rasterio numpy
python Tools/DemLibrary/prepare_linz_library.py
```

The 1 m CDEM files remain local/regional source material for streamed terrain refinement and Cubus density. They are never intended to define the entire 500 km macro surface directly.

## Terrain index

Run:

```powershell
python Tools/DemLibrary/analyze_dem_library.py
```

This writes:

```text
Content/Cubus/TerrainSources/DEM/Prepared/library_index.json
```

The index records relief, slope distributions, roughness, ridge/valley strength, structural direction, drainage estimates, scale persistence, terrain class and macro/regional/local suitability for each CDEM patch.

The 500 km runtime composer reads this index and chooses macro source patches according to their measured terrain character. Interior lattice cells preferentially select higher-relief/high-macro-suitability sources while outward cells trend toward gentler terrain. Seeded selection, mirror/quarter-turn transforms and overlap produce a different quilt for each world seed, after which the global hydrology/evolution pass makes the assembled terrain belong to one drainage system.

## Source data and licensing

Provider: Toitu Te Whenua Land Information New Zealand (LINZ).

Licence: CC BY 4.0.

Attribution: `Sourced from LINZ. CC BY 4.0`.

Source GeoTIFFs are public Cloud Optimised GeoTIFFs in the LINZ `nz-coastal` and `nz-elevation` AWS Open Data buckets. They are cached beneath `Saved/DemSourceCache` and are not shipped directly with the game.

Prepared `.cdem` files are Git-LFS assets and are staged as Non-UFS content for packaged builds.

## Production hierarchy

```text
500 km x 500 km world
    |
    +-- Macro DEM quilt (~128 m source samples)
    |      real 8 m DEM -> ~32 km CDEM patches -> overlap/remix
    |
    +-- Global hydrology / landscape evolution
    |
    +-- Regional streamed refinement
    |      classified 1 m LiDAR source patches
    |
    +-- Cubus volumetric density
           cliffs, caves, overhangs, digging, sub-metre detail
```

## CDEM v1

Little-endian binary format:

```text
uint32 magic       = 'CDEM'
uint32 version     = 1
int32  width
int32  height
float  cellSizeM
float  minimumElevationM
float  maximumElevationM
float  meanElevationM
float  elevationM[width * height]
```

Elevation samples are row-major float32 metres.
