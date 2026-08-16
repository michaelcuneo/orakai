#pragma once

#include "CoreMinimal.h"
#include "CubusCore/Generation/CubusTerrainDrainage.h"

/**
 * Controls drainage-driven modification of the authored 1 m DEM.
 * Widths and depths are expressed in physical metres and scale continuously
 * with upstream contributing area and Strahler order.
 */
struct ORAKAI_API FCubusTerrainCarvingSettings
{
    FCubusTerrainDrainageSettings Drainage;

    /** Broad valley envelope. */
    float HeadwaterValleyHalfWidthMeters = 18.0f;
    float MajorValleyHalfWidthMeters = 420.0f;
    float HeadwaterValleyDepthMeters = 3.0f;
    float MajorValleyDepthMeters = 95.0f;

    /** Floodplain nested inside the valley envelope. */
    float HeadwaterFloodplainHalfWidthMeters = 5.0f;
    float MajorFloodplainHalfWidthMeters = 150.0f;
    float HeadwaterFloodplainDepthMeters = 0.8f;
    float MajorFloodplainDepthMeters = 18.0f;

    /** River channel and inner banks. */
    float HeadwaterChannelHalfWidthMeters = 1.2f;
    float MajorChannelHalfWidthMeters = 32.0f;
    float HeadwaterChannelDepthMeters = 0.8f;
    float MajorChannelDepthMeters = 12.0f;
    float BankWidthScale = 2.6f;

    /** Area response curve. 1 gives linear area scaling; <1 broadens tributaries. */
    float AreaExponent = 0.38f;

    /** Maximum number of nearby drainage segments considered per raster sample. */
    int32 MaxNearbySegments = 24;
};

struct ORAKAI_API FCubusTerrainCarvingSample
{
    float OriginalHeightMeters = 0.0f;
    float CarvedHeightMeters = 0.0f;
    float TotalIncisionMeters = 0.0f;
    float ValleyWeight = 0.0f;
    float FloodplainWeight = 0.0f;
    float BankWeight = 0.0f;
    float ChannelWeight = 0.0f;
    float ContributingAreaSquareKm = 0.0f;
    int32 StrahlerOrder = 0;
};

/**
 * Explicit pre-voxel generation stage that projects drainage topology onto the
 * 1 m terrain raster. This class never mutates density chunks or streaming state.
 */
class ORAKAI_API FCubusTerrainCarving
{
public:
    /** Return a carved copy of an already-generated structural raster tile. */
    static FCubusTerrainRasterTile CarveTile(
        const FCubusTerrainRasterTile& StructuralTile,
        const FCubusTerrainCarvingSettings& Settings
    );

    /** Evaluate the same carving function continuously for loader/debug previews. */
    static FCubusTerrainCarvingSample Sample(
        double WorldXmeters,
        double WorldYmeters,
        const FCubusTerrainCarvingSettings& Settings
    );
};
