#pragma once

#include "CoreMinimal.h"
#include "CubusCore/Generation/CubusTerrainRaster.h"

/**
 * Settings for drainage analysis over the structural terrain raster.
 *
 * Analysis deliberately runs coarser than the authored 1 m raster. The solver
 * determines catchments and river topology; the later carving pass will project
 * the resulting network back onto the 1 m terrain.
 */
struct ORAKAI_API FCubusTerrainDrainageSettings
{
    /** Structural terrain source shared with the 1 m raster. */
    FCubusTerrainRasterSettings Raster;

    /** Drainage-routing cell size in metres. */
    float AnalysisCellSizeMeters = 8.0f;

    /** Interior cells per solved drainage region. */
    int32 RegionCellCount = 256;

    /**
     * World-space origin of drainage region (0,0).
     *
     * Loader generation can centre one large analysis domain over the authored
     * DEM instead of forcing a hydrology boundary through world XY zero.
     */
    FVector2D RegionOriginMeters = FVector2D::ZeroVector;

    /** Extra cells solved around each region to stabilise boundary routing. */
    int32 HaloCellCount = 96;

    /** Minimum contributing area before a cell is exposed as a stream. */
    float StreamSourceAreaSquareKm = 0.20f;

    /** Accumulation used to classify a major river. */
    float MajorRiverAreaSquareKm = 18.0f;

    /** Small epsilon used by priority-flood to create an unambiguous descent. */
    float FillEpsilonMeters = 0.001f;
};

/** One routed segment of the drainage graph. */
struct ORAKAI_API FCubusTerrainDrainageSegment
{
    FVector2D StartMeters = FVector2D::ZeroVector;
    FVector2D EndMeters = FVector2D::ZeroVector;
    float StartElevationMeters = 0.0f;
    float EndElevationMeters = 0.0f;
    float ContributingAreaSquareKm = 0.0f;
    int32 StrahlerOrder = 1;
};

/** One continuous query into the solved drainage field. */
struct ORAKAI_API FCubusTerrainDrainageSample
{
    float RawHeightMeters = 0.0f;
    float FilledHeightMeters = 0.0f;
    float ContributingAreaSquareKm = 0.0f;
    int32 StrahlerOrder = 1;
    FVector2D FlowDirection = FVector2D::ZeroVector;
    bool bStream = false;
};

/**
 * Deterministic pre-voxel drainage solution.
 *
 * Regions sample the structural terrain source, fill enclosed depressions with
 * a priority-flood pass, route D8 receivers, accumulate contributing area and
 * calculate Strahler stream order. No terrain height is modified here.
 */
class ORAKAI_API FCubusTerrainDrainage
{
public:
    static FCubusTerrainDrainageSample Sample(
        double WorldXmeters,
        double WorldYmeters,
        const FCubusTerrainDrainageSettings& Settings
    );

    /** Collect stream segments whose start cell lies in the requested bounds. */
    static void CollectSegments(
        const FBox2D& WorldBoundsMeters,
        const FCubusTerrainDrainageSettings& Settings,
        TArray<FCubusTerrainDrainageSegment>& OutSegments
    );
};
