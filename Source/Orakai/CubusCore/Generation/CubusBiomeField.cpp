#include "CubusCore/Generation/CubusBiomeField.h"

#include "CubusCore/Data/CubusGeologyProfile.h"
#include "CubusCore/Generation/CubusGenerationSeeds.h"

FCubusBiomeFieldSettings FCubusBiomeField::MakeSettings(
    const UCubusGeologyProfile* GeologyProfile,
    const int32 BiomeSeed,
    const int32 RiverSeed
)
{
    FCubusBiomeFieldSettings Settings;
    Settings.BiomeOffsetX = FCubusGenerationSeeds::DomainOffsetX(BiomeSeed);
    Settings.BiomeOffsetY = FCubusGenerationSeeds::DomainOffsetY(BiomeSeed);
    Settings.RiverOffsetX = FCubusGenerationSeeds::DomainOffsetX(RiverSeed);
    Settings.RiverOffsetY = FCubusGenerationSeeds::DomainOffsetY(RiverSeed);

    if (!IsValid(GeologyProfile))
    {
        return Settings;
    }

    Settings.bEnabled = GeologyProfile->bGenerateBiomes;
    Settings.Frequency = GeologyProfile->BiomeFrequency;
    Settings.ForestThreshold = GeologyProfile->ForestThreshold;
    Settings.WetlandRiverDistance = GeologyProfile->WetlandRiverDistance;
    Settings.RockySlopeThreshold = GeologyProfile->RockySlopeThreshold;
    Settings.RockyMinimumWorldZ = static_cast<float>(
        GeologyProfile->RockyMinimumWorldZ
    );
    Settings.PlainsSurfaceMaterialId = GeologyProfile->PlainsSurfaceMaterialId;
    Settings.ForestSurfaceMaterialId = GeologyProfile->ForestSurfaceMaterialId;
    Settings.RockySurfaceMaterialId = GeologyProfile->RockySurfaceMaterialId;
    Settings.WetlandSurfaceMaterialId = GeologyProfile->WetlandSurfaceMaterialId;
    Settings.bGenerateRivers = GeologyProfile->bGenerateRivers;
    Settings.RiverFrequency = GeologyProfile->RiverFrequency;
    Settings.RiverWarpAmplitude = GeologyProfile->RiverWarpAmplitude;
    Settings.RiverWarpFrequency = GeologyProfile->RiverWarpFrequency;
    Settings.Definitions = GeologyProfile->BiomeDefinitions;
    return Settings;
}

