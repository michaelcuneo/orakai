#pragma once

#include "CoreMinimal.h"

/**
 * Physical settings for the pre-erosion terrain skeleton.
 *
 * Every distance is authored in metres/kilometres. This layer deliberately has
 * no voxel-size setting: it describes the landscape that will later be sampled
 * into whatever density resolution the renderer needs.
 */
struct ORAKAI_API FCubusTerrainStructureSettings
{
    int32 Seed = 1337;

    float BaseElevationMeters = 40.0f;
    float RegionalReliefMeters = 220.0f;
    float RegionalScaleKm = 64.0f;

    /** Approximate spacing between major mountain systems. */
    float MountainSystemSpacingKm = 26.0f;

    /** Distance between control nodes along a connected range spine. */
    float RangeNodeSpacingKm = 11.0f;

    /** Core rock belt and broad foothill widths around each range spine. */
    float RangeCoreHalfWidthKm = 2.2f;
    float RangeShoulderHalfWidthKm = 7.5f;

    /** Maximum uplift contributed by the range itself. */
    float RangeReliefMeters = 1150.0f;

    /** Node jitter bends a system into long natural arcs instead of straight bands. */
    float AlongSpineJitter = 0.18f;
    float AcrossSpineJitter = 0.24f;

    /** Sparse lower ridges connecting neighbouring systems. */
    float BranchChance = 0.13f;
    float BranchReliefScale = 0.52f;
    float BranchWidthScale = 0.58f;

    /** Massifs sit on the structural spine rather than appearing as free noise peaks. */
    float MassifRadiusKm = 3.4f;
    float MassifReliefMeters = 620.0f;

    /** Broad passes locally reduce the range crest before drainage is solved. */
    float PassChance = 0.42f;
    float PassHalfWidthKm = 1.2f;
    float PassDepthMeters = 260.0f;

    /** Broad lowlands between mountain systems. */
    float BasinCellSizeKm = 22.0f;
    float BasinRadiusKm = 13.0f;
    float BasinDepthMeters = 240.0f;

    // ---------------------------------------------------------------------
    // Nested pre-erosion morphology.
    //
    // These are subordinate to the explicit mountain/basin skeleton above.
    // They ensure that a local 1-4 km crop already contains believable relief
    // for later drainage and erosion instead of being one smooth macro slope.
    // ---------------------------------------------------------------------

    /** Broad hill masses and intermontane shoulders. */
    float HillScaleKm = 3.2f;
    float HillReliefMeters = 230.0f;

    /** Branching subordinate ridge/spur scale. */
    float SpurScaleKm = 1.15f;
    float SpurReliefMeters = 145.0f;

    /** Pre-existing shallow swales that drainage can capture and deepen. */
    float SwaleScaleMeters = 360.0f;
    float SwaleDepthMeters = 42.0f;

    /** Fine rock/soil morphology retained by the final 1 m DEM. */
    float FineScaleMeters = 95.0f;
    float FineReliefMeters = 16.0f;

    /** Low-frequency domain warping prevents repetitive parallel ridges. */
    float MorphologyWarpMeters = 520.0f;
};

/** Diagnostics for one sample of the broad, pre-drainage terrain skeleton. */
struct ORAKAI_API FCubusTerrainStructureSample
{
    float HeightMeters = 0.0f;
    float RegionalHeightMeters = 0.0f;
    float RangeWeight = 0.0f;
    float RangeCoreWeight = 0.0f;
    float MassifWeight = 0.0f;
    float BasinWeight = 0.0f;
    float PassWeight = 0.0f;
    float HillWeight = 0.0f;
    float SpurWeight = 0.0f;
    float SwaleWeight = 0.0f;
    float FineWeight = 0.0f;
    float DistanceToRangeMeters = MAX_flt;
};

/**
 * Deterministic structural landscape generator.
 *
 * Mountain systems are explicit connected polylines. Massifs are attached to
 * their control nodes, sparse branches connect neighbouring systems, and basin
 * bowls occupy the spaces between ranges. Multi-scale morphology is layered
 * underneath later hydrology/erosion so local DEM crops already contain nested
 * hill, ridge, spur and swale structure without inventing mountains from noise.
 */
class ORAKAI_API FCubusTerrainStructure
{
public:
    static FCubusTerrainStructureSample Sample(
        double WorldXmeters,
        double WorldYmeters,
        const FCubusTerrainStructureSettings& Settings
    );
};
