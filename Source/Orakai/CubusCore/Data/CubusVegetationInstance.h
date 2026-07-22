#pragma once

#include "CoreMinimal.h"

/**
 * Deterministic vegetation placement generated for one terrain column.
 * Rendering is intentionally handled separately from generation.
 */
struct FCubusVegetationInstance
{
    FIntVector WorldVoxel = FIntVector::ZeroValue;
    float RotationYaw = 0.0f;
    float Scale = 1.0f;
    int32 TypeId = 0;
};