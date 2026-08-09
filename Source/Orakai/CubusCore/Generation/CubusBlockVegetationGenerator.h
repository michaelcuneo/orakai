#pragma once

#include "CoreMinimal.h"

class FCubusBlockChunkData;
class UCubusGeologyProfile;
class FCubusTerrainDensityField;

/**
 * Generates deterministic vegetation placement data without rendering meshes.
 */
class ORAKAI_API FCubusBlockVegetationGenerator
{
public:
    static void Generate(
        FCubusBlockChunkData& Chunk,
        const UCubusGeologyProfile* GeologyProfile,
        const FCubusTerrainDensityField* DensityField = nullptr,
        bool bGenerateWater = false,
        int32 WaterLevel = 0
    );

private:
    static bool IsSpacedTreeCandidate(
        int32 WorldX,
        int32 WorldY,
        int32 Seed,
        float TargetDensity
    );

    static uint32 HashWorldColumn(int32 WorldX, int32 WorldY, int32 Salt);
    static float HashToUnitFloat(uint32 Hash);
};
