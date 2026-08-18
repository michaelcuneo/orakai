#pragma once

#include "CoreMinimal.h"

class FCubusBlockChunkData;
class FCubusTerrainDensityField;
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
    float VoxelSize,
    const FCubusTerrainDensityField* DensityField = nullptr,
    bool bGenerateWater = false,
    int32 WaterLevel = 0
    );
};