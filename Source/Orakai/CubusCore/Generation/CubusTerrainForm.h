#pragma once

#include "CoreMinimal.h"

/**
 * Plain settings shared by block and density terrain generation.
 *
 * Terrain form is deliberately independent of Actors/UObjects so the same
 * deterministic sample can be used on worker threads by either renderer.
 */
struct ORAKAI_API FCubusTerrainFormSettings
{
    float BaseHeight = 8.0f;

    float ContinentAmplitude = 18.0f;
    float ContinentFrequency = 0.003f;

    float HillAmplitude = 10.0f;
    float HillFrequency = 0.015f;

    float DetailAmplitude = 2.0f;
    float DetailFrequency = 0.08f;

    float RidgeAmplitude = 16.0f;
    float RidgeFrequency = 0.012f;

    float ValleyDepth = 14.0f;
    float ValleyFrequency = 0.006f;
    float ValleyWidth = 0.08f;
    float ValleyFalloff = 0.22f;
    float ValleyWarpAmplitude = 24.0f;
    float ValleyWarpFrequency = 0.004f;

    float RegionFrequency = 0.0025f;
    float PlainsThreshold = -0.25f;
    float PlainsBlend = 0.18f;
    float MountainThreshold = 0.30f;
    float MountainBlend = 0.20f;
};

/** Useful diagnostics accompanying one natural-terrain height sample. */
struct ORAKAI_API FCubusTerrainFormSample
{
    float Height = 8.0f;
    float PlainsWeight = 0.0f;
    float RollingWeight = 1.0f;
    float MountainWeight = 0.0f;
    float MountainCore = 0.0f;
    float FoothillWeight = 0.0f;
    float Drainage = 0.0f;
    float Ridge = 0.0f;
};

/**
 * Deterministic multi-scale terrain form.
 *
 * Kilometre-scale warped plate boundaries establish mountain ranges and
 * foothills. Smaller ridged multifractal detail then forms peaks inside those
 * ranges, while a separate main-channel/tributary field cuts broad valleys.
 * Fine detail is suppressed on valley floors and plains so the result reads
 * as geography rather than uniformly distributed noise.
 */
class ORAKAI_API FCubusTerrainForm
{
public:
    static FCubusTerrainFormSample Sample(
        float WorldX,
        float WorldY,
        const FCubusTerrainFormSettings& Settings
    );

private:
    static float SampleNoise(
        float WorldX,
        float WorldY,
        float Frequency
    );

    static float SampleFbm(
        float WorldX,
        float WorldY,
        float Frequency,
        int32 Octaves,
        float Lacunarity,
        float Gain
    );

    static float SampleRidgedFbm(
        float WorldX,
        float WorldY,
        float Frequency,
        int32 Octaves
    );

    static float SampleChannelMask(
        float WorldX,
        float WorldY,
        float Frequency,
        float Width,
        float Falloff
    );

    static float SmoothStep(
        float EdgeMinimum,
        float EdgeMaximum,
        float Value
    );
};
