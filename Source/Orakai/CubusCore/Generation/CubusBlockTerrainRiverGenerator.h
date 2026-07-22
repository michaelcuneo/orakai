#pragma once

#include "CoreMinimal.h"

class FCubusBlockChunkData;
class UCubusGeologyProfile;

/**
 * Applies deterministic world-space river channels to generated terrain.
 */
class ORAKAI_API FCubusBlockTerrainRiverGenerator
{
public:
    static void Apply(
        FCubusBlockChunkData& Chunk,
        const UCubusGeologyProfile* GeologyProfile
    );

private:
    static float SampleRiverDistance(
        int32 WorldX,
        int32 WorldY,
        const UCubusGeologyProfile* GeologyProfile
    );

    static float SmoothStep(
        float EdgeMinimum,
        float EdgeMaximum,
        float Value
    );
};