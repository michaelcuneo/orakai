#pragma once

#include "CoreMinimal.h"

class FCubusBlockChunkData;

/**
 * Generates deterministic block terrain using world voxel coordinates.
 */
class ORAKAI_API FCubusBlockTerrainGenerator
{
public:
    static void GenerateFlatTerrain(
        FCubusBlockChunkData& Chunk,
        int32 SurfaceWorldZ,
        int32 SurfaceMaterialId,
        int32 SubsurfaceMaterialId
    );

    static void GenerateHeightTerrain(
        FCubusBlockChunkData& Chunk,
        int32 BaseHeight,
        float ContinentAmplitude,
        float ContinentFrequency,
        float HillAmplitude,
        float HillFrequency,
        float DetailAmplitude,
        float DetailFrequency,
        float RidgeAmplitude,
        float RidgeFrequency,
        float ValleyDepth,
        float ValleyFrequency,
        float ValleyWidth,
        float ValleyFalloff,
        float ValleyWarpAmplitude,
        float ValleyWarpFrequency,
        float RegionFrequency,
        float PlainsThreshold,
        float PlainsBlend,
        float MountainThreshold,
        float MountainBlend,
        int32 SurfaceMaterialId,
        int32 SubsurfaceMaterialId,
        int32 RockMaterialId,
        int32 SnowMaterialId,
        float RockSlopeThreshold,
        int32 SnowMinimumHeight,
        bool bGenerateWater,
        int32 WaterLevel,
        int32 WaterMaterialId
    );

private:
    struct FTerrainRegionWeights
    {
        float Plains = 0.0f;
        float Rolling = 1.0f;
        float Mountains = 0.0f;
    };

    static int32 SampleTerrainHeight(
        int32 WorldX,
        int32 WorldY,
        int32 BaseHeight,
        float ContinentAmplitude,
        float ContinentFrequency,
        float HillAmplitude,
        float HillFrequency,
        float DetailAmplitude,
        float DetailFrequency,
        float RidgeAmplitude,
        float RidgeFrequency,
        float ValleyDepth,
        float ValleyFrequency,
        float ValleyWidth,
        float ValleyFalloff,
        float ValleyWarpAmplitude,
        float ValleyWarpFrequency,
        float RegionFrequency,
        float PlainsThreshold,
        float PlainsBlend,
        float MountainThreshold,
        float MountainBlend
    );

    static int32 SelectSurfaceMaterial(
        int32 WorldX,
        int32 WorldY,
        int32 SurfaceWorldZ,
        int32 BaseHeight,
        float ContinentAmplitude,
        float ContinentFrequency,
        float HillAmplitude,
        float HillFrequency,
        float DetailAmplitude,
        float DetailFrequency,
        float RidgeAmplitude,
        float RidgeFrequency,
        float ValleyDepth,
        float ValleyFrequency,
        float ValleyWidth,
        float ValleyFalloff,
        float ValleyWarpAmplitude,
        float ValleyWarpFrequency,
        float RegionFrequency,
        float PlainsThreshold,
        float PlainsBlend,
        float MountainThreshold,
        float MountainBlend,
        int32 SurfaceMaterialId,
        int32 RockMaterialId,
        int32 SnowMaterialId,
        float RockSlopeThreshold,
        int32 SnowMinimumHeight
    );

    static FTerrainRegionWeights SampleTerrainRegions(
        int32 WorldX,
        int32 WorldY,
        float Frequency,
        float PlainsThreshold,
        float PlainsBlend,
        float MountainThreshold,
        float MountainBlend
    );

    static float SampleNoise(
        int32 WorldX,
        int32 WorldY,
        float Frequency
    );

    static float SampleNoise(
        float WorldX,
        float WorldY,
        float Frequency
    );

    static float SampleRidgedNoise(
        int32 WorldX,
        int32 WorldY,
        float Frequency
    );

    static float SampleValleyMask(
        int32 WorldX,
        int32 WorldY,
        float Frequency,
        float Width,
        float Falloff,
        float WarpAmplitude,
        float WarpFrequency
    );

    static float SmoothStep(
        float EdgeMinimum,
        float EdgeMaximum,
        float Value
    );
};