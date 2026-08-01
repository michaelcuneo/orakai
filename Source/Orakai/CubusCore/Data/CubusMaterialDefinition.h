#pragma once

#include "CoreMinimal.h"
#include "CubusMaterialDefinition.generated.h"

class UMaterialInterface;
class UTexture2D;

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
 * PBR texture set used by one face group of a block material.
 * ORM uses red=ambient occlusion, green=roughness and blue=metallic.
 */
USTRUCT(BlueprintType)
struct ORAKAI_API FCubusBlockSurfaceTextures
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PBR")
    TObjectPtr<UTexture2D> BaseColor = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PBR")
    TObjectPtr<UTexture2D> Normal = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PBR")
    TObjectPtr<UTexture2D> ORM = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "PBR")
    TObjectPtr<UTexture2D> Height = nullptr;

    FORCEINLINE bool HasAnyTexture() const
    {
        return
            IsValid(BaseColor.Get()) ||
            IsValid(Normal.Get()) ||
            IsValid(ORM.Get()) ||
            IsValid(Height.Get());
    }
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

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Rendering"
    )
    bool bRenderable = false;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Rendering"
    )
    bool bOccludesBlockFaces = false;

    /**
     * Parent material used for this block. Point this at M_CubusBlockPBR.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Rendering"
    )
    TObjectPtr<UMaterialInterface> Material = nullptr;

    /** Side is also the fallback for empty top or bottom texture sets. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering|PBR")
    FCubusBlockSurfaceTextures SideSurface;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering|PBR")
    FCubusBlockSurfaceTextures TopSurface;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering|PBR")
    FCubusBlockSurfaceTextures BottomSurface;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Rendering|PBR",
        meta = (ClampMin = "0.01")
    )
    float TextureScale = 1.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Rendering|PBR",
        meta = (ClampMin = "0.0")
    )
    float HeightStrength = 0.25f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering|PBR")
    FLinearColor Tint = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering|PBR")
    FLinearColor EmissiveColor = FLinearColor::Black;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Rendering|PBR",
        meta = (ClampMin = "0.0")
    )
    float EmissiveStrength = 0.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Rendering|PBR",
        meta = (ClampMin = "0.0", ClampMax = "1.0")
    )
    float SideTopBlendStart = 0.7f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "Rendering|PBR",
        meta = (ClampMin = "0.01")
    )
    float SideTopBlendSharpness = 4.0f;

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

    FORCEINLINE bool UsesPbrTextures() const
    {
        return
            SideSurface.HasAnyTexture() ||
            TopSurface.HasAnyTexture() ||
            BottomSurface.HasAnyTexture();
    }

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
