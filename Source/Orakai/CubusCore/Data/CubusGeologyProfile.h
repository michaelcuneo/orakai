#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "CubusGeologyProfile.generated.h"

USTRUCT(BlueprintType)
struct FCubusStrataLayer
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Geology")
    int32 MaterialId = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Geology", meta = (ClampMin = "0", UIMin = "0", UIMax = "512"))
    int32 MinimumDepth = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Geology", meta = (ClampMin = "1", UIMin = "1", UIMax = "512"))
    int32 MaximumDepth = 32;
};

USTRUCT(BlueprintType)
struct FCubusOreRule
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Geology")
    int32 MaterialId = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Geology")
    int32 NoiseSeed = 193;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Geology")
    int32 MinimumWorldZ = -256;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Geology")
    int32 MaximumWorldZ = 32;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Geology", meta = (ClampMin = "0.000001", UIMin = "0.001", UIMax = "0.5"))
    float Frequency = 0.04f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Geology", meta = (ClampMin = "-1.0", ClampMax = "1.0", UIMin = "-1.0", UIMax = "1.0"))
    float Threshold = 0.75f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cubus|Geology")
    TArray<int32> ReplaceableMaterialIds;
};

UCLASS(BlueprintType, ClassGroup = "Cubus", meta = (DisplayName = "Cubus Geology Profile"))
class ORAKAI_API UCubusGeologyProfile : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Strata")
    TArray<FCubusStrataLayer> StrataLayers;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Rivers")
    bool bGenerateRivers = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Rivers", meta = (ClampMin = "0.000001", UIMin = "0.0001", UIMax = "0.02"))
    float RiverFrequency = 0.0025f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Rivers", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "0.5"))
    float RiverChannelWidth = 0.055f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Rivers", meta = (ClampMin = "0.001", ClampMax = "1.0", UIMin = "0.01", UIMax = "1.0"))
    float RiverValleyWidth = 0.22f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Rivers", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "32.0"))
    float RiverValleyDepth = 7.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Rivers", meta = (ClampMin = "1", UIMin = "1", UIMax = "24"))
    int32 RiverChannelDepth = 4;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Rivers", meta = (ClampMin = "1", UIMin = "1", UIMax = "8"))
    int32 RiverWaterDepth = 2;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Rivers", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "256.0"))
    float RiverWarpAmplitude = 48.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Rivers", meta = (ClampMin = "0.000001", UIMin = "0.0001", UIMax = "0.05"))
    float RiverWarpFrequency = 0.006f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Rivers")
    int32 RiverbedMaterialId = 3;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Rivers")
    int32 RiverWaterMaterialId = 5;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Biomes")
    bool bGenerateBiomes = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Biomes", meta = (ClampMin = "0.000001", UIMin = "0.0001", UIMax = "0.05"))
    float BiomeFrequency = 0.004f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Biomes", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float ForestThreshold = 0.15f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Biomes", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "16.0"))
    float WetlandRiverDistance = 0.10f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Biomes", meta = (ClampMin = "0.0", UIMin = "0.0", UIMax = "32.0"))
    float RockySlopeThreshold = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Biomes")
    int32 RockyMinimumWorldZ = 48;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Biomes")
    int32 PlainsSurfaceMaterialId = 1;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Biomes")
    int32 ForestSurfaceMaterialId = 7;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Biomes")
    int32 RockySurfaceMaterialId = 3;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Biomes")
    int32 WetlandSurfaceMaterialId = 8;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Caves")
    bool bGenerateCaves = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Caves")
    int32 CaveMinimumWorldZ = -256;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Caves")
    int32 CaveMaximumWorldZ = 24;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Caves", meta = (ClampMin = "1", UIMin = "1", UIMax = "32"))
    int32 CaveSurfaceClearance = 5;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Caves", meta = (ClampMin = "0.000001", UIMin = "0.005", UIMax = "0.2"))
    float CavePrimaryFrequency = 0.035f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Caves", meta = (ClampMin = "0.000001", UIMin = "0.005", UIMax = "0.2"))
    float CaveSecondaryFrequency = 0.07f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Caves", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0"))
    float CaveThreshold = 0.16f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cubus|Geology|Ore")
    TArray<FCubusOreRule> OreRules;
};
