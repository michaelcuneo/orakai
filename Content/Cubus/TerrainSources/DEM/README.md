# Orakai Real DEM Terrain Sources

This folder contains prepared real-world elevation patches used as geological source material for seeded Orakai worlds.

## Physical-scale terrain hierarchy

Orakai does **not** stretch a 1 km LiDAR crop into a mountain or mountain range. Source physical extent is preserved and different DEM tiers provide different spatial frequencies of the world.

```text
500 km x 500 km Orakai island
    |
    +-- Macro128km
    |      ~131 km real source windows
    |      128 m/sample, ~1024 x 1024
    |      mountain belts, major basins, long drainage systems
    |
    +-- Macro64km
    |      ~66 km real source windows
    |      64 m/sample, ~1024 x 1024
    |      ranges, plateaus, major valleys
    |
    +-- Macro32km
    |      ~33 km real source windows
    |      32 m/sample, ~1024 x 1024
    |      complete mountains and regional drainage
    |
    +-- Regional8km
    |      ~8 km real source windows
    |      8 m/sample, ~1024 x 1024
    |      valleys, hills, coastal systems
    |
    +-- Local1km
           ~1 km real LiDAR windows
           1 m/sample, 1024 x 1024
           cliffs, gullies, ridge texture and local landform detail
```

The global 4097 x 4097 world DEM is roughly 122 m/sample across 500 km. Runtime global terrain composition uses the 128/64/32 km tiers. The 8 km and 1 km tiers are source material for streamed regional/local refinement and finally Cubus volumetric density.

## Build the multiscale library

The large-scale tiers are derived from the **New Zealand Contour-Interpolated 8 m DEM** (LINZ, CC BY 4.0). LINZ describes this source as suitable for cartographic visualisation rather than terrain analysis, so Orakai treats it as geometric source material and recomputes hydrology / landscape evolution after composition.

Prepare all large and regional tiers with:

```powershell
python -m pip install --upgrade rasterio numpy
python Tools/DemLibrary/prepare_multiscale_dem_library.py
```

Defaults prepare approximately:

```text
Macro128km    12 patches
Macro64km     24 patches
Macro32km     48 patches
Regional8km   96 patches
```

For more source diversity:

```powershell
python Tools/DemLibrary/prepare_multiscale_dem_library.py --count-multiplier 2
```

For only the production global tiers:

```powershell
python Tools/DemLibrary/prepare_macro_dem_library.py
```

That compatibility command now builds `Macro128km`, `Macro64km`, and `Macro32km`; the old single-size macro system is retired.

The multiscale builder reads every LINZ COG intersecting a requested NZTM source window and resamples directly into the target ~1024² output. A 131 km source area therefore remains a genuine 131 km terrain sample without allocating a 16,384² intermediate raster or enlarging a smaller patch.

Prepared files live under:

```text
Content/Cubus/TerrainSources/DEM/Prepared/
    Macro128km/
    Macro64km/
    Macro32km/
    Regional8km/
    ...1 m LiDAR source folders...
```

## High-resolution 1 m library

The 1 m library is deliberately broad and includes coastal, mountain, volcanic, valley, plateau and lowland source families from LINZ collections across New Zealand. It is prepared with:

```powershell
python Tools/DemLibrary/prepare_linz_library.py
```

Those 1024 x 1024 files are approximately 1 km x 1 km and are **local morphology only**. They are useful for cliffs, gullies, individual ridge shoulders, stream cuts and surface character, not for deciding where an entire mountain range exists.

## Runtime global composition

`CubusDemPyramid` loads the three production macro tiers independently:

```text
Macro128km  dominant long-wavelength relief
     +
Macro64km   medium-scale real terrain contribution
     +
Macro32km   mountain/regional real terrain contribution
     =
500 km global relief field
```

Every tier is sampled at approximately its real physical extent. Seeded overlapping placement, mirror/quarter-turn transforms and terrain-character selection create a new arrangement each world. The structured coastline SDF is then applied, followed by global hydrology and landscape evolution.

The runtime logs the number of sources actually loaded from each tier so a missing or under-populated library is immediately visible.

## Terrain index

Run:

```powershell
python Tools/DemLibrary/analyze_dem_library.py
```

This writes:

```text
Content/Cubus/TerrainSources/DEM/Prepared/library_index.json
```

The index records relief, slope distributions, roughness, ridge/valley strength, structural direction, drainage estimates, scale persistence, terrain class and macro/regional/local suitability for each CDEM patch. All preparation scripts rebuild it automatically unless explicitly told not to.

## Source data and licensing

Provider: Toitu Te Whenua Land Information New Zealand (LINZ).

Licence: CC BY 4.0.

Attribution: `Sourced from LINZ. CC BY 4.0`.

Source GeoTIFFs are public Cloud Optimised GeoTIFFs in the LINZ `nz-coastal` and `nz-elevation` AWS Open Data buckets. They are cached beneath `Saved/DemSourceCache` when local caching is required and are not shipped directly with the game.

Prepared `.cdem` files are Git-LFS assets and are staged as Non-UFS content for packaged builds.

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
