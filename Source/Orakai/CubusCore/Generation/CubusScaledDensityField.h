#pragma once

#include "CoreMinimal.h"

#include "CubusCore/Generation/CubusDensityField.h"

/**
 * Coordinate-space adapter over an existing density field.
 *
 * The wrapped field remains expressed in canonical world-voxel coordinates.
 * This adapter exposes a logical coordinate space whose samples are farther
 * apart in the wrapped field. A scale of 4 therefore makes one logical sample
 * step cover four canonical voxel samples without changing the underlying
 * terrain function.
 *
 * This is used by terrain LOD tiles so the existing density mesher can keep
 * its fixed 32-cell topology while covering a larger canonical world region.
 */
class ORAKAI_API FCubusScaledDensityField final : public ICubusDensityField
{
public:
    FCubusScaledDensityField(
        const ICubusDensityField& InSourceField,
        const FVector& InLogicalOrigin,
        const FVector& InSourceOrigin,
        const float InCoordinateScale
    )
        : SourceField(InSourceField)
        , LogicalOrigin(InLogicalOrigin)
        , SourceOrigin(InSourceOrigin)
        , CoordinateScale(FMath::Max(InCoordinateScale, UE_SMALL_NUMBER))
    {
    }

    virtual FCubusDensitySample Sample(
        const FIntVector& GlobalSampleCoordinate
    ) const override
    {
        return SourceField.SampleContinuous(
            MapCoordinate(
                FVector(
                    static_cast<double>(GlobalSampleCoordinate.X),
                    static_cast<double>(GlobalSampleCoordinate.Y),
                    static_cast<double>(GlobalSampleCoordinate.Z)
                )
            )
        );
    }

    virtual FCubusDensitySample SampleContinuous(
        const FVector& GlobalSampleCoordinate
    ) const override
    {
        return SourceField.SampleContinuous(
            MapCoordinate(GlobalSampleCoordinate)
        );
    }

    virtual FVector GetSampleOffsetInVoxels() const override
    {
        return SourceField.GetSampleOffsetInVoxels() /
            CoordinateScale;
    }

private:
    FVector MapCoordinate(
        const FVector& LogicalCoordinate
    ) const
    {
        return SourceOrigin +
            (LogicalCoordinate - LogicalOrigin) *
                CoordinateScale;
    }

    const ICubusDensityField& SourceField;

    FVector LogicalOrigin = FVector::ZeroVector;
    FVector SourceOrigin = FVector::ZeroVector;
    float CoordinateScale = 1.0f;
};
