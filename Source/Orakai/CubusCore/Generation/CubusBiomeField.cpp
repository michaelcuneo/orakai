#include "CubusCore/Generation/CubusBiomeField.h"

#include "CubusCore/Data/CubusGeologyProfile.h"
#include "CubusCore/Generation/CubusGenerationSeeds.h"

namespace
{
    ECubusClimateProvince ResolveClimateProvince(const FCubusBiomeClimateContext& Climate)
    {
        const float Moisture = FMath::Clamp(
            Climate.MoistureTransport * 0.68f + (0.5f + Climate.ProvinceMoisture * 0.5f) * 0.32f,
            0.0f,
            1.0f
        );
        const float Temperature = FMath::Clamp(0.5f + Climate.ProvinceTemperature * 0.5f, 0.0f, 1.0f);

        if (Moisture >= 0.64f)
        {
            return Temperature < 0.44f ? ECubusClimateProvince::HumidCool : ECubusClimateProvince::HumidTemperate;
        }
        if (Moisture <= 0.36f)
        {
            return Temperature >= 0.52f ? ECubusClimateProvince::DryWarm : ECubusClimateProvince::DryCool;
        }
        if (Climate.Continentality >= 0.62f)
        {
            return ECubusClimateProvince::ContinentalInterior;
        }
        return ECubusClimateProvince::TemperateTransition;
    }

    ECubusElevationZone ResolveElevationZone(const float ElevationNormalized)
    {
        if (ElevationNormalized < 0.18f) return ECubusElevationZone::Lowland;
        if (ElevationNormalized < 0.38f) return ECubusElevationZone::Foothill;
        if (ElevationNormalized < 0.62f) return ECubusElevationZone::Montane;
        if (ElevationNormalized < 0.82f) return ECubusElevationZone::Subalpine;
        if (ElevationNormalized < 1.00f) return ECubusElevationZone::Alpine;
        return ECubusElevationZone::Nival;
    }

    ECubusTerrainPosition ResolveTerrainPosition(
        const float Floodplain,
        const float ValleyFloor,
        const float LowerSlope,
        const float Shoulder,
        const float Ridge
    )
    {
        if (Floodplain >= 0.55f) return ECubusTerrainPosition::Floodplain;
        if (ValleyFloor >= 0.52f) return ECubusTerrainPosition::ValleyFloor;
        if (Ridge >= 0.52f) return ECubusTerrainPosition::Ridge;
        if (Shoulder >= 0.46f) return ECubusTerrainPosition::Shoulder;
        if (LowerSlope >= 0.44f) return ECubusTerrainPosition::LowerSlope;
        return ECubusTerrainPosition::Midslope;
    }
}

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
    Settings.HydrologySettings.RiverSeed = RiverSeed;
    if (!IsValid(GeologyProfile))
    {
        return Settings;
    }

    Settings.bEnabled = GeologyProfile->bGenerateBiomes;
    Settings.Frequency = GeologyProfile->BiomeFrequency;
    Settings.PrevailingWindDirection = GeologyProfile->BiomePrevailingWindDirection;
    Settings.SolarDirection = GeologyProfile->BiomeSolarDirection;
    Settings.ForestThreshold = FMath::IsNearlyEqual(GeologyProfile->ForestThreshold, 0.15f, 0.0001f)
        ? 0.0f
        : GeologyProfile->ForestThreshold;
    Settings.WetlandRiverDistance = GeologyProfile->WetlandRiverDistance;
    Settings.RockySlopeThreshold = FMath::IsNearlyEqual(GeologyProfile->RockySlopeThreshold, 4.0f, 0.001f)
        ? 1.15f
        : GeologyProfile->RockySlopeThreshold;
    Settings.RockyMinimumWorldZ = static_cast<float>(GeologyProfile->RockyMinimumWorldZ);
    Settings.PlainsSurfaceMaterialId = GeologyProfile->PlainsSurfaceMaterialId;
    Settings.ForestSurfaceMaterialId = GeologyProfile->ForestSurfaceMaterialId;
    Settings.RockySurfaceMaterialId = GeologyProfile->RockySurfaceMaterialId;
    Settings.WetlandSurfaceMaterialId = GeologyProfile->WetlandSurfaceMaterialId;
    Settings.bGenerateRivers = GeologyProfile->bGenerateRivers;
    Settings.RiverFrequency = GeologyProfile->RiverFrequency;
    Settings.RiverWarpAmplitude = GeologyProfile->RiverWarpAmplitude;
    Settings.RiverWarpFrequency = GeologyProfile->RiverWarpFrequency;
    Settings.HydrologySettings.bEnabled = false;
    Settings.HydrologySettings.ValleyDepth = FMath::Max(0.0f, GeologyProfile->RiverValleyDepth);
    Settings.HydrologySettings.ChannelDepth = FMath::Max(0.0f, static_cast<float>(GeologyProfile->RiverChannelDepth));
    Settings.HydrologySettings.ChannelHalfWidth = FMath::Max(2.0f, GeologyProfile->RiverChannelWidth * 64.0f);
    Settings.HydrologySettings.ValleyHalfWidth = FMath::Max(
        Settings.HydrologySettings.ChannelHalfWidth + 4.0f,
        GeologyProfile->RiverValleyWidth * 128.0f
    );
    Settings.Definitions = GeologyProfile->BiomeDefinitions;
    return Settings;
}

FCubusBiomeFieldSettings FCubusBiomeField::BindHydrology(
    const FCubusBiomeFieldSettings& BaseSettings,
    const FCubusHydrologySettings& HydrologySettings
)
{
    FCubusBiomeFieldSettings Result = BaseSettings;
    Result.HydrologySettings = HydrologySettings;
    Result.bGenerateRivers = BaseSettings.bGenerateRivers && HydrologySettings.bEnabled;

    const FCubusTerrainFormSettings& Terrain = HydrologySettings.TerrainFormSettings;
    const float StructuralNivalWorldZ = Terrain.BaseHeight + FMath::Max(64.0f, Terrain.RidgeAmplitude * 4.0f);
    Result.NivalWorldZ = FMath::Max(
        Result.NivalWorldZ > HydrologySettings.SeaLevel + 1.0f ? Result.NivalWorldZ : StructuralNivalWorldZ,
        StructuralNivalWorldZ
    );
    return Result;
}

