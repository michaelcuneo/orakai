#include "CubusCore/Weather/CubusWeatherMaterialUtilities.h"

#include "CubusCore/Vegetation/CubusVegetationWindUtilities.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace CubusWeatherMaterialUtilities
{
    bool LooksLikeWeatherCollection(const UMaterialParameterCollection& Collection)
    {
        const FString Name = Collection.GetName();
        return
            Name.Contains(TEXT("UltraDynamicWeather"), ESearchCase::IgnoreCase) ||
            Name.Contains(TEXT("Ultra_Dynamic_Weather"), ESearchCase::IgnoreCase) ||
            Name.Contains(TEXT("Weather_Parameters"), ESearchCase::IgnoreCase) ||
            Name.Contains(TEXT("UDW"), ESearchCase::IgnoreCase);
    }

    bool LooksLikeWetness(const FString& Name)
    {
        return
            Name.Contains(TEXT("Wetness"), ESearchCase::IgnoreCase) ||
            Name.Contains(TEXT("SurfaceWet"), ESearchCase::IgnoreCase) ||
            Name.Contains(TEXT("GroundWet"), ESearchCase::IgnoreCase);
    }

    bool LooksLikeRainAmount(const FString& Name)
    {
        if (!Name.Contains(TEXT("Rain"), ESearchCase::IgnoreCase) &&
            !Name.Contains(TEXT("Precipitation"), ESearchCase::IgnoreCase))
        {
            return false;
        }

        return
            Name.Contains(TEXT("Intensity"), ESearchCase::IgnoreCase) ||
            Name.Contains(TEXT("Amount"), ESearchCase::IgnoreCase) ||
            Name.Contains(TEXT("Strength"), ESearchCase::IgnoreCase) ||
            Name.Contains(TEXT("Coverage"), ESearchCase::IgnoreCase) ||
            Name.Equals(TEXT("Rain"), ESearchCase::IgnoreCase) ||
            Name.Equals(TEXT("Precipitation"), ESearchCase::IgnoreCase);
    }

    bool TryReadActorAmount(
        const AActor* WeatherActor,
        const TConstArrayView<FName> Candidates,
        float& OutValue
    )
    {
        for (const FName Candidate : Candidates)
        {
            if (FCubusVegetationWindUtilities::TryReadFloatProperty(
                WeatherActor,
                Candidate,
                OutValue
            ))
            {
                return true;
            }
        }

        return false;
    }

    void SampleActorFallback(
        const AActor& WeatherActor,
        FCubusWeatherMaterialSample& OutSample
    )
    {
        for (TFieldIterator<FProperty> It(WeatherActor.GetClass()); It; ++It)
        {
            const FProperty* Property = *It;
            if (Property == nullptr)
            {
                continue;
            }

            float Value = 0.0f;
            if (!FCubusVegetationWindUtilities::TryReadFloatProperty(
                &WeatherActor,
                Property->GetFName(),
                Value
            ))
            {
                continue;
            }

            const FString Name = Property->GetName();

            if (!OutSample.bHasSurfaceWetness && LooksLikeWetness(Name))
            {
                OutSample.bHasSurfaceWetness = true;
                OutSample.SurfaceWetness =
                    FCubusWeatherMaterialUtilities::NormalizeWeatherAmount(Value);
            }
            else if (!OutSample.bHasRainIntensity && LooksLikeRainAmount(Name))
            {
                OutSample.bHasRainIntensity = true;
                OutSample.RainIntensity =
                    FCubusWeatherMaterialUtilities::NormalizeWeatherAmount(Value);
            }
        }
    }

    void SampleCollections(
        UWorld& World,
        FCubusWeatherMaterialSample& OutSample
    )
    {
        for (TObjectIterator<UMaterialParameterCollection> It; It; ++It)
        {
            UMaterialParameterCollection* Collection = *It;
            if (!IsValid(Collection) || !LooksLikeWeatherCollection(*Collection))
            {
                continue;
            }

            UMaterialParameterCollectionInstance* Instance =
                World.GetParameterCollectionInstance(Collection);
            if (!IsValid(Instance))
            {
                continue;
            }

            for (const FCollectionScalarParameter& Parameter :
                 Collection->ScalarParameters)
            {
                const FString Name = Parameter.ParameterName.ToString();
                const bool bWetness = LooksLikeWetness(Name);
                const bool bRain = LooksLikeRainAmount(Name);

                if ((!bWetness && !bRain) ||
                    (bWetness && OutSample.bHasSurfaceWetness) ||
                    (bRain && OutSample.bHasRainIntensity))
                {
                    continue;
                }

                float Value = 0.0f;
                if (!Instance->GetScalarParameterValue(
                    Parameter.ParameterName,
                    Value
                ))
                {
                    continue;
                }

                const float Normalized =
                    FCubusWeatherMaterialUtilities::NormalizeWeatherAmount(Value);

                if (bWetness)
                {
                    OutSample.bHasSurfaceWetness = true;
                    OutSample.SurfaceWetness = Normalized;
                }
                else
                {
                    OutSample.bHasRainIntensity = true;
                    OutSample.RainIntensity = Normalized;
                }
            }
        }
    }
}

