#pragma once

#include "CoreMinimal.h"

#include "CubusCore/Data/CubusDensitySample.h"

/**
 * Read-only scalar field consumed by Cubus density meshers.
 *
 * Coordinates are global density-sample coordinates. Implementations may be
 * procedural, sparse-edit backed, or adapters over another voxel format.
 */
class ORAKAI_API ICubusDensityField
{
public:
    virtual ~ICubusDensityField() = default;

    virtual FCubusDensitySample Sample(
        const FIntVector& GlobalSampleCoordinate
    ) const = 0;

    /**
     * Spatial offset of each integer sample in voxel units.
     *
     * A true corner-sampled density field normally returns zero. The block
     * adapter returns 0.5 so block-cell centres line up with the density grid.
     */
    virtual FVector GetSampleOffsetInVoxels() const
    {
        return FVector::ZeroVector;
    }
};