FCubusBiomeSample FCubusBiomeField::Sample(
    const float WorldX,
    const float WorldY,
    const float SurfaceWorldZ,
    const float Slope,
    const FCubusBiomeFieldSettings& InSettings
)
{
    FCubusBiomeFieldSettings Settings = InSettings;
    Settings.Frequency = FMath::Max(0.000001f, Settings.Frequency);
    Settings.ForestThreshold = FMath::Clamp(Settings.ForestThreshold, -1.0f, 1.0f);
    Settings.WetlandRiverDistance = FMath::Clamp(
        Settings.WetlandRiverDistance,
        0.0f,
        1.0f
    );
    Settings.RockySlopeThreshold = FMath::Max(0.01f, Settings.RockySlopeThreshold);
    Settings.PlainsSurfaceMaterialId = FMath::Max(1, Settings.PlainsSurfaceMaterialId);
    Settings.ForestSurfaceMaterialId = FMath::Max(1, Settings.ForestSurfaceMaterialId);
    Settings.RockySurfaceMaterialId = FMath::Max(1, Settings.RockySurfaceMaterialId);
    Settings.WetlandSurfaceMaterialId = FMath::Max(1, Settings.WetlandSurfaceMaterialId);

    FCubusBiomeSample Result;
    Result.SurfaceMaterialId = Settings.PlainsSurfaceMaterialId;
    if (!Settings.bEnabled)
    {
        return Result;
    }

    const float BiomeX = WorldX + static_cast<float>(Settings.BiomeOffsetX);
    const float BiomeY = WorldY + static_cast<float>(Settings.BiomeOffsetY);
    const float WarpFrequency = Settings.Frequency * 0.32f;
    const float WarpAmplitude = FMath::Clamp(
        0.16f / Settings.Frequency,
        24.0f,
        128.0f
    );
    const float WarpedX = BiomeX + SampleFbm(
        BiomeX + 3527.0f,
        BiomeY - 1871.0f,
        WarpFrequency,
        3,
        0.5f
    ) * WarpAmplitude;
    const float WarpedY = BiomeY + SampleFbm(
        BiomeX - 6173.0f,
        BiomeY + 2593.0f,
        WarpFrequency,
        3,
        0.5f
    ) * WarpAmplitude;

    const float MoistureSignal = SampleFbm(
        WarpedX + 4219.0f,
        WarpedY - 1877.0f,
        Settings.Frequency * 0.55f,
        4,
        0.52f
    );
    const float TemperatureSignal = SampleFbm(
        WarpedX - 8111.0f,
        WarpedY + 3203.0f,
        Settings.Frequency * 0.38f,
        3,
        0.5f
    );
    const float CanopySignal =
        MoistureSignal * 0.72f +
        SampleFbm(
            WarpedX + 1013.0f,
            WarpedY + 6427.0f,
            Settings.Frequency * 1.35f,
            2,
            0.45f
        ) * 0.28f;

    Result.Moisture = FMath::Clamp(0.5f + MoistureSignal * 0.5f, 0.0f, 1.0f);
    const float ElevationCooling = FMath::Clamp(
        (SurfaceWorldZ - Settings.RockyMinimumWorldZ) / 96.0f,
        -0.2f,
        0.55f
    );
    Result.Temperature = FMath::Clamp(
        0.5f + TemperatureSignal * 0.5f - ElevationCooling,
        0.0f,
        1.0f
    );
    Result.RiverDistance = Settings.bGenerateRivers
        ? SampleRiverDistance(WorldX, WorldY, Settings)
        : 1.0f;

    const float GentleTerrain = 1.0f - SmoothStep(
        Settings.RockySlopeThreshold * 0.42f,
        Settings.RockySlopeThreshold,
        Slope
    );
    const float HighCountry = SmoothStep(
        Settings.RockyMinimumWorldZ - 10.0f,
        Settings.RockyMinimumWorldZ + 10.0f,
        SurfaceWorldZ
    );
    const float SteepCountry = SmoothStep(
        Settings.RockySlopeThreshold * 0.62f,
        Settings.RockySlopeThreshold,
        Slope
    );
    const float RiverInfluence = Settings.bGenerateRivers
        ? 1.0f - SmoothStep(
            Settings.WetlandRiverDistance,
            FMath::Max(
                Settings.WetlandRiverDistance + 0.001f,
                Settings.WetlandRiverDistance * 2.4f
            ),
            Result.RiverDistance
        )
        : 0.0f;
    const float SaturatedGround = SmoothStep(0.46f, 0.70f, Result.Moisture);
    const float ForestClimate = SmoothStep(
        Settings.ForestThreshold - 0.16f,
        Settings.ForestThreshold + 0.16f,
        CanopySignal
    );

    Result.WetlandWeight =
        RiverInfluence *
        SaturatedGround *
        GentleTerrain *
        (1.0f - HighCountry);
    Result.RockyWeight = FMath::Max(HighCountry, SteepCountry) *
        (1.0f - Result.WetlandWeight);
    Result.ForestWeight =
        ForestClimate *
        GentleTerrain *
        (1.0f - Result.WetlandWeight) *
        (1.0f - Result.RockyWeight);
    Result.PlainsWeight = FMath::Max(
        0.0f,
        1.0f - Result.WetlandWeight - Result.RockyWeight - Result.ForestWeight
    );

    const float TotalWeight =
        Result.PlainsWeight +
        Result.ForestWeight +
        Result.RockyWeight +
        Result.WetlandWeight;
    if (TotalWeight > KINDA_SMALL_NUMBER)
    {
        Result.PlainsWeight /= TotalWeight;
        Result.ForestWeight /= TotalWeight;
        Result.RockyWeight /= TotalWeight;
        Result.WetlandWeight /= TotalWeight;
    }

    float DominantWeight = Result.PlainsWeight;
    Result.DominantBiome = ECubusBiomeKind::Plains;
    Result.SurfaceMaterialId = Settings.PlainsSurfaceMaterialId;

    if (Result.ForestWeight > DominantWeight)
    {
        DominantWeight = Result.ForestWeight;
        Result.DominantBiome = ECubusBiomeKind::Forest;
        Result.SurfaceMaterialId = Settings.ForestSurfaceMaterialId;
    }
    if (Result.RockyWeight > DominantWeight)
    {
        DominantWeight = Result.RockyWeight;
        Result.DominantBiome = ECubusBiomeKind::Rocky;
        Result.SurfaceMaterialId = Settings.RockySurfaceMaterialId;
    }
    if (Result.WetlandWeight > DominantWeight)
    {
        Result.DominantBiome = ECubusBiomeKind::Wetland;
        Result.SurfaceMaterialId = Settings.WetlandSurfaceMaterialId;
    }

    float BestDefinitionScore = 0.0f;
    for (int32 DefinitionIndex = 0;
         DefinitionIndex < Settings.Definitions.Num();
         ++DefinitionIndex)
    {
        const FCubusBiomeDefinition& Definition =
            Settings.Definitions[DefinitionIndex];
        const float MoistureTolerance = FMath::Clamp(
            Definition.MoistureTolerance,
            0.01f,
            1.0f
        );
        const float TemperatureTolerance = FMath::Clamp(
            Definition.TemperatureTolerance,
            0.01f,
            1.0f
        );
        const float MoistureSuitability = 1.0f - SmoothStep(
            MoistureTolerance * 0.62f,
            MoistureTolerance,
            FMath::Abs(Result.Moisture - Definition.TargetMoisture)
        );
        const float TemperatureSuitability = 1.0f - SmoothStep(
            TemperatureTolerance * 0.62f,
            TemperatureTolerance,
            FMath::Abs(Result.Temperature - Definition.TargetTemperature)
        );
        const float MinimumHeight = FMath::Min(
            Definition.MinimumWorldZ,
            Definition.MaximumWorldZ
        );
        const float MaximumHeight = FMath::Max(
            Definition.MinimumWorldZ,
            Definition.MaximumWorldZ
        );
        const float HeightSuitability =
            SmoothStep(MinimumHeight - 8.0f, MinimumHeight, SurfaceWorldZ) *
            (1.0f - SmoothStep(MaximumHeight, MaximumHeight + 8.0f, SurfaceWorldZ));
        const float MaximumSlope = FMath::Max(0.01f, Definition.MaximumSlope);
        const float SlopeSuitability = 1.0f - SmoothStep(
            MaximumSlope * 0.78f,
            MaximumSlope,
            Slope
        );

        float ArchetypeWeight = Result.PlainsWeight;
        switch (Definition.Archetype)
        {
            case ECubusBiomeKind::Forest:
                ArchetypeWeight = Result.ForestWeight;
                break;
            case ECubusBiomeKind::Rocky:
                ArchetypeWeight = Result.RockyWeight;
                break;
            case ECubusBiomeKind::Wetland:
                ArchetypeWeight = Result.WetlandWeight;
                break;
            default:
                break;
        }

        const float DefinitionScore =
            MoistureSuitability *
            TemperatureSuitability *
            HeightSuitability *
            SlopeSuitability *
            FMath::Lerp(0.35f, 1.0f, ArchetypeWeight) *
            FMath::Max(0.01f, Definition.Priority);
        if (DefinitionScore > BestDefinitionScore)
        {
            BestDefinitionScore = DefinitionScore;
            Result.BiomeDefinitionIndex = DefinitionIndex;
            Result.DominantBiome = Definition.Archetype;
            Result.SurfaceMaterialId = FMath::Max(
                1,
                Definition.SurfaceMaterialId
            );
        }
    }

    return Result;
}

