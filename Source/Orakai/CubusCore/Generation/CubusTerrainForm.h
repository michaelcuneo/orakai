#pragma once

#include "CoreMinimal.h"

/**
 * Plain settings shared by block and density terrain generation.
 *
 * Terrain form is deliberately independent of Actors/UObjects so the same
 * deterministic sample can be used on worker threads by either renderer.
 *
 * v30 defaults are authored in real-world metric scale. The legacy amplitude
 * and frequency fields remain available for compatibility, but the physical
 * model is authoritative when bUsePhysicalWorldScale is true.
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

    /** Broad vertical scale for the legacy terrain path. */
    float MountainElevationScale = 2.60f;

    /** Physical size of one canonical voxel. Runtime density sets this from the owning world. */
    float VoxelSizeCm = 80.0f;

    /** Use real-world metric landform dimensions instead of legacy raw frequencies/amplitudes. */
    bool bUsePhysicalWorldScale = true;

    // Horizontal geomorphology scale.
    float ClimateProvinceScaleKm = 80.0f;
    float MountainSystemSpacingKm = 16.0f;
    float MajorValleySpacingKm = 12.0f;
    float MassifScaleKm = 4.5f;
    float TributaryValleyScaleKm = 3.0f;
    float RollingHillScaleMeters = 1200.0f;
    float BroadReliefScaleMeters = 320.0f;
    float LocalReliefScaleMeters = 110.0f;

    // Vertical geomorphology scale. These are real metres, converted once to canonical voxels.
    float RegionalReliefMeters = 420.0f;
    float MountainReliefMeters = 1450.0f;
    float ValleyReliefMeters = 360.0f;
    float RollingHillReliefMeters = 150.0f;
    float BroadReliefMeters = 34.0f;
    float LocalReliefMeters = 8.0f;
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
    float MassifWeight = 0.0f;
    float Escarpment = 0.0f;
    float ValleyCarve = 0.0f;
    float Cirque = 0.0f;
    float SurfaceRoughness = 0.0f;
    float ErosionRills = 0.0f;
};

/**
 * Deterministic multi-scale terrain form.
 *
 * The physical path is explicitly hierarchical: regional relief -> mountain
 * systems -> massifs -> major valleys -> tributaries -> hills -> broad/local
 * relief. Named sizes are converted from metres using the canonical voxel size,
 * so changing voxel resolution does not silently change the world's geography.
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
