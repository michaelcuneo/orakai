#pragma once

#include "CoreMinimal.h"
#include "CubusCore/Generation/CubusHydrologyField.h"

class FCubusBlockChunkData;
class UCubusGeologyProfile;

/**
 * Applies the shared terrain-derived hydrology network to block terrain.
 */
class ORAKAI_API FCubusBlockTerrainRiverGenerator
{
public:
    static void Apply(
        FCubusBlockChunkData& Chunk,
        const UCubusGeologyProfile* GeologyProfile,
        const FCubusHydrologySettings& HydrologySettings
    );

private:
    static float SmoothStep(
        float EdgeMinimum,
        float EdgeMaximum,
        float Value
    );
};
