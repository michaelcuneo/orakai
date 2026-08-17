#!/usr/bin/env python3
"""Compatibility entrypoint for Orakai's production macro DEM pyramid.

The old single ~32 km macro library was not large enough to represent complete
mountain systems. This command now prepares the three global physical-scale
source tiers instead:

    Macro128km  -> ~131 km source windows at 128 m/sample
    Macro64km   ->  ~66 km source windows at  64 m/sample
    Macro32km   ->  ~33 km source windows at  32 m/sample

For the full pyramid including Regional8km, run
Tools/DemLibrary/prepare_multiscale_dem_library.py directly.
"""

from __future__ import annotations

import sys

import prepare_multiscale_dem_library as multiscale


def main() -> int:
    print("prepare_macro_dem_library.py now builds the 128/64/32 km physical-scale macro pyramid.")
    original_argv = sys.argv
    try:
        # Preserve explicit user options while supplying the production macro tiers.
        sys.argv = [
            original_argv[0],
            "--tier", "macro128",
            "--tier", "macro64",
            "--tier", "macro32",
            *original_argv[1:],
        ]
        return multiscale.main()
    finally:
        sys.argv = original_argv


if __name__ == "__main__":
    raise SystemExit(main())
