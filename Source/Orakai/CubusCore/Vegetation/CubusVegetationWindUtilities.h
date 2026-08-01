#pragma once

#include "CoreMinimal.h"

class AActor;
class UObject;

class ORAKAI_API FCubusVegetationWindUtilities
{
public:
    static bool TryReadFloatProperty(
        const UObject* Source,
        FName PropertyName,
        float& OutValue
    );

    static bool TryWriteFloatProperty(
        UObject* Target,
        FName PropertyName,
        float Value
    );

    static bool TryReadVectorLikeProperty(
        const UObject* Source,
        FName PropertyName,
        FVector& OutValue
    );

    static bool TryWriteVectorLikeProperty(
        UObject* Target,
        FName PropertyName,
        const FVector& Value
    );

    static bool TryWriteBoolProperty(
        UObject* Target,
        FName PropertyName,
        bool Value
    );

    static AActor* ResolveUltraDynamicWeatherActor(
        UWorld* World
    );

    static AActor* ResolveGlobalFoliageActor(
        UWorld* World
    );

    static void AssignLikelyWindProviderActor(
        UObject* Target,
        AActor* WindProviderActor
    );

    static UObject* ResolveTransformProviderDataFromObject(
        UObject* Candidate
    );

    static UObject* ResolveWindTransformProviderFromActor(
        AActor* CandidateActor
    );

    static int32 ApplyWindToObject(
        UObject* Target,
        const FVector& WindDirection,
        float WindIntensity
    );

    static int32 InvokeLikelyWindRefreshFunctions(
        UObject* Target
    );
};