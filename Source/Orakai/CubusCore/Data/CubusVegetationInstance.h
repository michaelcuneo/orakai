#pragma once

#include "CoreMinimal.h"

namespace CubusVegetationBiome
{
    constexpr int32 Plains = 1 << 0;
    constexpr int32 Forest = 1 << 1;
    constexpr int32 Rocky = 1 << 2;
    constexpr int32 Wetland = 1 << 3;
    constexpr int32 All = Plains | Forest | Rocky | Wetland;
}

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
    int32 BiomeMask = CubusVegetationBiome::All;
};