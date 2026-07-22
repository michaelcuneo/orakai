#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "CubusGeologyProfile.generated.h"

USTRUCT(BlueprintType)
struct FCubusStrataLayer
{
    GENERATED_BODY()

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Geology"
    )
    int32 MaterialId = 1;

    /*
     * Depth beneath the generated terrain surface where
     * this layer begins.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Geology",
        meta = (
            ClampMin = "0",
            UIMin = "0",
            UIMax = "512"
        )
    )
    int32 MinimumDepth = 0;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Geology",
        meta = (
            ClampMin = "1",
            UIMin = "1",
            UIMax = "512"
        )
    )
    int32 MaximumDepth = 32;
};

USTRUCT(BlueprintType)
struct FCubusOreRule
{
    GENERATED_BODY()

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Geology"
    )
    int32 MaterialId = 1;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Geology"
    )
    int32 MinimumWorldZ = -256;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Geology"
    )
    int32 MaximumWorldZ = 32;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Geology",
        meta = (
            ClampMin = "0.000001",
            UIMin = "0.001",
            UIMax = "0.5"
        )
    )
    float Frequency = 0.04f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Geology",
        meta = (
            ClampMin = "-1.0",
            ClampMax = "1.0",
            UIMin = "-1.0",
            UIMax = "1.0"
        )
    )
    float Threshold = 0.75f;

    /*
     * Only replace these base materials.
     * Empty means any solid terrain material.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category = "Cubus|Geology"
    )
    TArray<int32> ReplaceableMaterialIds;
};

UCLASS(
    BlueprintType,
    ClassGroup = "Cubus",
    meta = (DisplayName = "Cubus Geology Profile")
)
class ORAKAI_API UCubusGeologyProfile : public UDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Cubus|Geology"
    )
    TArray<FCubusStrataLayer> StrataLayers;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Cubus|Geology"
    )
    TArray<FCubusOreRule> OreRules;
};