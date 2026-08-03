#include "CubusCore/Generation/CubusDensityEditField.h"

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

    const FIntVector MinimumCoordinate(
        FMath::FloorToInt(GlobalSampleCoordinate.X),
        FMath::FloorToInt(GlobalSampleCoordinate.Y),
        FMath::FloorToInt(GlobalSampleCoordinate.Z)
    );

    const FVector Alpha(
        GlobalSampleCoordinate.X -
            static_cast<double>(MinimumCoordinate.X),
        GlobalSampleCoordinate.Y -
            static_cast<double>(MinimumCoordinate.Y),
        GlobalSampleCoordinate.Z -
            static_cast<double>(MinimumCoordinate.Z)
    );

    float InterpolatedDensityDelta = 0.0f;
    float StrongestMaterialWeight = 0.0f;
    int32 InterpolatedMaterialId = 0;

    for (int32 Z = 0; Z <= 1; ++Z)
    {
        const float WeightZ = Z == 0
            ? 1.0f - static_cast<float>(Alpha.Z)
            : static_cast<float>(Alpha.Z);

        for (int32 Y = 0; Y <= 1; ++Y)
        {
            const float WeightY = Y == 0
                ? 1.0f - static_cast<float>(Alpha.Y)
                : static_cast<float>(Alpha.Y);

            for (int32 X = 0; X <= 1; ++X)
            {
                const float WeightX = X == 0
                    ? 1.0f - static_cast<float>(Alpha.X)
                    : static_cast<float>(Alpha.X);

                const float Weight = WeightX * WeightY * WeightZ;
                const FCubusDensityEdit* Edit = Edits.Find(
                    MinimumCoordinate + FIntVector(X, Y, Z)
                );

                if (Edit == nullptr || Weight <= 0.0f)
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
