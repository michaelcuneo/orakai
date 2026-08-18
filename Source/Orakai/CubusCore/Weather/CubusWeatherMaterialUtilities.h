#pragma once

#include "CoreMinimal.h"

class AActor;
class UWorld;

/** Material-facing weather state sampled from the active world weather actor. */
struct ORAKAI_API FCubusWeatherMaterialSample
{
    bool bHasRainIntensity = false;
    float RainIntensity = 0.0f;

    bool bHasSurfaceWetness = false;
    float SurfaceWetness = 0.0f;
};

/**
 * Adapts Ultra Dynamic Weather without taking ownership of the weather system.
 *
 * The adapter deliberately uses reflected properties and loaded material
 * parameter collections. This keeps Orakai independent of UDW Blueprint
 * generated classes while still consuming the live state UDW publishes.
 */
class ORAKAI_API FCubusWeatherMaterialUtilities
{
public:
    static AActor* ResolveWeatherActor(UWorld* World);

    static FCubusWeatherMaterialSample Sample(
        UWorld* World,
        const AActor* WeatherActor
    );

    /** Accepts common weather scales: 0..1, 0..10 and 0..100. */
    static float NormalizeWeatherAmount(float Value);

    static float AdvanceWetness(
        float CurrentWetness,
        const FCubusWeatherMaterialSample& Sample,
        float DeltaSeconds,
        float WettingRate,
        float DryingRate
    );
};
