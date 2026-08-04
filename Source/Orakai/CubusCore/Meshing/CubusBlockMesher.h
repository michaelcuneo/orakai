#pragma once

#include "CoreMinimal.h"

#include "CubusCore/Meshing/CubusMeshData.h"

class UCubusMaterialRegistry;
struct FCubusBlockChunkNeighborhood;

using FCubusMaterialMeshMap = TMap<int32, FCubusMeshData>;

/**
 * Extracts a coarse geological surface from authoritative block occupancy.
 *
 * Storage, edits and materials remain block based. Rendering converts each
 * solid block centre to a positive density sample and each empty or liquid
 * block centre to a negative sample, then runs the shared density mesher at
 * exactly one sample per canonical voxel.
 */
class ORAKAI_API FCubusBlockMesher
{
public:
    static void BuildChunk(
        const FCubusBlockChunkNeighborhood& Neighborhood,
        const UCubusMaterialRegistry* MaterialRegistry,
        float VoxelSize,
        FCubusMaterialMeshMap& OutMaterialMeshes,
        int32& OutGeneratedFaceCount
    );
};
