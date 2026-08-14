#include "CubusCore/Generation/CubusBiomeField.h"

#include "CubusCore/Data/CubusGeologyProfile.h"
#include "CubusCore/Generation/CubusGenerationSeeds.h"

FCubusBiomeFieldSettings FCubusBiomeField::MakeSettings(const UCubusGeologyProfile* GeologyProfile, const int32 BiomeSeed,
														const int32 RiverSeed)
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
	Settings.ForestThreshold = FMath::IsNearlyEqual(GeologyProfile->ForestThreshold, 0.15f, 0.0001f) ? 0.0f : GeologyProfile->ForestThreshold;
	Settings.WetlandRiverDistance = GeologyProfile->WetlandRiverDistance;
	Settings.RockySlopeThreshold = FMath::IsNearlyEqual(GeologyProfile->RockySlopeThreshold, 4.0f, 0.001f) ? 1.15f : GeologyProfile->RockySlopeThreshold;
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
	Settings.HydrologySettings.ValleyHalfWidth = FMath::Max(Settings.HydrologySettings.ChannelHalfWidth + 4.0f, GeologyProfile->RiverValleyWidth * 128.0f);
	Settings.Definitions = GeologyProfile->BiomeDefinitions;
	return Settings;
}

FCubusBiomeFieldSettings FCubusBiomeField::BindHydrology(const FCubusBiomeFieldSettings& BaseSettings, const FCubusHydrologySettings& HydrologySettings)
{
	FCubusBiomeFieldSettings Result = BaseSettings;
	Result.HydrologySettings = HydrologySettings;
	Result.bGenerateRivers = BaseSettings.bGenerateRivers && HydrologySettings.bEnabled;
	return Result;
}

