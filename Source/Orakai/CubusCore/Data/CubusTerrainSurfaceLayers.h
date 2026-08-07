#pragma once

#include "CoreMinimal.h"

#include "CubusTerrainSurfaceLayers.generated.h"

/**
 * Terrain-wide distribution rules. These settings decide where terrain
 * materials and surface objects may appear; they do not control the visual
 * appearance of an individual material.
 */
USTRUCT(BlueprintType)
struct ORAKAI_API FCubusTerrainSurfaceLayerSettings
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material Distribution", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float GrassMinimumNormalZ = 0.72f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material Distribution", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float RockMaximumNormalZ = 0.58f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material Distribution", meta = (ClampMin = "0.001", ClampMax = "1.0"))
    float SlopeBlendWidth = 0.12f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material Distribution", meta = (ClampMin = "0.0", Units = "cm"))
    float SandMaximumWorldHeight = 400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material Distribution", meta = (ClampMin = "0.0", Units = "cm"))
    float SnowMinimumWorldHeight = 6000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material Distribution", meta = (ClampMin = "1.0", Units = "cm"))
    float HeightBlendWidth = 400.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clutter", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float GrassClutterDensity = 0.18f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clutter", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float StoneClutterDensity = 0.045f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clutter", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float OrganicClutterDensity = 0.025f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clutter", meta = (ClampMin = "10.0", Units = "cm"))
    float ClutterMinimumSpacing = 65.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Clutter", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float ClutterMaximumSlopeNormalZ = 0.62f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Features", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float BoulderDensity = 0.008f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Features", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float OutcropDensity = 0.003f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Features", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float FallenLogDensity = 0.0015f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Features", meta = (ClampMin = "100.0", Units = "cm"))
    float FeatureMinimumSpacing = 900.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Features", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float CliffMaximumNormalZ = 0.42f;
};

struct ORAKAI_API FCubusTerrainSurfaceLayerMasks
{
    float Flat = 0.0f;
    float Steep = 0.0f;
    float Sand = 0.0f;
    float Snow = 0.0f;
    float GrassClutter = 0.0f;
    float StoneClutter = 0.0f;
    float OrganicClutter = 0.0f;
    float Boulder = 0.0f;
    float Outcrop = 0.0f;
    float FallenLog = 0.0f;
};

ORAKAI_API FCubusTerrainSurfaceLayerMasks EvaluateCubusTerrainSurfaceLayers(
    const FCubusTerrainSurfaceLayerSettings& Settings,
    const FVector& WorldPosition,
    const FVector& WorldNormal,
    int32 WorldSeed
);
