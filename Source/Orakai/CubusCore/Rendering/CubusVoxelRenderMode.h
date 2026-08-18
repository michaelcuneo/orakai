#pragma once

#include "CoreMinimal.h"

#include "CubusVoxelRenderMode.generated.h"

/**
 * Selects the geometry representation built by a streamed Cubus voxel chunk.
 * The mode is authored on the configured chunk Blueprint class defaults; the
 * underlying world and voxel coordinate space remain shared in every mode.
 */
UENUM(BlueprintType)
enum class ECubusVoxelRenderMode : uint8
{
    Blocks UMETA(DisplayName = "Blocks"),
    Density UMETA(DisplayName = "Density"),
    Hybrid UMETA(DisplayName = "Hybrid (Blocks + Density)")
};
