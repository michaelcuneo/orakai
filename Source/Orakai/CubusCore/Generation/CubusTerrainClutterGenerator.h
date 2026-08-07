#pragma once

#include "CoreMinimal.h"

class FCubusBlockChunkData;
class UCubusMaterialRegistry;

/**
 * Appends deterministic terrain clutter to an already-generated vegetation
 * list. Rendering remains owned by the world vegetation actor and its shared
 * HISM batches.
 */
class ORAKAI_API FCubusTerrainClutterGenerator
{
public:
    static void Append(
        FCubusBlockChunkData& Chunk,
        const UCubusMaterialRegistry* MaterialRegistry,
        float VoxelSize
    );

private:
    static int32 FindSurfaceLocalZ(
        const FCubusBlockChunkData& Chunk,
        int32 LocalX,
        int32 LocalY
    );
};
