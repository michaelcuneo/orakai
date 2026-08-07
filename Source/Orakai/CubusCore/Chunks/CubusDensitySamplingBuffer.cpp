#include "CubusCore/Chunks/CubusDensitySamplingBuffer.h"

#include "CubusCore/Chunks/CubusChunkConstants.h"
#include "CubusCore/Generation/CubusDensityField.h"

void FCubusDensitySamplingBuffer::Build(
    const FIntVector& InChunkCoordinate,
    const ICubusDensityField& DensityField
)
{
    ChunkCoordinate = InChunkCoordinate;
    SampleOffsetInVoxels = DensityField.GetSampleOffsetInVoxels();
    Samples.SetNumUninitialized(SampleCount);

    const FIntVector GlobalSampleOrigin =
        ChunkCoordinate * Cubus::ChunkSize;

    for (
        int32 LocalZ = MinimumLocalSample;
        LocalZ <= MaximumLocalSample;
        ++LocalZ
    )
    {
        for (
            int32 LocalY = MinimumLocalSample;
            LocalY <= MaximumLocalSample;
            ++LocalY
        )
        {
            for (
                int32 LocalX = MinimumLocalSample;
                LocalX <= MaximumLocalSample;
                ++LocalX
            )
            {
                const FIntVector LocalCoordinate(
                    LocalX,
                    LocalY,
                    LocalZ
                );

                Samples[Flatten(LocalCoordinate)] =
                    DensityField.Sample(
                        GlobalSampleOrigin +
                        LocalCoordinate
                    );
            }
        }
    }
}

void FCubusDensitySamplingBuffer::Reset()
{
    ChunkCoordinate = FIntVector::ZeroValue;
    SampleOffsetInVoxels = FVector::ZeroVector;
    Samples.Reset();
}

const FCubusDensitySample*
FCubusDensitySamplingBuffer::GetSample(
    const FIntVector& LocalSampleCoordinate
) const
{
    if (
        !IsBuilt() ||
        !IsBufferedCoordinate(LocalSampleCoordinate)
    )
    {
        return nullptr;
    }

    return &Samples[Flatten(LocalSampleCoordinate)];
}

const FCubusDensitySample&
FCubusDensitySamplingBuffer::GetSampleChecked(
    const FIntVector& LocalSampleCoordinate
) const
{
    check(IsBuilt());
    check(IsBufferedCoordinate(LocalSampleCoordinate));
    return Samples[Flatten(LocalSampleCoordinate)];
}

FVector FCubusDensitySamplingBuffer::GetGradientChecked(
    const FIntVector& LocalSampleCoordinate
) const
{
    check(
        LocalSampleCoordinate.X >= 0 &&
        LocalSampleCoordinate.X <= Cubus::ChunkSize &&
        LocalSampleCoordinate.Y >= 0 &&
        LocalSampleCoordinate.Y <= Cubus::ChunkSize &&
        LocalSampleCoordinate.Z >= 0 &&
        LocalSampleCoordinate.Z <= Cubus::ChunkSize
    );

    const float GradientX =
        GetSampleChecked(
            LocalSampleCoordinate +
            FIntVector(1, 0, 0)
        ).Density -
        GetSampleChecked(
            LocalSampleCoordinate -
            FIntVector(1, 0, 0)
        ).Density;

    const float GradientY =
        GetSampleChecked(
            LocalSampleCoordinate +
            FIntVector(0, 1, 0)
        ).Density -
        GetSampleChecked(
            LocalSampleCoordinate -
            FIntVector(0, 1, 0)
        ).Density;

    const float GradientZ =
        GetSampleChecked(
            LocalSampleCoordinate +
            FIntVector(0, 0, 1)
        ).Density -
        GetSampleChecked(
            LocalSampleCoordinate -
            FIntVector(0, 0, 1)
        ).Density;

    return FVector(
        GradientX,
        GradientY,
        GradientZ
    ) * 0.5;
}

bool FCubusDensitySamplingBuffer::IsBufferedCoordinate(
    const FIntVector& LocalSampleCoordinate
)
{
    return
        LocalSampleCoordinate.X >= MinimumLocalSample &&
        LocalSampleCoordinate.X <= MaximumLocalSample &&
        LocalSampleCoordinate.Y >= MinimumLocalSample &&
        LocalSampleCoordinate.Y <= MaximumLocalSample &&
        LocalSampleCoordinate.Z >= MinimumLocalSample &&
        LocalSampleCoordinate.Z <= MaximumLocalSample;
}

int32 FCubusDensitySamplingBuffer::Flatten(
    const FIntVector& LocalSampleCoordinate
)
{
    check(IsBufferedCoordinate(LocalSampleCoordinate));

    const int32 X =
        LocalSampleCoordinate.X -
        MinimumLocalSample;

    const int32 Y =
        LocalSampleCoordinate.Y -
        MinimumLocalSample;

    const int32 Z =
        LocalSampleCoordinate.Z -
        MinimumLocalSample;

    return
        X +
        SampleDimension *
        (
            Y +
            SampleDimension * Z
        );
}
