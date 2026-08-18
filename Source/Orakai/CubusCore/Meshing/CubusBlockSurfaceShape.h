#pragma once

#include "CoreMinimal.h"

struct FCubusBlockChunkNeighborhood;
class UCubusMaterialRegistry;

/**
 * Render-time shape selected from surrounding voxel occupancy.
 * Shape is derived during meshing and is never persisted in voxel storage.
 */
enum class ECubusBlockSurfaceShape : uint8
{
    Cube = 0,

    RampPositiveX,
    RampNegativeX,
    RampPositiveY,
    RampNegativeY,

    CornerLowNegativeXNegativeY,
    CornerLowNegativeXPositiveY,
    CornerLowPositiveXNegativeY,
    CornerLowPositiveXPositiveY
};

struct ORAKAI_API FCubusBlockSurfaceClassification
{
    ECubusBlockSurfaceShape Shape = ECubusBlockSurfaceShape::Cube;

    bool IsShaped() const
    {
        return Shape != ECubusBlockSurfaceShape::Cube;
    }

    bool IsRamp() const
    {
        return
            Shape == ECubusBlockSurfaceShape::RampPositiveX ||
            Shape == ECubusBlockSurfaceShape::RampNegativeX ||
            Shape == ECubusBlockSurfaceShape::RampPositiveY ||
            Shape == ECubusBlockSurfaceShape::RampNegativeY;
    }

    bool IsCorner() const
    {
        return IsShaped() && !IsRamp();
    }
};

/**
 * Classifies exposed, supported solid cells as ramps or inverse corner cuts.
 * Ambiguous patterns remain cubes, but every returned non-cube shape has a
 * closed mesh template and does not depend on neighbouring faces for sealing.
 */
class ORAKAI_API FCubusBlockSurfaceClassifier
{
public:
    static FCubusBlockSurfaceClassification Classify(
        const FCubusBlockChunkNeighborhood& Neighborhood,
        const UCubusMaterialRegistry* MaterialRegistry,
        int32 X,
        int32 Y,
        int32 Z
    );
};