FCubusBiomeClimateContext FCubusBiomeField::SampleClimate(
    const float WorldX,
    const float WorldY,
    const FCubusBiomeFieldSettings& InSettings
)
{
    FCubusBiomeFieldSettings Settings = InSettings;
    Settings.Frequency = FMath::Max(0.000001f, Settings.Frequency);

    const float BiomeX = WorldX + static_cast<float>(Settings.BiomeOffsetX);
    const float BiomeY = WorldY + static_cast<float>(Settings.BiomeOffsetY);
    const float ClimateProvinceFrequency = Settings.Frequency * 0.18f;
    const float ClimateRegionalFrequency = Settings.Frequency * 0.62f;
    const float ClimateLocalFrequency = Settings.Frequency * 1.55f;
    const float WarpFrequency = ClimateProvinceFrequency * 0.55f;
    const float WarpAmplitude = FMath::Clamp(0.18f / ClimateProvinceFrequency, 96.0f, 520.0f);
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

    FCubusBiomeClimateContext Result;
    Result.ProvinceMoisture = SampleFbm(WarpedX + 4219.0f, WarpedY - 1877.0f, ClimateProvinceFrequency * 0.95f, 4, 0.52f);
    Result.RegionalMoisture = SampleFbm(WarpedX - 11549.0f, WarpedY + 9011.0f, ClimateRegionalFrequency * 0.90f, 3, 0.50f);
    Result.LocalHumidity = SampleFbm(WarpedX + 1013.0f, WarpedY + 6427.0f, ClimateLocalFrequency, 2, 0.45f);
    Result.ProvinceTemperature = SampleFbm(WarpedX - 8111.0f, WarpedY + 3203.0f, ClimateProvinceFrequency * 0.78f, 4, 0.52f);
    Result.RegionalTemperature = SampleFbm(WarpedX + 14831.0f, WarpedY - 10427.0f, ClimateRegionalFrequency * 0.82f, 3, 0.50f);
    Result.LocalTemperature = SampleFbm(WarpedX - 2791.0f, WarpedY - 15317.0f, ClimateLocalFrequency * 0.72f, 2, 0.45f);
    Result.MoistureTransport = FMath::Clamp(
        0.5f + SampleFbm(WarpedX + 34817.0f, WarpedY - 28411.0f, ClimateProvinceFrequency * 0.54f, 3, 0.52f) * 0.5f,
        0.0f,
        1.0f
    );
    Result.Continentality = FMath::Clamp(
        0.5f + SampleFbm(WarpedX - 26339.0f, WarpedY + 31847.0f, ClimateProvinceFrequency * 0.43f, 3, 0.52f) * 0.5f,
        0.0f,
        1.0f
    );
    Result.CommunityPatch[0] = FMath::Clamp(0.5f + SampleFbm(WarpedX + 17011.0f, WarpedY - 9017.0f, Settings.Frequency * 0.95f, 2, 0.48f) * 0.5f, 0.0f, 1.0f);
    Result.CommunityPatch[1] = FMath::Clamp(0.5f + SampleFbm(WarpedX - 2477.0f, WarpedY + 19421.0f, Settings.Frequency * 1.25f, 2, 0.48f) * 0.5f, 0.0f, 1.0f);
    Result.CommunityPatch[2] = FMath::Clamp(0.5f + SampleFbm(WarpedX + 29831.0f, WarpedY + 4711.0f, Settings.Frequency * 1.55f, 2, 0.46f) * 0.5f, 0.0f, 1.0f);
    Result.CommunityPatch[3] = FMath::Clamp(0.5f + SampleFbm(WarpedX - 15331.0f, WarpedY - 22051.0f, Settings.Frequency * 0.72f, 2, 0.50f) * 0.5f, 0.0f, 1.0f);
    Result.ParentMaterial = FMath::Clamp(
        0.5f + SampleFbm(WarpedX + 7937.0f, WarpedY - 12011.0f, Settings.Frequency * 0.88f, 3, 0.50f) * 0.5f,
        0.0f,
        1.0f
    );
    return Result;
}

FCubusBiomeClimateContext FCubusBiomeField::LerpClimate(
    const FCubusBiomeClimateContext& A,
    const FCubusBiomeClimateContext& B,
    const float Alpha
)
{
    const float T = FMath::Clamp(Alpha, 0.0f, 1.0f);
    FCubusBiomeClimateContext Result;
    Result.ProvinceMoisture = FMath::Lerp(A.ProvinceMoisture, B.ProvinceMoisture, T);
    Result.RegionalMoisture = FMath::Lerp(A.RegionalMoisture, B.RegionalMoisture, T);
    Result.LocalHumidity = FMath::Lerp(A.LocalHumidity, B.LocalHumidity, T);
    Result.ProvinceTemperature = FMath::Lerp(A.ProvinceTemperature, B.ProvinceTemperature, T);
    Result.RegionalTemperature = FMath::Lerp(A.RegionalTemperature, B.RegionalTemperature, T);
    Result.LocalTemperature = FMath::Lerp(A.LocalTemperature, B.LocalTemperature, T);
    Result.MoistureTransport = FMath::Lerp(A.MoistureTransport, B.MoistureTransport, T);
    Result.Continentality = FMath::Lerp(A.Continentality, B.Continentality, T);
    for (int32 Index = 0; Index < 4; ++Index)
    {
        Result.CommunityPatch[Index] = FMath::Lerp(A.CommunityPatch[Index], B.CommunityPatch[Index], T);
    }
    Result.ParentMaterial = FMath::Lerp(A.ParentMaterial, B.ParentMaterial, T);
    return Result;
}

