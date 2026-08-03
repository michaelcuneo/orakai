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

FVector FCubusDensityEditField::GetSampleOffsetInVoxels() const
{
    return GeneratedField.GetSampleOffsetInVoxels();
}
