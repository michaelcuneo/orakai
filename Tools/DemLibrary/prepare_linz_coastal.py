#!/usr/bin/env python3
"""Download LINZ coastal 1 m DEM tiles and prepare compact Orakai CDEM patches.

Default source: Auckland - Waiheke Island Coastal LiDAR 1m DEM (2025-2026)
Licence: CC-BY-4.0, Toitu Te Whenua Land Information New Zealand.

Requires Python 3.12+ and Rasterio 1.5+:
    py -m pip install --upgrade numpy rasterio

Run from the Orakai repository root:
    py Tools/DemLibrary/prepare