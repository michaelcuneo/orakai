#pragma once

#include "CoreMinimal.h"

#include "CubusBiomeTypes.generated.h"

/** Broad habitat behaviour used by vegetation and gameplay rules. */
UENUM(BlueprintType)
enum class ECubusBiomeKind : uint8
{
	Plains,
	Forest,
	Rocky,
	Wetland
};

/**
 * One client-authored biome definition.
 *
 * Archetype is deliberately broad: it controls generic vegetation/gameplay
 * behaviour, while Name identifies the actual ecological community (for
 * example MontaneForest, RiparianForest, AlpineMeadow or Marsh).
 *
 * Climate uses target/tolerance envelopes. Terrain/environment controls use
 * inclusive ranges so old assets remain permissive by default while authored
 * biomes can opt into much more specific niches.
 */
USTRUCT(BlueprintType)
struct ORAKAI_API FCubusBiomeDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes")
	FName Name = NAME_None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes")
	ECubusBiomeKind Archetype = ECubusBiomeKind::Plains;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes", meta = (ClampMin = "1"))
	int32 SurfaceMaterialId = 1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Climate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TargetMoisture = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Climate", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float MoistureTolerance = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Climate", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TargetTemperature = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Climate", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float TemperatureTolerance = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Terrain")
	float MinimumWorldZ = -100000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Terrain")
	float MaximumWorldZ = 100000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Terrain", meta = (ClampMin = "0.0"))
	float MaximumSlope = 2.0f;

	/** Normalized accumulated/depositional soil depth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Terrain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumSoilDepth = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Terrain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaximumSoilDepth = 1.0f;

	/** 0 = dry/ridge-like, 1 = concentrated drainage/floodplain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Hydrology", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumDrainage = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Hydrology", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaximumDrainage = 1.0f;

	/** 0 = no river influence, 1 = channel/floodplain centre. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Hydrology", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumRiverInfluence = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Hydrology", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaximumRiverInfluence = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Exposure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumRockExposure = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Exposure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaximumRockExposure = 1.0f;

	/** Long-term wind/topographic exposure. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Exposure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumExposure = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Exposure", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaximumExposure = 1.0f;

	/** Combined soil, water and climate productivity estimate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Terrain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MinimumFertility = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes|Terrain", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MaximumFertility = 1.0f;

	/** Width of the soft transition at environmental range boundaries. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes", meta = (ClampMin = "0.01", ClampMax = "0.5"))
	float TransitionSoftness = 0.12f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes", meta = (ClampMin = "0.01"))
	float Priority = 1.0f;
};