FCubusBiomeSample FCubusBiomeField::Sample(const float WorldX, const float WorldY, const float SurfaceWorldZ, const float Slope,
	const FCubusBiomeFieldSettings& InSettings, const FCubusBiomeTerrainContext& TerrainContext)
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

	const float BiomeX = WorldX + static_cast<float>(Settings.BiomeOffsetX);
	const float BiomeY = WorldY + static_cast<float>(Settings.BiomeOffsetY);
	const float ClimateFrequency = Settings.Frequency * 0.28f;
	const float WarpFrequency = ClimateFrequency * 0.38f;
	const float WarpAmplitude = FMath::Clamp(0.12f / ClimateFrequency, 64.0f, 320.0f);
	const float WarpedX = BiomeX + SampleFbm(BiomeX + 3527.0f, BiomeY - 1871.0f, WarpFrequency, 3, 0.5f) * WarpAmplitude;
	const float WarpedY = BiomeY + SampleFbm(BiomeX - 6173.0f, BiomeY + 2593.0f, WarpFrequency, 3, 0.5f) * WarpAmplitude;
	const float MacroMoisture = SampleFbm(WarpedX + 4219.0f, WarpedY - 1877.0f, ClimateFrequency * 0.72f, 4, 0.52f);
	const float MacroTemperature = SampleFbm(WarpedX - 8111.0f, WarpedY + 3203.0f, ClimateFrequency * 0.50f, 3, 0.5f);
	const float LocalHumidity = SampleFbm(WarpedX + 1013.0f, WarpedY + 6427.0f, Settings.Frequency * 1.15f, 2, 0.45f);

	Result.RiverDistance = Settings.bGenerateRivers ? SampleRiverDistance(WorldX, WorldY, Settings) : 1.0f;
	Result.RiverInfluence = Settings.bGenerateRivers
		? 1.0f - SmoothStep(Settings.WetlandRiverDistance, FMath::Max(Settings.WetlandRiverDistance + 0.001f, Settings.WetlandRiverDistance * 2.8f), Result.RiverDistance)
		: 0.0f;
	Result.Drainage = FMath::Clamp(TerrainContext.Drainage, 0.0f, 1.0f);
	Result.RockExposure = FMath::Clamp(TerrainContext.RockExposure, 0.0f, 1.0f);

	const float GradientLength = TerrainContext.Gradient.Size();
	const FVector2D UphillDirection = GradientLength > KINDA_SMALL_NUMBER ? TerrainContext.Gradient / GradientLength : FVector2D::ZeroVector;
	const FVector2D PrevailingWind = FVector2D(0.82f, 0.57f).GetSafeNormal();
	const float Windwardness = FMath::Clamp(FVector2D::DotProduct(UphillDirection, PrevailingWind) * 0.5f + 0.5f, 0.0f, 1.0f);
	const float TopographicExposure = FMath::Clamp(
		TerrainContext.Ridge * 0.42f + TerrainContext.MountainCore * 0.28f +
		SmoothStep(Settings.RockySlopeThreshold * 0.30f, Settings.RockySlopeThreshold, Slope) * 0.30f, 0.0f, 1.0f);
	Result.Exposure = FMath::Clamp(TopographicExposure * FMath::Lerp(0.72f, 1.0f, Windwardness), 0.0f, 1.0f);

	const float GentleGround = 1.0f - SmoothStep(Settings.RockySlopeThreshold * 0.35f, Settings.RockySlopeThreshold * 1.05f, Slope);
	const float DepositionalGround = FMath::Clamp(Result.Drainage * 0.58f + Result.RiverInfluence * 0.42f, 0.0f, 1.0f);
	const float SoilRetention = FMath::Clamp(GentleGround * (1.0f - Result.RockExposure) * (1.0f - Result.Exposure * 0.55f), 0.0f, 1.0f);
	Result.SoilDepth = FMath::Clamp(SoilRetention * FMath::Lerp(0.55f, 1.0f, DepositionalGround), 0.0f, 1.0f);

	const float OrographicMoisture = Windwardness * TerrainContext.FoothillWeight * 0.13f;
	const float DrainageMoisture = Result.Drainage * 0.16f + Result.RiverInfluence * 0.26f;
	const float ExposureDrying = Result.Exposure * 0.13f + Result.RockExposure * 0.08f;
	Result.Moisture = FMath::Clamp(0.50f + MacroMoisture * 0.34f + LocalHumidity * 0.07f + OrographicMoisture + DrainageMoisture - ExposureDrying, 0.0f, 1.0f);

	const float ElevationCooling = FMath::Clamp((SurfaceWorldZ - Settings.RockyMinimumWorldZ) / 120.0f, -0.18f, 0.52f);
	Result.Temperature = FMath::Clamp(0.52f + MacroTemperature * 0.34f - ElevationCooling - Result.Exposure * 0.07f + Result.Drainage * 0.035f, 0.0f, 1.0f);

	const float ClimateProductivity = SmoothStep(0.12f, 0.48f, Result.Moisture) * (1.0f - SmoothStep(0.88f, 1.0f, Result.Moisture)) *
		SmoothStep(0.10f, 0.42f, Result.Temperature) * (1.0f - SmoothStep(0.86f, 1.0f, Result.Temperature));
	Result.Fertility = FMath::Clamp(Result.SoilDepth * 0.58f + ClimateProductivity * 0.30f + DepositionalGround * 0.12f, 0.0f, 1.0f);

	const float GentleTerrain = 1.0f - SmoothStep(Settings.RockySlopeThreshold * 0.42f, Settings.RockySlopeThreshold, Slope);
	const float HighCountry = SmoothStep(Settings.RockyMinimumWorldZ - 10.0f, Settings.RockyMinimumWorldZ + 18.0f, SurfaceWorldZ);
	const float SteepCountry = SmoothStep(Settings.RockySlopeThreshold * 0.62f, Settings.RockySlopeThreshold, Slope);
	const float SaturatedGround = SmoothStep(0.58f, 0.82f, Result.Moisture) * SmoothStep(0.30f, 0.68f, Result.SoilDepth);
	const float ForestClimate = SmoothStep(0.30f, 0.68f, Result.Fertility) * SmoothStep(0.26f, 0.58f, Result.Moisture);

	Result.WetlandWeight = FMath::Clamp(FMath::Max(Result.RiverInfluence * SaturatedGround, Result.Drainage * SaturatedGround * 0.72f) * GentleTerrain * (1.0f - HighCountry), 0.0f, 1.0f);
	Result.RockyWeight = FMath::Clamp(FMath::Max(FMath::Max(HighCountry * Result.Exposure, SteepCountry), Result.RockExposure) * (1.0f - Result.WetlandWeight), 0.0f, 1.0f);
	Result.ForestWeight = FMath::Clamp(ForestClimate * GentleTerrain * (1.0f - Result.Exposure * 0.58f) * (1.0f - Result.WetlandWeight) * (1.0f - Result.RockyWeight), 0.0f, 1.0f);
	Result.PlainsWeight = FMath::Max(0.0f, 1.0f - Result.WetlandWeight - Result.RockyWeight - Result.ForestWeight);

	const float TotalWeight = Result.PlainsWeight + Result.ForestWeight + Result.RockyWeight + Result.WetlandWeight;
	if (TotalWeight > KINDA_SMALL_NUMBER)
	{
		Result.PlainsWeight /= TotalWeight;
		Result.ForestWeight /= TotalWeight;
		Result.RockyWeight /= TotalWeight;
		Result.WetlandWeight /= TotalWeight;
	}

	float DominantWeight = Result.PlainsWeight;
	Result.DominantBiome = ECubusBiomeKind::Plains;
	Result.BiomeName = TEXT("Plains");
	Result.BiomeStrength = Result.PlainsWeight;
	Result.SurfaceMaterialId = Settings.PlainsSurfaceMaterialId;
	if (Result.ForestWeight > DominantWeight)
	{
		DominantWeight = Result.ForestWeight;
		Result.DominantBiome = ECubusBiomeKind::Forest;
		Result.BiomeName = TEXT("Forest");
		Result.BiomeStrength = Result.ForestWeight;
		Result.SurfaceMaterialId = Settings.ForestSurfaceMaterialId;
	}
	if (Result.RockyWeight > DominantWeight)
	{
		DominantWeight = Result.RockyWeight;
		Result.DominantBiome = ECubusBiomeKind::Rocky;
		Result.BiomeName = TEXT("Rocky");
		Result.BiomeStrength = Result.RockyWeight;
		Result.SurfaceMaterialId = Settings.RockySurfaceMaterialId;
	}
	if (Result.WetlandWeight > DominantWeight)
	{
		Result.DominantBiome = ECubusBiomeKind::Wetland;
		Result.BiomeName = TEXT("Wetland");
		Result.BiomeStrength = Result.WetlandWeight;
		Result.SurfaceMaterialId = Settings.WetlandSurfaceMaterialId;
	}

	float BestDefinitionScore = 0.0f;
	for (int32 DefinitionIndex = 0; DefinitionIndex < Settings.Definitions.Num(); ++DefinitionIndex)
	{
		const FCubusBiomeDefinition& Definition = Settings.Definitions[DefinitionIndex];
		const float MoistureTolerance = FMath::Clamp(Definition.MoistureTolerance, 0.01f, 1.0f);
		const float TemperatureTolerance = FMath::Clamp(Definition.TemperatureTolerance, 0.01f, 1.0f);
		const float MoistureSuitability = 1.0f - SmoothStep(MoistureTolerance * 0.62f, MoistureTolerance, FMath::Abs(Result.Moisture - Definition.TargetMoisture));
		const float TemperatureSuitability = 1.0f - SmoothStep(TemperatureTolerance * 0.62f, TemperatureTolerance, FMath::Abs(Result.Temperature - Definition.TargetTemperature));
		const float MinimumHeight = FMath::Min(Definition.MinimumWorldZ, Definition.MaximumWorldZ);
		const float MaximumHeight = FMath::Max(Definition.MinimumWorldZ, Definition.MaximumWorldZ);
		const float HeightSpan = FMath::Max(8.0f, MaximumHeight - MinimumHeight);
		const float HeightSoftness = FMath::Clamp(HeightSpan * 0.08f, 6.0f, 48.0f);
		const float HeightSuitability = SmoothStep(MinimumHeight - HeightSoftness, MinimumHeight, SurfaceWorldZ) * (1.0f - SmoothStep(MaximumHeight, MaximumHeight + HeightSoftness, SurfaceWorldZ));
		const float MaximumSlope = FMath::Max(0.01f, Definition.MaximumSlope);
		const float SlopeSuitability = 1.0f - SmoothStep(MaximumSlope * 0.78f, MaximumSlope, Slope);
		const float Softness = FMath::Clamp(Definition.TransitionSoftness, 0.01f, 0.5f);
		const float DefinitionSuitability = MoistureSuitability * TemperatureSuitability * HeightSuitability * SlopeSuitability *
			RangeSuitability(Result.SoilDepth, Definition.MinimumSoilDepth, Definition.MaximumSoilDepth, Softness) *
			RangeSuitability(Result.Drainage, Definition.MinimumDrainage, Definition.MaximumDrainage, Softness) *
			RangeSuitability(Result.RiverInfluence, Definition.MinimumRiverInfluence, Definition.MaximumRiverInfluence, Softness) *
			RangeSuitability(Result.RockExposure, Definition.MinimumRockExposure, Definition.MaximumRockExposure, Softness) *
			RangeSuitability(Result.Exposure, Definition.MinimumExposure, Definition.MaximumExposure, Softness) *
			RangeSuitability(Result.Fertility, Definition.MinimumFertility, Definition.MaximumFertility, Softness);
		const float DefinitionScore = DefinitionSuitability * FMath::Max(0.01f, Definition.Priority);
		if (DefinitionScore > BestDefinitionScore)
		{
			BestDefinitionScore = DefinitionScore;
			Result.BiomeDefinitionIndex = DefinitionIndex;
			Result.DominantBiome = Definition.Archetype;
			Result.BiomeName = Definition.Name.IsNone() ? FName(*FString::Printf(TEXT("Biome_%d"), DefinitionIndex)) : Definition.Name;
			Result.BiomeStrength = FMath::Clamp(DefinitionSuitability, 0.0f, 1.0f);
			Result.SurfaceMaterialId = FMath::Max(1, Definition.SurfaceMaterialId);
		}
	}
	return Result;
}

