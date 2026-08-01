#pragma once

#include "CoreMinimal.h"

struct FCubusVegetationInstance;

struct FCubusVegetationRandomizationSettings
{
    bool bEnabled = true;
    int32 Seed = 1337;

    float PruneProbability = 0.15f;

    float ScaleJitterMin = 0.85f;
    float ScaleJitterMax = 1.2f;

    float PositionJitterVoxelFraction = 0.38f;
    float YawJitterDegrees = 35.0f;
};

struct FCubusResolvedVegetationPlacement
{
    FVector Location = FVector::ZeroVector;
    float Scale = 1.0f;
    float Yaw = 0.0f;

    bool bPruned = false;
    bool bRandomized = false;

    float AppliedRandomScale = 1.0f;
};

class ORAKAI_API FCubusVegetationPlacement
{
public:
    void Reset();

    uint32 CalculateRandomizationSettingsHash(
        const FCubusVegetationRandomizationSettings& Settings
    ) const;

    FCubusResolvedVegetationPlacement Resolve(
        const FCubusVegetationInstance& Instance,
        const FVector& BaseWorldLocation,
        float VoxelSize,
        float BaseScale,
        const FCubusVegetationRandomizationSettings& Settings
    );

private:
    struct FRandomizationSample
    {
        bool bPruned = false;

        float ScaleMultiplier = 1.0f;

        FVector2f PositionJitterUnit =
            FVector2f::ZeroVector;

        float YawJitterUnit = 0.0f;
    };

    void EnsureRandomizationStream(
        const FCubusVegetationRandomizationSettings& Settings
    );

    TMap<uint64, FRandomizationSample> SamplesByPlant;

    FRandomStream RandomStream;

    bool bStreamInitialized = false;

    int32 SeedSnapshot = 0;
    uint32 SettingsHashSnapshot = 0;
};