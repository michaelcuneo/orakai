#pragma once

#include "CoreMinimal.h"

#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Data/CubusDensitySample.h"

class ICubusDensityField;

/**
 * Temporary density cache for one 32-cubed mesh-cell chunk.
 *
 * The owned cells use lower sample coordinates 0..31. Their corners require
 * samples 0..32, and central-difference normals require one additional sample
 * on either side, so the buffered local range is -1..33 (35 cubed samples).
 */
class ORAKAI_API FCubusDensitySamplingBuffer
{
public:
    static constexpr int32 MinimumLocalSample = -1;
    static constexpr int32 MaximumLocalSample =
        Cubus::ChunkSize + 1;
    static constexpr int32 SampleDimension =
        Cubus::ChunkSize + 3;
    static constexpr int32 SampleCount =
        SampleDimension *
        SampleDimension *
        SampleDimension;

    void Build(
        const FIntVector& InChunkCoordinate,
        const ICubusDensityField& DensityField
    );

    void Reset();

    bool IsBuilt() const
    {
        return Samples.Num() == SampleCount;
    }

    const FIntVector& GetChunkCoordinate() const
    {
        return ChunkCoordinate;
    }

    const FVector& GetSampleOffsetInVoxels() const
    {
        return SampleOffsetInVoxels;
    }

    const FCubusDensitySample* GetSample(
        const FIntVector& LocalSampleCoordinate
    ) const;

    const FCubusDensitySample& GetSampleChecked(
        const FIntVector& LocalSampleCoordinate
    ) const;

    FVector GetGradientChecked(
        const FIntVector& LocalSampleCoordinate
    ) const;

    static bool IsBufferedCoordinate(
        const FIntVector& LocalSampleCoordinate
    );

private:
    static int32 Flatten(
        const FIntVector& LocalSampleCoordinate
    );

    FIntVector ChunkCoordinate = FIntVector::ZeroValue;
    FVector SampleOffsetInVoxels = FVector::ZeroVector;
    TArray<FCubusDensitySample> Samples;
};
