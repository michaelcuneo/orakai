#pragma once

#include "CoreMinimal.h"

struct FCubusBlockChunkNeighborhood;
class UCubusMaterialRegistry;

/**
 * Render-time shape selected from surrounding voxel occupancy.
 *
 * Shape is intentionally not stored in FCubusBlockVoxel. This keeps terrain
 * persistence block-based while allowing the mesher to reinterpret generated
 * natural terrain as slopes and corners.
 */
enum class ECubusBlockSurfaceShape : uint8
{
    Cube = 0,
    RampPositiveX,
    RampNegativeX,
    RampPositiveY,
    RampNegativeY
};

struct ORAKAI_API FCubusBlockSurfaceClassification
{
    ECubusBlockSurfaceShape Shape = ECubusBlockSurfaceShape::Cube;

    bool IsRamp() const
    {
        return Shape != ECubusBlockSurfaceShape::Cube;
    }
};

/**
 * Classifies exposed solid terrain cells without modifying voxel storage.
 *
 * The first pass recognises one-cell height transitions. A ramp is emitted
 * only when the current voxel has an exposed top, solid support beneath it,
 * a solid continuation at the current level on the uphill side, and a
 * one-cell-lower solid surface on the downhill side. Ambiguous junctions
 * remain cubes until corner templates are introduced.
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
