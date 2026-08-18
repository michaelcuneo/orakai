#include "CubusCore/Generation/CubusDensityEditField.h"

namespace CubusDensityEditField
{
    void BuildCubicBSplineWeights(const float T, float OutWeights[4])
    {
        const float T2 = T * T;
        const float T3 = T2 * T;
        const float OneMinusT = 1.0f - T;

        OutWeights[0] = OneMinusT * OneMinusT * OneMinusT / 6.0f;
        OutWeights[1] = (3.0f * T3 - 6.0f * T2 + 4.0f) / 6.0f;
        OutWeights[2] = (-3.0f * T3 + 3.0f * T2 + 3.0f * T + 1.0f) / 6.0f;
        OutWeights[3] = T3 / 6.0f;
    }
}

FCubusDensityEditField::FCubusDensityEditField(
    const ICubusDensityField& InGeneratedField,
    const FCubusDensityEditMap& InEdits
)
    : GeneratedField(InGeneratedField)
    , Edits(InEdits)
{
}

FCubusDensitySample FCubusDensityEditField::Sample(
    const FIntVector& GlobalSampleCoordinate
) const
{
    FCubusDensitySample Result =
        GeneratedField.Sample(GlobalSampleCoordinate);

    const FCubusDensityEdit* Edit =
        Edits.Find(GlobalSampleCoordinate);

    if (Edit == nullptr)
    {
        return Result;
    }

    Result.Density += Edit->DensityDelta;

    if (Result.Density <= 0.0f)
    {
        Result.MaterialId = 0;
    }
    else if (Edit->MaterialId > 0)
    {
        Result.MaterialId = Edit->MaterialId;
    }
    else
    {
        Result.MaterialId = FMath::Max(1, Result.MaterialId);
    }

    return Result;
}

FCubusDensitySample FCubusDensityEditField::SampleContinuous(
    const FVector& GlobalSampleCoordinate
) const
{
    FCubusDensitySample Result =
        GeneratedField.SampleContinuous(GlobalSampleCoordinate);

    const FIntVector BaseCoordinate(
        FMath::FloorToInt(GlobalSampleCoordinate.X),
        FMath::FloorToInt(GlobalSampleCoordinate.Y),
        FMath::FloorToInt(GlobalSampleCoordinate.Z)
    );

    const FVector Alpha(
        GlobalSampleCoordinate.X - static_cast<double>(BaseCoordinate.X),
        GlobalSampleCoordinate.Y - static_cast<double>(BaseCoordinate.Y),
        GlobalSampleCoordinate.Z - static_cast<double>(BaseCoordinate.Z)
    );

    float WeightsX[4];
    float WeightsY[4];
    float WeightsZ[4];
    CubusDensityEditField::BuildCubicBSplineWeights(
        static_cast<float>(Alpha.X),
        WeightsX
    );
    CubusDensityEditField::BuildCubicBSplineWeights(
        static_cast<float>(Alpha.Y),
        WeightsY
    );
    CubusDensityEditField::BuildCubicBSplineWeights(
        static_cast<float>(Alpha.Z),
        WeightsZ
    );

    float InterpolatedDensityDelta = 0.0f;
    float StrongestMaterialWeight = 0.0f;
    int32 InterpolatedMaterialId = 0;

    // Cubic B-spline interpolation samples four edit points per axis. Unlike
    // trilinear interpolation, this produces continuous first and second
    // derivatives across sample-cell boundaries, removing the planar facets
    // that were visible in high-resolution near-field density meshes.
    for (int32 Z = 0; Z < 4; ++Z)
    {
        for (int32 Y = 0; Y < 4; ++Y)
        {
            for (int32 X = 0; X < 4; ++X)
            {
                const float Weight =
                    WeightsX[X] *
                    WeightsY[Y] *
                    WeightsZ[Z];

                if (Weight <= KINDA_SMALL_NUMBER)
                {
                    continue;
                }

                const FIntVector SampleCoordinate =
                    BaseCoordinate + FIntVector(X - 1, Y - 1, Z - 1);

                const FCubusDensityEdit* Edit =
                    Edits.Find(SampleCoordinate);

                if (Edit == nullptr)
                {
                    continue;
                }

                InterpolatedDensityDelta +=
                    Edit->DensityDelta * Weight;

                if (
                    Edit->MaterialId > 0 &&
                    Weight > StrongestMaterialWeight
                )
                {
                    StrongestMaterialWeight = Weight;
                    InterpolatedMaterialId = Edit->MaterialId;
                }
            }
        }
    }

    Result.Density += InterpolatedDensityDelta;

    if (Result.Density <= 0.0f)
    {
        Result.MaterialId = 0;
    }
    else if (InterpolatedMaterialId > 0)
    {
        Result.MaterialId = InterpolatedMaterialId;
    }
    else
    {
        Result.MaterialId = FMath::Max(1, Result.MaterialId);
    }

    return Result;
}

FVector FCubusDensityEditField::GetSampleOffsetInVoxels() const
{
    return GeneratedField.GetSampleOffsetInVoxels();
}
