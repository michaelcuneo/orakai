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
 * One client-authored biome definition. There is no fixed definition count;
 * several definitions may share an archetype while using different climate
 * envelopes and surface materials.
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float TargetMoisture = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes", meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float MoistureTolerance = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float TargetTemperature = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes", meta = (ClampMin = "0.01", ClampMax = "1.0"))
    float TemperatureTolerance = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes")
    float MinimumWorldZ = -100000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes")
    float MaximumWorldZ = 100000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes", meta = (ClampMin = "0.0"))
    float MaximumSlope = 1000.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Biomes", meta = (ClampMin = "0.01"))
    float Priority = 1.0f;
};
