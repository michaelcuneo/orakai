#pragma once

#include "CoreMinimal.h"

/**
 * One scalar sample in the Cubus density field.
 *
 * Positive values are solid, negative values are empty, and zero is the
 * default isosurface. MaterialId describes the solid side of the sample.
 */
struct ORAKAI_API FCubusDensitySample
{
    float Density = -1.0f;
    int32 MaterialId = 0;

    FORCEINLINE bool IsSolid(
        const float IsoLevel = 0.0f
    ) const
    {
        return Density > IsoLevel;
    }
};
