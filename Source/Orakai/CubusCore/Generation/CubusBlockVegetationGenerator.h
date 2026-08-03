#pragma once

#include "CoreMinimal.h"

class FCubusBlockChunkData;
class UCubusGeologyProfile;

/**
 * Generates deterministic vegetation placement data without rendering meshes.
 */
class ORAKAI_API FCubusBlockVegetationGenerator
{
public:
    static void Generate(
        FCubusBlockChunkData& Chunk,
        const UCubusGeologyProfile* GeologyProfile
    );

private:
    static int32 FindSurfaceLocalZ(
        const FCubusBlockChunkData& Chunk,
        int32 LocalX,
        int32 LocalY
    );

    static bool IsSpacedTreeCandidate(
        int32 WorldX,
        int32 WorldY,
        int32 Seed,
        float TargetDensity
    );

    static uint32 HashWorldColumn(int32 WorldX, int32 WorldY, int32 Salt);
    static float HashToUnitFloat(uint32 Hash);
};