FCubusBiomeSample FCubusBiomeField::Sample(
    const float WorldX,
    const float WorldY,
    const float SurfaceWorldZ,
    const float Slope,
    const FCubusBiomeFieldSettings& InSettings,
    const FCubusBiomeTerrainContext& TerrainContext,
    const FCubusBiomeClimateContext* ClimateContext
)
{
    FCubusBiomeFieldSettings Settings = InSettings;
    Settings.Frequency = FMath::Max(0.000001f, Settings.Frequency);
    Settings.ForestThreshold = FMath::Clamp(Settings.ForestThreshold, -1.0f, 1.0f);
    Settings.WetlandRiverDistance = FMath::Clamp(Settings.WetlandRiverDistance, 0.0f, 1.0f);
    Settings.RockySlopeThreshold = FMath::Max(0.01f, Settings.RockySlopeThreshold);
    Settings.PlainsSurfaceMaterialId = FMath::Max(1, Settings.PlainsSurfaceMaterialId);
    Settings.ForestSurfaceMaterialId = FMath::Max(1, Settings.ForestSurfaceMaterialId);
    Settings.RockySurfaceMaterialId = FMath::Max(1, Settings.RockySurfaceMaterialId);
    Settings.WetlandSurfaceMaterialId = FMath::Max(1, Settings.WetlandSurfaceMaterialId);

    FCubusBiomeSample Result;
    Result.SurfaceWorldZ = SurfaceWorldZ;
    Result.Slope = Slope;
    Result.SurfaceMaterialId = Settings.PlainsSurfaceMaterialId;
    if (!Settings.bEnabled)
    {
        return Result;
    }

    /* Reuse the authoritative terrain form rather than inventing biome-only geography. */
    FCubusBiomeTerrainContext Ecology = TerrainContext;
    const float TerrainX = WorldX + static_cast<float>(Settings.HydrologySettings.TerrainOffsetX);
    const float TerrainY = WorldY + static_cast<float>(Settings.HydrologySettings.TerrainOffsetY);
    const FCubusTerrainFormSample Form = TerrainContext.bHasTerrainFormSample
        ? TerrainContext.TerrainFormSample
        : FCubusTerrainForm::Sample(
            TerrainX,
            TerrainY,
            Settings.HydrologySettings.TerrainFormSettings
        );
    Ecology.Drainage = FMath::Max(Ecology.Drainage, Form.Drainage);
    Ecology.MountainCore = FMath::Max(Ecology.MountainCore, Form.MountainCore);
    Ecology.FoothillWeight = FMath::Max(Ecology.FoothillWeight, Form.FoothillWeight);
    Ecology.Ridge = FMath::Max(Ecology.Ridge, Form.Ridge);
    if (Ecology.RockExposure <= KINDA_SMALL_NUMBER)
    {
        const float CliffExposure = SmoothStep(
  Settings.RockySlopeThreshold * 0.45f,
  Settings.RockySlopeThreshold,
  Slope
        );
        Ecology.RockExposure = FMath::Clamp(
  CliffExposure * (
      Form.MountainCore * 0.72f +
      Form.FoothillWeight * 0.32f +
      Form.Ridge * 0.28f
  ),
  0.0f,
  1.0f
        );
    }

    /* Slow climate noise can be supplied from a coarse deterministic lattice. */
    const FCubusBiomeClimateContext Climate = ClimateContext != nullptr
        ? *ClimateContext
        : SampleClimate(WorldX, WorldY, Settings);
    Result.ClimateProvince = ResolveClimateProvince(Climate);
    Result.ClimateMoistureTransport = Climate.MoistureTransport;
    Result.ClimateContinentality = Climate.Continentality;

    const bool bHasHydrology = Settings.bGenerateRivers && Settings.HydrologySettings.bEnabled;
    const FCubusHydrologySample Hydrology = bHasHydrology
        ? (TerrainContext.bHasHydrologySample
            ? TerrainContext.HydrologySample
            : FCubusHydrologyField::Sample(WorldX, WorldY, Settings.HydrologySettings))
        : FCubusHydrologySample();
    Result.RiverDistance = bHasHydrology
        ? FCubusHydrologyField::NormalizeRiverDistance(Hydrology, Settings.HydrologySettings)
        : 1.0f;
    Result.RiverInfluence = bHasHydrology
        ? 1.0f - SmoothStep(
  Settings.WetlandRiverDistance,
  FMath::Max(Settings.WetlandRiverDistance + 0.001f, Settings.WetlandRiverDistance * 2.8f),
  Result.RiverDistance
        )
        : 0.0f;
    Result.Drainage = FMath::Clamp(Ecology.Drainage, 0.0f, 1.0f);
    Result.RockExposure = FMath::Clamp(Ecology.RockExposure, 0.0f, 1.0f);
    Result.FlowConvergence = FMath::Clamp(
        FMath::Max(Result.Drainage * 0.58f, bHasHydrology ? Hydrology.ChannelStrength : 0.0f),
        0.0f,
        1.0f
    );

    /* Relative elevation is derived from the same structural snow floor used by density snow. */
    const FCubusTerrainFormSettings& TerrainFormSettings = Settings.HydrologySettings.TerrainFormSettings;
    const float StructuralNivalWorldZ = TerrainFormSettings.BaseHeight + FMath::Max(64.0f, TerrainFormSettings.RidgeAmplitude * 4.0f);
    const float SeaLevel = Settings.HydrologySettings.SeaLevel;
    const float NivalWorldZ = FMath::Max(
        Settings.NivalWorldZ > SeaLevel + 1.0f ? Settings.NivalWorldZ : StructuralNivalWorldZ,
        StructuralNivalWorldZ
    );
    const float ElevationSpan = FMath::Max(24.0f, NivalWorldZ - SeaLevel);
    Result.ElevationNormalized = FMath::Max(0.0f, (SurfaceWorldZ - SeaLevel) / ElevationSpan);
    Result.ElevationZone = ResolveElevationZone(Result.ElevationNormalized);
    Result.TreeLineWeight = 1.0f - SmoothStep(0.70f, 0.91f, Result.ElevationNormalized);
    Result.AlpineInfluence = SmoothStep(0.70f, 0.92f, Result.ElevationNormalized);
    Result.NivalInfluence = SmoothStep(0.98f, 1.06f, Result.ElevationNormalized);

    const float GradientLength = Ecology.Gradient.Size();
    const FVector2D UphillDirection = GradientLength > KINDA_SMALL_NUMBER
        ? Ecology.Gradient / GradientLength
        : FVector2D::ZeroVector;
    const FVector2D DownhillDirection = UphillDirection * -1.0f;
    FVector2D PrevailingWind = Settings.PrevailingWindDirection;
    if (PrevailingWind.SizeSquared() <= KINDA_SMALL_NUMBER)
    {
        PrevailingWind = FVector2D(0.82f, 0.57f);
    }
    PrevailingWind.Normalize();

    FVector2D SolarDirection = Settings.SolarDirection;
    if (SolarDirection.SizeSquared() <= KINDA_SMALL_NUMBER)
    {
        SolarDirection = FVector2D(-0.42f, -0.91f);
    }
    SolarDirection.Normalize();
    const float Windwardness = GradientLength > KINDA_SMALL_NUMBER
        ? FMath::Clamp(FVector2D::DotProduct(UphillDirection, PrevailingWind) * 0.5f + 0.5f, 0.0f, 1.0f)
        : 0.5f;
    const float TopographicExposure = FMath::Clamp(
        Ecology.Ridge * 0.42f +
        Ecology.MountainCore * 0.28f +
        SmoothStep(Settings.RockySlopeThreshold * 0.30f, Settings.RockySlopeThreshold, Slope) * 0.30f,
        0.0f,
        1.0f
    );
    const float SlopeAspectStrength = SmoothStep(0.08f, Settings.RockySlopeThreshold * 0.85f, Slope);
    const float SolarFacing = GradientLength > KINDA_SMALL_NUMBER
        ? FVector2D::DotProduct(DownhillDirection, SolarDirection)
        : 0.0f;
    Result.AspectRadians = GradientLength > KINDA_SMALL_NUMBER
        ? FMath::Atan2(UphillDirection.Y, UphillDirection.X)
        : 0.0f;
    const FCubusBiomeTopographicClimateContext TopographicClimate = TerrainContext.bHasTopographicClimateSample
        ? TerrainContext.TopographicClimateSample
        : FCubusBiomeTopographicClimateContext();
    Result.OrographicLift = FMath::Clamp(
        TerrainContext.bHasTopographicClimateSample
            ? TopographicClimate.OrographicLift
            : Windwardness * (Ecology.FoothillWeight * 0.46f + Ecology.MountainCore * 0.34f),
        0.0f,
        1.0f
    );
    Result.HorizonRainShadow = FMath::Clamp(
        TerrainContext.bHasTopographicClimateSample ? TopographicClimate.RainShadow : 0.0f,
        0.0f,
        1.0f
    );
    Result.SolarOcclusion = FMath::Clamp(
        TerrainContext.bHasTopographicClimateSample ? TopographicClimate.SolarOcclusion : 0.0f,
        0.0f,
        1.0f
    );
    Result.SkyViewFactor = FMath::Clamp(
        TerrainContext.bHasTopographicClimateSample ? TopographicClimate.SkyViewFactor : 1.0f,
        0.0f,
        1.0f
    );

    const float AspectSolarExposure = FMath::Clamp(
        0.50f + SolarFacing * 0.36f * SlopeAspectStrength + (1.0f - TopographicExposure) * 0.08f,
        0.0f,
        1.0f
    );
    Result.SolarExposure = FMath::Clamp(
        AspectSolarExposure * FMath::Lerp(1.0f, 0.42f, Result.SolarOcclusion) *
        FMath::Lerp(0.88f, 1.04f, Result.SkyViewFactor),
        0.0f,
        1.0f
    );
    Result.WindExposure = FMath::Clamp(
        TopographicExposure * FMath::Lerp(0.62f, 1.16f, Windwardness) *
        FMath::Lerp(0.82f, 1.08f, Result.SkyViewFactor),
        0.0f,
        1.0f
    );
    Result.WindShelter = 1.0f - Result.WindExposure;
    const float LocalRainShadow = FMath::Clamp(
        (1.0f - Windwardness) * (Ecology.FoothillWeight * 0.34f + Ecology.MountainCore * 0.58f),
        0.0f,
        1.0f
    );
    Result.RainShadow = FMath::Clamp(
        LocalRainShadow * 0.48f + Result.HorizonRainShadow * 0.78f,
        0.0f,
        1.0f
    );
    Result.Exposure = FMath::Clamp(
        Result.WindExposure * 0.68f + Result.SolarExposure * 0.08f +
        Result.RockExposure * 0.16f + Result.SkyViewFactor * 0.08f,
        0.0f,
        1.0f
    );

    const float GentleGround = 1.0f - SmoothStep(
        Settings.RockySlopeThreshold * 0.35f,
        Settings.RockySlopeThreshold * 1.05f,
        Slope
    );

    /*
     * Landform position is derived from actual terrain/hydrology signals, not
     * another biome noise layer. This lets one climate province express dry
     * ridges, sheltered shoulders, cold valley floors and active floodplains.
     */
    Result.FloodplainInfluence = FMath::Clamp(
        GentleGround * FMath::Max(Result.RiverInfluence, Result.FlowConvergence * Result.Drainage * 0.72f),
        0.0f,
        1.0f
    );
    Result.ValleyFloorInfluence = FMath::Clamp(
        GentleGround * FMath::Max(Result.Drainage * 0.78f, Result.FlowConvergence * 0.88f) *
        (1.0f - Ecology.Ridge * 0.72f),
        0.0f,
        1.0f
    );
    Result.RidgeInfluence = FMath::Clamp(
        Ecology.Ridge * (1.0f - Result.FlowConvergence * 0.62f) * FMath::Lerp(0.72f, 1.08f, Result.WindExposure),
        0.0f,
        1.0f
    );
    Result.ShoulderInfluence = FMath::Clamp(
        SlopeAspectStrength * (Ecology.Ridge * 0.46f + Ecology.FoothillWeight * 0.30f) *
        (1.0f - Result.ValleyFloorInfluence * 0.74f),
        0.0f,
        1.0f
    );
    Result.LowerSlopeInfluence = FMath::Clamp(
        SlopeAspectStrength * (Result.FlowConvergence * 0.62f + Result.Drainage * 0.38f) *
        (1.0f - Result.FloodplainInfluence * 0.62f),
        0.0f,
        1.0f
    );
    Result.TerrainPosition = ResolveTerrainPosition(
        Result.FloodplainInfluence,
        Result.ValleyFloorInfluence,
        Result.LowerSlopeInfluence,
        Result.ShoulderInfluence,
        Result.RidgeInfluence
    );
    Result.ColdAirPooling = FMath::Clamp(
        Result.ValleyFloorInfluence * Result.WindShelter *
        FMath::Lerp(1.0f, 0.62f, Result.SolarExposure) *
        (1.0f - Result.RidgeInfluence * 0.80f),
        0.0f,
        1.0f
    );

    const float DepositionalGround = FMath::Clamp(
        Result.FlowConvergence * 0.48f + Result.Drainage * 0.22f + Result.RiverInfluence * 0.30f,
        0.0f,
        1.0f
    );

    /*
     * Soil now inherits the density world's actual rock fabric. Hard coherent
     * rock tends toward thin coarse regolith; fractured/softer rock weathers
     * deeper, but fractures also increase drainage through the substrate.
     * Standalone biome sampling retains the old seeded parent-material field
     * as a neutral fallback when no density geology context is available.
     */
    Result.SubstrateHardness = TerrainContext.bHasSubstrateSample
        ? FMath::Clamp(TerrainContext.SubstrateHardness, 0.0f, 1.0f)
        : FMath::Clamp(1.0f - Climate.ParentMaterial * 0.72f, 0.0f, 1.0f);
    Result.FractureDensity = TerrainContext.bHasSubstrateSample
        ? FMath::Clamp(TerrainContext.FractureDensity, 0.0f, 1.0f)
        : FMath::Clamp(Climate.ParentMaterial * 0.44f, 0.0f, 1.0f);
    const float SubstrateWeatherability = FMath::Clamp(
        (1.0f - Result.SubstrateHardness) * 0.62f + Result.FractureDensity * 0.38f,
        0.0f,
        1.0f
    );
    Result.SoilCoarseness = FMath::Clamp(
        Result.SubstrateHardness * 0.46f +
        Result.FractureDensity * 0.32f +
        Result.RockExposure * 0.28f -
        DepositionalGround * 0.20f,
        0.0f,
        1.0f
    );
    Result.SoilPermeability = FMath::Clamp(
        0.16f +
        Result.SoilCoarseness * 0.34f +
        Result.FractureDensity * 0.26f +
        Result.RockExposure * 0.12f -
        Result.FlowConvergence * 0.10f,
        0.0f,
        1.0f
    );
    Result.WaterHoldingCapacity = FMath::Clamp(
        0.28f +
        (1.0f - Result.SoilCoarseness) * 0.36f +
        SubstrateWeatherability * 0.18f +
        DepositionalGround * 0.20f -
        Result.RockExposure * 0.12f,
        0.0f,
        1.0f
    );

    const float SlopeErosion = SmoothStep(
        Settings.RockySlopeThreshold * 0.28f,
        Settings.RockySlopeThreshold * 1.10f,
        Slope
    );
    Result.Erosion = FMath::Clamp(
        SlopeErosion * 0.50f + Result.WindExposure * 0.22f + Result.RockExposure * 0.20f - DepositionalGround * 0.24f,
        0.0f,
        1.0f
    );
    const float FloodDisturbance = FMath::Clamp(
        Result.FloodplainInfluence * (0.28f + Result.RiverInfluence * 0.72f),
        0.0f,
        1.0f
    );
    Result.Disturbance = FMath::Clamp(
        FMath::Max(
            FMath::Max(Result.Erosion, FloodDisturbance),
            FMath::Max(
                Result.WindExposure * 0.62f + Form.SurfaceRoughness * 0.18f,
                Form.ErosionRills * 0.72f + Result.RockExposure * 0.28f
            )
        ),
        0.0f,
        1.0f
    );
    const float SoilRetention = FMath::Clamp(
        GentleGround * (1.0f - Result.RockExposure) * (1.0f - Result.Erosion * 0.72f),
        0.0f,
        1.0f
    );
    Result.SoilDepth = FMath::Clamp(
        SoilRetention *
        FMath::Lerp(0.62f, 1.16f, SubstrateWeatherability) *
        FMath::Lerp(0.54f, 1.18f, DepositionalGround),
        0.0f,
        1.0f
    );

    const float OrographicMoisture = Result.OrographicLift * FMath::Lerp(0.08f, 0.22f, Climate.MoistureTransport);
    const float RegionalTransportMoisture = (Climate.MoistureTransport - 0.5f) * 0.18f;
    const float ContinentalDrying = (Climate.Continentality - 0.5f) * 0.10f;
    const float DrainageMoisture = Result.FlowConvergence * 0.12f + Result.RiverInfluence * 0.24f;
    const float ExposureDrying = Result.WindExposure * 0.10f + Result.SolarExposure * 0.06f + Result.RockExposure * 0.07f;
    Result.Moisture = FMath::Clamp(
        0.50f +
        Climate.ProvinceMoisture * 0.26f +
        Climate.RegionalMoisture * 0.18f +
        Climate.LocalHumidity * 0.08f +
        RegionalTransportMoisture +
        OrographicMoisture +
        DrainageMoisture +
        Result.ColdAirPooling * 0.055f -
        ContinentalDrying -
        Result.RainShadow * FMath::Lerp(0.14f, 0.26f, Climate.Continentality) -
        ExposureDrying,
        0.0f,
        1.0f
    );

    Result.SurfaceWetness = FMath::Clamp(
        Result.Moisture * 0.34f +
        Result.RiverInfluence * 0.28f +
        Result.FlowConvergence * 0.22f +
        Result.Drainage * 0.16f -
        Result.SolarExposure * 0.08f,
        0.0f,
        1.0f
    );
    Result.GroundwaterPotential = FMath::Clamp(
        Result.RiverInfluence * 0.42f +
        Result.FlowConvergence * 0.28f +
        DepositionalGround * 0.18f +
        Result.WindShelter * 0.12f,
        0.0f,
        1.0f
    );
    Result.SoilSaturation = FMath::Clamp(
        Result.SurfaceWetness * FMath::Lerp(0.64f, 1.08f, Result.WaterHoldingCapacity) *
        (1.0f - Result.SoilPermeability * 0.44f) +
        Result.GroundwaterPotential * 0.32f,
        0.0f,
        1.0f
    );
    Result.SoilMoisture = FMath::Clamp(
        Result.Moisture * 0.44f + Result.SoilSaturation * 0.46f + Result.GroundwaterPotential * 0.10f,
        0.0f,
        1.0f
    );

    /* Elevation modifies climate, but it is no longer a single hard HighCountry switch. */
    const float ElevationCooling = FMath::Clamp(Result.ElevationNormalized, 0.0f, 1.25f) * 0.36f;
    const float ContinentalThermalContrast = FMath::Lerp(0.78f, 1.30f, Climate.Continentality);
    const float SolarTemperature = (Result.SolarExposure - 0.5f) * 0.10f * ContinentalThermalContrast;
    Result.Temperature = FMath::Clamp(
        0.50f +
        Climate.ProvinceTemperature * 0.32f +
        Climate.RegionalTemperature * 0.20f +
        Climate.LocalTemperature * 0.08f -
        ElevationCooling +
        SolarTemperature -
        Result.WindExposure * 0.035f -
        Result.ColdAirPooling * 0.11f * ContinentalThermalContrast,
        0.0f,
        1.0f
    );

    const float ClimateProductivity =
        SmoothStep(0.12f, 0.48f, Result.SoilMoisture) *
        (1.0f - SmoothStep(0.90f, 1.0f, Result.SoilSaturation)) *
        SmoothStep(0.10f, 0.42f, Result.Temperature) *
        (1.0f - SmoothStep(0.86f, 1.0f, Result.Temperature));
    Result.OrganicMatter = FMath::Clamp(
        ClimateProductivity * Result.SoilDepth * (1.0f - Result.Erosion * 0.68f) *
        FMath::Lerp(0.72f, 1.08f, Result.SoilMoisture) * (1.0f - Result.NivalInfluence),
        0.0f,
        1.0f
    );
    Result.Fertility = FMath::Clamp(
        Result.SoilDepth * 0.34f +
        Result.OrganicMatter * 0.34f +
        DepositionalGround * 0.18f +
        SubstrateWeatherability * 0.14f,
        0.0f,
        1.0f
    );

    /* Compatibility archetypes are now projections of the richer ecology, not the primary classifier. */
    const float GentleTerrain = 1.0f - SmoothStep(
        Settings.RockySlopeThreshold * 0.42f,
        Settings.RockySlopeThreshold,
        Slope
    );
    const float SteepCountry = SmoothStep(
        Settings.RockySlopeThreshold * 0.62f,
        Settings.RockySlopeThreshold,
        Slope
    );
    const float SaturatedGround = SmoothStep(0.48f, 0.78f, Result.SoilSaturation) *
        SmoothStep(0.24f, 0.62f, Result.SoilDepth);
    const float ForestClimate =
        SmoothStep(0.22f, 0.62f, Result.Fertility) *
        SmoothStep(0.24f, 0.58f, Result.SoilMoisture) *
        (1.0f - SmoothStep(0.92f, 1.0f, Result.SoilSaturation));
    Result.CanopyPotential = FMath::Clamp(
        ForestClimate * Result.TreeLineWeight *
        FMath::Lerp(0.38f, 1.0f, Result.SoilDepth) *
        (1.0f - Result.RockExposure * 0.78f) *
        (1.0f - Result.Disturbance * 0.64f) *
        FMath::Lerp(0.78f, 1.08f, Result.WindShelter),
        0.0f,
        1.0f
    );
    Result.CanopyOpenness = 1.0f - Result.CanopyPotential;

    Result.WetlandWeight = FMath::Clamp(
        FMath::Max(
  Result.RiverInfluence * SaturatedGround,
  Result.GroundwaterPotential * SaturatedGround * 0.90f
        ) * GentleTerrain * (1.0f - Result.NivalInfluence),
        0.0f,
        1.0f
    );
    Result.RockyWeight = FMath::Clamp(
        FMath::Max(
  FMath::Max(SteepCountry, Result.RockExposure),
  FMath::Max(
      Result.AlpineInfluence * (1.0f - Result.SoilDepth * 0.68f) * (0.40f + Result.Exposure * 0.60f),
      Result.NivalInfluence * 0.86f
  )
        ) * (1.0f - Result.WetlandWeight),
        0.0f,
        1.0f
    );
    Result.ForestWeight = FMath::Clamp(
        ForestClimate * GentleTerrain * Result.TreeLineWeight *
        FMath::Lerp(0.52f, 1.10f, Result.CanopyPotential) *
        (1.0f - Result.WindExposure * 0.58f) *
        (1.0f - Result.WetlandWeight) *
        (1.0f - Result.RockyWeight * 0.88f),
        0.0f,
        1.0f
    );
    Result.PlainsWeight = FMath::Max(
        0.0f,
        1.0f - Result.WetlandWeight - Result.RockyWeight - Result.ForestWeight
    );

    const float TotalWeight = Result.PlainsWeight + Result.ForestWeight + Result.RockyWeight + Result.WetlandWeight;
    if (TotalWeight > KINDA_SMALL_NUMBER)
    {
        Result.PlainsWeight /= TotalWeight;
        Result.ForestWeight /= TotalWeight;
        Result.RockyWeight /= TotalWeight;
        Result.WetlandWeight /= TotalWeight;
    }

    auto InsertCommunity = [&Result](FCubusBiomeCommunityBlend Candidate, const float Score)
    {
        if (Score <= KINDA_SMALL_NUMBER)
        {
  return;
        }

        Candidate.Weight = Score;
        int32 InsertIndex = Result.CommunityBlendCount;
        for (int32 Index = 0; Index < Result.CommunityBlendCount; ++Index)
        {
  if (Score > Result.CommunityBlend[Index].Weight)
  {
      InsertIndex = Index;
      break;
  }
        }

        if (InsertIndex >= FCubusBiomeSample::MaxCommunityBlendCount)
        {
  return;
        }

        if (Result.CommunityBlendCount < FCubusBiomeSample::MaxCommunityBlendCount)
        {
  ++Result.CommunityBlendCount;
        }

        for (int32 Index = Result.CommunityBlendCount - 1; Index > InsertIndex; --Index)
        {
  Result.CommunityBlend[Index] = Result.CommunityBlend[Index - 1];
        }
        Result.CommunityBlend[InsertIndex] = Candidate;
    };

    auto PatchFactor = [&Climate](const int32 Slot, const float Strength)
    {
        const float Patch = Climate.CommunityPatch[FMath::Abs(Slot) % 4];
        return FMath::Lerp(1.0f, FMath::Lerp(0.72f, 1.28f, Patch), FMath::Clamp(Strength, 0.0f, 1.0f));
    };

    const float LowlandBand = 1.0f - SmoothStep(0.24f, 0.40f, Result.ElevationNormalized);
    const float FoothillBand = SmoothStep(0.12f, 0.28f, Result.ElevationNormalized) *
        (1.0f - SmoothStep(0.44f, 0.58f, Result.ElevationNormalized));
    const float MontaneBand = SmoothStep(0.30f, 0.46f, Result.ElevationNormalized) *
        (1.0f - SmoothStep(0.68f, 0.80f, Result.ElevationNormalized));
    const float SubalpineBand = SmoothStep(0.58f, 0.70f, Result.ElevationNormalized) *
        (1.0f - SmoothStep(0.88f, 0.98f, Result.ElevationNormalized));
    const float AlpineBand = Result.AlpineInfluence * (1.0f - Result.NivalInfluence);

    if (Settings.Definitions.Num() == 0)
    {
        auto AddBuiltIn = [&](const FName Name, const ECubusBiomeKind Archetype, const int32 MaterialId,
                    const float Suitability, const int32 PatchSlot, const float PatchStrength)
        {
  FCubusBiomeCommunityBlend Candidate;
  Candidate.Name = Name;
  Candidate.Archetype = Archetype;
  Candidate.SurfaceMaterialId = FMath::Max(1, MaterialId);
  Candidate.Suitability = FMath::Clamp(Suitability, 0.0f, 1.0f);
  InsertCommunity(Candidate, Candidate.Suitability * PatchFactor(PatchSlot, PatchStrength));
        };

        AddBuiltIn(
  TEXT("RiparianForest"), ECubusBiomeKind::Forest, Settings.ForestSurfaceMaterialId,
  ForestClimate * Result.RiverInfluence * Result.TreeLineWeight * GentleTerrain *
      RangeSuitability(Result.SoilSaturation, 0.20f, 0.82f, 0.12f) *
      FMath::Lerp(0.45f, 1.0f, Result.SoilDepth) *
      FMath::Lerp(0.72f, 1.08f, Result.CanopyPotential) *
      (1.0f - Result.Disturbance * 0.32f),
  0, 0.20f
        );
        AddBuiltIn(
  TEXT("TemperateForest"), ECubusBiomeKind::Forest, Settings.ForestSurfaceMaterialId,
  ForestClimate * Result.TreeLineWeight * GentleTerrain *
      FMath::Max(LowlandBand, FoothillBand * 0.88f) *
      (1.0f - Result.RiverInfluence * 0.55f) *
      (1.0f - Result.WindExposure * 0.48f) *
      FMath::Lerp(0.48f, 1.12f, Result.CanopyPotential),
  1, 0.34f
        );
        AddBuiltIn(
  TEXT("MontaneForest"), ECubusBiomeKind::Forest, Settings.ForestSurfaceMaterialId,
  ForestClimate * MontaneBand * Result.TreeLineWeight * GentleTerrain *
      RangeSuitability(Result.SoilMoisture, 0.28f, 0.88f, 0.12f) *
      (1.0f - Result.RockExposure * 0.72f) *
      FMath::Lerp(0.52f, 1.10f, Result.CanopyPotential),
  2, 0.32f
        );
        AddBuiltIn(
  TEXT("SubalpineWoodland"), ECubusBiomeKind::Forest, Settings.ForestSurfaceMaterialId,
  SubalpineBand * Result.TreeLineWeight * GentleTerrain *
      RangeSuitability(Result.SoilMoisture, 0.20f, 0.82f, 0.12f) *
      FMath::Lerp(0.35f, 1.0f, Result.SoilDepth) *
      (1.0f - Result.RockExposure * 0.62f),
  3, 0.28f
        );
        AddBuiltIn(
  TEXT("OpenWoodland"), ECubusBiomeKind::Forest, Settings.ForestSurfaceMaterialId,
  Result.TreeLineWeight * GentleTerrain * FMath::Max(LowlandBand, FoothillBand) *
      RangeSuitability(Result.SoilMoisture, 0.16f, 0.58f, 0.12f) *
      RangeSuitability(Result.SoilDepth, 0.20f, 0.72f, 0.12f) *
      FMath::Lerp(0.72f, 1.0f, Result.SolarExposure) *
      (1.0f - Result.WetlandWeight),
  0, 0.38f
        );
        AddBuiltIn(
  TEXT("CoolValleyForest"), ECubusBiomeKind::Forest, Settings.ForestSurfaceMaterialId,
  ForestClimate * Result.ValleyFloorInfluence * Result.TreeLineWeight * GentleTerrain *
      FMath::Lerp(0.46f, 1.0f, Result.ColdAirPooling) *
      FMath::Lerp(0.52f, 1.0f, Result.SoilDepth) *
      FMath::Lerp(0.58f, 1.05f, Result.CanopyPotential),
  1, 0.24f
        );
        AddBuiltIn(
  TEXT("FloodplainMeadow"), ECubusBiomeKind::Plains, Settings.PlainsSurfaceMaterialId,
  Result.FloodplainInfluence * GentleTerrain *
      RangeSuitability(Result.SoilSaturation, 0.24f, 0.78f, 0.12f) *
      FMath::Lerp(0.42f, 1.0f, Result.Fertility) * Result.CanopyOpenness,
  2, 0.22f
        );
        AddBuiltIn(
  TEXT("AvalancheMeadow"), ECubusBiomeKind::Plains, Settings.PlainsSurfaceMaterialId,
  SubalpineBand * Result.Disturbance * Result.CanopyOpenness *
      RangeSuitability(Slope, Settings.RockySlopeThreshold * 0.18f, Settings.RockySlopeThreshold * 0.78f, 0.14f) *
      FMath::Lerp(0.35f, 1.0f, Result.SoilDepth),
  3, 0.18f
        );
        AddBuiltIn(
  TEXT("WindPrunedSubalpineScrub"), ECubusBiomeKind::Rocky, Settings.RockySurfaceMaterialId,
  FMath::Max(SubalpineBand, AlpineBand * 0.62f) * Result.WindExposure * Result.CanopyOpenness *
      RangeSuitability(Result.SoilDepth, 0.08f, 0.62f, 0.12f) * (1.0f - Result.NivalInfluence),
  0, 0.20f
        );
        AddBuiltIn(
  TEXT("WindwardMontaneForest"), ECubusBiomeKind::Forest, Settings.ForestSurfaceMaterialId,
  ForestClimate * FMath::Max(FoothillBand * 0.72f, MontaneBand) * Result.TreeLineWeight *
      Result.OrographicLift * RangeSuitability(Result.SoilMoisture, 0.34f, 0.94f, 0.12f) *
      FMath::Lerp(0.52f, 1.10f, Result.SoilDepth) *
      FMath::Lerp(0.62f, 1.08f, Result.CanopyPotential) *
      (1.0f - Result.RainShadow * 0.72f),
  0, 0.30f
        );
        AddBuiltIn(
  TEXT("ShadedRavineForest"), ECubusBiomeKind::Forest, Settings.ForestSurfaceMaterialId,
  ForestClimate * FMath::Max(Result.ValleyFloorInfluence, Result.LowerSlopeInfluence * 0.82f) *
      Result.TreeLineWeight * FMath::Lerp(0.32f, 1.0f, Result.SolarOcclusion) *
      FMath::Lerp(0.52f, 1.06f, Result.SoilDepth) *
      FMath::Lerp(0.50f, 1.08f, Result.WaterHoldingCapacity) *
      (1.0f - Result.Disturbance * 0.42f),
  1, 0.24f
        );
        AddBuiltIn(
  TEXT("LeewardSteppe"), ECubusBiomeKind::Plains, Settings.PlainsSurfaceMaterialId,
  FMath::Max(LowlandBand, FoothillBand) * GentleTerrain * Result.RainShadow * Result.CanopyOpenness *
      RangeSuitability(Result.SoilMoisture, 0.06f, 0.46f, 0.12f) *
      FMath::Lerp(0.68f, 1.08f, Result.SolarExposure) *
      FMath::Lerp(0.72f, 1.08f, Result.SoilCoarseness) *
      (1.0f - Result.WetlandWeight),
  2, 0.34f
        );
        AddBuiltIn(
  TEXT("DeepColluvialForest"), ECubusBiomeKind::Forest, Settings.ForestSurfaceMaterialId,
  ForestClimate * Result.LowerSlopeInfluence * Result.TreeLineWeight * GentleTerrain *
      FMath::Lerp(0.42f, 1.0f, Result.SoilDepth) *
      FMath::Lerp(0.50f, 1.08f, Result.WaterHoldingCapacity) *
      FMath::Lerp(0.58f, 1.04f, Result.WindShelter) *
      (1.0f - Result.Disturbance * 0.46f),
  1, 0.26f
        );
        AddBuiltIn(
  TEXT("FracturedRockHeath"), ECubusBiomeKind::Rocky, Settings.RockySurfaceMaterialId,
  FMath::Max(FoothillBand, FMath::Max(MontaneBand, SubalpineBand * 0.72f)) *
      Result.FractureDensity * Result.CanopyOpenness *
      RangeSuitability(Result.SoilCoarseness, 0.34f, 0.90f, 0.12f) *
      RangeSuitability(Result.SoilMoisture, 0.12f, 0.66f, 0.12f) *
      (1.0f - Result.NivalInfluence),
  2, 0.24f
        );
        AddBuiltIn(
  TEXT("DryRockWoodland"), ECubusBiomeKind::Forest, Settings.ForestSurfaceMaterialId,
  FMath::Max(LowlandBand, FoothillBand) * Result.TreeLineWeight * GentleTerrain *
      RangeSuitability(Result.SoilMoisture, 0.10f, 0.52f, 0.12f) *
      RangeSuitability(Result.SoilCoarseness, 0.28f, 0.82f, 0.12f) *
      FMath::Lerp(0.70f, 1.04f, Result.SolarExposure) *
      FMath::Lerp(0.34f, 0.88f, Result.CanopyOpenness) *
      (1.0f - Result.WetlandWeight),
  3, 0.30f
        );
        AddBuiltIn(
  TEXT("Meadow"), ECubusBiomeKind::Plains, Settings.PlainsSurfaceMaterialId,
  GentleTerrain * FMath::Max(LowlandBand, FMath::Max(FoothillBand, MontaneBand * 0.72f)) *
      FMath::Lerp(0.30f, 1.0f, Result.Fertility) *
      FMath::Lerp(0.32f, 1.0f, Result.SoilDepth) *
      (1.0f - Result.SoilSaturation * 0.66f) *
      (1.0f - Result.ForestWeight * 0.58f),
  1, 0.30f
        );
        AddBuiltIn(
  TEXT("DryGrassland"), ECubusBiomeKind::Plains, Settings.PlainsSurfaceMaterialId,
  GentleTerrain * FMath::Max(LowlandBand, FoothillBand) *
      RangeSuitability(Result.SoilMoisture, 0.08f, 0.48f, 0.12f) *
      FMath::Lerp(0.70f, 1.0f, Result.SolarExposure) *
      (1.0f - Result.RiverInfluence * 0.72f),
  2, 0.34f
        );
        AddBuiltIn(
  TEXT("WetMeadow"), ECubusBiomeKind::Wetland, Settings.WetlandSurfaceMaterialId,
  GentleTerrain * Result.SoilSaturation * FMath::Lerp(0.45f, 1.0f, Result.GroundwaterPotential) *
      FMath::Lerp(0.35f, 1.0f, Result.SoilDepth) *
      (1.0f - Result.RiverInfluence * 0.58f) *
      (1.0f - Result.NivalInfluence),
  3, 0.22f
        );
        AddBuiltIn(
  TEXT("Marsh"), ECubusBiomeKind::Wetland, Settings.WetlandSurfaceMaterialId,
  GentleTerrain * SmoothStep(0.60f, 0.86f, Result.SoilSaturation) *
      SmoothStep(0.30f, 0.70f, Result.GroundwaterPotential) *
      (1.0f - Result.NivalInfluence),
  0, 0.20f
        );
        AddBuiltIn(
  TEXT("RiparianWetland"), ECubusBiomeKind::Wetland, Settings.WetlandSurfaceMaterialId,
  GentleTerrain * Result.RiverInfluence * SmoothStep(0.42f, 0.76f, Result.SoilSaturation) *
      (1.0f - Result.NivalInfluence),
  1, 0.18f
        );
        AddBuiltIn(
  TEXT("AlpineMeadow"), ECubusBiomeKind::Plains, Settings.PlainsSurfaceMaterialId,
  AlpineBand * GentleTerrain * FMath::Lerp(0.30f, 1.0f, Result.SoilDepth) *
      RangeSuitability(Result.SoilMoisture, 0.18f, 0.78f, 0.12f) *
      (1.0f - Result.RockExposure * 0.76f),
  2, 0.24f
        );
        AddBuiltIn(
  TEXT("AlpineHeath"), ECubusBiomeKind::Rocky, Settings.RockySurfaceMaterialId,
  AlpineBand * RangeSuitability(Result.SoilDepth, 0.08f, 0.62f, 0.12f) *
      RangeSuitability(Result.SoilMoisture, 0.10f, 0.64f, 0.12f) *
      FMath::Lerp(0.55f, 1.0f, Result.Exposure),
  3, 0.24f
        );
        AddBuiltIn(
  TEXT("Scree"), ECubusBiomeKind::Rocky, Settings.RockySurfaceMaterialId,
  FMath::Max(AlpineBand, SubalpineBand * 0.55f) * Result.Erosion *
      FMath::Max(Result.RockExposure, SteepCountry) * (1.0f - Result.WetlandWeight),
  0, 0.18f
        );
        AddBuiltIn(
  TEXT("ExposedRock"), ECubusBiomeKind::Rocky, Settings.RockySurfaceMaterialId,
  FMath::Max(Result.RockExposure, SteepCountry) * FMath::Lerp(0.52f, 1.0f, Result.Exposure) *
      (1.0f - Result.WetlandWeight),
  1, 0.14f
        );
        AddBuiltIn(
  TEXT("NivalRock"), ECubusBiomeKind::Rocky, Settings.RockySurfaceMaterialId,
  Result.NivalInfluence * FMath::Lerp(0.58f, 1.0f, FMath::Max(Result.RockExposure, Result.Erosion)),
  2, 0.10f
        );
    }
    else
    {
        for (int32 DefinitionIndex = 0; DefinitionIndex < Settings.Definitions.Num(); ++DefinitionIndex)
        {
  const FCubusBiomeDefinition& Definition = Settings.Definitions[DefinitionIndex];
  const float MoistureTolerance = FMath::Clamp(Definition.MoistureTolerance, 0.01f, 1.0f);
  const float TemperatureTolerance = FMath::Clamp(Definition.TemperatureTolerance, 0.01f, 1.0f);
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
  const float MinimumHeight = FMath::Min(Definition.MinimumWorldZ, Definition.MaximumWorldZ);
  const float MaximumHeight = FMath::Max(Definition.MinimumWorldZ, Definition.MaximumWorldZ);
  const float HeightSpan = FMath::Max(8.0f, MaximumHeight - MinimumHeight);
  const float HeightSoftness = FMath::Clamp(HeightSpan * 0.08f, 6.0f, 48.0f);
  const float HeightSuitability = SmoothStep(
      MinimumHeight - HeightSoftness,
      MinimumHeight,
      SurfaceWorldZ
  ) * (1.0f - SmoothStep(MaximumHeight, MaximumHeight + HeightSoftness, SurfaceWorldZ));
  const float MaximumSlope = FMath::Max(0.01f, Definition.MaximumSlope);
  const float SlopeSuitability = 1.0f - SmoothStep(MaximumSlope * 0.78f, MaximumSlope, Slope);
  const float Softness = FMath::Clamp(Definition.TransitionSoftness, 0.01f, 0.5f);

  const float DefinitionSuitability =
      MoistureSuitability * TemperatureSuitability * HeightSuitability * SlopeSuitability *
      RangeSuitability(Result.ElevationNormalized, Definition.MinimumElevationNormalized, Definition.MaximumElevationNormalized, Softness) *
      RangeSuitability(Result.SoilDepth, Definition.MinimumSoilDepth, Definition.MaximumSoilDepth, Softness) *
      RangeSuitability(Result.Drainage, Definition.MinimumDrainage, Definition.MaximumDrainage, Softness) *
      RangeSuitability(Result.RiverInfluence, Definition.MinimumRiverInfluence, Definition.MaximumRiverInfluence, Softness) *
      RangeSuitability(Result.SoilSaturation, Definition.MinimumSoilSaturation, Definition.MaximumSoilSaturation, Softness) *
      RangeSuitability(Result.GroundwaterPotential, Definition.MinimumGroundwaterPotential, Definition.MaximumGroundwaterPotential, Softness) *
      RangeSuitability(Result.RockExposure, Definition.MinimumRockExposure, Definition.MaximumRockExposure, Softness) *
      RangeSuitability(Result.Exposure, Definition.MinimumExposure, Definition.MaximumExposure, Softness) *
      RangeSuitability(Result.SolarExposure, Definition.MinimumSolarExposure, Definition.MaximumSolarExposure, Softness) *
                RangeSuitability(Result.OrographicLift, Definition.MinimumOrographicLift, Definition.MaximumOrographicLift, Softness) *
                RangeSuitability(Result.RainShadow, Definition.MinimumRainShadow, Definition.MaximumRainShadow, Softness) *
                RangeSuitability(Result.SolarOcclusion, Definition.MinimumSolarOcclusion, Definition.MaximumSolarOcclusion, Softness) *
      RangeSuitability(Result.Fertility, Definition.MinimumFertility, Definition.MaximumFertility, Softness) *
                    RangeSuitability(Result.OrganicMatter, Definition.MinimumOrganicMatter, Definition.MaximumOrganicMatter, Softness) *
                RangeSuitability(Result.SubstrateHardness, Definition.MinimumSubstrateHardness, Definition.MaximumSubstrateHardness, Softness) *
                RangeSuitability(Result.FractureDensity, Definition.MinimumFractureDensity, Definition.MaximumFractureDensity, Softness) *
                RangeSuitability(Result.SoilCoarseness, Definition.MinimumSoilCoarseness, Definition.MaximumSoilCoarseness, Softness) *
                RangeSuitability(Result.WaterHoldingCapacity, Definition.MinimumWaterHoldingCapacity, Definition.MaximumWaterHoldingCapacity, Softness) *
                RangeSuitability(Result.Erosion, Definition.MinimumErosion, Definition.MaximumErosion, Softness) *
                RangeSuitability(Result.ValleyFloorInfluence, Definition.MinimumValleyFloorInfluence, Definition.MaximumValleyFloorInfluence, Softness) *
                RangeSuitability(Result.ColdAirPooling, Definition.MinimumColdAirPooling, Definition.MaximumColdAirPooling, Softness) *
                RangeSuitability(Result.Disturbance, Definition.MinimumDisturbance, Definition.MaximumDisturbance, Softness) *
                RangeSuitability(Result.CanopyPotential, Definition.MinimumCanopyPotential, Definition.MaximumCanopyPotential, Softness);

  FCubusBiomeCommunityBlend Candidate;
  Candidate.DefinitionIndex = DefinitionIndex;
  Candidate.Name = Definition.Name.IsNone()
      ? FName(*FString::Printf(TEXT("Biome_%d"), DefinitionIndex))
      : Definition.Name;
  Candidate.Archetype = Definition.Archetype;
  Candidate.SurfaceMaterialId = FMath::Max(1, Definition.SurfaceMaterialId);
  Candidate.Suitability = FMath::Clamp(DefinitionSuitability, 0.0f, 1.0f);
  const float DefinitionScore = Candidate.Suitability * FMath::Max(0.01f, Definition.Priority) *
      PatchFactor(DefinitionIndex, Definition.PatchStrength);
  InsertCommunity(Candidate, DefinitionScore);
        }
    }

    if (Result.CommunityBlendCount == 0)
    {
        FCubusBiomeCommunityBlend Fallback;
        Fallback.Name = TEXT("OpenGrassland");
        Fallback.Archetype = ECubusBiomeKind::Plains;
        Fallback.SurfaceMaterialId = Settings.PlainsSurfaceMaterialId;
        Fallback.Suitability = 1.0f;
        InsertCommunity(Fallback, 1.0f);
    }

    float CommunityWeightTotal = 0.0f;
    for (int32 Index = 0; Index < Result.CommunityBlendCount; ++Index)
    {
        CommunityWeightTotal += Result.CommunityBlend[Index].Weight;
    }
    if (CommunityWeightTotal > KINDA_SMALL_NUMBER)
    {
        for (int32 Index = 0; Index < Result.CommunityBlendCount; ++Index)
        {
  Result.CommunityBlend[Index].Weight /= CommunityWeightTotal;
        }
    }

    Result.PlainsWeight = 0.0f;
    Result.ForestWeight = 0.0f;
    Result.RockyWeight = 0.0f;
    Result.WetlandWeight = 0.0f;
    for (int32 Index = 0; Index < Result.CommunityBlendCount; ++Index)
    {
        const FCubusBiomeCommunityBlend& Community = Result.CommunityBlend[Index];
        switch (Community.Archetype)
        {
            case ECubusBiomeKind::Forest: Result.ForestWeight += Community.Weight; break;
            case ECubusBiomeKind::Rocky: Result.RockyWeight += Community.Weight; break;
            case ECubusBiomeKind::Wetland: Result.WetlandWeight += Community.Weight; break;
            case ECubusBiomeKind::Plains:
            default: Result.PlainsWeight += Community.Weight; break;
        }
    }

    const FCubusBiomeCommunityBlend& DominantCommunity = Result.CommunityBlend[0];
    Result.BiomeDefinitionIndex = DominantCommunity.DefinitionIndex;
    Result.DominantBiome = DominantCommunity.Archetype;
    Result.BiomeName = DominantCommunity.Name;
    Result.BiomeStrength = DominantCommunity.Suitability;
    Result.SurfaceMaterialId = DominantCommunity.SurfaceMaterialId;
    return Result;
}

