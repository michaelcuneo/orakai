#pragma once

#include "CoreMinimal.h"

#include "CubusCore/Data/CubusBlockVoxel.h"
#include "CubusCore/Generation/CubusDensityField.h"

using FCubusBlockVoxelSampler =
    TFunction<FCubusBlockVoxel(const FIntVector&)>;

/**
 * Transitional density-field adapter over the existing block voxel space.
 *
 * Every block-cell centre becomes one scalar sample. Solid cells are positive
 * and empty or liquid cells are negative, which makes the extracted surface
 * align with the existing block boundaries while allowing smooth normals.
 */
class ORAKAI_API FCubusBlockDensityField final : public ICubusDensityField
{
public:
    explicit FCubusBlockDensityField(
        FCubusBlockVoxelSampler InVoxelSampler,
        bool bInTreatWaterAsEmpty = true,
        float InDensityMagnitude = 1.0f
    );

    virtual FCubusDensitySample Sample(
        const FIntVector& GlobalSampleCoordinate
    ) const override;

    virtual FVector GetSampleOffsetInVoxels() const override
    {
        return FVector(0.5, 0.5, 0.5);
    }

private:
    FCubusBlockVoxelSampler VoxelSampler;
    bool bTreatWaterAsEmpty = true;
    float DensityMagnitude = 1.0f;
};