float FCubusBiomeField::SampleRiverDistance(
    const float WorldX,
    const float WorldY,
    const FCubusBiomeFieldSettings& InSettings
)
{
    FCubusBiomeFieldSettings Settings = InSettings;
    Settings.RiverFrequency = FMath::Max(0.000001f, Settings.RiverFrequency);
    Settings.RiverWarpFrequency = FMath::Max(0.000001f, Settings.RiverWarpFrequency);
    Settings.RiverWarpAmplitude = FMath::Max(0.0f, Settings.RiverWarpAmplitude);

    const float RiverX = WorldX + static_cast<float>(Settings.RiverOffsetX);
    const float RiverY = WorldY + static_cast<float>(Settings.RiverOffsetY);
    const float WarpX = SampleFbm(
        RiverX + 7919.0f,
        RiverY - 3571.0f,
        Settings.RiverWarpFrequency,
        3,
        0.5f
    ) * Settings.RiverWarpAmplitude;
    const float WarpY = SampleFbm(
        RiverX - 2953.0f,
        RiverY + 6211.0f,
        Settings.RiverWarpFrequency,
        3,
        0.5f
    ) * Settings.RiverWarpAmplitude;
    const float WarpedX = RiverX + WarpX;
    const float WarpedY = RiverY + WarpY;

    const float MainDistance = FMath::Abs(
        SampleFbm(
            WarpedX,
            WarpedY,
            Settings.RiverFrequency,
            2,
            0.32f
        )
    );
    const float TributaryDistance = FMath::Abs(
        SampleFbm(
            WarpedX * 0.78f - WarpedY * 0.63f + 4651.0f,
            WarpedX * 0.63f + WarpedY * 0.78f - 1307.0f,
            Settings.RiverFrequency * 1.9f,
            2,
            0.3f
        )
    );
    const float Catchment = SmoothStep(
        -0.12f,
        0.38f,
        SampleFbm(
            WarpedX - 8837.0f,
            WarpedY + 5279.0f,
            Settings.RiverFrequency * 0.72f,
            2,
            0.5f
        )
    );
    const float GatedTributaryDistance = FMath::Lerp(
        1.0f,
        TributaryDistance,
        Catchment
    );
    return FMath::Min(MainDistance, GatedTributaryDistance);
}

