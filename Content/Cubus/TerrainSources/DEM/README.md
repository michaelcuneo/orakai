# Orakai Real DEM Terrain Sources

This folder contains prepared real-world elevation patches used as geological source material for seeded Orakai worlds.

## Source library

Orakai's DEM source library is built from open Toitu Te Whenua Land Information New Zealand (LINZ) elevation collections. The current preparer includes deliberately different terrain families:

- Auckland - Waiheke Island Coastal LiDAR 1 m DEM (2025-2026)
- Northland - Whangarei to Bream Bay Coastal LiDAR 1 m DEM (2025)
- Southland - Bluff Coastal LiDAR 1 m DEM (2025)
- Otago - Dunedin Coastal LiDAR 1 m DEM (2025)
- West Coast - Okuru Coastal LiDAR 1 m DEM (2026)
- Canterbury - Banks Peninsula LiDAR 1 m DEM (2023)
- Taranaki LiDAR 1 m DEM (2021)

Provider/licensor: Toitu Te Whenua Land Information New Zealand (LINZ)

Licence: CC BY 4.0

Attribution: `Licensed by Toitu Te Whenua Land Information New Zealand for re-use under CC-BY-4.0.`

The source GeoTIFFs are public Cloud Optimised GeoTIFFs in the LINZ `nz-coastal` and `nz-elevation` AWS Open Data buckets. They are downloaded into `Saved/DemSourceCache` and are **not** shipped directly with the game.

## Prepare the library

From the repository root with the project's active Python environment:

```powershell
python -m pip install --upgrade rasterio numpy
python Tools/DemLibrary/prepare_linz_library.py
```

The multi-source preparer scans each selected STAC collection, prioritises substantial COG assets, skips sparse edge tiles, ranks mostly-valid 1024 x 1024 m crops by terrain relief, and writes compact Orakai `.cdem` patches beneath:

```text
Content/Cubus/TerrainSources/DEM/Prepared/
  Waiheke/
  WhangareiBreamBay/
  Bluff/
  Dunedin/
  Okuru/
  BanksPeninsula/
  Taranaki/
```

Useful subsets:

```powershell
python Tools/DemLibrary/prepare_linz_library.py --source coastal
python Tools/DemLibrary/prepare_linz_library.py --source land
python Tools/DemLibrary/prepare_linz_library.py --source taranaki --source okuru
python Tools/DemLibrary/prepare_linz_library.py --tiles-per-source 3
```

The original Waiheke-only preparer remains available for focused testing:

```powershell
python Tools/DemLibrary/prepare_linz_coastal.py
```

`.cdem` files are Git-LFS assets and are staged as Non-UFS content for packaged builds. `CubusDemIslandGenerator` discovers them recursively, so newly prepared source folders become available without hardcoding individual CDEM paths in C++.

## What Orakai does with the data

The game does **not** reproduce any source location as its playable world. Absolute source elevation datum is discarded. A world seed can choose real DEM patches and then independently:

- crop/select source landforms;
- rotate and mirror them;
- spatially warp their coordinates;
- blend multiple real landforms;
- rescale relief;
- place them into the 500 km world hierarchy at an appropriate scale;
- solve hydrology;
- optionally run bounded erosion/hillslope evolution;
- feed regional/local surfaces into Cubus density/volumetric geology.

The 1 m CDEM library is primarily local/regional source material. It must not be stretched directly across the 500 km global world. Large-scale terrain structure should be assembled at a coarser tier, with these high-resolution sources providing realistic landform character during regional refinement.

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
