#pragma once

#include "CoreMinimal.h"
#include "CubusCore/Data/CubusBiomeTypes.h"
#include "CubusCore/Generation/CubusHydrologyField.h"

class UCubusGeologyProfile;

/** Thread-safe settings copied from a geology profile for biome sampling. */
struct ORAKAI_API FCubusBiomeFieldSettings
{
	bool  bEnabled			   = false;
	float Frequency			   = 0.004f;
	float ForestThreshold	   = 0.15f;
	float WetlandRiverDistance = 0.10f;
	float RockySlopeThreshold  = 1.15f;
	float RockyMinimumWorldZ   = 48.0f;

	int32 PlainsSurfaceMaterialId  = 1;
	int32 ForestSurfaceMaterialId  = 7;
	int32 RockySurfaceMaterialId   = 3;
	int32 WetlandSurfaceMaterialId = 8;

	int32 BiomeOffsetX = 0;
	int32 BiomeOffsetY = 0;

	bool bGenerateRivers = false;

	float RiverFrequency = 0.0025f;
	float RiverWarpAmplitude = 48.0f;
	float RiverWarpFrequency = 0.006f;
	int32 RiverOffsetX = 0;
	int32 RiverOffsetY = 0;

	FCubusHydrologySettings HydrologySettings;
	TArray<FCubusBiomeDefinition> Definitions;
};

/** Terrain/environment context supplied by the authoritative density column. */
struct ORAKAI_API FCubusBiomeTerrainContext
{
	float Drainage = 0.0f;
	float RockExposure = 0.0f;
	float MountainCore = 0.0f;
	float FoothillWeight = 0.0f;
	float Ridge = 0.0f;
	FVector2D Gradient = FVector2D::ZeroVector;
};

/** Continuous environment/biome classification at one density-world column. */
struct ORAKAI_API FCubusBiomeSample
{
	ECubusBiomeKind DominantBiome		 = ECubusBiomeKind::Plains;
	FName			BiomeName			 = TEXT("Plains");
	float			BiomeStrength		 = 1.0f;
	float			PlainsWeight		 = 1.0f;
	float			ForestWeight		 = 0.0f;
	float			RockyWeight			 = 0.0f;
	float			WetlandWeight		 = 0.0f;
	float			Moisture			 = 0.5f;
	float			Temperature			 = 0.5f;
	float			RiverDistance		 = 1.0f;
	float			RiverInfluence		 = 0.0f;
	float			Drainage			 = 0.0f;
	float			SoilDepth			 = 0.5f;
	float			RockExposure		 = 0.0f;
	float			Exposure			 = 0.0f;
	float			Fertility			 = 0.5f;
	float			SurfaceWorldZ		 = 0.0f;
	float			Slope				 = 0.0f;
	int32			SurfaceMaterialId	 = 1;
	int32			BiomeDefinitionIndex = INDEX_NONE;
};

/** Deterministic continuous ecology engine for the density world. */
class ORAKAI_API FCubusBiomeField
{
public:
	static FCubusBiomeFieldSettings MakeSettings(const UCubusGeologyProfile* GeologyProfile, int32 BiomeSeed, int32 RiverSeed);

	static FCubusBiomeFieldSettings BindHydrology(
		const FCubusBiomeFieldSettings& BaseSettings,
		const FCubusHydrologySettings& HydrologySettings
	);

	static FCubusBiomeSample Sample(
		float WorldX,
		float WorldY,
		float SurfaceWorldZ,
		float Slope,
		const FCubusBiomeFieldSettings& Settings,
		const FCubusBiomeTerrainContext& TerrainContext = FCubusBiomeTerrainContext()
	);

	static float SampleRiverDistance(float WorldX, float WorldY, const FCubusBiomeFieldSettings& Settings);

private:
	static float SampleNoise(float WorldX, float WorldY, float Frequency);
	static float SampleFbm(float WorldX, float WorldY, float Frequency, int32 Octaves, float Gain);
	static float SmoothStep(float EdgeMinimum, float EdgeMaximum, float Value);
	static float RangeSuitability(float Value, float Minimum, float Maximum, float Softness);
};
