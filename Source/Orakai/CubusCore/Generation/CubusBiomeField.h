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

	/* Legacy river-noise controls retained for asset compatibility. */
	float RiverFrequency = 0.0025f;
	float RiverWarpAmplitude = 48.0f;
	float RiverWarpFrequency = 0.006f;
	int32 RiverOffsetX = 0;
	int32 RiverOffsetY = 0;

	/** Authoritative terrain-derived drainage context, bound by density generation. */
	FCubusHydrologySettings HydrologySettings;

	/** Authored biome envelopes. These are first-class biome selectors. */
	TArray<FCubusBiomeDefinition> Definitions;
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
	float			SurfaceWorldZ		 = 0.0f;
	float			Slope				 = 0.0f;
	int32			SurfaceMaterialId	 = 1;
	int32			BiomeDefinitionIndex = INDEX_NONE;
};

/**
 * Deterministic biome engine for the density world.
 *
 * Climate signals are combined with the real density surface, terrain slope
 * and the same hydrology context used to carve the world. Legacy
 * Plains/Forest/Rocky/Wetland values remain broad ecology archetypes for
 * consumers such as vegetation, while authored definitions are the actual
 * selectable biomes.
 */
class ORAKAI_API FCubusBiomeField
{
public:
	static FCubusBiomeFieldSettings MakeSettings(const UCubusGeologyProfile* GeologyProfile, int32 BiomeSeed, int32 RiverSeed);

	/** Bind the exact hydrology context used by density terrain. */
	static FCubusBiomeFieldSettings BindHydrology(
		const FCubusBiomeFieldSettings& BaseSettings,
		const FCubusHydrologySettings& HydrologySettings
	);

	static FCubusBiomeSample Sample(float WorldX, float WorldY, float SurfaceWorldZ, float Slope, const FCubusBiomeFieldSettings& Settings);

	/** Normalized hydrological distance shared by carving and biome rules. */
	static float SampleRiverDistance(float WorldX, float WorldY, const FCubusBiomeFieldSettings& Settings);

private:
	static float SampleNoise(float WorldX, float WorldY, float Frequency);

	static float SampleFbm(float WorldX, float WorldY, float Frequency, int32 Octaves, float Gain);

	static float SmoothStep(float EdgeMinimum, float EdgeMaximum, float Value);
};
