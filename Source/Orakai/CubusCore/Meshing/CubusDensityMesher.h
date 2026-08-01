#pragma once

#include "CoreMinimal.h"

#include "CubusCore/Meshing/CubusMeshData.h"

class FCubusDensitySamplingBuffer;

/**
 * Extracts a smooth isosurface from a buffered Cubus density field.
 */
class ORAKAI_API FCubusDensityMesher
{
public:
    static void BuildChunk(
        const FCubusDensitySamplingBuffer& DensityBuffer,
        float VoxelSize,
        float IsoLevel,
        TMap<int32, FCubusMeshData>& OutMaterialMeshes,
        int32& OutGeneratedTriangleCount
    );
};
