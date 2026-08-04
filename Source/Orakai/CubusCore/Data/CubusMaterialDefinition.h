#pragma once

#include "CoreMinimal.h"
#include "CubusMaterialDefinition.generated.h"

class UMaterialInterface;
class UTexture2D;

UENUM(BlueprintType)
enum class ECubusMatterState : uint8
{
    Empty  UMETA(DisplayName = "Empty"),
    Solid  UMETA(DisplayName = "Solid"),
    Liquid UMETA(DisplayName = "Liquid"),
    Gas    UMETA(DisplayName = "Gas")
};

/**
 * Rendering data for one terrain material used by both smooth density terrain
 * and block-shaped terrain edits. Construction pieces will use a separate
 * building-material asset and are intentionally not represented here.
 */
USTRUCT(BlueprintType)
struct ORAKAI_API FCubusDensitySurfaceTextures
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Textures")
    TObjectPtr<UTexture2D> BaseColor = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Textures")
    TObjectPtr<UTexture2D> Normal = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Textures")
    TObjectPtr<UTexture2D> ORM = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Textures")
    TObjectPtr<UTexture2D> Height = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Textures")
    TObjectPtr<UTexture2D> MacroColor = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Textures")
    TObjectPtr<UTexture2D> DetailNormal = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projection", meta = (ClampMin = "0.0001"))
    float WorldScale = 0.01f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projection", meta = (ClampMin = "0.1"))
    float TriplanarSharpness = 6.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Macro Detail", meta = (ClampMin = "0.000001"))
    float MacroScale = 0.0005f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Macro Detail", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float MacroStrength = 0.2f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Micro Detail", meta = (ClampMin = "0.0001"))
    float DetailScale = 0.08f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Micro Detail", meta = (ClampMin = "0.0"))
    float DetailNormalStrength = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blending", meta = (ClampMin = "0.0"))
    float HeightStrength = 0.35f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Blending", meta = (ClampMin = "0.01"))
    float BlendContrast = 4.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    FLinearColor Tint = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance")
    FLinearColor EmissiveColor = FLinearColor::Black;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Appearance", meta = (ClampMin = "0.0"))
    float EmissiveStrength = 0.0f;

    FORCEINLINE bool HasAnyTexture() const
    {
        return IsValid(BaseColor.Get()) || IsValid(Normal.Get()) ||
            IsValid(ORM.Get()) || IsValid(Height.Get()) ||
            IsValid(MacroColor.Get()) || IsValid(DetailNormal.Get());
    }
};

USTRUCT(BlueprintType)
struct ORAKAI_API FCubusMaterialDefinition
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity", meta = (ClampMin = "0", ClampMax = "65535"))
    int32 MaterialId = 0;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FName Name = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physical")
    ECubusMatterState State = ECubusMatterState::Empty;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering")
    bool bRenderable = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering")
    bool bOccludesBlockFaces = false;

    /** Optional fallback material used when the generated terrain material is unavailable. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering")
    TObjectPtr<UMaterialInterface> Material = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rendering", meta = (DisplayName = "Terrain Surface", ShowOnlyInnerProperties))
    FCubusDensitySurfaceTextures DensitySurface;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physical", meta = (ClampMin = "0.0"))
    float Density = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Physical", meta = (ClampMin = "0.0"))
    float Hardness = 0.0f;

    FORCEINLINE bool UsesDensityTextures() const
    {
        return DensitySurface.HasAnyTexture();
    }

    FORCEINLINE bool IsEmpty() const { return State == ECubusMatterState::Empty; }
    FORCEINLINE bool IsSolid() const { return State == ECubusMatterState::Solid; }
    FORCEINLINE bool IsLiquid() const { return State == ECubusMatterState::Liquid; }
    FORCEINLINE bool IsGas() const { return State == ECubusMatterState::Gas; }
};
