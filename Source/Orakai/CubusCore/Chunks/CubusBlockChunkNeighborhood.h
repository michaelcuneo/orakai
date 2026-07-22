#pragma once

#include "CoreMinimal.h"

class FCubusBlockChunkData;
struct FCubusBlockVoxel;

/**
 * Read-only neighbourhood used while meshing one block chunk.
 *
 * The centre chunk is required. Adjacent chunks are optional;
 * a missing adjacent chunk is treated as empty space.
 */
struct ORAKAI_API FCubusBlockChunkNeighborhood
{
    const FCubusBlockChunkData* Centre = nullptr;

    const FCubusBlockChunkData* PositiveX = nullptr;
    const FCubusBlockChunkData* NegativeX = nullptr;

    const FCubusBlockChunkData* PositiveY = nullptr;
    const FCubusBlockChunkData* NegativeY = nullptr;

    const FCubusBlockChunkData* PositiveZ = nullptr;
    const FCubusBlockChunkData* NegativeZ = nullptr;

    const FCubusBlockVoxel* GetVoxel(
        int32 X,
        int32 Y,
        int32 Z
    ) const;
};