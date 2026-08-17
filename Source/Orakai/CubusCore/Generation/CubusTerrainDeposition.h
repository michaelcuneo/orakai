#pragma once

#include "CoreMinimal.h"
#include "CubusCore/Generation/CubusTerrainRaster.h"

/**
 * Conservative post-erosion deposition/alluvial pass.
 *
 * Material is accumulated primarily in shallow concave terrain and at slope
 * breaks. The pass does not invent rivers; it broadens natural valley bottoms,
 * softens alluvial shoulders and creates fan-like deposition zones before the
 * final fine erosion pass cuts smaller drainage back into the surface.
 */
struct ORAKAI_API FCubusTerrainDepositionSettings
{
    int32 Iterations = 3;

    /** Maximum slope that can receive substantial deposition. */
    float MaximumDepositionSlopeDegrees = 9.0f;

    /** Fraction of local concavity filled during one iteration. */
    float ConcavityFillStrength = 0.30f;

    /** Deposition at abrupt transitions from steep to gentler ground. */
    float SlopeBreakStrength = 0.18f;

    /** Maximum terrain raise per iteration, in metres. */
    float MaxDepositPerIterationMeters = 0.45f;

    /** Very light lateral diffusion over depositional surfaces. */
    float FloodplainDiffusion = 0.075f;
};

class ORAKAI_API FCubusTerrainDeposition
{
public:
    static FCubusTerrainRasterTile DepositTile(
        const FCubusTerrainRasterTile& ErodedTile,
        const FCubusTerrainDepositionSettings& Settings
    );
};
