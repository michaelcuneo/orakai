#pragma once

#include "CoreMinimal.h"
#include "CubusCore/Generation/CubusTerrainCarving.h"

/**
 * Drainage-aware post-erosion deposition/alluvial pass.
 *
 * Floodplains are built from the same routed stream geometry used by terrain
 * carving, so their width and elevation scale with catchment area and Strahler
 * order instead of relying on generic local smoothing. Alluvial fans are
 * emitted where a steep routed reach breaks onto a substantially gentler
 * downstream reach. A small additive-only local deposition pass remains for
 * shallow concavities and slope breaks after the larger landforms are placed.
 */
struct ORAKAI_API FCubusTerrainDepositionSettings
{
    /** Additive local sediment settling iterations after hydrologic deposition. */
    int32 Iterations = 3;

    /** Maximum slope that can receive substantial local deposition. */
    float MaximumDepositionSlopeDegrees = 9.0f;

    /** Fraction of local concavity filled during one local iteration. */
    float ConcavityFillStrength = 0.30f;

    /** Local deposition at abrupt transitions from steep to gentler ground. */
    float SlopeBreakStrength = 0.18f;

    /** Maximum local terrain raise per iteration, in metres. */
    float MaxDepositPerIterationMeters = 0.45f;

    /** Very light additive lateral settling over depositional surfaces. */
    float FloodplainDiffusion = 0.075f;

    /** Maximum aggradation above the carved floodplain target for major rivers. */
    float MaximumFloodplainAggradationMeters = 4.0f;

    /** Keep active channels below the surrounding depositional surface. */
    float FloodplainChannelExclusionScale = 1.45f;

    /** Minimum incoming reach grade required to form an alluvial fan. */
    float FanMinimumIncomingSlope = 0.075f;

    /** Downstream reach must be this grade or gentler to receive a fan. */
    float FanMaximumDownstreamSlope = 0.045f;

    /** Minimum absolute grade reduction across the fan apex. */
    float FanMinimumSlopeDrop = 0.040f;

    /** Physical fan radius range from small tributaries to mature catchments. */
    float MinimumFanRadiusMeters = 42.0f;
    float MaximumFanRadiusMeters = 280.0f;

    /** Maximum thickness added near a mature fan apex. */
    float MaximumFanThicknessMeters = 5.0f;

    /** Half-angle of the downstream depositional fan sector. */
    float FanHalfAngleDegrees = 58.0f;

    /** Spatial-index cell width for floodplain segment queries. */
    float SegmentBucketSizeMeters = 96.0f;
};

class ORAKAI_API FCubusTerrainDeposition
{
public:
    /**
     * Return a deposited copy of an eroded terrain tile.
     *
     * CarvingSettings provides the river/floodplain geometry used earlier in
     * the pipeline, while DrainageSegments is the already-solved global stream
     * network collected by the loader. Reusing both keeps this pass hydrologic,
     * deterministic and avoids solving drainage again per terrain tile.
     */
    static FCubusTerrainRasterTile DepositTile(
        const FCubusTerrainRasterTile& ErodedTile,
        const FCubusTerrainDepositionSettings& Settings,
        const FCubusTerrainCarvingSettings& CarvingSettings,
        const TArray<FCubusTerrainDrainageSegment>& DrainageSegments
    );
};
