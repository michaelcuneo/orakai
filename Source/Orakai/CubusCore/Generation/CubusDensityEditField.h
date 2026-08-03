#pragma once

#include "CoreMinimal.h"

#include "CubusCore/Generation/CubusDensityField.h"

/**
 * One sparse, player-authored change to a generated density sample.
 *
 * DensityDelta is added to the generated scalar field. Positive values add
 * solid terrain, negative values remove it. MaterialId overrides the generated
 * material only when the edited result is solid.
 */
struct ORAKAI_API FCubusDensityEdit
{
    float DensityDelta = 0.0f;
    int32 MaterialId = 0;
};

using FCubusDensityEditMap = TMap<FIntVector, FCubusDensityEdit>;

/**
 * Immutable adapter combining a generated density field with a sparse edit
 * snapshot. It is deliberately free of Actor/UObject state so a chunk can
 * safely sample it on a worker thread later.
 */
class ORAKAI_API FCubusDensityEditField final : public ICubusDensityField
{
public:
    FCubusDensityEditField(
        const ICubusDensityField& InGeneratedField,
        const FCubusDensityEditMap& InEdits
    );

    virtual FCubusDensitySample Sample(
        const FIntVector& GlobalSampleCoordinate
    ) const override;

    virtual FVector GetSampleOffsetInVoxels() const override;

private:
    const ICubusDensityField& GeneratedField;
    const FCubusDensityEditMap& Edits;
};
