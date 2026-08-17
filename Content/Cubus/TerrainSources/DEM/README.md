# Orakai Real DEM Terrain Sources

This folder contains prepared real-world elevation patches used as geological source material for seeded Orakai islands.

## Current source

**Auckland - Waiheke Island Coastal LiDAR 1m DEM (2025-2026)**

- Provider/licensor: Toitu Te Whenua Land Information New Zealand (LINZ)
- Product: bare-earth 1 m coastal DEM
- CRS of source GeoTIFFs: NZGD2000 / New Zealand Transverse Mercator 2000 (EPSG:2193)
- Licence: CC BY 4.0
- Attribution: `Licensed by Toitu Te Whenua Land Information New Zealand for re-use under CC-BY-4.0.`

The source GeoTIFFs are public Cloud Optimised GeoTIFFs in the LINZ `nz-coastal` AWS Open Data bucket. They are downloaded into `Saved/DemSourceCache` and are **not** shipped directly with the game.

## Prepare the library

From the repository root on Windows:

```powershell
py -m pip install rasterio numpy
py Tools/DemLibrary/prepare_linz_coastal.py
```

The preparer downloads a small subset of the Waiheke 1 m DEM collection and writes compact Orakai `.cdem` patches beneath:

```text
Content/Cubus/TerrainSources/DEM/Prepared/Waiheke/
```

`.cdem` files are Git-LFS assets and are staged as Non-UFS content for packaged builds.

## What Orakai does with the data

The game does **not** reproduce Waiheke as its playable map. Absolute NZ elevation datum is discarded. A world seed chooses real DEM patches and then independently:

- crops/selects source landforms;
- rotates and mirrors them;
- spatially warps their coordinates;
- blends multiple real landforms;
- rescales relief;
- applies a seeded 500 m island coastline and ocean perimeter;
- solves hydrology;
- optionally runs bounded erosion/hillslope evolution;
- feeds the resulting surface into Cubus density/volumetric geology.

The intent is to use real geology as high-quality source material while still producing a different playable island for different seeds.

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
