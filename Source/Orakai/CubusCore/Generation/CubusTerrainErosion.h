#pragma once

#include "CoreMinimal.h"
#include "CubusCore/Generation/CubusTerrainRaster.h"

/**
 * Post-carving hillslope erosion/weathering over the authored 1 m DEM.
 *
 * This stage is intentionally local and conservative: broad watershed topology
 * is already owned by drainage/carving. Erosion relaxes over-steep slopes,
 * transports material downslope and deepens naturally concave rills/gullies.
 * The loader gives each tile a wide halo so repeated stencil passes remain
 * deterministic and seam-safe across 512 m tile boundaries.
 */
struct ORAKAI_API FCubusTerrainErosionSettings
{
    /** Number of Jacobi-style erosion iterations. Limited by the tile halo. */
    int32 Iterations = 5;

    /** Approximate repose angle above which loose material moves downslope. */
    float TalusAngleDegrees = 33.0f;

    /** Fraction of excess talus transported during one iteration. */
    float ThermalTransport = 0.22f;

    /** Strength of extra incision in locally concave, draining terrain. */
    float ConcavityIncision = 0.24f;

    /** Maximum local gully incision applied in a single iteration, metres. */
    float MaxIncisionPerIterationMeters = 0.65f;

    /** Slopes gentler than this receive little or no fluvial incision. */
    float MinimumIncisionSlopeDegrees = 2.5f;

    /** Soft diffusion applied only on convex/rough slopes after transport. */
    float HillslopeDiffusion = 0.055f;
};

class ORAKAI_API FCubusTerrainErosion
{
public:
    /** Return an eroded copy of an already carved DEM tile. */
    static FCubusTerrainRasterTile ErodeTile(
        const FCubusTerrainRasterTile& CarvedTile,
        const FCubusTerrainErosionSettings& Settings
    );
};
