#pragma once

#include "CoreMinimal.h"
#include "CubusMaterialDefinition.generated.h"

class UMaterialInterface;

/**
 * Broad physical state of a voxel material.
 */
UENUM(BlueprintType)
enum class ECubusMatterState : uint8
{
    Empty  UMETA(DisplayName = "Empty"),
    Solid  UMETA(DisplayName = "Solid"),
    Liquid UMETA(DisplayName = "Liquid"),
    Gas    UMETA(DisplayName = "Gas")
};

/**
 * Shared definition referenced by voxel MaterialId.
 */
USTRUCT(BlueprintType)
struct ORAKAI_API FCubusMaterialDefinition
{
    GENERATED_BODY()

public:
    /**
     * Stable identifier stored inside voxel data.
     *
     * ID 0 is reserved for empty air.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Identity",
        meta = (
            ClampMin = "0",
            ClampMax = "65535"
        )
    )
    int32 MaterialId = 0;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Identity"
    )
    FName Name = NAME_None;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Identity"
    )
    FText DisplayName;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Physical"
    )
    ECubusMatterState State = ECubusMatterState::Empty;

    /**
     * Whether this material currently generates block geometry.
     *
     * Liquids and gases can exist in storage before their renderers exist.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Rendering"
    )
    bool bRenderable = false;

    /**
     * Whether this material hides a neighbouring block face.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Rendering"
    )
    bool bOccludesBlockFaces = false;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Rendering"
    )
    TObjectPtr<UMaterialInterface> Material = nullptr;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Physical",
        meta = (ClampMin = "0.0")
    )
    float Density = 0.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Physical",
        meta = (ClampMin = "0.0")
    )
    float Hardness = 0.0f;

    FORCEINLINE bool IsEmpty() const
    {
        return State == ECubusMatterState::Empty;
    }

    FORCEINLINE bool IsSolid() const
    {
        return State == ECubusMatterState::Solid;
    }

    FORCEINLINE bool IsLiquid() const
    {
        return State == ECubusMatterState::Liquid;
    }

    FORCEINLINE bool IsGas() const
    {
        return State == ECubusMatterState::Gas;
    }
};