float FCubusBiomeField::SampleRiverDistance(const float WorldX, const float WorldY, const FCubusBiomeFieldSettings& Settings)
{
	if (!Settings.bGenerateRivers || !Settings.HydrologySettings.bEnabled)
	{
		return 1.0f;
	}
	const FCubusHydrologySample Hydrology = FCubusHydrologyField::Sample(WorldX, WorldY, Settings.HydrologySettings);
	return FCubusHydrologyField::NormalizeRiverDistance(Hydrology, Settings.HydrologySettings);
}

float FCubusBiomeField::SampleNoise(const float WorldX, const float WorldY, const float Frequency)
{
	return FMath::PerlinNoise2D(FVector2D(WorldX * FMath::Max(0.000001f, Frequency), WorldY * FMath::Max(0.000001f, Frequency)));
}

float FCubusBiomeField::SampleFbm(const float WorldX, const float WorldY, const float Frequency, const int32 Octaves, const float Gain)
{
	float Sum = 0.0f;
	float Weight = 1.0f;
	float TotalWeight = 0.0f;
	float CurrentFrequency = FMath::Max(0.000001f, Frequency);
	for (int32 Octave = 0; Octave < FMath::Clamp(Octaves, 1, 6); ++Octave)
	{
		Sum += SampleNoise(WorldX + static_cast<float>(Octave) * 1747.0f, WorldY - static_cast<float>(Octave) * 2081.0f, CurrentFrequency) * Weight;
		TotalWeight += Weight;
		CurrentFrequency *= 2.03f;
		Weight *= FMath::Clamp(Gain, 0.01f, 0.99f);
	}
	return TotalWeight > KINDA_SMALL_NUMBER ? Sum / TotalWeight : 0.0f;
}

float FCubusBiomeField::SmoothStep(const float EdgeMinimum, const float EdgeMaximum, const float Value)
{
	if (EdgeMaximum <= EdgeMinimum)
	{
		return Value >= EdgeMaximum ? 1.0f : 0.0f;
	}
	const float T = FMath::Clamp((Value - EdgeMinimum) / (EdgeMaximum - EdgeMinimum), 0.0f, 1.0f);
	return T * T * (3.0f - 2.0f * T);
}

float FCubusBiomeField::RangeSuitability(const float Value, const float Minimum, const float Maximum, const float Softness)
{
	const float MinValue = FMath::Clamp(FMath::Min(Minimum, Maximum), 0.0f, 1.0f);
	const float MaxValue = FMath::Clamp(FMath::Max(Minimum, Maximum), 0.0f, 1.0f);
	const float SafeSoftness = FMath::Clamp(Softness, 0.01f, 0.5f);
	return SmoothStep(MinValue - SafeSoftness, MinValue, Value) * (1.0f - SmoothStep(MaxValue, MaxValue + SafeSoftness, Value));
}
