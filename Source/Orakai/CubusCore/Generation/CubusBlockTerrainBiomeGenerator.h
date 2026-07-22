#pragma once

#include "CoreMinimal.h"

class FCubusBlockChunkData;
class UCubusGeologyProfile;

/**
 * Applies deterministic surface biome materials after river shaping.
 */
class ORAKAI_API FCubusBlockTerrainBiomeGenerator
{
public:
    static void Apply(
        FCubusBlockChunkData& Chunk,
        const UCubusGeologyProfile* GeologyProfile
    );

private:
    static int32 FindSurfaceLocalZ(
        const FCubusBlockChunkData& Chunk,
        int32 LocalX,
        int32 LocalY
    );

    static float SampleBiomeNoise(
        int32 WorldX,
        int32 WorldY,
        float Frequency
    );

    static float SampleRiverDistance(
        int32 WorldX,
        int32 WorldY,
        const UCubusGeologyProfile* GeologyProfile
    );
};