AActor* FCubusWeatherMaterialUtilities::ResolveWeatherActor(UWorld* World)
{
    return FCubusVegetationWindUtilities::ResolveUltraDynamicWeatherActor(World);
}

FCubusWeatherMaterialSample FCubusWeatherMaterialUtilities::Sample(
    UWorld* World,
    const AActor* WeatherActor
)
{
    FCubusWeatherMaterialSample Result;

    if (!IsValid(World) || !IsValid(WeatherActor))
    {
        return Result;
    }

    static const FName WetnessCandidates[] =
    {
        TEXT("SurfaceWetness"),
        TEXT("Surface_Wetness"),
        TEXT("GroundWetness"),
        TEXT("Ground_Wetness"),
        TEXT("Wetness")
    };

    float Wetness = 0.0f;
    if (CubusWeatherMaterialUtilities::TryReadActorAmount(
        WeatherActor,
        MakeArrayView(WetnessCandidates),
        Wetness
    ))
    {
        Result.bHasSurfaceWetness = true;
        Result.SurfaceWetness = NormalizeWeatherAmount(Wetness);
    }

    static const FName RainCandidates[] =
    {
        TEXT("RainIntensity"),
        TEXT("Rain_Intensity"),
        TEXT("CurrentRainIntensity"),
        TEXT("Current_Rain_Intensity"),
        TEXT("RainAmount"),
        TEXT("Rain_Amount"),
        TEXT("PrecipitationIntensity"),
        TEXT("Precipitation_Intensity"),
        TEXT("Rain")
    };

    float Rain = 0.0f;
    if (CubusWeatherMaterialUtilities::TryReadActorAmount(
        WeatherActor,
        MakeArrayView(RainCandidates),
        Rain
    ))
    {
        Result.bHasRainIntensity = true;
        Result.RainIntensity = NormalizeWeatherAmount(Rain);
    }

    CubusWeatherMaterialUtilities::SampleCollections(*World, Result);
    CubusWeatherMaterialUtilities::SampleActorFallback(*WeatherActor, Result);
    return Result;
}

float FCubusWeatherMaterialUtilities::NormalizeWeatherAmount(const float Value)
{
    const float NonNegative = FMath::Max(0.0f, Value);
    if (NonNegative <= 1.0f)
    {
        return NonNegative;
    }

    if (NonNegative <= 10.0f)
    {
        return NonNegative / 10.0f;
    }

    return FMath::Clamp(NonNegative / 100.0f, 0.0f, 1.0f);
}

float FCubusWeatherMaterialUtilities::AdvanceWetness(
    const float CurrentWetness,
    const FCubusWeatherMaterialSample& Sample,
    const float DeltaSeconds,
    const float WettingRate,
    const float DryingRate
)
{
    const float Current = FMath::Clamp(CurrentWetness, 0.0f, 1.0f);
    const float Step = FMath::Max(0.0f, DeltaSeconds);

    if (Sample.bHasSurfaceWetness)
    {
        float Target = FMath::Clamp(Sample.SurfaceWetness, 0.0f, 1.0f);

        if (Sample.bHasRainIntensity &&
            Sample.RainIntensity > KINDA_SMALL_NUMBER)
        {
            Target = FMath::Max(
                Target,
                Current +
                    FMath::Clamp(Sample.RainIntensity, 0.0f, 1.0f) *
                    FMath::Max(0.0f, WettingRate) * Step
            );
            Target = FMath::Min(Target, 1.0f);
        }

        const float Rate = Target > Current ? WettingRate : DryingRate;
        return FMath::FInterpConstantTo(
            Current,
            Target,
            Step,
            FMath::Max(0.0f, Rate)
        );
    }

    const float Rain = Sample.bHasRainIntensity
        ? FMath::Clamp(Sample.RainIntensity, 0.0f, 1.0f)
        : 0.0f;

    const float Delta = Rain > KINDA_SMALL_NUMBER
        ? Rain * FMath::Max(0.0f, WettingRate) * Step
        : -FMath::Max(0.0f, DryingRate) * Step;

    return FMath::Clamp(Current + Delta, 0.0f, 1.0f);
}