float FCubusBiomeField::SampleRiverDistance(
    const float WorldX,
    const float WorldY,
    const FCubusBiomeFieldSettings& Settings
)
{
    if (!Settings.bGenerateRivers || !Settings.HydrologySettings.bEnabled)
    {
        return 1.0f;
    }
    const FCubusHydrologySample Hydrology = FCubusHydrologyField::Sample(
        WorldX,
        WorldY,
        Settings.HydrologySettings
    );
    return FCubusHydrologyField::NormalizeRiverDistance(Hydrology, Settings.HydrologySettings);
}

float FCubusBiomeField::SampleNoise(
    const float WorldX,
    const float WorldY,
    const float Frequency
)
{
    return FMath::PerlinNoise2D(
        FVector2D(WorldX * FMath::Max(0.000001f, Frequency), WorldY * FMath::Max(0.000001f, Frequency))
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
    if (EdgeMaximum <= EdgeMinimum)
    {
        return Value >= EdgeMaximum ? 1.0f : 0.0f;
    }
    const float T = FMath::Clamp(
        (Value - EdgeMinimum) / (EdgeMaximum - EdgeMinimum),
        0.0f,
        1.0f
    );
    return T * T * (3.0f - 2.0f * T);
}

float FCubusBiomeField::RangeSuitability(
    const float Value,
    const float Minimum,
    const float Maximum,
    const float Softness
)
{
    const float MinValue = FMath::Min(Minimum, Maximum);
    const float MaxValue = FMath::Max(Minimum, Maximum);
    const float SafeSoftness = FMath::Clamp(Softness, 0.01f, 0.5f);
    const float Enter = Value >= MinValue
        ? 1.0f
        : SmoothStep(MinValue - SafeSoftness, MinValue, Value);
    const float Exit = Value <= MaxValue
        ? 1.0f
        : 1.0f - SmoothStep(MaxValue, MaxValue + SafeSoftness, Value);
    return FMath::Clamp(Enter * Exit, 0.0f, 1.0f);
}
