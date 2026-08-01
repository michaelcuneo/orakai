#include "CubusCore/Generation/CubusBlockDensityField.h"

FCubusBlockDensityField::FCubusBlockDensityField(
    FCubusBlockVoxelSampler InVoxelSampler,
    const bool bInTreatWaterAsEmpty,
    const float InDensityMagnitude
)
    : VoxelSampler(MoveTemp(InVoxelSampler))
    , bTreatWaterAsEmpty(bInTreatWaterAsEmpty)
    , DensityMagnitude(FMath::Max(KINDA_SMALL_NUMBER, InDensityMagnitude))
{
}

FCubusDensitySample FCubusBlockDensityField::Sample(
    const FIntVector& GlobalSampleCoordinate
) const
{
    FCubusDensitySample Result;
    Result.Density = -DensityMagnitude;
    Result.MaterialId = 0;

    if (!VoxelSampler)
    {
        return Result;
    }

    const FCubusBlockVoxel Voxel =
        VoxelSampler(GlobalSampleCoordinate);

    const bool bSolid =
        !Voxel.IsEmpty() &&
        (!bTreatWaterAsEmpty || !Voxel.IsWater());

    if (!bSolid)
    {
        return Result;
    }

    Result.Density = DensityMagnitude;
    Result.MaterialId = FMath::Max(1, Voxel.MaterialId);
    return Result;
}
