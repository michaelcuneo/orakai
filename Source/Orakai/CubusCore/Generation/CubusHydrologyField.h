#pragma once

#include "CoreMinimal.h"
#include "CubusCore/Generation/CubusTerrainForm.h"

/**
 * Immutable settings for the deterministic macro hydrology solver.
 *
 * Hydrology is solved on a deliberately coarser grid than density meshing.
 * The coarse grid represents catchments and flow routing; the final density
 * field then evaluates smooth banks and channels around those flow lines.
 */
struct ORAKAI_API FCubusHydrologySettings
{
    bool bEnabled = false;

    FCubusTerrainFormSettings TerrainFormSettings;
    int32 TerrainOffsetX = 0;
    int32 TerrainOffsetY = 0;
    int32 RiverSeed = 0;

    float SeaLevel = 8.0f;

    /** Canonical voxel metres represented by one hydrology cell. */
    float CellSize = 16.0f;

    /** Interior cells per cached hydrology region. */
    int32 RegionCellCount = 96;

    /** Extra cells solved around each region to stabilise boundary routing. */
    int32 HaloCellCount = 48;

    /** Contributing hydrology cells required before a stream is exposed. */
    float SourceAccumulation = 160.0f;

    /** Accumulation at which a channel reaches full major-river strength. */
    float MajorRiverAccumulation = 2200.0f;

    /** Small headwater channel half-width in canonical voxel metres. */
    float ChannelHalfWidth = 2.5f;

    /** Broad walkable valley half-width at full river strength. */
    float ValleyHalfWidth = 28.0f;

    /** Broad valley incision at full river strength. */
    float ValleyDepth = 7.0f;

    /** Additional smooth channel incision at the centreline. */
    float ChannelDepth = 3.0f;
};

/** One continuous hydrology query at a world-space XY coordinate. */
struct ORAKAI_API FCubusHydrologySample
{
    float DistanceToChannel = MAX_flt;
    float FlowAccumulation = 0.0f;
    float ChannelStrength = 0.0f;
    float HydraulicHeight = 0.0f;
    FVector2D FlowDirection = FVector2D::ZeroVector;

    bool IsChannel() const
    {
        return DistanceToChannel < MAX_flt * 0.5f;
    }
};

/**
 * Deterministic terrain-derived hydrology.
 *
 * Each cached region samples the uncarved macro terrain, fills enclosed sinks
 * with a priority-flood pass, routes every cell through a D8 downhill receiver,
 * and accumulates runoff downstream. River segments are therefore consequences
 * of elevation and contributing area rather than noise contours.
 *
 * The overlap halo makes neighbouring region queries agree over the playable
 * interior while retaining bounded work for an effectively infinite world.
 */
class ORAKAI_API FCubusHydrologyField
{
public:
    static FCubusHydrologySample Sample(
        float WorldX,
        float WorldY,
        const FCubusHydrologySettings& Settings
    );

    /** 0 on a channel centreline, 1 at or beyond the configured valley edge. */
    static float NormalizeRiverDistance(
        const FCubusHydrologySample& Sample,
        const FCubusHydrologySettings& Settings
    );
};
