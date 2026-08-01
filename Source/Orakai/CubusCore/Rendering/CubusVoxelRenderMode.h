#pragma once

#include "CoreMinimal.h"

#include "CubusVoxelRenderMode.generated.h"

/**
 * Selects which geometry representation a Cubus world asks each chunk to
 * build. The underlying voxel data remains shared in every mode.
 */
UENUM(BlueprintType)
enum class ECubusVoxelRenderMode : uint8
{
    Blocks UMETA(DisplayName = "Blocks"),
    Density UMETA(DisplayName = "Density"),
    Hybrid UMETA(DisplayName = "Hybrid (Blocks + Density)")
};
