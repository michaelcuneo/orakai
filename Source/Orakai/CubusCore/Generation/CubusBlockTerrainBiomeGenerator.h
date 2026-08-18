#pragma once

#include "CoreMinimal.h"

class FCubusBlockChunkData;
class UCubusGeologyProfile;
struct FCubusHydrologySettings;

/**
 * Applies deterministic surface biome materials after river shaping.
 */
class ORAKAI_API FCubusBlockTerrainBiomeGenerator
{
public:
    static void Apply(
        FCubusBlockChunkData& Chunk,
        const UCubusGeologyProfile* GeologyProfile,
        const FCubusHydrologySettings* HydrologySettings = nullptr
    );

private:
    static int32 FindSurfaceLocalZ(
        const FCubusBlockChunkData& Chunk,
        int32 LocalX,
        int32 LocalY
    );
};