float FCubusBiomeField::SampleNoise(
    const float WorldX,
    const float WorldY,
    const float Frequency
)
{
    return FMath::PerlinNoise2D(
        FVector2D(
            WorldX * FMath::Max(0.000001f, Frequency),
            WorldY * FMath::Max(0.000001f, Frequency)
        )
    );
}

float FCubusBiomeField::SampleFbm(
    const float WorldX,
    const float WorldY,
    const float Frequency,
    const int32 Octaves,
    const float Gain
)
{
    float Sum = 0.0f;
    float Weight = 1.0f;
    float TotalWeight = 0.0f;
    float CurrentFrequency = FMath::Max(0.000001f, Frequency);

    for (int32 Octave = 0; Octave < FMath::Clamp(Octaves, 1, 6); ++Octave)
    {
        Sum += SampleNoise(
            WorldX + static_cast<float>(Octave) * 1747.0f,
            WorldY - static_cast<float>(Octave) * 2081.0f,
            CurrentFrequency
        ) * Weight;
        TotalWeight += Weight;
        CurrentFrequency *= 2.03f;
        Weight *= FMath::Clamp(Gain, 0.01f, 0.99f);
    }

    return TotalWeight > KINDA_SMALL_NUMBER ? Sum / TotalWeight : 0.0f;
}

float FCubusBiomeField::SmoothStep(
    const float EdgeMinimum,
    const float EdgeMaximum,
    const float Value
)
{
    if (FMath::IsNearlyEqual(EdgeMinimum, EdgeMaximum))
    {
        return Value >= EdgeMaximum ? 1.0f : 0.0f;
    }
    const float Alpha = FMath::Clamp(
        (Value - EdgeMinimum) / (EdgeMaximum - EdgeMinimum),
        0.0f,
        1.0f
    );
    return Alpha * Alpha * (3.0f - 2.0f * Alpha);
}
